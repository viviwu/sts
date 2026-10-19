// clang-format off
/**
  ******************************************************************************
  * @file           : server_main.cpp
  * @author         : vivi wu
  * @brief          : qts_server single-process trading server entry (feed, strategy, OMS, ledger).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#ifndef _WIN32
  #include <execinfo.h>
  #include <unistd.h>
#endif

#if defined(_WIN32)
  #include <process.h>
#else
  #include <unistd.h>
#endif

#include "nlohmann/json.hpp"
#include "qts/broker/channel_factory.hpp"
#include "qts/broker/simulated_ama_feed.hpp"
#include "qts/config/config.hpp"
#include "qts/config/credentials.hpp"
#include "qts/execution/execution_api.hpp"
#include "qts/gateway/gateway_service.hpp"
#include "qts/gateway/gateway_transport.hpp"
#include "qts/market/quote_feed.hpp"
#include "qts/oms/order_manager.hpp"
#include "qts/ops/admin_socket.hpp"
#include "qts/ops/control.hpp"
#include "qts/ops/log.hpp"
#include "qts/ops/metrics.hpp"
#include "qts/ops/metrics_endpoint.hpp"
#include "qts/persistence/event_store.hpp"
#include "qts/persistence/file_ledger.hpp"
#include "qts/persistence/ledger.hpp"
#include "qts/persistence/sqlite_store.hpp"
#include "qts/research/ledger_verify.hpp"
#include "qts/research/report.hpp"
#include "qts/strategy/grid_strategy.hpp"
#include "qts/strategy/rebalance_strategy.hpp"
#ifdef QTS_HAS_VENDOR_ADAPTERS
  // Vendor-gated (adapters/ only builds with QTS_ENABLE_VENDOR_SDKS=ON):
  // these headers stay vendor-free (pimpl), core never sees vendor headers.
  #include "ama_quote_adapter.hpp"
  #include "qts/config/credentials.hpp"
  #include "quantapi_trade_adapter.hpp"
#endif

namespace {

// SIGINT/SIGTERM stop flag (M9-0): the demo loop polls it every tick and
// shuts down gracefully (final reconcile -> report -> exit 0). Mutable by
// necessity: POSIX signal handlers may only touch sig_atomic_t globals.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile std::sig_atomic_t g_stop_flag = 0;

// SIGHUP reload flag: re-reads risk limits/switches and the log level from
// the config file without restarting (checked every demo tick; the
// one-shot drill never reloads).
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile std::sig_atomic_t g_reload_flag = 0;

void on_stop_signal(int /*signal*/) { g_stop_flag = 1; }

void on_reload_signal(int /*signal*/) { g_reload_flag = 1; }

#ifndef _WIN32
// Crash telemetry: a segfault/abort prints a backtrace to stderr (which the
// operator redirects to a file) instead of dying silent. Async-signal-safe
// only (backtrace + fd write); the process then re-raises to keep the
// core-dump behavior for deeper forensics.
void on_crash_signal(int signal) {
  char header[64]{0};
  const char* name = signal == SIGSEGV ? "SIGSEGV" : "SIGABRT";
  int length = 0;
  while (name[length] != '\0') ++length;
  const char prefix[] = "FATAL: qts_server crashed on ";
  for (std::size_t i = 0; i < sizeof(prefix) - 1; ++i) header[i] = prefix[i];
  for (int i = 0; i < length; ++i) header[sizeof(prefix) - 1 + i] = name[i];
  header[sizeof(prefix) - 1 + length] = '\n';
  (void)::write(STDERR_FILENO, header, sizeof(prefix) + static_cast<std::size_t>(length));
  void* frames[32]{nullptr};
  const int depth = ::backtrace(frames, 32);
  ::backtrace_symbols_fd(frames, depth, STDERR_FILENO);
  struct sigaction def {};
  def.sa_handler = SIG_DFL;
  (void)::sigaction(signal, &def, nullptr);
  (void)::raise(signal);
}

void arm_crash_handler() {
  struct sigaction action {};
  action.sa_handler = on_crash_signal;
  (void)::sigemptyset(&action.sa_mask);
  action.sa_flags = SA_RESETHAND | SA_NODEFER;
  (void)::sigaction(SIGSEGV, &action, nullptr);
  (void)::sigaction(SIGABRT, &action, nullptr);
}
#endif

// Compact calendar date for run-unique order-id prefixes ("2026-10-02" ->
// "20261002"); non-date characters pass through untouched.
std::string compact_date(const std::string& trade_date) {
  std::string compact;
  for (const char mark : trade_date) {
    if (mark != '-') compact += mark;
  }
  return compact;
}

// Trading-day label for the ledger envelopes (local calendar date at
// startup; multi-day runs restamp on settle_day in the future).
std::string today_label() {
  const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  char buf[16]{0};
  if (std::strftime(buf, sizeof(buf), "%Y-%m-%d", &local) == 0) return {};
  return std::string{buf};
}

// Structured logger (M9-1/M9.2): library-owned dual sink, always stdout,
// optionally tee'd into a daily-rotating unique file derived from log.file.
// Each line is flushed so a second terminal can tail it while running.
using qts::log::Logger;

// Debug field helpers (M9.3): short enum names so debug logs stay readable.
// Every call site below uses debug level, so the default info-level stdout
// stays byte-identical (drill counts, progress lines, JSON shape).
const char* side_name(qts::Side side) { return side == qts::Side::buy ? "buy" : "sell"; }

const char* status_name(qts::OrderStatus status) {
  switch (status) {
    case qts::OrderStatus::pending_new:
      return "pending_new";
    case qts::OrderStatus::live:
      return "live";
    case qts::OrderStatus::partially_filled:
      return "partially_filled";
    case qts::OrderStatus::filled:
      return "filled";
    case qts::OrderStatus::pending_cancel:
      return "pending_cancel";
    case qts::OrderStatus::cancelled:
      return "cancelled";
    case qts::OrderStatus::rejected:
      return "rejected";
  }
  return "unknown";
}

std::vector<qts::log::Field> intent_fields(const qts::OrderIntent& intent) {
  return {{"client_order_id", intent.client_order_id},         {"instrument", intent.instrument_id}, {"side", side_name(intent.side)}, {"quantity", std::to_string(intent.quantity)},
          {"limit_price", std::to_string(intent.limit_price)}, {"trace_id", intent.trace_id}};
}

void log_report(Logger& log, const qts::ExecutionReport& report) {
  log.debug("broker", "execution_report",
            {{"client_order_id", report.client_order_id},
             {"status", status_name(report.status)},
             {"last_quantity", std::to_string(report.last_quantity)},
             {"last_price", std::to_string(report.last_price)},
             {"reason", report.reason}});
}

// Async SQLite projector (M9-3): keeps the hot path free of DB I/O by
// pushing ledger events into a worker queue. Startup resumes above the
// stored watermark; replays converge idempotently. Backend-agnostic: the
// factory picks SQLite (current) or PostgreSQL (formal phase) from config.
struct SqliteProjector {
  std::unique_ptr<qts::AsyncEventSink> sink;
  std::uint64_t last_seen{0};

  void maybe_enable(const std::string& sqlite_file, const std::string& pg_conninfo, qts::EventLedger* ledger) {
    auto store = qts::open_event_store(sqlite_file, pg_conninfo);
    if (store == nullptr) return;
    sink = std::make_unique<qts::AsyncEventSink>(std::move(store));
    // Resume above whatever the projection already holds.
    push_new(ledger);
  }

  void push_new(qts::EventLedger* ledger) {
    if (sink == nullptr) return;
    auto fresh = ledger->replay(last_seen + 1);
    for (auto& event : fresh) sink->push(std::move(event));
    last_seen = ledger->latest_event_id();
  }

  // Cannot be const: draining the queue and joining the worker thread
  // mutate the sink by design.
  // NOLINTNEXTLINE(readability-make-member-function-const)
  void finish() {
    if (sink == nullptr) return;
    sink->wait_empty();
    sink->shutdown();
  }
};

void sleep_interruptible(std::uint64_t interval_ms) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{interval_ms};
  while (g_stop_flag == 0 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
  }
}

// Strategy selection from config: first enabled entry wins; grid is the
// only type today, unknown types fail closed (validated earlier as well).
// The id prefix makes parent ids unique across restarts (strategy/date/pid)
// so recovered books never collide with a fresh run's numbering.
// Venue identity (exchange/board/security account) is derived from the
// instruments master + accounts table so ashare validation passes once the
// master is configured; without a master entry the legacy unknown identity
// is kept and the venue-agnostic path applies.
// Per-target venue identity resolves from the instrument master (unknown
// + default lot when the master is absent, mirroring the grid fallback);
// the security identity is shared account-wide.
qts::RebalanceTarget resolve_rebalance_target(const qts::StrategyConfig::TargetWeight& entry, const qts::AppConfig& config, const qts::AccountConfig& account) {
  qts::RebalanceTarget resolved{};
  resolved.instrument_id = entry.instrument;
  resolved.weight_bps = entry.weight_bps;
  resolved.security_account_id = account.security_account_id;
  for (const auto& master : config.instruments) {
    if (master.id != entry.instrument) continue;
    resolved.exchange = qts::parse_exchange(master.exchange);
    resolved.board = qts::parse_board(master.board);
    resolved.lot_step = master.lot_step;
    break;
  }
  return resolved;
}

std::unique_ptr<qts::Strategy> make_rebalance_strategy(const qts::StrategyConfig& configured, const qts::AppConfig& config, const std::string& id_prefix) {
  for (const auto& account : config.accounts) {
    if (account.id != configured.account) continue;
    qts::RebalanceConfig rebalancing{};
    rebalancing.strategy_id = configured.id;
    rebalancing.account_id = configured.account;
    rebalancing.band_bps = configured.rebalance_band_bps;
    rebalancing.id_prefix = id_prefix;
    for (const auto& target : configured.rebalance_targets) rebalancing.targets.push_back(resolve_rebalance_target(target, config, account));
    if (rebalancing.targets.empty()) break;  // no targets: fail below, never trust the caller
    return std::make_unique<qts::RebalanceStrategy>(std::move(rebalancing));
  }
  // Unknown account (fail-closed) or no targets (rejected earlier by
  // validation, but never trust the caller here).
  throw std::runtime_error("rebalance strategy has no resolvable targets: " + configured.id);
}

std::unique_ptr<qts::Strategy> make_strategy(const qts::StrategyConfig& configured, const qts::AppConfig& config, const std::string& id_prefix) {
  if (configured.type == "rebalance") return make_rebalance_strategy(configured, config, id_prefix);
  if (configured.type != "grid") throw std::runtime_error("unknown strategy type: " + configured.type);
  qts::GridConfig grid{};
  grid.strategy_id = configured.id;
  grid.account_id = configured.account;
  grid.instrument_id = configured.instrument;
  for (const auto& entry : config.instruments) {
    if (entry.id == configured.instrument) {
      grid.exchange = qts::parse_exchange(entry.exchange);
      grid.board = qts::parse_board(entry.board);
      break;
    }
  }
  for (const auto& account : config.accounts) {
    if (account.id == configured.account) {
      grid.security_account_id = account.security_account_id;
      break;
    }
  }
  grid.lower = configured.lower;
  grid.upper = configured.upper;
  grid.grids = configured.grids;
  grid.quantity_per_order = configured.quantity;
  grid.id_prefix = id_prefix;
  return std::make_unique<qts::GridStrategy>(grid, configured.start);
}

std::unique_ptr<qts::ExecutionAlgorithm> make_execution(const qts::ExecutionConfig& configured) {
  if (configured.algo == "immediate") return std::make_unique<qts::ImmediateAlgorithm>();
  if (configured.algo == "twap") {
    return std::make_unique<qts::TwapAlgorithm>(qts::TwapConfig{configured.twap_slices, std::chrono::milliseconds{configured.twap_interval_ms}});
  }
  if (configured.algo == "pov") return std::make_unique<qts::PovAlgorithm>(qts::PovConfig{configured.pov_participation_bps});
  if (configured.algo == "vwap") {
    return std::make_unique<qts::VwapAlgorithm>(qts::VwapConfig{configured.vwap_weights, std::chrono::milliseconds{configured.vwap_interval_ms}});
  }
  if (configured.algo == "iceberg") return std::make_unique<qts::IcebergAlgorithm>(qts::IcebergConfig{configured.iceberg_display});
  throw std::runtime_error("unknown execution algo: " + configured.algo);
}

// Shared per-tick/per-bar runtime for the drill and demo paths below is
// threaded through as explicit parameters: a context struct with reference
// members trips avoid-const-or-ref-data-members, and pointers only hide it.
// Rollover clock state: armed once at startup, re-armed after each firing.
struct DayRollover {
  int hour{0};
  int minute{1};
  std::string trade_date;
  std::time_t next{0};
};

void feed_reference_all(qts::ChannelSet& channels, const qts::InstrumentId& symbol, qts::Price price) {
  for (auto& channel : channels.channels) {
    if (static_cast<bool>(channel.set_reference_price)) channel.set_reference_price(symbol, price);
  }
}

void drain_all(qts::ChannelSet& channels) {
  for (auto& channel : channels.channels) {
    if (static_cast<bool>(channel.drain)) channel.drain();
    if (static_cast<bool>(channel.deliver_due)) channel.deliver_due();
  }
}

// Synthetic standing depth around the reference price (demo only): a
// symmetric ladder so M6 book matching (partial fills, resting orders,
// re-matching on fresh depth) actually runs in production instead of only
// in unit tests. Levels must arrive best-first per the OrderBook contract.
qts::OrderBook make_depth(qts::Price last, std::uint64_t levels, qts::Quantity quantity, qts::Price tick) {
  qts::OrderBook book{};
  for (std::uint64_t level = 1; level <= levels; ++level) {
    const qts::Price distance = static_cast<qts::Price>(level) * tick;
    book.bids.push_back({last - distance, quantity});
    book.asks.push_back({last + distance, quantity});
  }
  return book;
}

void post_depth(qts::ChannelSet& channels, const qts::AppConfig& config, const qts::InstrumentId& symbol, qts::Price last) {
  if (config.demo.depth_levels == 0) return;
  qts::OrderBook book = make_depth(last, config.demo.depth_levels, config.demo.depth_quantity, config.demo.depth_tick);
  for (auto& channel : channels.channels) {
    if (static_cast<bool>(channel.set_order_book)) channel.set_order_book(symbol, book);
  }
}

// AMA book source (M11-2): the quote already carries production L10 depth,
// so the paper channel matches against it instead of a synthetic ladder.
// Best-first order is preserved from the normalizer contract.
void post_quote_depth(qts::ChannelSet& channels, const qts::QuoteEvent& quote) {
  if (quote.bid_levels.empty() || quote.ask_levels.empty()) return;
  qts::OrderBook book{};
  book.bids.reserve(quote.bid_levels.size());
  book.asks.reserve(quote.ask_levels.size());
  for (const auto& level : quote.bid_levels) book.bids.push_back({level.price, level.quantity});
  for (const auto& level : quote.ask_levels) book.asks.push_back({level.price, level.quantity});
  for (auto& channel : channels.channels) {
    if (static_cast<bool>(channel.set_order_book)) channel.set_order_book(quote.instrument_id, book);
  }
}

// AMA symbol derivation (M11-2): one walk per master entry, CODE.SUFFIX
// validated at load. Mid-band start, tick-scaled step clamped to the range.
qts::SimulatedAmaSymbol ama_symbol_from(const qts::InstrumentConfig& entry) {
  const std::size_t dot = entry.id.find('.');
  if (dot == std::string::npos || dot + 1 >= entry.id.size()) throw std::runtime_error("ama feed instrument id must be CODE.SUFFIX: " + entry.id);
  const std::string code = entry.id.substr(0, dot);
  const std::string suffix = entry.id.substr(dot + 1);
  int market = 0;
  if (suffix == "SSE") {
    market = 101;
  } else if (suffix == "SZSE") {
    market = 102;
  } else if (suffix == "BSE") {
    market = 2;
  } else {
    throw std::runtime_error("ama feed instrument suffix must be SSE|SZSE|BSE: " + entry.id);
  }
  qts::SimulatedAmaSymbol symbol{};
  symbol.market_type = market;
  symbol.security_code = code;
  symbol.low = entry.limit_down > 0 ? entry.limit_down : 800;
  symbol.high = entry.limit_up > 0 ? entry.limit_up : 1'200;
  if (symbol.high <= symbol.low) throw std::runtime_error("ama feed instrument needs limit_up > limit_down: " + entry.id);
  symbol.start = (entry.limit_down > 0 && entry.limit_up > 0) ? (entry.limit_down + entry.limit_up) / 2 : 1'000;
  symbol.step = entry.tick * 50;
  if (symbol.step <= 0) symbol.step = 1;
  const qts::Price span = symbol.high - symbol.low;
  if (symbol.step > span / 2) symbol.step = span / 2 > 0 ? span / 2 : 1;
  symbol.limit_down = entry.limit_down;
  symbol.limit_up = entry.limit_up;
  return symbol;
}

// Live state snapshot (zero-IPC ops): a small JSON file the admin CLI reads
// while the server runs. Complements the SQLite projection (which lags
// async appends) with counters, live opens and the current tick. Best
// effort: a failed write never fails the run. The same payload serves the
// query socket (admin_socket.hpp), so file and socket can never disagree.
std::string build_live_state(std::uint64_t tick, const std::string& trade_date, int submitted, int rejected, qts::OrderManager& oms, const qts::Portfolio& book, std::uint64_t ledger_events) {
  nlohmann::json state;
  state["tick"] = tick;
  state["trade_date"] = trade_date;
  state["submitted"] = submitted;
  state["rejected"] = rejected;
  state["ledger_events"] = ledger_events;
  state["cash"] = book.cash();
  nlohmann::json opens = nlohmann::json::array();
  for (const auto& order : oms.open_orders()) {
    opens.push_back({{"id", order.client_order_id}, {"status", status_name(order.status)}, {"cum", order.cumulative_quantity}});
  }
  state["open"] = std::move(opens);
  nlohmann::json positions = nlohmann::json::array();
  for (const auto& strategy : book.strategies()) {
    for (const auto& entry : book.all_positions_for(strategy)) {
      positions.push_back({{"strategy", strategy}, {"instrument", entry.first}, {"qty", entry.second.total}, {"avg", entry.second.average_cost}, {"realized", book.realized_for(strategy)}});
    }
  }
  state["positions"] = std::move(positions);
  return state.dump();
}

void write_live_state(
    const std::string& path, std::uint64_t tick, const std::string& trade_date, int submitted, int rejected, qts::OrderManager& oms, const qts::Portfolio& book, std::uint64_t ledger_events) {
  if (path.empty()) return;
  std::ofstream out{path, std::ios::trunc};
  if (out) out << build_live_state(tick, trade_date, submitted, rejected, oms, book, ledger_events) << '\n';
}

// Live config reload (SIGHUP): risk limits/switches/rate, the instrument
// master and the log level take effect without a restart. Anything
// structural (channels, strategy, ledger, rollover time) keeps requiring a
// restart and is ignored here by design. A bad file keeps the old config
// (fail closed, loudly).
void maybe_reload_config(Logger& log, const std::string& config_path, qts::RiskEngine& risk, const qts::SystemClock& wall_clock) {
  if (g_reload_flag == 0) return;
  g_reload_flag = 0;
  qts::AppConfig reloaded{};
  try {
    reloaded = qts::load_app_config(config_path);
  } catch (const std::exception& e) {
    log.error("server", "config reload rejected, keeping old", {{"error", e.what()}});
    return;
  }
  risk.set_limits(reloaded.risk);
  risk.set_switches({reloaded.switches.blocked_strategies.begin(), reloaded.switches.blocked_strategies.end()}, {reloaded.switches.blocked_accounts.begin(), reloaded.switches.blocked_accounts.end()},
                    {reloaded.switches.blocked_instruments.begin(), reloaded.switches.blocked_instruments.end()});
  risk.set_rate_limit(reloaded.risk.rate_max_orders, std::chrono::milliseconds{reloaded.risk.rate_window_ms}, &wall_clock);
  if (!reloaded.instruments.empty()) {
    // No allowlist (decision 2026-10-16): the master only carries
    // tick/lot/band validation, never a trade permission list.
    std::unordered_map<qts::InstrumentId, qts::AshareInstrumentRules> master;
    for (const auto& entry : reloaded.instruments) {
      master.emplace(entry.id, qts::instrument_rules_from(entry));
      if (entry.limit_down > 0 || entry.limit_up > 0) risk.set_price_band(entry.id, {entry.limit_down, entry.limit_up});
    }
    risk.set_ashare_master(std::move(master));
  } else {
    risk.clear_ashare_master();
  }
  qts::log::Level level = qts::log::Level::info;
  if (!qts::log::parse_level(reloaded.log_level, level)) level = qts::log::Level::info;
  qts::log::set_level(level);
  log.info("server", "config reloaded", {{"file", config_path}, {"log_level", reloaded.log_level}});
}

// Orderly shutdown: withdraw every working order so the run ends
// reconcilable (drill stays strict and never auto-cancels). Dispatch runs
// through the shared control plane (parents fan out through the execution
// algorithm first, stragglers fall to the direct sweep).
void cancel_all_open(Logger& log, qts::OrderManager& oms, qts::ExecutionAlgorithm& execution, qts::ChannelSet& channels) {
  if (oms.open_orders().empty()) return;
  log.info("server", "cancelling open orders", {{"count", std::to_string(oms.open_orders().size())}});
  const std::size_t done = qts::control::cancel_all_open(oms, execution);
  log.info("server", "cancel complete", {{"withdrawn", std::to_string(done)}});
  drain_all(channels);
}

// Operator inbox (M11-4, zero IPC): one JSON command per line, appended by
// qts_admin send while the server runs. Malformed lines are logged and
// skipped (never wedging the loop); a shrunk file resets the offset.
struct ControlInbox {
  std::string path;
  std::uint64_t offset{0};
  bool access_logged{false};

  void poll(Logger& log, qts::control::ControlContext& context) {
    if (path.empty()) return;
    std::ifstream input{path, std::ios::binary};
    if (!input) {
      if (!access_logged) {
        log.warn("server", "control inbox unreadable", {{"file", path}});
        access_logged = true;
      }
      return;
    }
    input.seekg(0, std::ios::end);
    const auto end = static_cast<std::uint64_t>(input.tellg());
    if (end < offset) offset = 0;  // rotated or replaced: start over
    input.seekg(static_cast<std::streamoff>(offset));
    std::string line;
    while (std::getline(input, line)) {
      offset += static_cast<std::uint64_t>(line.size()) + 1;
      if (line.empty()) continue;
      qts::control::ControlCommand command{};
      std::string error;
      if (!qts::control::parse_command(line, command, error)) {
        log.error("server", "control line rejected", {{"error", error}});
        continue;
      }
      const qts::control::ControlResult result = qts::control::apply_command(command, context);
      if (result.ok) {
        log.info("server", "control applied", {{"cmd", command.name}, {"result", result.message}});
      } else {
        log.warn("server", "control refused", {{"cmd", command.name}, {"result", result.message}});
      }
    }
    access_logged = false;
  }
};

// Broker-truth account snapshots (M14 settlement): request every account,
// collect through the TradeApi snapshot handler, bounded-wait for stragglers
// (paper answers inline; vendor channels complete async). Returns whatever
// arrived — callers report partial coverage instead of failing the verb,
// and a 5s timeout never hangs the tick loop.
qts::gateway::SnapshotBatch fetch_account_snapshots(Logger& log, qts::ChannelSet& channels, qts::TradeApi& trade, const std::vector<qts::AccountId>& accounts) {
  qts::gateway::SnapshotBatch batch{};
  std::vector<qts::TradeAccountSnapshot> collected;
  bool collecting = true;
  trade.set_account_snapshot_handler([&](const qts::TradeAccountSnapshot& snapshot) {
    if (collecting) collected.push_back(snapshot);
  });
  for (const auto& account : accounts) trade.request_account_snapshot(account);
  drain_all(channels);
  constexpr std::int64_t kSnapshotWaitMs = 5000;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{kSnapshotWaitMs};
  while (g_stop_flag == 0 && collected.size() < accounts.size() && std::chrono::steady_clock::now() < deadline) {
    drain_all(channels);
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
  }
  collecting = false;
  trade.set_account_snapshot_handler({});
  if (collected.size() < accounts.size()) {
    log.warn("server", "snapshot query incomplete", {{"collected", std::to_string(collected.size())}, {"accounts", std::to_string(accounts.size())}});
  }
  const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  char stamp[32]{0};
  std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
  batch.snapshots = std::move(collected);
  batch.captured_at = stamp;
  return batch;
}

// Known top-level accounts for gateway query validation (unknown accounts
// refuse loudly; empty keeps callers unfiltered).
std::vector<qts::AccountId> gateway_account_ids(const qts::AppConfig& config) {
  std::vector<qts::AccountId> out;
  for (const auto& account : config.accounts) out.push_back(account.id);
  return out;
}

// Per-account channel books for gateway queries (true分账): each account
// resolves to its own broker channel's book, fed by that channel's fills.
// Accounts on bookless (vendor) channels stay unmapped and refuse honestly
// instead of serving another account's numbers.
std::unordered_map<qts::AccountId, const qts::Portfolio*> gateway_account_books(const qts::AppConfig& config, qts::ChannelSet& channels) {
  std::unordered_map<qts::AccountId, const qts::Portfolio*> out;
  for (const auto& account : config.accounts) {
    for (auto& channel : channels.channels) {
      if (channel.name != account.broker || !channel.book) continue;
      out[account.id] = &channel.book();
      break;
    }
  }
  return out;
}

#ifdef QTS_HAS_VENDOR_ADAPTERS
// ---- counter reference-data + snapshot cadence (ADR-054, tick thread) ----
// Full securities sweep on counter-login rising edge (and on trade-date
// change for daily bands); fund/asset snapshots every kRefDataCadenceSec
// into the gateway live-cash view plus a change log. Batches flow through
// the adapter drain (bounded per tick); a failed sweep keeps the previous
// cache (unknown is not failure). Reference data never enters the JSONL
// ledger (projection only, rebuildable from a sweep).
constexpr std::int64_t kRefDataCadenceSec = 60;

std::string utc_stamp_now() {
  const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  char stamp[32]{0};
  std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
  return stamp;
}

struct CounterRefState {
  bool counter_was_up{false};
  std::string swept_fund;
  std::string swept_date;
  std::chrono::steady_clock::time_point last_snapshot{std::chrono::steady_clock::now()};
  std::string last_seen_stamp;
  std::vector<qts::SecurityInfoRow> master_rows;  // current sweep accumulation
};

qts::Exchange exchange_from_suffix(const std::string& id) {
  const std::size_t dot = id.rfind('.');
  if (dot == std::string::npos) return qts::Exchange::unknown;
  std::string suffix = id.substr(dot + 1);
  for (char& c : suffix) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
  }
  try {
    return qts::parse_exchange(suffix);
  } catch (const std::exception&) {
    return qts::Exchange::unknown;
  }
}

// Counter-first master (ADR-054): swept stock rows replace the yaml entries
// they cover (tick/lot/bands); uncovered yaml entries stay as fallback.
// Board stays unknown (STAR/ChiNext market-order protection rule skipped,
// documented gap); price bands apply where the counter reports them.
void apply_counter_master(Logger& log, qts::RiskEngine& risk, const qts::AppConfig& config, const std::vector<qts::SecurityInfoRow>& rows) {
  std::unordered_map<qts::InstrumentId, qts::AshareInstrumentRules> master;
  std::unordered_set<qts::InstrumentId> covered;
  for (const auto& row : rows) {
    if (row.security_type != 1 || row.price_tick <= 0) continue;  // trade stocks with a tick
    qts::AshareInstrumentRules rules;
    rules.instrument_id = row.instrument_id;
    rules.exchange = exchange_from_suffix(row.instrument_id);
    rules.price_tick = row.price_tick;
    rules.minimum_quantity = row.buy_lower > 0 ? row.buy_lower : row.buy_unit;
    rules.quantity_step = row.buy_unit > 0 ? row.buy_unit : 0;
    rules.maximum_quantity = 0;
    rules.market_orders_allowed = true;
    rules.limit_down = row.limit_down;
    rules.limit_up = row.limit_up;
    master.emplace(row.instrument_id, rules);
    covered.insert(row.instrument_id);
  }
  for (const auto& entry : config.instruments) {
    if (covered.find(entry.id) != covered.end()) continue;
    master.emplace(entry.id, qts::instrument_rules_from(entry));
  }
  std::size_t bands = 0;
  for (const auto& entry : master) {
    if (entry.second.limit_down > 0 || entry.second.limit_up > 0) {
      risk.set_price_band(entry.first, {entry.second.limit_down, entry.second.limit_up});
      ++bands;
    }
  }
  risk.set_ashare_master(std::move(master));
  log.info("server", "counter master applied", {{"rows", std::to_string(rows.size())}, {"bands", std::to_string(bands)}});
}

qts::QuantapiTradeAdapter* refdata_adapter(qts::ChannelSet& channels) {
  for (auto& channel : channels.channels) {
    if (channel.name == "quantapi" && channel.api != nullptr) return static_cast<qts::QuantapiTradeAdapter*>(channel.api.get());
  }
  return nullptr;
}

// Persistent control-plane handlers (set once; the settle path restores
// them after its one-shot fetches, all on the tick thread).
void arm_counter_refdata(Logger& log,
                         qts::ChannelSet& channels,
                         const qts::AppConfig& config,
                         qts::RiskEngine& risk,
                         qts::SqliteEventStore* ref_cache,
                         const std::shared_ptr<qts::gateway::GatewayService::CounterSnapshotCache>& live_cache,
                         CounterRefState& state) {
  if (refdata_adapter(channels) == nullptr) return;
  auto* adapter = refdata_adapter(channels);
  adapter->set_security_info_handler([&](const qts::AccountId& account, const std::vector<qts::SecurityInfoRow>& rows, bool last) {
    if (ref_cache != nullptr && !rows.empty()) {
      try {
        ref_cache->upsert_instruments(rows, utc_stamp_now());
      } catch (const std::exception& e) {
        log.warn("server", "instruments cache upsert failed", {{"error", e.what()}});
      }
    }
    state.master_rows.insert(state.master_rows.end(), rows.begin(), rows.end());
    if (!last) return;
    apply_counter_master(log, risk, config, state.master_rows);
    state.master_rows.clear();
    (void)account;
  });
  adapter->set_account_snapshot_handler([live_cache](const qts::TradeAccountSnapshot& snapshot) { live_cache->update_account(snapshot.account_id, snapshot, utc_stamp_now()); });
  adapter->set_asset_snapshot_handler([live_cache](const qts::TradeAssetSnapshot& snapshot) { live_cache->update_asset(snapshot.account_id, snapshot, utc_stamp_now()); });
}

void poll_counter_refdata(Logger& log,
                          qts::ChannelSet& channels,
                          const qts::AppConfig& config,
                          const std::string& trade_date,
                          const std::shared_ptr<qts::gateway::GatewayService::CounterSnapshotCache>& live_cache,
                          CounterRefState& state) {
  qts::QuantapiTradeAdapter* adapter = refdata_adapter(channels);
  if (adapter == nullptr || !adapter->connected()) {
    state.counter_was_up = false;
    return;
  }
  const std::string fund = adapter->session_fund();
  if (fund.empty()) {
    state.counter_was_up = false;
    return;
  }
  const bool rising = !state.counter_was_up;
  state.counter_was_up = true;
  if ((rising && state.swept_fund != fund) || state.swept_date != trade_date) {
    adapter->request_security_info(fund, {});  // full-market sweep, both markets
    adapter->request_account_snapshot(fund);
    adapter->request_asset_snapshot(fund);
    state.swept_fund = fund;
    state.swept_date = trade_date;
    state.master_rows.clear();
    state.last_snapshot = std::chrono::steady_clock::now();
    log.info("server", "counter sweep issued", {{"fund", fund}});
  } else if (std::chrono::steady_clock::now() - state.last_snapshot >= std::chrono::seconds{kRefDataCadenceSec}) {
    adapter->request_account_snapshot(fund);
    adapter->request_asset_snapshot(fund);
    state.last_snapshot = std::chrono::steady_clock::now();
  }
  // Change log for reconciliation visibility (snapshots land via drain).
  qts::gateway::GatewayService::CounterSnapshot latest;
  if (live_cache->lookup(fund, latest) && latest.has_account && latest.captured_at != state.last_seen_stamp) {
    state.last_seen_stamp = latest.captured_at;
    log.info("server", "counter snapshot",
             {{"fund", fund},
              {"cash", std::to_string(latest.account.available_cash)},
              {"positions", std::to_string(latest.account.positions.size())},
              {"asset", std::to_string(latest.has_asset ? latest.asset.total_asset : 0)},
              {"at", latest.captured_at}});
  }
}
#endif  // QTS_HAS_VENDOR_ADAPTERS

// Paper broker names (channel_factory paper set): accounts on these stay
// open on the gateway; anything else is a live counter account and needs
// its broker password at login (ADR-050).
bool is_paper_broker(const std::string& broker) { return broker == "simulated" || broker == "alternate"; }

// Gateway login gate (ADR-050/053): live-broker accounts need their broker
// password, supplied by the trader at login — never from server config/env
// (v1: trader-driven C/S; automation pre-login goes through the same
// adapter login below). No gateway-side password tables exist by design.
// The login name may be the cust id, fund id, a shareholder id, or a
// quantapi config account id that equals one of those; success binds the
// canonical counter fund id. The counter session persists past gateway
// logout (strategies outlive the client, v1).
qts::gateway::GatewayAuth make_gateway_auth(Logger& log, const qts::AppConfig& config, qts::ChannelSet& channels) {
  qts::gateway::GatewayAuth auth{};
  for (const auto& account : config.accounts) {
    if (is_paper_broker(account.broker)) continue;
    auth.login_accounts.push_back(account.id);
  }
  if (auth.login_accounts.empty()) return auth;
    // QuantAPI is the only vendor trade channel today; anything else stays
    // login-impossible (fail closed) until its credential mapping lands.
#ifdef QTS_HAS_VENDOR_ADAPTERS
  qts::QuantapiTradeAdapter* quantapi = nullptr;
  for (auto& channel : channels.channels) {
    if (channel.name == "quantapi" && channel.api != nullptr) quantapi = static_cast<qts::QuantapiTradeAdapter*>(channel.api.get());
  }
  if (quantapi == nullptr) {
    log.warn("server", "gateway live accounts have no quantapi channel (logins will refuse)", {});
    return auth;
  }
  auth.counter_login = [quantapi](const qts::AccountId& name, const std::string& password, std::string& canonical) {
    if (!quantapi->login(name, password)) return false;
    canonical = quantapi->session_fund();
    if (canonical.empty()) canonical = name;
    return true;
  };
#else
  (void)channels;
  log.warn("server", "gateway live accounts need a vendor-gated build (logins will refuse)", {});
#endif
  return auth;
}

// Downstream gateway front (ADR-050): the GatewayService core plus its
// socket/TCP shell, serviced from the demo tick like the query socket above
// (same thread owns OMS/book/service, no extra thread). Quotes fan out per
// tick; execution reports forward through one EventBus subscription, so
// frontend subscribers see the same fills the ledger records. Empty socket
// and empty bind disable the transport. No gateway-side passwords: paper
// accounts stay open, live-broker accounts log in with their broker
// password (see make_gateway_auth).
struct GatewayFront {
  GatewayFront(Logger& log,
               const qts::AppConfig& config,
               qts::control::ControlContext control,
               const qts::Portfolio& book,
               std::unordered_map<qts::AccountId, const qts::Portfolio*> books,
               qts::ChannelSet& channels,
               qts::gateway::GatewayService::StockFunder stock_funder,
               qts::EventBus& events,
               qts::EventLedger* ledger,
               qts::gateway::GatewayService::AccountSnapshotProvider snapshots)
      : service(control, book, gateway_account_ids(config), std::move(books)),
        bus(events) {
    service.set_snapshot_provider(std::move(snapshots));
    service.set_attestation_ledger(ledger);
    service.set_stock_funder(stock_funder);
    if (config.gateway.socket.empty() && config.gateway.bind.empty()) return;
    qts::gateway::GatewayAuth auth = make_gateway_auth(log, config, channels);
    const bool live_gate = !auth.login_accounts.empty();
    try {
      transport = std::make_unique<qts::gateway::GatewayTransport>(service, config.gateway.socket, config.gateway.bind, std::move(auth));
    } catch (const std::exception& e) {
      throw std::runtime_error(std::string{"gateway socket failed: "} + e.what());
    }
    log.info("server", live_gate ? "gateway enabled (live accounts need broker-password login)" : "gateway enabled (paper mode, socket permissions are the access control)",
             {{"path", config.gateway.socket}, {"bind", transport->tcp_endpoint()}});
    reports = bus.subscribe_execution([this](const qts::ExecutionReport& report) { service.on_execution_report(report); });
    subscribed = true;
  }

  ~GatewayFront() {
    if (subscribed) bus.unsubscribe_execution(reports);
  }

  GatewayFront(const GatewayFront&) = delete;
  GatewayFront& operator=(const GatewayFront&) = delete;
  GatewayFront(GatewayFront&&) = delete;
  GatewayFront& operator=(GatewayFront&&) = delete;

  void poll() {
    if (transport != nullptr) transport->poll_once();
  }

  void set_counter_snapshot_cache(std::shared_ptr<qts::gateway::GatewayService::CounterSnapshotCache> cache) { service.set_counter_snapshot_cache(std::move(cache)); }

  void on_quote(const qts::QuoteEvent& quote) { service.on_quote(quote); }

 private:
  qts::gateway::GatewayService service;
  std::unique_ptr<qts::gateway::GatewayTransport> transport;
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  qts::EventBus& bus;
  qts::EventBus::Subscription reports{0};
  bool subscribed{false};
};

// Live risk wiring: position caps are dead unless something reports fills
// back. Reports carry no side, so sync net positions from the paper book
// (authoritative for simulated fills) after each drain. Flattened symbols
// vanish from all_positions(), so the known set pins them at zero instead
// of leaving stale nets in the engine. Day PnL is measured against the
// rollover baseline for the daily loss limit.
void sync_risk_positions(
    qts::RiskEngine& risk, qts::OrderManager& oms, const qts::Portfolio& book, const std::vector<qts::StrategyId>& baskets, std::unordered_set<qts::InstrumentId>& known, qts::Price day_baseline) {
  for (const auto& entry : book.all_positions()) known.insert(entry.first);
  for (const auto& instrument : known) {
    const qts::PositionView view = book.position(instrument);
    risk.set_position(instrument, view.total);
    for (const auto& basket : baskets) risk.set_sellable(basket, instrument, book.position_for(basket, instrument).sellable);
  }
  // Live sells reserve sellable until their fills/cancels land (absolute
  // set from the working set, so nothing leaks across syncs).
  risk.set_sell_reserved(oms.live_sell_exposure());
  // Buying-power state follows the book too (fills spend it, sells restore
  // it); live buys reserve it until their fills/cancels land.
  risk.set_cash(book.cash());
  risk.set_buy_reserved(oms.live_buy_exposure());
  risk.set_day_pnl(book.realized_pnl() - day_baseline);
  risk.set_equity(book.equity());
}

void submit_children(qts::OrderManager& oms, qts::ExecutionAlgorithm& execution, Logger& log, int& submitted, int& rejected) {
  for (const auto& child : execution.poll(std::chrono::system_clock::now())) {
    if (oms.submit(child)) {
      ++submitted;
      log.debug("execution", "child_accepted", intent_fields(child));
    } else {
      ++rejected;
      log.warn("server", "submit rejected", {{"client_order_id", child.client_order_id}});
    }
  }
}

// Startup funding into the ledger (M11-7): opening cash/inventory become
// replayable events so admin/SQLite/verify see the same book as the broker.
// Fresh ledgers only (restarts re-read history: the ledger is the
// authority, config drift never rewrites it). Silent when nothing was
// explicitly funded, so untouched drills stay byte-identical.
void maybe_record_funding(Logger& log, const qts::AppConfig& config, qts::EventLedger* ledger, const std::string& trade_date) {
  const bool funded = config.paper_sim.starting_cash_set || !config.paper_sim.starting_positions.empty();
  if (!funded) return;
  if (ledger->latest_event_id() != 0) {
    log.debug("server", "funding already recorded, skipping", {});
    return;
  }
  auto append_funding = [&](qts::FundingDeposit funding) {
    qts::LedgerEvent event{};
    event.type = qts::LedgerEventType::funding_deposited;
    event.funding = std::move(funding);
    event.envelope.trace_id = "funding";
    event.envelope.config_version = config.config_version;
    event.envelope.rules_version = config.rules_version;
    event.envelope.trade_date = trade_date;
    ledger->append(std::move(event));
  };
  if (config.paper_sim.starting_cash_set) {
    qts::FundingDeposit cash{};
    cash.cash = config.paper_sim.starting_cash;
    append_funding(std::move(cash));
  }
  for (const auto& position : config.paper_sim.starting_positions) {
    qts::FundingDeposit deposit{};
    deposit.strategy = position.strategy;
    deposit.instrument = position.instrument;
    deposit.quantity = position.quantity;
    deposit.average_cost = position.avg_cost;
    append_funding(std::move(deposit));
  }
  log.info("server", "funding recorded",
           {{"cash_set", config.paper_sim.starting_cash_set ? "true" : "false"},
            {"positions", std::to_string(config.paper_sim.starting_positions.size())},
            {"ledger_events", std::to_string(ledger->latest_event_id())}});
}

void arm_rollover(const qts::AppConfig& config, qts::OrderManager& oms, Logger& log, DayRollover& rollover) {
  // Validated HH:MM at load; the fallback below is unreachable in practice.
  if (!qts::parse_rollover(config.trading_day.rollover, rollover.hour, rollover.minute)) {
    rollover.hour = 0;
    rollover.minute = 1;
  }
  rollover.trade_date = today_label();
  rollover.next = qts::next_rollover_after(std::time(nullptr), rollover.hour, rollover.minute);
  oms.set_trade_date(rollover.trade_date);
  log.info("server", "trading day armed", {{"trade_date", rollover.trade_date}, {"rollover", config.trading_day.rollover}});
}

#ifdef QTS_HAS_VENDOR_ADAPTERS
// Live vendor sessions (M7-2 server wiring). Everything here runs only in
// vendor-gated builds; default builds never see these symbols.

std::string vendor_env_or(const char* name, const std::string& fallback) {
  const char* value = std::getenv(name);
  return value != nullptr ? std::string(value) : fallback;
}

// QuantAPI order/map sidecar: yaml wins, otherwise next to the ledger file
// (same epoch as the alternate channel), temp dir when ledger-less.
std::string quantapi_sidecar_for(const qts::AppConfig& config) {
  if (!config.quantapi.sidecar_path.empty()) return config.quantapi.sidecar_path;
  if (!config.ledger_file.empty()) {
    const std::size_t slash = config.ledger_file.find_last_of('/');
    const std::string dir = slash == std::string::npos ? "." : config.ledger_file.substr(0, slash);
    return dir + "/quantapi.sidecar";
  }
  return (std::filesystem::temp_directory_path() / "quantapi.sidecar").string();
}

// Factory hook for build_channels: "quantapi" only, anything else stays
// unknown (fail closed). ADR-053: no customer secrets here — the channel is
// built un-logged (standby); the trader logs in via the gateway and the
// counter session is built lazily. Env automation (headless, no client)
// passes through the same adapter login() below.
std::unique_ptr<qts::Channel> make_vendor_trade_channel(const std::string& name, qts::EventBus& /*events*/, const qts::AppConfig& config) {
  if (name != "quantapi") return nullptr;
  qts::QuantapiSessionConfig session;
  session.locations = config.quantapi.locations;
  session.order_way = config.quantapi.order_way;
  session.sidecar_path = quantapi_sidecar_for(config);
  // Gateway user: yaml session param first (ADR-053), env fallback kept for
  // automation compat; pair or nothing (config validation enforces yaml).
  session.agw_user = !config.quantapi.agw_user.empty() ? config.quantapi.agw_user : vendor_env_or("QTS_QUANTAPI_AGW_USER", "");
  session.agw_password = !config.quantapi.agw_password.empty() ? config.quantapi.agw_password : vendor_env_or("QTS_QUANTAPI_AGW_PASSWORD", "");
  if (session.agw_user.empty() != session.agw_password.empty()) throw std::runtime_error("quantapi agw user/password must be set together");
  // Terminal fingerprint: explicit pin wins (automation), otherwise the
  // adapter generates the mandated shape at connect().
  session.client_feature_code = vendor_env_or("QTS_QUANTAPI_CLIENT_FEATURE_CODE", "");
  auto adapter = std::make_unique<qts::QuantapiTradeAdapter>(std::move(session));
  qts::QuantapiTradeAdapter* raw = adapter.get();
  qts::Channel channel{};
  channel.name = name;
  channel.api = std::move(adapter);
  channel.drain = [raw] { raw->drain(); };
  return std::make_unique<qts::Channel>(std::move(channel));
}

// Standby bring-up (ADR-053): channels validate + connect (TCP ready), the
// counter Login waits for the first gateway login. Automation (headless)
// with gateway-side env credentials pre-logs in through the same path.
bool start_vendor_trade_sessions(Logger& log, qts::ChannelSet& channels) {
  for (auto& channel : channels.channels) {
    if (channel.name != "quantapi" || channel.api == nullptr) continue;
    auto* adapter = static_cast<qts::QuantapiTradeAdapter*>(channel.api.get());
    if (adapter->started()) continue;
    if (!adapter->start() || !adapter->connect()) {
      log.error("server", "quantapi standby failed", {{"channel", channel.name}});
      return false;
    }
    log.info("server", "quantapi standby (TCP ready, counter login on gateway login)", {{"locations", adapter->config().locations}});
  }
  // Automation pre-login (no client): env holds the trader identity the
  // gateway side would otherwise type in. Absent env = pure C/S standby.
  qts::BrokerCredentials automation{};
  if (qts::try_load_broker_credentials("QTS_QUANTAPI", automation) && !automation.password.empty() && (!automation.cust_id.empty() || !automation.fund_account_id.empty())) {
    for (auto& channel : channels.channels) {
      if (channel.name != "quantapi" || channel.api == nullptr) continue;
      auto* adapter = static_cast<qts::QuantapiTradeAdapter*>(channel.api.get());
      const std::string identity = !automation.cust_id.empty() ? automation.cust_id : automation.fund_account_id;
      if (!adapter->login(identity, automation.password)) {
        log.error("server", "quantapi automation login failed", {{"channel", channel.name}});
        return false;
      }
      log.info("server", "quantapi pre-logged (automation)", {{"fund", adapter->session_fund()}});
    }
  }
  return true;
}

// Scope-exit stop for every started vendor session (idempotent stops;
// the catch keeps teardown non-throwing).
struct VendorSessionGuard {
  qts::ChannelSet* channels{nullptr};
  qts::AmaQuoteAdapter* ama{nullptr};
  ~VendorSessionGuard() {
    try {
      if (ama != nullptr) ama->stop();
      if (channels != nullptr) {
        for (auto& channel : channels->channels) {
          if (channel.name != "quantapi" || channel.api == nullptr) continue;
          static_cast<qts::QuantapiTradeAdapter*>(channel.api.get())->stop();
        }
      }
    } catch (...) {
      // Teardown must not throw; sessions are already idempotent-stopped.
    }
  }
};

// Server-owned live AMA feed: quotes arrive on our drain() pump (same
// thread as the demo tick), so pending needs no lock.
struct LiveAmaFeed {
  std::unique_ptr<qts::AmaQuoteAdapter> adapter;
  std::vector<qts::QuoteEvent> pending;
};

std::unique_ptr<LiveAmaFeed> make_live_ama(Logger& log, const qts::AppConfig& config, int& exit_code) {
  if (!config.ama.enabled) return nullptr;
  qts::AmaSessionConfig session;
  try {
    const qts::BrokerCredentials credentials = qts::load_broker_credentials("QTS_AMA");
    session.username = credentials.cust_id;
    session.password = credentials.password;
  } catch (const std::exception& e) {
    log.error("server", "ama credentials unavailable", {{"error", e.what()}});
    exit_code = 2;
    return nullptr;
  }
  for (const std::string& endpoint : config.ama.ums_servers) {
    const std::size_t colon = endpoint.rfind(':');
    int port = 0;
    try {
      if (colon == std::string::npos || colon == 0 || colon + 1 >= endpoint.size()) throw std::invalid_argument{"shape"};
      port = std::stoi(endpoint.substr(colon + 1));
    } catch (const std::exception&) {
      log.error("server", "ama endpoint malformed", {{"endpoint", endpoint}});
      exit_code = 2;
      return nullptr;
    }
    if (port < 1 || port > 65535) {
      log.error("server", "ama endpoint malformed", {{"endpoint", endpoint}});
      exit_code = 2;
      return nullptr;
    }
    session.ums_servers.push_back(qts::AmaUmsServer{endpoint.substr(0, colon), static_cast<std::uint16_t>(port)});
  }
  session.max_staleness = std::chrono::milliseconds{config.ama.staleness_ms};
  auto feed = std::make_unique<LiveAmaFeed>();
  feed->adapter = std::make_unique<qts::AmaQuoteAdapter>();
  qts::AmaQuoteAdapter* raw = feed->adapter.get();
  raw->set_quote_handler([live = feed.get()](const qts::QuoteEvent& quote) { live->pending.push_back(quote); });
  if (!raw->start(session)) {
    log.error("server", "ama vendor Init rejected the session", {});
    exit_code = 2;
    return nullptr;
  }
  std::vector<qts::InstrumentId> universe;
  for (const auto& entry : config.instruments) universe.push_back(entry.id);
  raw->subscribe(universe);
  log.info("server", "live ama feed started", {{"vendor_version", raw->vendor_version()}, {"instruments", std::to_string(universe.size())}});
  return feed;
}

void pull_live_quotes(LiveAmaFeed& live, std::vector<qts::QuoteEvent>& quotes) {
  live.adapter->drain();
  quotes.insert(quotes.end(), live.pending.begin(), live.pending.end());
  live.pending.clear();
}
#endif

// Startup audit for live channels (ADR-051): no approval files. Any
// non-paper channel means the broker password came from env; the password
// checked again at gateway login is the single authorization.
void log_live_channels(Logger& log, const qts::ChannelSet& channels, const std::string& trade_date) {
  std::string live_names;
  for (const auto& channel : channels.channels) {
    if (channel.name == "simulated" || channel.name == "alternate") continue;
    if (!live_names.empty()) live_names += ",";
    live_names += channel.name;
  }
  if (live_names.empty()) return;
  log.info("server", "live channels up (standby: counter login on gateway login)", {{"channels", live_names}, {"trade_date", trade_date}});
}

// Channel selection (accounts[].broker + routing.channels): yaml-only
// switching between paper channels. Returns the paper feed for market
// data, or nullptr with an error logged when the config cannot trade.
qts::Channel* setup_channels(
    Logger& log, const qts::AppConfig& config, qts::EventBus& events, const qts::SystemClock& wall_clock, qts::ChannelSet& channels, qts::TradeApi*& trade, const std::string& trade_date) {
  try {
#ifdef QTS_HAS_VENDOR_ADAPTERS
    channels = qts::build_channels(config, events, &wall_clock, make_vendor_trade_channel, trade_date);
#else
    channels = qts::build_channels(config, events, &wall_clock, {}, trade_date);
#endif
  } catch (const std::exception& e) {
    log.error("server", "channel build failed", {{"error", e.what()}});
    return nullptr;
  }
  qts::Channel* paper_feed = channels.paper();
  if (paper_feed == nullptr) {
    log.error("server", "no paper feed channel", {{"detail", "strategy needs a book; vendor-only setups arrive with real market data"}});
    return nullptr;
  }
#ifdef QTS_HAS_VENDOR_ADAPTERS
  if (!start_vendor_trade_sessions(log, channels)) return nullptr;
#endif
  std::string channel_names;
  for (const auto& channel : channels.channels) {
    if (!channel_names.empty()) channel_names += ",";
    channel_names += channel.name;
  }
  trade = &channels.trade_for_oms();
  log.info("server", "channels ready", {{"channels", channel_names}, {"trade", std::string{trade->channel()}}});
  log_live_channels(log, channels, trade_date);
  return paper_feed;
}

// Trader strategy registry (null in drill runs, which have no inbox):
// yaml instances are adopted by setup_strategies, runtime strategy_start
// instances join the same fan-out vector with per-generation id prefixes.
std::unique_ptr<qts::control::StrategyRegistry> make_strategy_registry(std::vector<std::unique_ptr<qts::Strategy>>& strategies, const qts::AppConfig& config, const std::string& trade_date) {
  if (!config.demo.enabled) {
    return nullptr;
  }
#ifdef _WIN32
  const auto registry_pid = static_cast<std::int64_t>(_getpid());
#else
  const auto registry_pid = static_cast<std::int64_t>(::getpid());
#endif
  return std::make_unique<qts::control::StrategyRegistry>(
      strategies, [&](const qts::StrategyConfig& configured, const std::string& id_prefix) { return make_strategy(configured, config, id_prefix); },
      compact_date(trade_date) + "-" + std::to_string(registry_pid));
}

// Strategy/execution selection: every enabled strategy runs (each keeps a
// per-run id prefix of strategy/date/pid, so parents never collide across
// strategies or restarts); the single execution algorithm is shared because
// parent ids are unique. Unknown types fail closed. The one-shot drill keeps
// the first enabled strategy only, so its counts stay byte-identical. A
// non-null registry adopts the yaml instances so trader start/stop/list
// commands see them (drill passes null: no inbox there).
bool setup_strategies(Logger& log,
                      const qts::AppConfig& config,
                      const std::string& trade_date,
                      std::vector<std::unique_ptr<qts::Strategy>>& strategies,
                      std::unique_ptr<qts::ExecutionAlgorithm>& execution,
                      const qts::StrategyConfig*& selected,
                      qts::control::StrategyRegistry* registry) {
  selected = nullptr;
  for (const auto& entry : config.strategies) {
    if (entry.enabled && selected == nullptr) selected = &entry;
  }
  try {
    execution = make_execution(config.execution);
  } catch (const std::exception& e) {
    log.error("server", "execution setup failed", {{"error", e.what()}});
    return false;
  }
  if (selected == nullptr) {
    // v1 trader-driven C/S: no auto-start strategies required (risk policy);
    // the trader starts them via the gateway and they join the same fan-out.
    // The one-shot drill still needs its single strategy (fail closed).
    if (!config.demo.enabled) {
      log.error("server", "no enabled strategy in config", {});
      return false;
    }
    log.warn("server", "no enabled strategy in config (standby: trader starts via gateway)", {});
    return true;
  }
  try {
#ifdef _WIN32
    const auto run_pid = static_cast<std::int64_t>(_getpid());
#else
    const auto run_pid = static_cast<std::int64_t>(::getpid());
#endif
    for (const auto& entry : config.strategies) {
      if (!entry.enabled) continue;
      strategies.push_back(make_strategy(entry, config, entry.id + "-" + compact_date(trade_date) + "-" + std::to_string(run_pid) + "-"));
      if (registry != nullptr) {
        registry->adopt(entry, strategies.back().get());
      }
    }
  } catch (const std::exception& e) {
    log.error("server", "strategy/execution setup failed", {{"error", e.what()}});
    return false;
  }
  std::string names;
  for (const auto& entry : config.strategies) {
    if (!entry.enabled) continue;
    if (!names.empty()) names += ",";
    names += entry.id;
  }
  log.info("server", "strategies ready", {{"strategies", names}, {"execution", config.execution.algo}});
  return true;
}

// Live risk wiring: wall clock for the submission throttle and initial
// kill-switch state from config (all empty by default = all open).
// When instruments are configured the master is injected (unknown symbols
// fail closed), price bands follow the static limits; no instrument
// allowlist is armed (decision 2026-10-16: the master validates tick/lot,
// never permission).
void arm_risk(Logger& log, const qts::AppConfig& config, qts::RiskEngine& risk, const qts::SystemClock& wall_clock) {
  if (config.risk.rate_max_orders > 0 && config.risk.rate_window_ms > 0) {
    risk.set_rate_limit(config.risk.rate_max_orders, std::chrono::milliseconds{config.risk.rate_window_ms}, &wall_clock);
    log.info("server", "rate limit armed", {{"max_orders", std::to_string(config.risk.rate_max_orders)}, {"window_ms", std::to_string(config.risk.rate_window_ms)}});
  }
  if (!config.instruments.empty()) {
    std::unordered_map<qts::InstrumentId, qts::AshareInstrumentRules> master;
    for (const auto& entry : config.instruments) {
      master.emplace(entry.id, qts::instrument_rules_from(entry));
      if (entry.limit_down > 0 || entry.limit_up > 0) risk.set_price_band(entry.id, {entry.limit_down, entry.limit_up});
    }
    risk.set_ashare_master(std::move(master));
    log.info("server", "instrument master armed", {{"count", std::to_string(config.instruments.size())}});
  }
  for (const auto& strategy_id : config.switches.blocked_strategies) risk.set_strategy_blocked(strategy_id, true);
  for (const auto& account_id : config.switches.blocked_accounts) risk.set_account_blocked(account_id, true);
  for (const auto& instrument_id : config.switches.blocked_instruments) risk.set_instrument_blocked(instrument_id, true);
}

// Snapshot/archive sidecars live next to the ledger file (same epoch).
std::string snapshot_path_for(const std::string& ledger_file) {
  const std::size_t slash = ledger_file.find_last_of('/');
  return (slash == std::string::npos ? std::string{"."} : ledger_file.substr(0, slash)) + "/ledger-snapshot.json";
}

std::string archive_stamp() {
  const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  char buf[20]{0};
  if (std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &local) == 0) return {"unknown-time"};
  return {buf};
}

// Broker truth query shared by boot recovery and reconnect recovery:
// collect re-emitted working orders (alert-only downstream). Paper channels
// answer inline; queued/async channels drain below; vendor channels complete
// through the same handler. Bounded wait, then proceed with whatever arrived
// rather than hanging the caller.
std::vector<qts::ExecutionReport> query_broker_truth(Logger& log, qts::ChannelSet& channels, qts::TradeApi& trade, qts::EventBus& events, const qts::AppConfig& config);

// Alert-only confrontation with broker truth (definition follows
// run_recovery); returns the diff count for summaries.
std::size_t reconcile_with_broker_truth(Logger& log, qts::OrderManager& oms, const std::vector<qts::ExecutionReport>& broker_open, const std::string& context);

// Snapshot path: ledger snapshot for rotation/crash recovery (epoch
// guarded by file size; see file_ledger.hpp).
bool restore_snapshot_opens(Logger& log, qts::OrderManager& oms, const qts::AppConfig& config) {
  if (config.ledger_file.empty()) return true;
  const std::string path = snapshot_path_for(config.ledger_file);
  std::error_code errc;
  if (!std::filesystem::exists(path, errc)) return true;
  std::error_code size_err;
  const auto live_bytes = std::filesystem::file_size(config.ledger_file, size_err);
  qts::OpenSnapshot snapshot{};
  try {
    snapshot = qts::read_open_snapshot(path);
  } catch (const std::exception& e) {
    log.error("server", "snapshot unreadable, ignoring", {{"error", e.what()}});
    return true;
  }
  if (size_err || live_bytes < snapshot.ledger_bytes) {
    log.warn("server", "snapshot stale for this ledger epoch, ignoring", {});
    return true;
  }
  std::size_t revived = 0;
  for (const auto& entry : snapshot.opens) {
    if (oms.find(entry.first.client_order_id) != nullptr) continue;  // file replay already covered it
    if (!oms.restore_order(entry.first, entry.second)) {
      log.error("server", "snapshot restore failed", {{"client_order_id", entry.first.client_order_id}});
      return false;
    }
    ++revived;
  }
  if (revived > 0) log.info("server", "snapshot restored", {{"revived", std::to_string(revived)}});
  return true;
}

// Boot recovery (§4.1, code): replay file history into the live OMS, then
// confront it with broker truth. Ledger replay restores working orders;
// the broker query only ALARMS (never self-heals) per runbook policy.
// Paper channels are amnesic across restarts, so rebuilt opens showing as
// missing-at-broker after a restart are expected and informational.
// Returns false when history itself is corrupt (refuse to start; restore
// the ledger backup instead of trading on a half-rebuilt book).
bool run_recovery(Logger& log, qts::ChannelSet& channels, qts::TradeApi& trade, qts::OrderManager& oms, qts::EventBus& events, const qts::AppConfig& config, qts::EventLedger* ledger) {
  const auto stored = ledger->replay(1);
  if (stored.empty()) {
    log.debug("server", "recovery skipped", {{"reason", "no ledger history"}});
    return true;
  }
  std::uint64_t previous_id = 0;
  bool first = true;
  for (const auto& event : stored) {
    if (!first && event.envelope.event_id <= previous_id) {
      log.error("server", "recovery failed", {{"error", "event ids not strictly increasing"}});
      return false;
    }
    first = false;
    previous_id = event.envelope.event_id;
    std::string error;
    if (!qts::replay::feed_recovery_event(oms, event, error)) {
      log.error("server", "recovery failed", {{"error", error}});
      return false;
    }
  }
  if (!restore_snapshot_opens(log, oms, config)) return false;
  const std::size_t restored = oms.open_orders().size();

  const auto broker_open = query_broker_truth(log, channels, trade, events, config);
  const std::size_t diffs = reconcile_with_broker_truth(log, oms, broker_open, "recovery");
  log.info("server", "recovery complete",
           {{"ledger_events", std::to_string(stored.size())}, {"restored_open", std::to_string(restored)}, {"broker_reports", std::to_string(broker_open.size())}, {"diffs", std::to_string(diffs)}});
  return true;
}

// Alert-only confrontation with broker truth, shared by boot recovery and
// reconnect recovery. Never self-heals: diffs are operator business.
// Returns the diff count for summaries.
std::size_t reconcile_with_broker_truth(Logger& log, qts::OrderManager& oms, const std::vector<qts::ExecutionReport>& broker_open, const std::string& context) {
  const auto diffs = oms.reconcile_open_orders(broker_open);
  for (const auto& diff : diffs) {
    log.error("server", context + " diff (alert only, no self-heal)", {{"client_order_id", diff.client_order_id}, {"reason", diff.reason}});
  }
  return diffs.size();
}

// Broker truth query shared by boot recovery and reconnect recovery:
// collect re-emitted working orders (alert-only downstream). Paper channels
// answer inline; queued/async channels drain below; vendor channels complete
// through the same handler. Bounded wait, then proceed with whatever arrived
// rather than hanging the caller.
std::vector<qts::ExecutionReport> query_broker_truth(Logger& log, qts::ChannelSet& channels, qts::TradeApi& trade, qts::EventBus& events, const qts::AppConfig& config) {
  std::vector<qts::ExecutionReport> broker_open;
  bool collecting = true;
  const qts::EventBus::Subscription collector = events.subscribe_execution([&](const qts::ExecutionReport& report) {
    if (collecting) broker_open.push_back(report);
  });
  std::size_t completions = 0;
  trade.set_recovery_complete_handler([&](const qts::AccountId&, bool /*success*/) { ++completions; });
  for (const auto& account : config.accounts) trade.request_open_orders(account.id);
  drain_all(channels);
  constexpr std::int64_t kRecoveryWaitMs = 5000;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{kRecoveryWaitMs};
  while (g_stop_flag == 0 && completions < config.accounts.size() && std::chrono::steady_clock::now() < deadline) {
    drain_all(channels);
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
  }
  collecting = false;
  events.unsubscribe_execution(collector);
  // The completion lambda above borrows this frame's locals: detach it
  // before returning so a late vendor recovery can never write dead stack.
  trade.set_recovery_complete_handler(nullptr);
  drain_all(channels);  // settle any deliverable already queued behind the detach
  if (completions < config.accounts.size()) {
    log.warn("server", "broker query timed out", {{"completed", std::to_string(completions)}, {"accounts", std::to_string(config.accounts.size())}});
  }
  return broker_open;
}

// Day boundary first: the new day's quotes must see settled books.
// Returns true when the day rolled (caller re-baselines day PnL).
bool maybe_rollover(Logger& log, qts::ChannelSet& channels, qts::OrderManager& oms, DayRollover& rollover) {
  const std::time_t now_tick = std::time(nullptr);
  if (now_tick < rollover.next) return false;
  for (auto& channel : channels.channels) {
    if (static_cast<bool>(channel.settle_day)) channel.settle_day();
  }
  oms.settle_day();
  rollover.trade_date = today_label();
  oms.set_trade_date(rollover.trade_date);
  rollover.next = qts::next_rollover_after(now_tick, rollover.hour, rollover.minute);
  log.info("server", "trading day rolled", {{"trade_date", rollover.trade_date}});
  return true;
}

void feed_intents(Logger& log, qts::ExecutionAlgorithm& execution, qts::Strategy& strategy, const qts::Portfolio& book, const qts::QuoteEvent& quote) {
  execution.on_quote(quote);  // no-op except for volume-tracking algorithms
  for (const auto& intent : strategy.on_quote(quote, book)) {
    log.debug("strategy", "intent", intent_fields(intent));
    execution.add_parent(intent);
  }
}

// Returns process exit code: nonzero on rejected submit (drill is strict).
int run_drill(Logger& log,
              qts::ChannelSet& channels,
              qts::Channel& paper_feed,
              const qts::InstrumentId& symbol,
              qts::OrderManager& oms,
              qts::RiskEngine& risk,
              qts::ExecutionAlgorithm& execution,
              qts::Strategy& strategy,
              qts::EventLedger* ledger,
              SqliteProjector& projector,
              const std::string& state_file,
              const std::vector<qts::StrategyId>& baskets,
              int& submitted) {
  const std::vector<qts::Price> path{1'000, 950, 900, 950, 1'000, 1'050, 1'100};
  std::uint64_t sequence = 0;
  std::unordered_set<qts::InstrumentId> known;
  sync_risk_positions(risk, oms, paper_feed.book(), baskets, known, 0);
  for (const qts::Price price : path) {
    feed_reference_all(channels, symbol, price);
    qts::QuoteEvent quote{};
    quote.instrument_id = symbol;
    quote.last_price = price;
    quote.sequence = ++sequence;
    log.debug("market", "quote", {{"instrument", symbol}, {"price", std::to_string(price)}, {"sequence", std::to_string(sequence)}});
    feed_intents(log, execution, strategy, paper_feed.book(), quote);
    for (const auto& child : execution.poll(std::chrono::system_clock::now())) {
      if (!oms.submit(child)) {
        log.debug("execution", "child_rejected", intent_fields(child));
        std::cerr << "submit rejected: " << child.client_order_id << '\n';
        return 1;
      }
      log.debug("execution", "child_accepted", intent_fields(child));
      ++submitted;
    }
    drain_all(channels);
    sync_risk_positions(risk, oms, paper_feed.book(), baskets, known, 0);
  }
  projector.push_new(ledger);
  write_live_state(state_file, sequence, today_label(), submitted, 0, oms, paper_feed.book(), ledger->latest_event_id());
  return 0;
}

// Fault drills (paper_sim chaos): drop listed channels on a tick schedule
// to exercise freeze/resume/failover. Real disconnects surface through the
// same trade.connected() check below.
struct ChaosState {
  std::unordered_set<std::string> down;
};

struct LinkState {
  ChaosState chaos;
  bool was_up{true};
};

void update_chaos(Logger& log, qts::ChannelSet& channels, const qts::AppConfig& config, std::uint64_t tick, ChaosState& chaos) {
  const auto& sim = config.paper_sim;
  if (sim.chaos_every_ticks == 0 || tick == 0) return;
  const bool in_window = (tick % sim.chaos_every_ticks) < sim.chaos_outage_ticks;
  for (auto& channel : channels.channels) {
    bool listed = false;
    for (const auto& name : sim.chaos_channels) {
      if (name == channel.name) {
        listed = true;
        break;
      }
    }
    if (!listed || !static_cast<bool>(channel.set_connected)) continue;
    const bool is_down = chaos.down.find(channel.name) != chaos.down.end();
    if (in_window && !is_down) {
      channel.set_connected(false);
      chaos.down.insert(channel.name);
      log.warn("server", "chaos fault injected", {{"channel", channel.name}, {"ticks", std::to_string(sim.chaos_outage_ticks)}});
    } else if (!in_window && is_down) {
      channel.set_connected(true);
      chaos.down.erase(channel.name);
      log.info("server", "chaos fault cleared", {{"channel", channel.name}});
    }
  }
}

// Per-tick link state: chaos faults, submission freeze on total outage,
// alert-only reconcile on recovery. Returns true while submissions flow;
// a partially connected Router keeps flowing through failover instead.
bool poll_link_state(Logger& log, qts::ChannelSet& channels, qts::TradeApi& trade, qts::OrderManager& oms, qts::EventBus& events, const qts::AppConfig& config, LinkState& link, std::uint64_t tick) {
  update_chaos(log, channels, config, tick, link.chaos);
  const bool link_up = trade.connected();
  if (!link_up) {
    if (link.was_up) {
      log.warn("server", "link down, freezing submissions", {});
      link.was_up = false;
    }
    return false;
  }
  if (!link.was_up) {
    log.info("server", "link recovered, reconciling", {});
    link.was_up = true;
    const auto broker_open = query_broker_truth(log, channels, trade, events, config);
    reconcile_with_broker_truth(log, oms, broker_open, "reconnect");
  }
  return true;
}

// Single-quote step of the demo loop, shared by both market sources: reference
// + depth, quality/phase gate, strategy, submit. Held quotes skip strategy and
// submit while cancels/queries stay live.
void handle_demo_quote(Logger& log,
                       qts::ChannelSet& channels,
                       const qts::AppConfig& config,
                       bool use_ama,
                       qts::OrderManager& oms,
                       qts::ExecutionAlgorithm& execution,
                       const std::vector<std::unique_ptr<qts::Strategy>>& strategies,
                       qts::Channel& paper_feed,
                       bool link_up,
                       std::uint64_t tick,
                       const qts::QuoteEvent& quote,
                       std::uint64_t& held_ticks,
                       int& submitted,
                       int& rejected) {
  feed_reference_all(channels, quote.instrument_id, quote.last_price);
  if (use_ama) {
    post_quote_depth(channels, quote);
  } else {
    post_depth(channels, config, quote.instrument_id, quote.last_price);
  }
  log.debug("market", "quote", {{"instrument", quote.instrument_id}, {"price", std::to_string(quote.last_price)}, {"sequence", std::to_string(quote.sequence)}});
  // Market gate (M11-0): dirty or non-continuous quotes hold new risk.
  const bool quote_ok = !qts::quote_holds_submissions(quote);
  if (!quote_ok) {
    ++held_ticks;
    log.debug("server", "quote held", {{"tick", std::to_string(tick)}, {"quality", std::to_string(static_cast<int>(quote.quality))}, {"phase", std::to_string(static_cast<int>(quote.market_phase))}});
  }
  execution.on_quote(quote);  // no-op except for volume-tracking algorithms
  // Multi-strategy fan-out: each strategy filters by its own instrument, so
  // unrelated quotes cost one string comparison. Parents carry distinct
  // per-strategy id prefixes, sharing one execution queue safely.
  std::size_t intent_count = 0;
  for (const auto& strategy : strategies) {
    const auto intents = quote_ok ? strategy->on_quote(quote, paper_feed.book()) : std::vector<qts::OrderIntent>{};
    intent_count += intents.size();
    for (const auto& intent : intents) {
      log.debug("strategy", "intent", intent_fields(intent));
      execution.add_parent(intent);
    }
  }
  log.debug("strategy", "intents", {{"count", std::to_string(intent_count)}, {"tick", std::to_string(tick)}});
  if (link_up && quote_ok) {
    submit_children(oms, execution, log, submitted, rejected);
  } else {
    log.debug("server", "submissions held", {{"tick", std::to_string(tick)}});
  }
}

// File quote source (feed == "file" only): null means another feed applies.
// Construction failures are fail-closed: logged here, exit code out.
std::unique_ptr<qts::FileQuoteFeed> make_file_feed(Logger& log, const qts::AppConfig& config, int& exit_code) {
  if (config.demo.feed != "file") return nullptr;
  try {
    auto file = std::make_unique<qts::FileQuoteFeed>(config.demo.file);
    log.info("server", "quote file loaded", {{"path", config.demo.file}, {"rows", std::to_string(file->size())}});
    return file;
  } catch (const std::exception& e) {
    log.error("server", "quote file failed", {{"error", e.what()}});
    exit_code = 2;
    return nullptr;
  }
}

// Non-AMA quote production for one demo tick: file rows replayed in order,
// otherwise the legacy single-symbol walk. False ends the demo early (EOF).
bool next_walk_quotes(
    qts::SimulatedQuoteFeed& walk, qts::FileQuoteFeed* file, Logger& log, const qts::InstrumentId& symbol, std::uint64_t tick, std::uint64_t& sequence, std::vector<qts::QuoteEvent>& quotes) {
  if (file != nullptr) {
    auto filed = file->next();
    if (!filed.has_value()) {
      log.info("server", "quote file exhausted", {{"tick", std::to_string(tick)}});
      return false;
    }
    quotes.push_back(*filed);
    return true;
  }
  quotes.push_back(walk.next(symbol, ++sequence));
  return true;
}

// AMA mock source (M11-2): one deterministic walk per master entry through
// the production snapshot normalizer. Null means the legacy walk applies.
std::unique_ptr<qts::SimulatedAmaFeed> make_ama_feed(const qts::AppConfig& config) {
  if (config.demo.feed != "ama") return nullptr;
  std::vector<qts::SimulatedAmaSymbol> symbols;
  symbols.reserve(config.instruments.size());
  for (const auto& entry : config.instruments) symbols.push_back(ama_symbol_from(entry));
  qts::SimulatedAmaConfig ama_config{};
  ama_config.seed = config.demo.seed;
  ama_config.auction_ticks = config.demo.auction_ticks;
  ama_config.halt_after = config.demo.halt_after;
  ama_config.halt_for = config.demo.halt_for;
  ama_config.stale_every = config.demo.stale_every;
  ama_config.crossed_every = config.demo.crossed_every;
  return std::make_unique<qts::SimulatedAmaFeed>(std::move(symbols), ama_config);
}

// Runtime baskets: yaml ids plus trader-started instances, so T+1 sellable
// sync sees strategies born after startup. Rebuilt per tick because
// strategy_start can land on any inbox poll.
std::vector<qts::StrategyId> merge_live_baskets(const std::vector<qts::StrategyId>& baskets, qts::control::StrategyRegistry* registry) {
  std::vector<qts::StrategyId> merged = baskets;
  if (registry == nullptr) return merged;
  for (const auto& strategy_id : registry->active_ids()) {
    if (std::find(merged.begin(), merged.end(), strategy_id) == merged.end()) {
      merged.push_back(strategy_id);
    }
  }
  return merged;
}

// Tick tail: drain fills, re-sync risk, project new events, and persist
// progress every 50 ticks. Split out of run_demo for the cognitive
// complexity gate; behavior is unchanged.
void finish_demo_tick(Logger& log,
                      qts::ChannelSet& channels,
                      qts::TradeApi& trade,
                      qts::Channel& paper_feed,
                      qts::OrderManager& oms,
                      qts::RiskEngine& risk,
                      qts::EventLedger* ledger,
                      SqliteProjector& projector,
                      const qts::AppConfig& config,
                      const std::string& trade_date,
                      const std::vector<qts::StrategyId>& baskets,
                      qts::control::StrategyRegistry* registry,
                      std::unordered_set<qts::InstrumentId>& known,
                      std::uint64_t day_baseline,
                      std::uint64_t tick,
                      int submitted,
                      int rejected) {
  drain_all(channels);
  sync_risk_positions(risk, oms, paper_feed.book(), merge_live_baskets(baskets, registry), known, static_cast<qts::Price>(day_baseline));
  projector.push_new(ledger);
  if (tick % 50 == 0) {
    log.info("server", "demo progress",
             {{"tick", std::to_string(tick)},
              {"submitted", std::to_string(submitted)},
              {"rejected", std::to_string(rejected)},
              {"ledger_events", std::to_string(ledger->latest_event_id())},
              {"open", std::to_string(oms.open_orders().size())},
              {"link", trade.connected() ? "up" : "down"}});
    write_live_state(config.state_file, tick, trade_date, submitted, rejected, oms, paper_feed.book(), ledger->latest_event_id());
  }
  if (config.demo.interval_ms > 0) sleep_interruptible(config.demo.interval_ms);
}

int run_demo(Logger& log,
             qts::ChannelSet& channels,
             qts::TradeApi& trade,
             qts::Channel& paper_feed,
             const qts::InstrumentId& symbol,
             qts::OrderManager& oms,
             qts::RiskEngine& risk,
             qts::ExecutionAlgorithm& execution,
             const std::vector<std::unique_ptr<qts::Strategy>>& strategies,
             qts::EventBus& events,
             qts::EventLedger* ledger,
             SqliteProjector& projector,
             const qts::AppConfig& config,
             const std::string& config_path,
             const qts::SystemClock& wall_clock,
             DayRollover& rollover,
             const std::vector<qts::StrategyId>& baskets,
             qts::control::StrategyRegistry* registry,
             int& submitted,
             int& rejected) {
  qts::SimulatedQuoteFeed walk{config.demo.seed, 1'000, 50, 800, 1'200};
  qts::SimulatedQuoteFeed::Faults faults{};
  faults.auction_ticks = config.demo.auction_ticks;
  faults.halt_after = config.demo.halt_after;
  faults.halt_for = config.demo.halt_for;
  faults.stale_every = config.demo.stale_every;
  faults.crossed_every = config.demo.crossed_every;
  walk.set_faults(faults);
  // AMA mock source (M11-2): one deterministic walk per master entry through
  // the production snapshot normalizer. Legacy single-symbol walk stays the
  // default; nothing below changes shape when feed == "simulated".
  const bool use_ama = config.demo.feed == "ama";
  const std::unique_ptr<qts::SimulatedAmaFeed> ama = make_ama_feed(config);
#ifdef QTS_HAS_VENDOR_ADAPTERS
  // Live market source (ama.enabled): real UMS snapshots through the
  // production adapter. Missing creds/endpoints refuse startup (fail
  // closed); a connected session delivers during the tick loop below.
  int live_exit = 0;
  std::unique_ptr<LiveAmaFeed> live_ama = make_live_ama(log, config, live_exit);
  if (live_exit != 0) return live_exit;
  VendorSessionGuard live_guard{nullptr, live_ama != nullptr ? live_ama->adapter.get() : nullptr};
  const bool ama_shaped = use_ama || live_ama != nullptr;
#else
  const bool ama_shaped = use_ama;
#endif  // File quote source: one CSV row per tick in file order; EOF ends the
  // demo early (same graceful shutdown as the tick budget). Missing or
  // malformed files refuse startup (fail-closed, like every other input).
  int feed_exit = 0;
  const std::unique_ptr<qts::FileQuoteFeed> file = make_file_feed(log, config, feed_exit);
  if (feed_exit != 0) return feed_exit;
  std::uint64_t sequence = 0;
  std::uint64_t tick = 0;
  std::uint64_t held_ticks = 0;
  std::unordered_set<qts::InstrumentId> known;
  std::uint64_t day_baseline = paper_feed.book().realized_pnl();
  LinkState link{};
  // Seed risk from opening inventory before the first quote: funded
  // positions are settled and sellable, so strict T+1 must see them on
  // tick 1 instead of rejecting the first sell before the first sync.
  sync_risk_positions(risk, oms, paper_feed.book(), merge_live_baskets(baskets, registry), known, static_cast<qts::Price>(day_baseline));
  risk.set_equity_base(paper_feed.book().equity());  // M13 loss-ratio baseline (startup snapshot)
  qts::control::ControlContext control{oms, execution, risk, registry};
  control.ledger = ledger;  // M15 switch-command journal (null ledgers stay silent)
  ControlInbox inbox{config.control_inbox, 0, false};
  // Live query socket (empty path disables): tick-serviced, so the tick
  // thread stays the only reader of OMS/book/ledger — no extra thread.
  // A bad socket path refuses startup loudly, like any misconfiguration.
  std::unique_ptr<qts::AdminSocket> query_socket;
  try {
    query_socket = std::make_unique<qts::AdminSocket>(config.control_socket);
  } catch (const std::exception& e) {
    log.error("server", "query socket failed", {{"error", e.what()}});
    return 2;
  }
  if (query_socket->enabled()) log.info("server", "query socket enabled", {{"path", config.control_socket}});
  // Downstream gateway (empty socket disables): same tick-serviced shape as
  // the query socket above. A bad socket path refuses startup loudly.
  // ADR-054 live-cash view: the shared counter snapshot cache is fed by the
  // control-plane handlers below and read by the gateway (tick thread only).
  std::unique_ptr<GatewayFront> gateway;
  // ADR-054 live-cash view (always compiled; fed only in vendor builds).
  auto live_cache = std::make_shared<qts::gateway::GatewayService::CounterSnapshotCache>();
#ifdef QTS_HAS_VENDOR_ADAPTERS
  std::unique_ptr<qts::SqliteEventStore> ref_cache;
  if (!config.sqlite_file.empty()) {
    try {
      ref_cache = std::make_unique<qts::SqliteEventStore>(config.sqlite_file);
    } catch (const std::exception& e) {
      log.warn("server", "instruments cache unavailable", {{"error", e.what()}});
    }
  }
  CounterRefState ref_state{};
#endif
  try {
    std::vector<qts::AccountId> snapshot_accounts;
    snapshot_accounts.reserve(config.accounts.size());
    for (const auto& account : config.accounts) snapshot_accounts.push_back(account.id);
    gateway = std::make_unique<GatewayFront>(
        log, config, control, paper_feed.book(), gateway_account_books(config, channels), channels,
        [feed = &paper_feed](const qts::StrategyId& strategy, const qts::InstrumentId& instrument, qts::Quantity quantity, qts::Price avg_cost) {
          if (feed->fund) feed->fund(strategy, instrument, quantity, avg_cost);
        },
        events, ledger,
        [&, snapshot_accounts] {
          auto batch = fetch_account_snapshots(log, channels, trade, snapshot_accounts);
#ifdef QTS_HAS_VENDOR_ADAPTERS
          // Settle one-shots borrow the adapter handler slots: re-arm the
          // persistent cadence handlers afterwards (tick thread only).
          arm_counter_refdata(log, channels, config, risk, ref_cache.get(), live_cache, ref_state);
#endif
          return batch;
        });
    gateway->set_counter_snapshot_cache(live_cache);
#ifdef QTS_HAS_VENDOR_ADAPTERS
    arm_counter_refdata(log, channels, config, risk, ref_cache.get(), live_cache, ref_state);
#endif
  } catch (const std::exception& e) {
    log.error("server", "gateway setup failed", {{"error", e.what()}});
    return 2;
  }
  log.info("server", "demo long-run started",
           {{"ticks", std::to_string(config.demo.ticks)}, {"interval_ms", std::to_string(config.demo.interval_ms)}, {"seed", std::to_string(config.demo.seed)}, {"feed", config.demo.feed}});
  while (g_stop_flag == 0 && (config.demo.ticks == 0 || tick < config.demo.ticks)) {
    ++tick;
    maybe_reload_config(log, config_path, risk, wall_clock);
    inbox.poll(log, control);
    query_socket->poll_once([&] { return build_live_state(tick, rollover.trade_date, submitted, rejected, oms, paper_feed.book(), ledger->latest_event_id()); });
    gateway->poll();
#ifdef QTS_HAS_VENDOR_ADAPTERS
    // Counter sweep on login rising edge + 60s fund/asset cadence (ADR-054).
    poll_counter_refdata(log, channels, config, rollover.trade_date, live_cache, ref_state);
#endif
    if (maybe_rollover(log, channels, oms, rollover)) {
      day_baseline = paper_feed.book().realized_pnl();
      risk.settle_day();  // M13 counters/streaks reset; blocks persist until lifted
      risk.set_equity_base(paper_feed.book().equity());
    }
    // Submission freeze: a fully disconnected trade port holds new orders
    // (parents keep queueing in the algorithm) while cancels, queries and
    // market data stay live.
    const bool link_up = poll_link_state(log, channels, trade, oms, events, config, link, tick);
    std::vector<qts::QuoteEvent> quotes;
    quotes.reserve(4);
    if (use_ama) {
      quotes = ama->step();
#ifdef QTS_HAS_VENDOR_ADAPTERS
    } else if (live_ama != nullptr) {
      pull_live_quotes(*live_ama, quotes);
#endif
    } else if (!next_walk_quotes(walk, file.get(), log, symbol, tick, sequence, quotes)) {
      break;
    }
    for (const auto& quote : quotes) {
      gateway->on_quote(quote);  // no-op without subscribers
      handle_demo_quote(log, channels, config, ama_shaped, oms, execution, strategies, paper_feed, link_up, tick, quote, held_ticks, submitted, rejected);
    }
    finish_demo_tick(log, channels, trade, paper_feed, oms, risk, ledger, projector, config, rollover.trade_date, baskets, registry, known, day_baseline, tick, submitted, rejected);
    // M13 safety net: act on risk-queued auto stops after the submit phase
    // (idle ticks stay silent, no log spam).
    const std::string auto_acted = qts::control::apply_auto_actions(control);
    if (!auto_acted.empty()) log.info("server", "risk auto action", {{"actions", auto_acted}});
  }
  log.info("server", g_stop_flag != 0 ? "demo stopped by signal" : "demo tick budget reached", {{"tick", std::to_string(tick)}, {"submitted", std::to_string(submitted)}});
  cancel_all_open(log, oms, execution, channels);
  drain_all(channels);
  projector.push_new(ledger);  // project the shutdown cancels too, or the db lags the ledger
  write_live_state(config.state_file, tick, rollover.trade_date, submitted, rejected, oms, paper_feed.book(), ledger->latest_event_id());
  return 0;
}

// Risk basket universe for per-basket T+1 sync: passthrough always plus
// every enabled strategy, so resting strategies still get their sellable
// synced (their live orders, if any, keep reserving).
std::vector<qts::StrategyId> risk_baskets(const qts::AppConfig& config) {
  std::vector<qts::StrategyId> baskets{qts::kPassthroughStrategy};
  for (const auto& entry : config.strategies) {
    if (entry.enabled) baskets.push_back(entry.id);
  }
  return baskets;
}

// Graceful-shutdown snapshot plus ledger rotation (P0 disk bound).
// Snapshot always lands (crash-recovery checkpoint); rotation archives
// ledger+db aside only when over cap AND no open orders remain (a rotated
// book with live opens would strand state, so rotation defers with a loud
// warn instead). SQLite/storage handles are already shut down here.
void write_shutdown_snapshot(Logger& log, qts::OrderManager& oms, qts::EventLedger* ledger, const qts::AppConfig& config) {
  if (config.ledger_file.empty()) return;
  std::unordered_map<std::string, qts::OrderIntent> intents;
  for (const auto& event : ledger->replay(1)) {
    if (event.type == qts::LedgerEventType::order_submitted || event.type == qts::LedgerEventType::risk_rejected) {
      intents[event.intent.client_order_id] = event.intent;
    }
  }
  qts::OpenSnapshot snapshot{};
  snapshot.last_event_id = ledger->latest_event_id();
  snapshot.config_version = config.config_version;
  snapshot.rules_version = config.rules_version;
  for (const auto& open : oms.open_orders()) {
    const auto found = intents.find(open.client_order_id);
    if (found == intents.end()) {
      log.warn("server", "snapshot skipping order without intent", {{"client_order_id", open.client_order_id}});
      continue;
    }
    snapshot.opens.emplace_back(found->second, open);
  }
  std::error_code errc;
  snapshot.ledger_bytes = std::filesystem::file_size(config.ledger_file, errc);
  const std::string snapshot_path = snapshot_path_for(config.ledger_file);
  try {
    qts::write_open_snapshot(snapshot_path, snapshot);
  } catch (const std::exception& e) {
    log.error("server", "snapshot write failed", {{"error", e.what()}});
    return;
  }
  log.info("server", "snapshot written", {{"opens", std::to_string(snapshot.opens.size())}, {"ledger_events", std::to_string(snapshot.last_event_id)}});

  const std::uint64_t cap_bytes = config.retention.ledger_rotate_mb * 1024ULL * 1024ULL;
  if (cap_bytes != 0 && snapshot.ledger_bytes <= cap_bytes) return;
  if (!snapshot.opens.empty()) {
    log.warn("server", "rotation deferred: open orders remain", {{"opens", std::to_string(snapshot.opens.size())}});
    return;
  }
  const std::size_t slash = config.ledger_file.find_last_of('/');
  const std::string dir = slash == std::string::npos ? std::string{"."} : config.ledger_file.substr(0, slash);
  const std::string archive = dir + "/archive/" + archive_stamp() + "/";
  std::filesystem::create_directories(archive, errc);
  std::filesystem::rename(config.ledger_file, archive + config.ledger_file.substr(config.ledger_file.find_last_of('/') + 1), errc);
  if (errc) {
    log.error("server", "rotation move failed", {{"file", config.ledger_file}});
    return;
  }
  if (!config.sqlite_file.empty()) {
    for (const std::string& suffix : {"", "-wal", "-shm"}) {
      const std::string db_path = config.sqlite_file + suffix;
      if (std::filesystem::exists(db_path, errc)) {
        std::filesystem::rename(db_path, archive + db_path.substr(db_path.find_last_of('/') + 1), errc);
        if (errc) {
          log.error("server", "rotation move failed", {{"file", db_path}});
          return;
        }
      }
    }
  }
  log.info("server", "ledger rotated", {{"archive", archive}, {"bytes", std::to_string(snapshot.ledger_bytes)}});
}

int finish_run(Logger& log, qts::OrderManager& oms, qts::EventLedger* ledger, SqliteProjector& projector, qts::MetricsRegistry& metrics, const qts::AppConfig& config, int submitted, int rejected) {
  projector.finish();
  write_shutdown_snapshot(log, oms, ledger, config);
  const auto diffs = oms.reconcile_open_orders({});
  if (!diffs.empty()) {
    for (const auto& diff : diffs) {
      log.error("server", "reconcile diff", {{"client_order_id", diff.client_order_id}, {"reason", diff.reason}});
    }
    std::cerr << "reconcile dirty: " << diffs.size() << " diffs\n";
    return 1;
  }
  const qts::RunReport report = qts::summarize_ledger(ledger->replay(1), config.config_version, config.rules_version);
  std::cout << "drill ok: submitted=" << submitted << " ledger_events=" << ledger->latest_event_id() << " open=" << oms.open_orders().size() << '\n';
  std::cout << "report: filled=" << report.filled << " gross=" << report.gross_pnl << " fees=" << report.total_fees << " net=" << report.net_pnl << " drawdown=" << report.max_drawdown
            << " versions_match=" << report.versions_match << '\n';
  log.info("server", "drill finished",
           {{"submitted", std::to_string(submitted)},
            {"rejected", std::to_string(rejected)},
            {"ledger_events", std::to_string(ledger->latest_event_id())},
            {"filled", std::to_string(report.filled)},
            {"versions_match", report.versions_match ? "true" : "false"}});
  metrics.write_prometheus(std::cout);
  return 0;
}

}  // namespace

// PostgreSQL DSN resolution: only the configured env NAME is read here;
// the DSN itself never lands in yaml, logs or the ledger. Unset name or
// unset variable both mean "no postgres backend" (fail-safe like broker
// credentials: absence is fine, presence must be exact).
std::string resolve_pg_conninfo(const qts::AppConfig& config) {
  if (config.postgres.conninfo_env.empty()) return {};
  const char* resolved = std::getenv(config.postgres.conninfo_env.c_str());
  return resolved != nullptr ? resolved : "";
}

// Async query projection (M9-3 SQLite, formal-phase PostgreSQL):
// resumes above the stored watermark. The DSN never appears here: only
// the configured env NAME is read, and the backend refuses to start
// misconfigured (both backends set, or PG stub without infra). False when
// the backend refuses (already logged).
bool setup_projection(Logger& log, const qts::AppConfig& config, qts::EventLedger* ledger, SqliteProjector& projector) {
  try {
    projector.maybe_enable(config.sqlite_file, resolve_pg_conninfo(config), ledger);
  } catch (const std::exception& e) {
    log.error("server", "projection failed", {{"error", e.what()}});
    return false;
  }
  if (projector.sink != nullptr) {
    const std::string where = !config.sqlite_file.empty() ? config.sqlite_file : std::string{"postgres:"} + config.postgres.conninfo_env;
    log.info("server", "projection enabled", {{"backend", where}, {"watermark", std::to_string(projector.sink->watermark())}});
  }
  return true;
}

// Prometheus scrape endpoint (empty listen disables; drills stay silent).
// A bad listen address refuses startup loudly, like any misconfiguration.
// Null when disabled.
std::unique_ptr<qts::MetricsEndpoint> enable_metrics_endpoint(Logger& log, const qts::AppConfig& config, qts::MetricsRegistry& metrics) {
  if (config.metrics_endpoint.listen.empty()) return nullptr;
  try {
    auto endpoint = std::make_unique<qts::MetricsEndpoint>(config.metrics_endpoint.listen, metrics);
    log.info("server", "metrics endpoint enabled", {{"listen", config.metrics_endpoint.listen}, {"port", std::to_string(endpoint->port())}});
    return endpoint;
  } catch (const std::exception& e) {
    log.error("server", "metrics endpoint failed", {{"error", e.what()}});
    throw;
  }
}

// M1 paper drill (default) and M9 long-run demo (demo.enabled):
// scripted price path -> grid quotes -> risk -> OMS -> simulated matching.
// Drill exits non-zero on any rejected submit or dirty end-of-run
// reconciliation. Demo logs-and-continues, stops on ticks/SIGINT.
// Usage: qts_server [config.yaml] (no arg reads <exe-dir>/cfg/qts.yaml,
// so ./qts_server works straight from bin/).
int main(int argc, char** argv) {
  std::string config_path;
  bool defaulted = false;
  if (argc > 1) {
    config_path = argv[1];
  } else {
    // Resolve against the executable directory (portable argv[0] form, no
    // /proc dependency): ./qts_server from bin/ finds bin/cfg/qts.yaml,
    // and <root>/bin/qts_server from the root finds the same file.
    std::string self = argv[0];
    const std::size_t mark = self.find_last_of("/\\");
    const std::string exe_dir = mark == std::string::npos ? std::string{} : self.substr(0, mark);
    config_path = exe_dir.empty() ? "cfg/qts.yaml" : exe_dir + "/cfg/qts.yaml";
    defaulted = true;
  }
  qts::AppConfig config{};
  try {
    config = qts::load_app_config(config_path);
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    if (defaulted) std::cerr << "hint: copy " << config_path.substr(0, config_path.size() - 8) << "paper.example.yaml to " << config_path << " and adjust channels\n";
    return 2;
  }

  // Unified ./bin layout: ledger/sqlite/log/state parent dirs are created on
  // startup (the log sink creates its own), so a fresh clone runs as-is.
  for (const std::string& path : {config.ledger_file, config.sqlite_file, config.state_file, config.control_inbox}) {
    if (path.empty()) continue;
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) continue;
    std::error_code errc;
    std::filesystem::create_directories(path.substr(0, slash), errc);
  }
  if (!config.control_inbox.empty()) {
    // Touch the inbox so operators can append without guessing creation.
    std::ofstream touch{config.control_inbox, std::ios::app};
  }

  // Retention first: prune old per-startup logs before this run opens its
  // own (best effort, never fails startup).
  qts::log::prune_log_files(config.log_file, config.retention.log_files);
  Logger log{config.log_file};
  qts::log::Level minimum = qts::log::Level::info;
  if (!qts::log::parse_level(config.log_level, minimum)) minimum = qts::log::Level::info;
  qts::log::set_level(minimum);
  std::signal(SIGINT, on_stop_signal);
  std::signal(SIGTERM, on_stop_signal);
  std::signal(SIGHUP, on_reload_signal);
#ifndef _WIN32
  arm_crash_handler();  // segfault/abort prints a backtrace, then keeps the core dump
#endif

  qts::EventBus events;
  // Wall clock outlives everything below it (rate limiter, lagged delivery).
  qts::SystemClock wall_clock{};
  qts::ChannelSet channels{};
  qts::TradeApi* trade = nullptr;
  // Live-money gate date (ADR-039): stamped before channel construction so
  // any non-paper broker validates against it; arm_rollover later arms the
  // same label for the run (a midnight-straddling startup is operator error,
  // fail-closed by date mismatch on the next start).
  qts::Channel* paper_feed = setup_channels(log, config, events, wall_clock, channels, trade, today_label());
  if (paper_feed == nullptr || trade == nullptr) return 2;
#ifdef QTS_HAS_VENDOR_ADAPTERS
  VendorSessionGuard vendor_sessions{&channels, nullptr};
#endif
  qts::RiskEngine risk{config.risk};
  arm_risk(log, config, risk, wall_clock);
  // Ledger selection (M5-1): empty ledger_file keeps the deterministic
  // in-memory drill; a set path appends across restarts via FileEventLedger.
  std::unique_ptr<qts::EventLedger> file_ledger;
  qts::InMemoryEventLedger memory_ledger{};
  if (!config.ledger_file.empty()) file_ledger = std::make_unique<qts::FileEventLedger>(config.ledger_file);
  qts::EventLedger* ledger = file_ledger != nullptr ? file_ledger.get() : static_cast<qts::EventLedger*>(&memory_ledger);
  log.info("server", file_ledger != nullptr ? "file ledger enabled" : "in-memory ledger",
           {{std::string("path"), file_ledger != nullptr ? config.ledger_file : std::string("memory")},
            {"log_level", config.log_level},
            {"log_file", log.has_file() ? log.file_path() : std::string("stdout-only")}});
  qts::MetricsRegistry metrics{};
  // Prometheus scrape endpoint (empty listen disables; drills stay silent).
  std::unique_ptr<qts::MetricsEndpoint> metrics_endpoint;
  try {
    metrics_endpoint = enable_metrics_endpoint(log, config, metrics);
  } catch (const std::exception&) {
    return 2;
  }
  // Constructed detached: boot recovery replays history without recording
  // duplicates, then the ledger attaches below before live traffic.
  qts::OrderManager oms{*trade, risk, events, nullptr};
  if (!run_recovery(log, channels, *trade, oms, events, config, ledger)) return 2;
  oms.set_ledger(ledger);
  oms.set_ledger_versions(config.config_version, config.rules_version);
  // Trading-day rollover driver: settle chain fires when the wall clock
  // crosses trading_day.rollover (validated HH:MM at load). True process
  // restarts stay post-startup-recovery work; until then the long run
  // settles in place with positions preserved.
  DayRollover rollover{};
  arm_rollover(config, oms, log, rollover);
  maybe_record_funding(log, config, ledger, rollover.trade_date);
  oms.set_metrics(&metrics);

  // Async query projection (M9-3 SQLite, formal-phase PostgreSQL):
  // resumes above the stored watermark (misconfiguration refuses loudly).
  SqliteProjector projector{};
  if (!setup_projection(log, config, ledger, projector)) return 2;

  // Key policy: broker secrets only come from the environment, never from
  // the yaml file. Paper needs none; absence is routine (debug only, the
  // live counter session comes from gateway login, never this prefix).
  try {
    qts::BrokerCredentials credentials{};
    if (qts::try_load_broker_credentials("QTS_PAPER", credentials)) {
      log.info("server", "broker credentials loaded", {{"credentials", credentials.redacted()}});
    } else {
      log.debug("server", "no broker credentials in environment", {});
    }
  } catch (const std::exception& e) {
    log.error("server", "bad broker credentials", {{"error", e.what()}});
    return 2;
  }

  const qts::StrategyConfig* selected = nullptr;
  std::vector<std::unique_ptr<qts::Strategy>> strategies;
  std::unique_ptr<qts::ExecutionAlgorithm> execution;
  // Trader strategy registry: yaml instances are adopted inside
  // setup_strategies; runtime strategy_start instances join the same
  // fan-out vector with per-generation id prefixes. Null in drill runs.
  // Built by helper to keep main() under the complexity gate.
  std::unique_ptr<qts::control::StrategyRegistry> registry_store = make_strategy_registry(strategies, config, rollover.trade_date);
  qts::control::StrategyRegistry* registry = registry_store.get();
  if (!setup_strategies(log, config, rollover.trade_date, strategies, execution, selected, registry)) return 2;
  // M4-4: execution sees every OMS report for fill-driven algorithms
  // (iceberg refresh, live-child pruning). No-op for Immediate/TWAP/POV.
  // The debug line makes broker acks/fills visible without touching OMS.
  events.subscribe_execution([&](const qts::ExecutionReport& report) {
    log_report(log, report);
    execution->on_execution_report(report);
  });

  int submitted = 0;
  int rejected = 0;
  // Risk basket universe: passthrough always plus every enabled strategy,
  // so per-basket T+1 sellable syncs even for strategies resting this run.
  const std::vector<qts::StrategyId> baskets = risk_baskets(config);
  // Drill stays single-strategy (first enabled) so its counts never move;
  // the long run fans out to every enabled strategy (empty in trader-driven
  // v1 standby: registry-started instances join the same fan-out).
  const qts::InstrumentId no_symbol{};
  const qts::InstrumentId& symbol = selected != nullptr ? selected->instrument : no_symbol;
  const int path_code = !config.demo.enabled
                            ? run_drill(log, channels, *paper_feed, selected->instrument, oms, risk, *execution, *strategies.front(), ledger, projector, config.state_file, baskets, submitted)
                            : run_demo(log, channels, *trade, *paper_feed, symbol, oms, risk, *execution, strategies, events, ledger, projector, config, config_path, wall_clock, rollover, baskets,
                                       registry, submitted, rejected);
  if (path_code != 0) return path_code;
  return finish_run(log, oms, ledger, projector, metrics, config, submitted, rejected);
}
