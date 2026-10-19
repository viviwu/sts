// clang-format off
/**
  ******************************************************************************
  * @file           : ama_readonly_main.cpp
  * @author         : vivi wu
  * @brief          : AMA read-only联调 tool: snapshot/tick verification against real UMS feed.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
// sts_ama_readonly — M7-2b 联调工具: AMA 行情只读链路 (ADR-029).
//
// Boots the AmaQuoteAdapter against a live UMS, subscribes exact codes,
// pumps drain() on this thread and emits one structured log line per
// QuoteEvent plus periodic counters. Read-only: it never touches TradeApi.
//
// Credentials come from the environment only (never argv, never files,
// never the repo): STS_AMA_CUST_ID = UMS username, STS_AMA_PASSWORD =
// UMS password, STS_AMA_FUND_ACCOUNT_ID = placeholder required by the
// fail-closed credential policy (unused for market data).
//
// Usage:
//   sts_ama_readonly --ums 192.168.1.101:11001[,ip:port...] \
//                    --instruments 600000.SSE,000001.SZSE \
//                    [--seconds 30] [--staleness-ms 3000]
//
// Exit codes: 0 events delivered, 1 connected but no events, 2 bad usage or
// credentials, 3 vendor Init rejected the config.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "ama_quote_adapter.hpp"
#include "sts/config/credentials.hpp"
#include "sts/ops/log.hpp"

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int) { g_stop.store(true); }

std::vector<std::string> split(const std::string& text, char sep) {
  std::vector<std::string> parts;
  std::size_t begin = 0;
  while (begin <= text.size()) {
    const std::size_t end = text.find(sep, begin);
    parts.push_back(text.substr(begin, end == std::string::npos ? std::string::npos : end - begin));
    if (end == std::string::npos) break;
    begin = end + 1;
  }
  return parts;
}

struct Options {
  std::string ums;
  std::string instruments;
  int seconds{30};
  int staleness_ms{3000};
};

bool parse_args(int argc, char** argv, Options& options, std::string& error) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help") {
      error.clear();
      return false;
    }
    if (i + 1 >= argc) {
      error = "missing value for " + arg;
      return false;
    }
    const std::string value = argv[++i];
    if (arg == "--ums") {
      options.ums = value;
    } else if (arg == "--instruments") {
      options.instruments = value;
    } else if (arg == "--seconds") {
      options.seconds = std::atoi(value.c_str());
    } else if (arg == "--staleness-ms") {
      options.staleness_ms = std::atoi(value.c_str());
    } else {
      error = "unknown argument " + arg;
      return false;
    }
  }
  if (options.ums.empty()) {
    error = "--ums is required";
    return false;
  }
  if (options.instruments.empty()) {
    error = "--instruments is required";
    return false;
  }
  if (options.seconds <= 0 || options.staleness_ms <= 0) {
    error = "--seconds and --staleness-ms must be positive";
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  std::string error;
  if (!parse_args(argc, argv, options, error)) {
    if (!error.empty()) std::cerr << "error: " << error << "\n";
    std::cerr << "usage: sts_ama_readonly --ums <ip:port[,ip:port...]> --instruments <id[,id...]> [--seconds N] [--staleness-ms N]\n";
    return 2;
  }

  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  sts::AmaSessionConfig config;
  try {
    const sts::BrokerCredentials credentials = sts::load_broker_credentials("STS_AMA");
    config.username = credentials.cust_id;
    config.password = credentials.password;
  } catch (const std::exception& ex) {
    sts::log::error(std::cout, "ama_readonly", "credentials unavailable", {{"detail", ex.what()}});
    return 2;
  }
  for (const std::string& endpoint : split(options.ums, ',')) {
    const std::size_t colon = endpoint.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= endpoint.size()) {
      sts::log::error(std::cout, "ama_readonly", "malformed UMS endpoint", {{"endpoint", endpoint}});
      return 2;
    }
    config.ums_servers.push_back(sts::AmaUmsServer{endpoint.substr(0, colon), static_cast<std::uint16_t>(std::stoi(endpoint.substr(colon + 1)))});
  }
  config.max_staleness = std::chrono::milliseconds{options.staleness_ms};

  sts::AmaQuoteAdapter adapter;
  adapter.set_quote_handler([](const sts::QuoteEvent& event) {
    sts::log::info(std::cout, "ama_quote", "quote",
                   {
                       {"instrument", event.instrument_id},
                       {"bid", std::to_string(event.bid_price)},
                       {"bid_qty", std::to_string(event.bid_quantity)},
                       {"ask", std::to_string(event.ask_price)},
                       {"ask_qty", std::to_string(event.ask_quantity)},
                       {"last", std::to_string(event.last_price)},
                       {"seq", std::to_string(event.sequence)},
                       {"quality", std::to_string(static_cast<int>(event.quality))},
                   });
  });

  if (!adapter.start(config)) {
    sts::log::error(std::cout, "ama_readonly", "vendor Init rejected the config (check UMS list and credential lengths)");
    return 3;
  }
  sts::log::info(std::cout, "ama_readonly", "started", {{"vendor_version", adapter.vendor_version()}});

  std::vector<sts::InstrumentId> instruments;
  for (const std::string& id : split(options.instruments, ',')) instruments.push_back(id);
  adapter.subscribe(instruments);

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{options.seconds};
  std::size_t delivered = 0;
  auto next_report = std::chrono::steady_clock::now() + std::chrono::seconds{5};
  while (!g_stop.load() && std::chrono::steady_clock::now() < deadline) {
    delivered += adapter.drain();
    const auto now = std::chrono::steady_clock::now();
    if (now >= next_report) {
      const auto stats = adapter.stats();
      sts::log::info(std::cout, "ama_readonly", "progress",
                     {
                         {"connected", adapter.connected() ? "true" : "false"},
                         {"received", std::to_string(stats.snapshots_received)},
                         {"delivered", std::to_string(stats.events_delivered)},
                         {"dropped", std::to_string(stats.snapshots_dropped)},
                         {"ticks", std::to_string(stats.ticks_seen)},
                         {"gaps", std::to_string(stats.tick_gaps)},
                         {"subs_applied", std::to_string(stats.subscriptions_applied)},
                         {"subs_invalid", std::to_string(stats.subscriptions_invalid)},
                     });
      next_report = now + std::chrono::seconds{5};
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
  }
  delivered += adapter.drain();

  // Replay tick sequence holes observed during the window (ADR-028).
  std::size_t backfill_requests = 0;
  for (const sts::AmaTickGap& gap : adapter.take_tick_gaps()) {
    if (adapter.request_tick_backfill(gap)) ++backfill_requests;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds{500});  // let paged replies land
  adapter.drain();

  const auto stats = adapter.stats();
  const bool connected = adapter.connected();
  adapter.stop();
  sts::log::info(std::cout, "ama_readonly", "final",
                 {
                     {"connected", connected ? "true" : "false"},
                     {"received", std::to_string(stats.snapshots_received)},
                     {"delivered", std::to_string(delivered)},
                     {"dropped", std::to_string(stats.snapshots_dropped)},
                     {"ticks", std::to_string(stats.ticks_seen)},
                     {"gaps", std::to_string(stats.tick_gaps)},
                     {"backfill_requests", std::to_string(backfill_requests)},
                     {"backfill_ticks", std::to_string(stats.backfill_ticks)},
                     {"logons", std::to_string(stats.logon_events)},
                     {"disconnects", std::to_string(stats.disconnect_events)},
                     {"incomplete", std::to_string(stats.incomplete_events)},
                 });
  if (delivered == 0) return 1;
  return 0;
}
