// clang-format off
/**
  ******************************************************************************
  * @file           : quantapi_readonly_main.cpp
  * @author         : vivi wu
  * @brief          : QuantAPI read-only联调 tool: login plus account/position/order queries.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
// sts_quantapi_readonly — M7-2c 联调工具: QuantAPI 交易只读链路 (ADR-029).
//
// Boots the QuantapiTradeAdapter against the live gateway over TCP, logs in,
// issues the two read-only control queries (open orders + account snapshot),
// pumps drain() on this thread and emits one structured log line per
// ExecutionReport / snapshot / recovery event plus a stats summary.
//
// Read-only by construction: this tool never calls submit() or cancel().
// Real order flow is M7-2d and requires the separate single-approval gate.
//
// Credentials come from the environment only (never argv, never files,
// never the repo): STS_QUANTAPI_CUST_ID, STS_QUANTAPI_FUND_ACCOUNT_ID,
// STS_QUANTAPI_PASSWORD (fail-closed triple via load_broker_credentials);
// STS_QUANTAPI_CLIENT_FEATURE_CODE is vendor-required; the optional gateway
// user pair STS_QUANTAPI_AGW_USER/STS_QUANTAPI_AGW_PASSWORD must be set
// together; STS_QUANTAPI_LOCATIONS defaults to the lab gateway; the sidecar
// (seq/map persistence) defaults to ./sts_quantapi_readonly.sidecar.
//
// Usage: sts_quantapi_readonly [--seconds 30]
//
// Exit codes: 0 snapshot received, 1 logged in but no snapshot within the
// window, 2 bad usage or credentials, 3 vendor Init rejected the config,
// 4 login failed or timed out.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "sts/config/credentials.hpp"
#include "sts/ops/log.hpp"
#include "quantapi_trade_adapter.hpp"

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int) { g_stop.store(true); }

const char* status_name(sts::OrderStatus status) {
  switch (status) {
    case sts::OrderStatus::pending_new:
      return "pending_new";
    case sts::OrderStatus::live:
      return "live";
    case sts::OrderStatus::partially_filled:
      return "partially_filled";
    case sts::OrderStatus::filled:
      return "filled";
    case sts::OrderStatus::pending_cancel:
      return "pending_cancel";
    case sts::OrderStatus::cancelled:
      return "cancelled";
    case sts::OrderStatus::rejected:
      return "rejected";
  }
  return "unknown";
}

std::string env_or(const char* name, const std::string& fallback) {
  const char* value = std::getenv(name);
  return value != nullptr ? std::string(value) : fallback;
}

}  // namespace

int main(int argc, char** argv) {
  int seconds = 30;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help") {
      std::cout << "usage: sts_quantapi_readonly [--seconds N]" << std::endl;
      return 0;
    }
    if (arg == "--seconds" && i + 1 < argc) {
      seconds = std::atoi(argv[++i]);
      continue;
    }
    std::cerr << "unknown argument: " << arg << std::endl;
    return 2;
  }

  sts::QuantapiSessionConfig config;
  try {
    const sts::BrokerCredentials credentials = sts::load_broker_credentials("STS_QUANTAPI");
    config.cust_id = credentials.cust_id;
    config.fund_account_id = credentials.fund_account_id;
    config.password = credentials.password;
  } catch (const std::exception& ex) {
    sts::log::error(std::cout, "quantapi_readonly", "credentials unavailable", {{"detail", ex.what()}});
    return 2;
  }
  config.client_feature_code = env_or("STS_QUANTAPI_CLIENT_FEATURE_CODE", "");
  if (config.client_feature_code.empty()) {
    sts::log::error(std::cout, "quantapi_readonly", "STS_QUANTAPI_CLIENT_FEATURE_CODE is required", {});
    return 2;
  }
  config.locations = env_or("STS_QUANTAPI_LOCATIONS", "172.16.109.17:32001");
  config.sidecar_path = env_or("STS_QUANTAPI_SIDECAR", "sts_quantapi_readonly.sidecar");
  config.agw_user = env_or("STS_QUANTAPI_AGW_USER", "");
  config.agw_password = env_or("STS_QUANTAPI_AGW_PASSWORD", "");
  if (config.agw_user.empty() != config.agw_password.empty()) {
    sts::log::error(std::cout, "quantapi_readonly", "AGW user/password must be set together", {});
    return 2;
  }

  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  sts::QuantapiTradeAdapter adapter(config);
  std::atomic<int> report_count{0};
  std::atomic<int> snapshot_count{0};
  std::atomic<int> recovery_count{0};
  adapter.set_execution_handler([&](const sts::ExecutionReport& report) {
    ++report_count;
    sts::log::info(std::cout, "quantapi_readonly", "execution_report",
                   {{"order_id", report.order_id},
                    {"client_order_id", report.client_order_id},
                    {"status", status_name(report.status)},
                    {"last_qty", std::to_string(report.last_quantity)},
                    {"last_price", std::to_string(report.last_price)},
                    {"cum_qty", std::to_string(report.cumulative_quantity)},
                    {"reject_code", std::to_string(report.reject_code)},
                    {"reason", report.reason}});
  });
  adapter.set_account_snapshot_handler([&](const sts::TradeAccountSnapshot& snapshot) {
    ++snapshot_count;
    sts::log::info(std::cout, "quantapi_readonly", "account_snapshot",
                   {{"account_id", snapshot.account_id},
                    {"total_asset", std::to_string(snapshot.total_asset)},
                    {"available_cash", std::to_string(snapshot.available_cash)},
                    {"withdrawable_cash", std::to_string(snapshot.withdrawable_cash)},
                    {"frozen_cash", std::to_string(snapshot.frozen_cash)},
                    {"positions", std::to_string(snapshot.positions.size())}});
    for (const sts::PositionSnapshot& position : snapshot.positions) {
      sts::log::info(std::cout, "quantapi_readonly", "position",
                     {{"instrument_id", position.instrument_id},
                      {"total", std::to_string(position.total_quantity)},
                      {"sellable", std::to_string(position.sellable_quantity)},
                      {"frozen", std::to_string(position.frozen_quantity)},
                      {"average_cost", std::to_string(position.average_cost)}});
    }
  });
  adapter.set_recovery_complete_handler([&](const sts::AccountId& account, bool success) {
    ++recovery_count;
    sts::log::info(std::cout, "quantapi_readonly", "recovery_complete", {{"account_id", account}, {"success", success ? "true" : "false"}});
  });

  if (!adapter.start()) {
    const sts::QuantapiAdapterStats stats = adapter.stats();
    adapter.stop();
    if (stats.init_calls == 0) {
      sts::log::error(std::cout, "quantapi_readonly", "adapter config rejected before vendor Init", {});
      return 2;
    }
    if (stats.login_calls == 0) {
      sts::log::error(std::cout, "quantapi_readonly", "vendor Init rejected the config", {{"init_rc", std::to_string(stats.init_rc)}});
      return 3;
    }
    sts::log::error(std::cout, "quantapi_readonly", "login failed or timed out", {});
    return 4;
  }
  sts::log::info(std::cout, "quantapi_readonly", "logged in", {{"locations", adapter.config().locations}});

  // Read-only control queries (archforce §4): open orders + account snapshot.
  adapter.request_open_orders(config.fund_account_id);
  adapter.request_account_snapshot(config.fund_account_id);

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
  while (!g_stop.load() && std::chrono::steady_clock::now() < deadline) {
    adapter.drain();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  adapter.drain();

  const sts::QuantapiAdapterStats stats = adapter.stats();
  adapter.stop();
  sts::log::info(std::cout, "quantapi_readonly", "stats",
                 {{"reports_enqueued", std::to_string(stats.reports_enqueued)},
                  {"reports_delivered", std::to_string(stats.reports_delivered)},
                  {"acks_received", std::to_string(stats.acks_received)},
                  {"rtns_received", std::to_string(stats.rtns_received)},
                  {"query_rows", std::to_string(stats.query_rows)},
                  {"unattributed_rtns", std::to_string(stats.unattributed_rtns)},
                  {"unmatched_acks", std::to_string(stats.unmatched_acks)},
                  {"status_unknown_drops", std::to_string(stats.status_unknown_drops)},
                  {"recovery_completes", std::to_string(stats.recovery_completes)},
                  {"logout_events", std::to_string(stats.logout_events)}});

  if (snapshot_count.load() > 0) return 0;
  sts::log::warn(std::cout, "quantapi_readonly", "logged in but no account snapshot received",
                 {{"reports", std::to_string(report_count.load())}, {"recoveries", std::to_string(recovery_count.load())}});
  return 1;
}
