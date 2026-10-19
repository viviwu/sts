// clang-format off
/**
  ******************************************************************************
  * @file           : config.hpp
  * @author         : vivi wu
  * @brief          : Application config schema authority, loader contract and validation.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "sts/broker/fee_schedule.hpp"
#include "sts/core/types.hpp"
#include "sts/risk/risk_engine.hpp"

namespace sts {

// Application configuration contract. This header is the schema authority:
// bin/cfg/*.yaml must match these field names and validation rules.
// File parsing is backed by yaml-cpp (FetchContent, pinned 0.8.0).
// Relative ledger/sqlite/log paths resolve against the config file's own
// directory (unified ./bin layout), so the gateway starts from anywhere.
struct AccountConfig {
  AccountId id;
  std::string broker;  // adapter name, e.g. "simulated"
  SecurityAccountId security_account_id;
};

// Per-channel routing statistics (M8-0, ADR-030). Fee is contractual;
// fill_rate_bps/latency_us start as yesterday's aggregates and move to
// online measurement in M8-2. An empty channels list disables routing.
struct ChannelStatsConfig {
  std::string channel;
  std::int64_t fee_bps{0};
  std::int64_t fill_rate_bps{0};  // 0..10000
  std::int64_t latency_us{0};
  std::int64_t split_weight{0};  // >0 joins M8-1 pro-rata splits
};

struct RoutingConfig {
  std::int64_t weight_fee{1};
  std::int64_t weight_latency{1};
  std::int64_t weight_fill{1};
  // EWMA blend per observed sample, 1..10000 (M8-2, default 2000 = 20%).
  // Fill-rate/latency start at the channels' configured yesterday values.
  std::int64_t ewma_alpha_bps{2000};
  std::vector<ChannelStatsConfig> channels;
};

struct StrategyConfig {
  StrategyId id;
  bool enabled{false};
  // "grid" or "rebalance" today; unknown types fail closed at startup.
  std::string type{"grid"};
  AccountId account{"paper"};
  InstrumentId instrument{"TEST"};
  Price lower{900};
  Price upper{1'100};
  int grids{4};
  Quantity quantity{100};
  Price start{1'000};  // grid anchor (first-quote alignment reference)
  // Allocated settled inventory for this strategy (stock allocation):
  // immediately sellable shares funded into the strategy book basket at
  // dynamic start (gateway stock/stock_avg). Distinct from start above
  // (a price, not inventory). stock_avg 0 means unknown cost (realized
  // PnL measured from zero).
  Quantity stock{0};
  Price stock_avg{0};
  // Rebalance targets (only for type "rebalance"): goal weights in bps of
  // the strategy's equity, summing to (0, 10000]; the rest is cash buffer.
  struct TargetWeight {
    InstrumentId instrument;
    int weight_bps{0};
  };
  std::vector<TargetWeight> rebalance_targets;
  int rebalance_band_bps{0};  // per-symbol notional deadband in bps
};

// Execution algorithm selection. Only one level is configured: the gateway
// builds a single algorithm instance shared by all parents. Params carry
// sane defaults so `algo: twap` alone is runnable.
struct ExecutionConfig {
  std::string algo{"immediate"};  // immediate|twap|pov|vwap|iceberg
  int twap_slices{4};
  std::int64_t twap_interval_ms{1'000};
  int pov_participation_bps{1'000};
  std::vector<int> vwap_weights{5000, 3000, 2000};
  std::int64_t vwap_interval_ms{1'000};
  Quantity iceberg_display{100};
};

// Paper-simulator fidelity knobs (self-owned channels only). Latency
// needs a live clock (wired by the gateway) and wall-clock time to elapse,
// so it only bites in paced long runs, never in the instant drill.
struct StartingPosition {
  InstrumentId instrument;
  Quantity quantity{0};
  Price avg_cost{0};
  // Owning strategy basket (M11-7 follow-up): empty means the passthrough
  // basket (manual/grid-assumed inventory). Portfolio-aware strategies
  // (rebalance) can only sell what is sellable inside their OWN basket, so
  // their opening inventory must name them here.
  StrategyId strategy;
};

struct PaperSimConfig {
  Price starting_cash{1'000'000};
  // True only when starting_cash was written explicitly: the gateway emits
  // a funding event for it (M11-7). Untouched configs keep the silent
  // legacy path (drill ledgers stay byte-identical).
  bool starting_cash_set{false};
  std::int64_t latency_ms{0};
  Price slippage_per_unit{0};
  int fee_rate_bp{0};  // legacy: commission-only fallback when fees: is absent
  // A-share fee schedule (M11-5, optional): when the fees: section is
  // present it replaces fee_bp entirely (commission/stamp/transfer in ppm,
  // per-fill minimum). Absent keeps the exact legacy commission behavior.
  bool use_fees{false};
  FeeSchedule fees;
  // Dedicated fee file (M11-6, optional): default schedule plus exact
  // instrument-id overrides, so ops adjusts rates without touching the main
  // config. When set it wins over the inline fees: section above.
  std::string fee_file;
  bool use_fee_table{false};
  FeeTable fee_table;
  // Opening inventory (M11-1): settled positions funded into every paper
  // channel at startup (day_buy = 0, immediately sellable). Cash is left
  // untouched: starting_cash is settled cash, positions are existing
  // holdings, matching broker snapshot semantics.
  std::vector<StartingPosition> starting_positions;
  // Fault drills (long runs only): every N ticks, drop the listed channels
  // for M ticks to exercise freeze/resume/failover. Empty channel list
  // disables. Unknown names fail validation.
  std::uint64_t chaos_every_ticks{0};
  std::uint64_t chaos_outage_ticks{0};
  std::vector<std::string> chaos_channels;
};

// Versioned instrument master (M11-1, optional): one entry per tradeable
// symbol. When empty the risk gate stays venue-agnostic (legacy drills).
// When non-empty, unknown instruments fail closed and every order is
// validated for tick/lot/band/identity against its entry.
struct InstrumentConfig {
  InstrumentId id;
  std::string exchange{"sse"};  // sse|szse|bse
  std::string board{"main"};    // main|star|chinext|beijing_select
  Price tick{1};
  Quantity min_qty{100};
  Quantity lot_step{100};
  Quantity max_qty{0};  // 0 = no cap
  bool sell_odd_lot_only{false};
  bool market_orders_allowed{true};
  Price limit_down{0};  // 0 = no static band
  Price limit_up{0};
};

// Live AMA market-data source (M7-2 server wiring, vendor-gated runtime).
// Unlike demo.feed (walks and files), this owns a real AmaQuoteAdapter:
// UMS endpoints come from yaml (1..8 "ip:port"), the UMS username/password
// from STS_AMA_* env at startup (key policy: secrets never enter yaml).
// Disabled by default; exclusive with demo.feed ama|file (one live market
// source at a time, fail closed). Subscribes the instruments master.
struct AmaFeedConfig {
  bool enabled{false};
  std::vector<std::string> ums_servers;
  std::int64_t staleness_ms{3000};
};

// Live QuantAPI trade channel (M7-2 server wiring, vendor-gated runtime).
// Non-secret session params live here (ADR-053): gateway user/password are
// session params, not trade secrets (private files hold real values; tracked
// files keep placeholders only). Customer identity/password are NEVER here:
// the trader passes them at gateway login and the counter session is built
// lazily (v1: one counter session per process, persists past gateway
// logout). Automation (headless, no client) pre-logs in from gateway-side
// env through the same login path. Referenced by accounts[].broker ==
// "quantapi" (one TradeApi per broker, shared by N accounts).
struct QuantapiChannelConfig {
  std::string locations;       // gateway "ip:port;ip:port", required when used
  std::string sidecar_path;    // seq/map persistence; empty = next to ledger
  std::string order_way{"4"};  // vendor prop, <=4 chars
  std::string agw_user;        // gateway user, optional; pair with agw_password
  std::string agw_password;    // gateway password (session param, see above)
};

// Formal-phase PostgreSQL projection (schema-compatible successor to the
// SQLite file below). Only the ENV NAME is configured here — the DSN
// itself never enters yaml, logs or the ledger (credentials policy, same
// as broker secrets). Empty disables; set alongside sqlite_file is an
// error (exactly one projection backend, enforced by the factory).
struct PostgresConfig {
  std::string conninfo_env;
};

// Prometheus scrape endpoint (observability wrap-up): "host:port" to serve
// the metrics registry on /metrics, empty disables (default: drills and
// offline tools stay silent).
struct MetricsEndpointConfig {
  std::string listen;
};

// Retention against unbounded disk growth (P0): per-pattern newest log
// files kept (0 keeps everything); the JSONL ledger rotates at graceful
// shutdown past ledger_rotate_mb (0 rotates every shutdown, handy for
// tests). Rotation archives ledger+db and only fires with no open orders
// (snapshot covers the rest); live files are never touched mid-run.
struct RetentionConfig {
  std::size_t log_files{20};
  std::uint64_t ledger_rotate_mb{64};
};

// Trading-day boundary (local HH:MM, default 00:01). The gateway fires
// the settle chain (channel books -> OMS dedup -> trade_date restamp) when
// the wall clock crosses it; A-share spot has no night session so shortly
// after midnight is safely outside trading and settlement batches.
struct TradingDayConfig {
  std::string rollover{"00:01"};
};

// Strict HH:MM 24-hour parse for the rollover above. Returns false (out
// untouched) on any malformed input.
bool parse_rollover(const std::string& text, int& hour, int& minute);
// Next local-time rollover strictly after `now` (day wrap via mktime, so
// DST transitions normalize instead of skipping a settle).
std::time_t next_rollover_after(std::time_t now, int hour, int minute);

// Continuous demo feed (M9-0, ADR-034). Disabled by default so the
// one-shot drill stays byte-identical. When enabled the gateway runs a
// deterministic simulated walk instead of the scripted price path:
// ticks == 0 means run until SIGINT; otherwise stop after ticks quotes.
struct DemoConfig {
  bool enabled{false};
  std::uint64_t ticks{200};
  std::uint64_t interval_ms{0};
  std::uint64_t seed{42};
  // Standing depth for M6 book simulation (0 levels = off, reference-price
  // fills). When on, the gateway posts a symmetric ladder around each quote.
  std::uint64_t depth_levels{0};
  Quantity depth_quantity{0};
  Price depth_tick{1};
  // Mock-market realism (M11-0, all opt-in, 0 = off): leading auction ticks,
  // halt window [halt_after, halt_after+halt_for), deterministic dirty ticks.
  std::uint64_t auction_ticks{0};
  std::uint64_t halt_after{0};
  std::uint64_t halt_for{0};
  std::uint64_t stale_every{0};
  std::uint64_t crossed_every{0};
  // Market source (M11-2, opt-in): "simulated" keeps the legacy single-symbol
  // walk; "ama" replays the tradeable universe through SimulatedAmaFeed
  // (production normalizer + tick tracker, one snapshot per instrument).
  // "file" replays one CSV row per tick from file (instrument,last,bid,
  // ask,bid_qty,ask_qty in file order; EOF ends the demo early).
  std::string feed{"simulated"};  // simulated|ama|file
  // Quote file for feed == "file" (resolved against the config directory).
  // Empty otherwise; missing file or malformed rows fail closed at startup.
  std::string file;
};

// Downstream gateway entry (ADR-050): Unix socket and/or TCP endpoint
// serving GatewayTransport (both empty disable). The gateway stores NO
// passwords: paper accounts stay open (socket permissions are the access
// control); live-broker accounts need their broker password at login (the
// same secret the server logged into the counter with, held in env).
struct GatewayConfig {
  std::string socket;  // Unix socket path, empty disables
  // TCP listen endpoint "host:port" (P1): empty disables. Loopback only by
  // default (e.g. 127.0.0.1:19001); non-loopback binds need an explicit
  // operator decision (no TLS in P1, see §9.6). Port 0 selects an
  // ephemeral port (tests); the server logs the bound endpoint.
  std::string bind;
};

struct AppConfig {
  // "paper" or "sandbox:<adapter>". "live" is rejected by validate()
  // until M3 completes funding, permission and disaster-recovery drills.
  std::string environment;
  std::string config_version;  // echoed into EventEnvelope::config_version
  std::string rules_version;   // echoed into EventEnvelope::rules_version
  // Optional JSONL event-ledger path (FileEventLedger). Empty keeps the
  // in-memory ledger: deterministic drills and tests. A set path makes the
  // gateway append across restarts and resume event ids from the file.
  std::string ledger_file;
  // Optional structured-log file sink (M9-1). Empty keeps stdout-only
  // logging; a set path appends one JSON object per line in addition to
  // stdout. Never carries secrets (see config/credentials.hpp).
  std::string log_file;
  // Minimum log level (M9.1): "debug"/"info"/"warn"/"error", default "info".
  // Unknown values fail validate(); the gateway applies it via log::set_level.
  std::string log_level{"info"};
  // Optional SQLite projection path (M9-3, ADR-034). Empty disables the
  // async query projection; a set path enables it. The JSONL ledger above
  // stays the recovery authority, SQLite is a restartable read model.
  std::string sqlite_file;
  // Optional PostgreSQL projection (formal phase): env NAME holding the
  // DSN (e.g. STS_PG_CONNINFO). Mutually exclusive with sqlite_file.
  PostgresConfig postgres;
  // Optional live-state snapshot path. When set, the server writes a small
  // JSON file every 50 ticks and at shutdown (counters, live opens,
  // per-strategy positions, watermarks); sts_admin reads it with zero IPC.
  // Empty disables. Relative paths resolve against the config directory.
  std::string state_file;
  // Optional operator inbox path (M11-4). When set, the server polls one
  // JSON command per line (sts_admin send appends them, zero IPC):
  // cancel/cancel_all/block_* /halt/resume/replace. Empty disables.
  // Relative paths resolve against the config directory.
  std::string control_inbox;
  // Optional live query socket path (observability): when set, the server
  // answers "state" with freshly built live-state JSON once per tick
  // (`sts_admin state --socket ...`). Unix-domain socket, empty disables.
  // Relative paths resolve against the config directory.
  std::string control_socket;
  // Retired approval file (ADR-051): no longer parsed; old keys are
  // ignored. Live authorization is the broker password (env).
  // Downstream gateway entry (ADR-050): Unix socket and/or TCP serving the
  // sts::gateway API to frontend apps (both empty disable). No passwords
  // live here: paper accounts stay open, live-broker accounts log in with
  // their broker password. Drills never enable the gateway at all.
  GatewayConfig gateway;
  std::vector<AccountConfig> accounts;
  RiskLimits risk;
  AmaFeedConfig ama;
  QuantapiChannelConfig quantapi;
  std::vector<StrategyConfig> strategies;
  std::vector<InstrumentConfig> instruments;
  RoutingConfig routing;
  DemoConfig demo;
  TradingDayConfig trading_day;
  PaperSimConfig paper_sim;
  RetentionConfig retention;
  RiskSwitches switches;
  ExecutionConfig execution;
  MetricsEndpointConfig metrics_endpoint;

  static AppConfig paper_defaults() {
    AppConfig config;
    config.environment = "paper";
    config.config_version = "1";
    config.rules_version = "1";
    config.log_level = "info";
    config.accounts.push_back({"paper", "simulated", ""});
    config.risk = RiskLimits{1000, 10'000'000, true};
    sts::StrategyConfig placeholder{};
    placeholder.id = "example";
    config.strategies.push_back(placeholder);
    return config;
  }

  // Returns true when valid; otherwise false with a human-readable reason.
  [[nodiscard]] bool validate(std::string& error) const {
    if (environment != "paper" && environment.compare(0, 8, "sandbox:") != 0) {
      error = R"(environment must be "paper" or "sandbox:<adapter>")";
      return false;
    }
    if (config_version.empty() || rules_version.empty()) {
      error = "config_version and rules_version must be non-empty";
      return false;
    }
    if (risk.max_order_quantity < 0 || risk.max_order_notional < 0 || risk.max_position_quantity < 0 || risk.daily_loss_limit < 0) {
      error = "risk limits must be non-negative (0 means no limit)";
      return false;
    }
    if (risk.max_consecutive_rejects < 0 || risk.reject_rate_min_orders < 0) {
      error = "risk reject gates must be non-negative (0 disables)";
      return false;
    }
    if (risk.max_reject_rate_bps < 0 || risk.max_reject_rate_bps > 10'000 || risk.max_loss_ratio_bps < 0 || risk.max_loss_ratio_bps > 10'000) {
      error = "risk ratio gates must be 0..10000 bps (0 disables)";
      return false;
    }
    if (risk.rate_window_ms < 0) {
      error = "risk.rate.window_ms must be non-negative";
      return false;
    }
    if (log_level != "debug" && log_level != "info" && log_level != "warn" && log_level != "error") {
      error = "log.level must be one of debug/info/warn/error";
      return false;
    }
    int rollover_hour = 0;
    int rollover_minute = 0;
    if (!parse_rollover(trading_day.rollover, rollover_hour, rollover_minute)) {
      error = "trading_day.rollover must be HH:MM (24-hour local time)";
      return false;
    }
    if (demo.depth_tick <= 0 || demo.depth_quantity < 0 || (demo.depth_levels > 0 && demo.depth_quantity == 0)) {
      error = "demo depth needs tick>0, quantity>=0, and quantity>0 when levels>0";
      return false;
    }
    if (demo.feed != "simulated" && demo.feed != "ama" && demo.feed != "file") {
      error = "demo.feed must be simulated|ama|file";
      return false;
    }
    if (demo.feed == "file" && demo.file.empty()) {
      error = "demo.feed file needs demo.file set";
      return false;
    }
    return valid_accounts(accounts, error) && valid_strategies(strategies, error) && valid_instruments(instruments, error) && valid_demo_feed(demo.feed, strategies, instruments, error) &&
           valid_vendor_feeds(ama, quantapi, demo, accounts, strategies, instruments, error) && valid_routing(routing, error) && valid_execution(execution, error) &&
           valid_paper_sim(paper_sim, accounts, error);
  }

  [[nodiscard]] static bool ama_master_contains(const std::vector<InstrumentConfig>& instruments, const std::string& instrument_id) {
    return std::any_of(instruments.begin(), instruments.end(), [&](const InstrumentConfig& entry) { return entry.id == instrument_id; });
  }

  [[nodiscard]] static bool valid_rebalance_universe(const StrategyConfig& strategy, const std::vector<InstrumentConfig>& instruments, std::string& error) {
    // A rebalancer's universe is its targets; the scalar instrument field
    // is unused (grid-only) and must not gate it.
    for (const auto& target : strategy.rebalance_targets) {
      if (!ama_master_contains(instruments, target.instrument)) {
        error = "strategy '" + strategy.id + "' target is not in the instruments master (ama feed): " + target.instrument;
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] static bool valid_demo_feed(const std::string& feed, const std::vector<StrategyConfig>& strategies, const std::vector<InstrumentConfig>& instruments, std::string& error) {
    if (feed != "ama") return true;
    if (instruments.empty()) {
      error = "demo.feed ama needs a non-empty instruments master";
      return false;
    }
    for (const auto& entry : instruments) {
      if (!valid_ama_instrument_id(entry, error)) return false;
    }
    for (const auto& strategy : strategies) {
      if (!strategy.enabled) continue;
      if (strategy.type == "rebalance") {
        if (!valid_rebalance_universe(strategy, instruments, error)) return false;
        continue;
      }
      if (!ama_master_contains(instruments, strategy.instrument)) {
        error = "strategy '" + strategy.id + "' instrument is not in the instruments master (ama feed)";
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] static bool valid_ums_endpoint(const std::string& endpoint) {
    const std::size_t colon = endpoint.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= endpoint.size()) return false;
    try {
      const int port = std::stoi(endpoint.substr(colon + 1));
      return port >= 1 && port <= 65535;
    } catch (const std::exception&) {
      return false;
    }
  }

  // Live vendor sources (M7-2 server wiring): AMA owns the market-data feed
  // (exclusive with the simulated/file demo feeds, same master discipline),
  // QuantAPI locations are required exactly when an account routes to it.
  // Customer identity/password are never configured (gateway login only);
  // the gateway user pair is a yaml session param (ADR-053).
  [[nodiscard]] static bool valid_vendor_feeds(const AmaFeedConfig& ama,
                                               const QuantapiChannelConfig& quantapi,
                                               const DemoConfig& demo,
                                               const std::vector<AccountConfig>& accounts,
                                               const std::vector<StrategyConfig>& strategies,
                                               const std::vector<InstrumentConfig>& instruments,
                                               std::string& error) {
    if (ama.enabled) {
      if (demo.feed != "simulated") {
        error = "ama.enabled is exclusive with demo.feed (live market source replaces walks/files)";
        return false;
      }
      if (ama.staleness_ms <= 0) {
        error = "ama.staleness_ms must be positive";
        return false;
      }
      if (ama.ums_servers.empty() || ama.ums_servers.size() > 8) {
        error = "ama.ums_servers needs 1..8 ip:port endpoints";
        return false;
      }
      for (const auto& endpoint : ama.ums_servers) {
        if (!valid_ums_endpoint(endpoint)) {
          error = "ama.ums_servers endpoint must be ip:port: " + endpoint;
          return false;
        }
      }
      if (!valid_demo_feed("ama", strategies, instruments, error)) return false;
    }
    bool quantapi_used = false;
    for (const auto& account : accounts) {
      if (account.broker == "quantapi") quantapi_used = true;
    }
    if (quantapi_used && quantapi.locations.empty()) {
      error = "quantapi.locations is required when an account uses broker quantapi";
      return false;
    }
    if (!quantapi.order_way.empty() && quantapi.order_way.size() > 4) {
      error = "quantapi.order_way must be <=4 chars";
      return false;
    }
    if (quantapi.agw_user.empty() != quantapi.agw_password.empty()) {
      error = "quantapi.agw_user/agw_password must be set together";
      return false;
    }
    return true;
  }

  [[nodiscard]] static bool valid_ama_instrument_id(const InstrumentConfig& entry, std::string& error) {
    const std::size_t dot = entry.id.find('.');
    if (dot == std::string::npos || dot + 1 >= entry.id.size()) {
      error = "ama feed instrument id must be CODE.SUFFIX: " + entry.id;
      return false;
    }
    const std::string suffix = entry.id.substr(dot + 1);
    if (suffix != "SSE" && suffix != "SZSE" && suffix != "BSE") {
      error = "ama feed instrument suffix must be SSE|SZSE|BSE: " + entry.id;
      return false;
    }
    std::string lowered;
    lowered.reserve(suffix.size());
    for (const char mark : suffix) lowered += static_cast<char>(std::tolower(static_cast<unsigned char>(mark)));
    if (lowered != entry.exchange) {
      error = "ama feed instrument suffix does not match exchange: " + entry.id;
      return false;
    }
    return true;
  }

  [[nodiscard]] static bool valid_instruments(const std::vector<InstrumentConfig>& instruments, std::string& error) {
    for (const auto& entry : instruments) {
      if (entry.id.empty()) {
        error = "instrument id must be non-empty";
        return false;
      }
      int seen = 0;
      for (const auto& other : instruments) {
        if (other.id == entry.id && ++seen > 1) {
          error = "duplicate instrument id: " + entry.id;
          return false;
        }
      }
      if (entry.exchange != "sse" && entry.exchange != "szse" && entry.exchange != "bse") {
        error = "unknown exchange for instrument '" + entry.id + "': " + entry.exchange;
        return false;
      }
      if (entry.board != "main" && entry.board != "star" && entry.board != "chinext" && entry.board != "beijing_select") {
        error = "unknown board for instrument '" + entry.id + "': " + entry.board;
        return false;
      }
      if (entry.tick <= 0 || entry.min_qty <= 0 || entry.lot_step <= 0 || entry.max_qty < 0 || entry.limit_down < 0 || entry.limit_up < 0) {
        error = "instrument '" + entry.id + "' needs tick/min/lot positive and max/limits non-negative";
        return false;
      }
      if (entry.limit_down > 0 && entry.limit_up > 0 && entry.limit_up < entry.limit_down) {
        error = "instrument '" + entry.id + "' needs limit_up >= limit_down";
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] static bool valid_fee_schedule(const FeeSchedule& schedule, const std::string& owner, std::string& error) {
    if (schedule.commission_ppm < 0 || schedule.commission_minimum < 0 || schedule.stamp_ppm < 0 || schedule.transfer_ppm < 0) {
      error = owner + " rates and minimum must be non-negative";
      return false;
    }
    return true;
  }

  [[nodiscard]] static bool valid_fee_table(const FeeTable& table, std::string& error) {
    if (!valid_fee_schedule(table.fallback, "paper_sim fee file default", error)) return false;
    for (const auto& entry : table.overrides) {
      if (entry.first.empty()) {
        error = "paper_sim fee file override id must be non-empty";
        return false;
      }
      if (!valid_fee_schedule(entry.second, "paper_sim fee file override " + entry.first, error)) return false;
    }
    return true;
  }

  [[nodiscard]] static bool valid_paper_sim(const PaperSimConfig& sim, const std::vector<AccountConfig>& accounts, std::string& error) {
    if (sim.starting_cash < 0 || sim.latency_ms < 0 || sim.slippage_per_unit < 0 || sim.fee_rate_bp < 0) {
      error = "paper_sim values must be non-negative";
      return false;
    }
    if (sim.fees.commission_ppm < 0 || sim.fees.commission_minimum < 0 || sim.fees.stamp_ppm < 0 || sim.fees.transfer_ppm < 0) {
      error = "paper_sim.fees rates and minimum must be non-negative";
      return false;
    }
    if (sim.use_fee_table) {
      if (!valid_fee_table(sim.fee_table, error)) return false;
    }
    for (const auto& position : sim.starting_positions) {
      if (position.instrument.empty() || position.quantity <= 0 || position.avg_cost < 0) {
        error = "paper_sim.starting_positions needs instrument, positive quantity and non-negative avg_cost";
        return false;
      }
    }
    if (sim.chaos_every_ticks == 0) return true;
    if (sim.chaos_outage_ticks == 0 || sim.chaos_channels.empty()) {
      error = "paper_sim chaos needs outage ticks and a non-empty channel list";
      return false;
    }
    for (const auto& name : sim.chaos_channels) {
      bool known = false;
      for (const auto& account : accounts) {
        if (account.broker == name) {
          known = true;
          break;
        }
      }
      if (!known) {
        error = "paper_sim chaos channel has no matching accounts[].broker: " + name;
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] static bool valid_execution(const ExecutionConfig& execution, std::string& error) {
    if (execution.algo != "immediate" && execution.algo != "twap" && execution.algo != "pov" && execution.algo != "vwap" && execution.algo != "iceberg") {
      error = "unknown execution algo: " + execution.algo;
      return false;
    }
    if (execution.twap_slices <= 0) {
      error = "execution.twap_slices must be positive";
      return false;
    }
    if (execution.pov_participation_bps < 0 || execution.pov_participation_bps > 10'000) {
      error = "execution.pov_participation_bps must be within 0..10000";
      return false;
    }
    if (execution.vwap_weights.empty()) {
      error = "execution.vwap_weights must not be empty";
      return false;
    }
    for (const int weight : execution.vwap_weights) {
      if (weight <= 0) {
        error = "execution.vwap_weights must be positive";
        return false;
      }
    }
    if (execution.iceberg_display <= 0) {
      error = "execution.iceberg_display must be positive";
      return false;
    }
    return true;
  }

 private:
  [[nodiscard]] static bool valid_accounts(const std::vector<AccountConfig>& accounts, std::string& error) {
    if (accounts.empty()) {
      error = "at least one account is required";
      return false;
    }
    for (const auto& account : accounts) {
      if (account.id.empty() || account.broker.empty()) {
        error = "account id and broker must be non-empty";
        return false;
      }
      int seen = 0;
      for (const auto& other : accounts) {
        if (other.id == account.id && ++seen > 1) {
          error = "duplicate account id: " + account.id;
          return false;
        }
      }
    }
    return true;
  }

  [[nodiscard]] static bool valid_strategies(const std::vector<StrategyConfig>& strategies, std::string& error) {
    for (const auto& strategy : strategies) {
      if (strategy.id.empty()) {
        error = "strategy id must be non-empty";
        return false;
      }
      int seen = 0;
      for (const auto& other : strategies) {
        if (other.id == strategy.id && ++seen > 1) {
          error = "duplicate strategy id: " + strategy.id;
          return false;
        }
      }
      if (strategy.type != "grid" && strategy.type != "rebalance") {
        error = "unknown strategy type for '" + strategy.id + "': " + strategy.type;
        return false;
      }
      if (strategy.type == "grid") {
        if (strategy.grids <= 0 || strategy.quantity <= 0 || strategy.upper <= strategy.lower) {
          error = "strategy '" + strategy.id + "' needs grids>0, quantity>0 and upper>lower";
          return false;
        }
      } else {
        if (!valid_rebalance(strategy, error)) return false;
      }
      // Allocated inventory is dynamic-start only (gateway stock):
      // yaml inventory stays in paper_sim.starting_positions (per strategy).
      if (strategy.stock != 0 || strategy.stock_avg != 0) {
        error = "strategy '" + strategy.id + "' stock is dynamic-start only: use paper_sim.starting_positions for yaml inventory";
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] static bool valid_rebalance(const StrategyConfig& strategy, std::string& error) {
    if (strategy.rebalance_targets.empty()) {
      error = "strategy '" + strategy.id + "' needs at least one rebalance target";
      return false;
    }
    if (strategy.rebalance_band_bps < 0) {
      error = "strategy '" + strategy.id + "' needs rebalance band_bps>=0";
      return false;
    }
    int total = 0;
    for (const auto& target : strategy.rebalance_targets) {
      if (target.instrument.empty()) {
        error = "strategy '" + strategy.id + "' has an empty rebalance instrument";
        return false;
      }
      int seen = 0;
      for (const auto& other : strategy.rebalance_targets) {
        if (other.instrument == target.instrument && ++seen > 1) {
          error = "strategy '" + strategy.id + "' duplicates rebalance instrument: " + target.instrument;
          return false;
        }
      }
      if (target.weight_bps < 0 || target.weight_bps > 10'000) {
        error = "strategy '" + strategy.id + "' needs rebalance weights within 0..10000";
        return false;
      }
      total += target.weight_bps;
    }
    if (total <= 0 || total > 10'000) {
      error = "strategy '" + strategy.id + "' needs rebalance weights summing to (0, 10000]";
      return false;
    }
    return true;
  }

  [[nodiscard]] static bool valid_channel_stats(const ChannelStatsConfig& channel, const std::vector<ChannelStatsConfig>& channels, std::string& error) {
    if (channel.channel.empty()) {
      error = "routing channel name must be non-empty";
      return false;
    }
    if (channel.fee_bps < 0 || channel.latency_us < 0 || channel.split_weight < 0 || channel.fill_rate_bps < 0 || channel.fill_rate_bps > 10'000) {
      error = "routing stats out of range for channel: " + channel.channel;
      return false;
    }
    int seen = 0;
    for (const auto& other : channels) {
      if (other.channel == channel.channel && ++seen > 1) {
        error = "duplicate routing channel: " + channel.channel;
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] static bool valid_routing(const RoutingConfig& routing, std::string& error) {
    if (routing.channels.empty()) return true;
    if (routing.weight_fee < 0 || routing.weight_latency < 0 || routing.weight_fill < 0) {
      error = "routing weights must be non-negative";
      return false;
    }
    if (routing.ewma_alpha_bps < 1 || routing.ewma_alpha_bps > 10'000) {
      error = "routing ewma_alpha_bps must be within 1..10000";
      return false;
    }
    for (const auto& channel : routing.channels) {
      if (!valid_channel_stats(channel, routing.channels, error)) return false;
    }
    return true;
  }
};

// Loads and validates a YAML config file. Throws std::runtime_error with
// file context on missing files, parse errors or validation failures.
[[nodiscard]] AppConfig load_app_config(const std::string& path);

// Dedicated fee table file (M11-6): default schedule plus exact
// instrument-id overrides. Missing file, bad shape and negative rates throw.
[[nodiscard]] FeeTable load_fee_table(const std::string& path);

// Instrument master conversions (M11-1): config strings to domain enums
// and rules. Unknown strings fail closed; callers validate first.
[[nodiscard]] Exchange parse_exchange(const std::string& text);
[[nodiscard]] AshareBoard parse_board(const std::string& text);
[[nodiscard]] AshareInstrumentRules instrument_rules_from(const InstrumentConfig& entry);

}  // namespace sts
