// clang-format off
/**
  ******************************************************************************
  * @file           : ops_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for metrics, log levels/filtering/files and credentials.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include <netdb.h>
#include <sys/socket.h>

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#if defined(_WIN32)
  #include <process.h>
  #define STS_TEST_PID _getpid()
#else
  #include <unistd.h>
  #define STS_TEST_PID ::getpid()
#endif

#include "sts/broker/simulated_broker.hpp"
#include "sts/config/credentials.hpp"
#include "sts/oms/order_manager.hpp"
#include "sts/ops/log.hpp"
#include "sts/ops/metrics.hpp"
#include "sts/ops/metrics_endpoint.hpp"

namespace {

void counters_and_gauges() {
  sts::MetricsRegistry metrics{};
  assert(metrics.get("missing") == 0);
  metrics.inc("orders_total");
  metrics.inc("orders_total", 2);
  assert(metrics.get("orders_total") == 3);
  metrics.set_gauge("open_orders", 5);
  assert(metrics.get("open_orders") == 5);
  metrics.set_gauge("open_orders", 2);
  assert(metrics.get("open_orders") == 2);

  std::ostringstream out;
  metrics.write_prometheus(out);
  const std::string text = out.str();
  assert(text.find("orders_total 3") != std::string::npos);
  assert(text.find("open_orders 2") != std::string::npos);
  assert(text.find("# TYPE orders_total counter") != std::string::npos);
  assert(text.find("# TYPE open_orders gauge") != std::string::npos);
}

void structured_log_lines() {
  sts::log::set_level(sts::log::Level::info);
  std::ostringstream out;
  sts::log::info(out, "gateway", "drill finished", {{"submitted", "6"}});
  const std::string line = out.str();
  assert(line.back() == '\n' && line.find('\n') == line.size() - 1);  // exactly one JSON object
  assert(line.find("\"level\":\"info\"") != std::string::npos);
  assert(line.find("\"component\":\"gateway\"") != std::string::npos);
  assert(line.find("\"submitted\":\"6\"") != std::string::npos);
  assert(line.find("\"time\":\"") != std::string::npos);

  std::ostringstream escaped;
  sts::log::warn(escaped, "c\"omponent", "a\nb", {});
  assert(escaped.str().find("\\\"") != std::string::npos);
  assert(escaped.str().find("\\n") != std::string::npos);
}

void log_levels_filter_debug() {
  sts::log::Level parsed = sts::log::Level::info;
  assert(sts::log::parse_level("debug", parsed) && parsed == sts::log::Level::debug);
  assert(sts::log::parse_level("error", parsed) && parsed == sts::log::Level::error);
  assert(!sts::log::parse_level("verbose", parsed));
  assert(std::string(sts::log::level_name(sts::log::Level::debug)) == "debug");

  sts::log::set_level(sts::log::Level::info);
  assert(sts::log::level() == sts::log::Level::info);
  assert(!sts::log::should_log(sts::log::Level::debug));
  assert(sts::log::should_log(sts::log::Level::error));
  std::ostringstream dropped;
  sts::log::debug(dropped, "gateway", "noisy", {});
  assert(dropped.str().empty());

  sts::log::set_level(sts::log::Level::debug);
  assert(sts::log::should_log(sts::log::Level::debug));
  std::ostringstream kept;
  sts::log::debug(kept, "gateway", "noisy", {});
  assert(kept.str().find("\"level\":\"debug\"") != std::string::npos);
  sts::log::set_level(sts::log::Level::info);  // leave the default for other tests
}

void log_file_names_differ_per_day_and_startup() {
  std::tm day_a{};
  day_a.tm_year = 126;
  day_a.tm_mon = 9;
  day_a.tm_mday = 2;
  day_a.tm_hour = 10;
  day_a.tm_min = 0;
  day_a.tm_sec = 0;
  std::tm day_b = day_a;
  day_b.tm_mday = 3;
  const std::string for_a = sts::log::resolve_log_file("/tmp/sts-demo.log", day_a, 100);
  const std::string for_b = sts::log::resolve_log_file("/tmp/sts-demo.log", day_b, 100);
  assert(for_a != for_b);  // daily names differ
  assert(for_a.find("20261002") != std::string::npos);
  assert(for_b.find("20261003") != std::string::npos);

  const std::string pid_a = sts::log::resolve_log_file("/tmp/sts-demo.log", day_a, 100);
  const std::string pid_b = sts::log::resolve_log_file("/tmp/sts-demo.log", day_a, 101);
  assert(pid_a != pid_b);  // per-startup names differ
  assert(pid_a.find("pid100") != std::string::npos);

  // strftime patterns expand: %Y-%m-%d becomes the calendar date.
  const std::string dated = sts::log::resolve_log_file("/tmp/sts-%Y%m%d.log", day_a, 100);
  assert(dated.find("%") == std::string::npos);
  assert(dated.find("20261002") != std::string::npos);

  assert(sts::log::resolve_log_file("").empty());
}

void logger_writes_unique_files() {
  sts::log::set_level(sts::log::Level::info);
  const auto dir = std::filesystem::temp_directory_path() / ("sts-log-test-" + std::to_string(STS_TEST_PID));
  std::filesystem::create_directories(dir);
  const std::string pattern = (dir / "sts-test.log").string();

  sts::log::Logger first{pattern};
  sts::log::Logger second{pattern};
  assert(first.has_file() && second.has_file());
  assert(first.file_path() != pattern);             // stamp inserted, never the raw pattern
  assert(first.file_path() != second.file_path());  // same second still collides safely
  first.info("gateway", "hello", {});
  first.debug("gateway", "noisy", {});  // dropped at info
  second.warn("gateway", "watch", {});
  std::string body;
  {
    std::ifstream in{first.file_path()};
    assert(static_cast<bool>(in));
    std::ostringstream out;
    out << in.rdbuf();
    body = out.str();
  }
  assert(body.find("\"level\":\"info\"") != std::string::npos);
  assert(body.find("\"message\":\"hello\"") != std::string::npos);
  assert(body.find("noisy") == std::string::npos);
  assert(body.find("\"time\":\"") != std::string::npos);

  sts::log::Logger stdout_only{""};
  assert(!stdout_only.has_file() && stdout_only.file_path().empty());

  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

void prune_keeps_newest_and_ignores_strangers() {
  const auto dir = std::filesystem::temp_directory_path() / ("sts-log-prune-" + std::to_string(STS_TEST_PID));
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir);
  const std::string pattern = (dir / "sts-prune.log").string();
  for (const std::string& name :
       {"sts-prune-20261001-000000-pid1.log", "sts-prune-20261002-000000-pid1.log", "sts-prune-20261003-000000-pid1.log", "unrelated.log", "sts-prune-20261004-000000-pid1.txt"}) {
    std::ofstream out{dir / name};
    out << "x\n";
  }
  sts::log::prune_log_files(pattern, 2);
  assert(std::filesystem::exists(dir / "sts-prune-20261003-000000-pid1.log"));
  assert(std::filesystem::exists(dir / "sts-prune-20261002-000000-pid1.log"));
  assert(!std::filesystem::exists(dir / "sts-prune-20261001-000000-pid1.log"));
  assert(std::filesystem::exists(dir / "unrelated.log"));                       // different stem kept
  assert(std::filesystem::exists(dir / "sts-prune-20261004-000000-pid1.txt"));  // different extension kept
  sts::log::prune_log_files(pattern, 0);                                        // 0 keeps everything
  assert(std::filesystem::exists(dir / "sts-prune-20261002-000000-pid1.log"));
  sts::log::prune_log_files("", 1);  // empty pattern is a no-op
  std::filesystem::remove_all(dir, ec);
}

void oms_feeds_metrics() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  sts::RiskEngine risk{{100, 10'000, true}};
  sts::MetricsRegistry metrics{};
  sts::OrderManager oms{broker, risk, events, nullptr};
  oms.set_metrics(&metrics);

  sts::OrderIntent ok{"m1", "paper", "test", "TEST", sts::Side::buy, sts::OrderType::limit, sts::TimeInForce::day, 10, 100};
  assert(oms.submit(ok));
  sts::OrderIntent too_big{"m2", "paper", "test", "TEST", sts::Side::buy, sts::OrderType::limit, sts::TimeInForce::day, 10'000, 100};
  assert(!oms.submit(too_big));  // risk rejection
  assert(oms.cancel("m1"));

  assert(metrics.get("oms_orders_submitted_total") == 2);
  assert(metrics.get("oms_orders_risk_rejected_total") == 1);
  assert(metrics.get("oms_cancels_requested_total") == 1);
  assert(metrics.get("oms_executions_applied_total") >= 2);  // live ack + cancel report
  assert(metrics.get("oms_open_orders") == 0);               // m1 cancelled, m2 rejected
}

std::string http_get(const std::string& host, std::uint16_t port, const std::string& target) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* list = nullptr;
  assert(getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &list) == 0);
  int fd = -1;
  for (addrinfo* item = list; item != nullptr; item = item->ai_next) {
    fd = ::socket(item->ai_family, item->ai_socktype, item->ai_protocol);
    if (fd < 0) continue;
    if (::connect(fd, item->ai_addr, item->ai_addrlen) == 0) break;
    ::close(fd);
    fd = -1;
  }
  freeaddrinfo(list);
  assert(fd >= 0);
  const std::string request = "GET " + target + " HTTP/1.0\r\n\r\n";
  assert(::send(fd, request.data(), request.size(), 0) == static_cast<ssize_t>(request.size()));
  std::string reply;
  char chunk[512]{0};
  for (;;) {
    const ssize_t done = ::recv(fd, chunk, sizeof(chunk) - 1, 0);
    if (done <= 0) break;
    reply.append(chunk, static_cast<std::size_t>(done));
  }
  ::close(fd);
  return reply;
}

void metrics_endpoint_serves_registry() {
  sts::MetricsRegistry metrics{};
  metrics.inc("oms_orders_submitted_total", 7);
  sts::MetricsEndpoint endpoint{"127.0.0.1:0", metrics};
  assert(endpoint.enabled() && endpoint.port() != 0);
  const std::string body = http_get("127.0.0.1", endpoint.port(), "/metrics");
  assert(body.find("HTTP/1.0 200") != std::string::npos);
  assert(body.find("oms_orders_submitted_total 7") != std::string::npos);
  const std::string missing = http_get("127.0.0.1", endpoint.port(), "/nope");
  assert(missing.find("404") != std::string::npos);
}

void metrics_endpoint_disabled_and_bad_listen() {
  sts::MetricsRegistry metrics{};
  sts::MetricsEndpoint off{"", metrics};
  assert(!off.enabled() && off.port() == 0);
  bool threw = false;
  try {
    sts::MetricsEndpoint bad{"no-colon-here", metrics};
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
}

void credentials_come_from_env_only() {
  ::unsetenv("STS_OPSTEST_CUST_ID");
  ::unsetenv("STS_OPSTEST_FUND_ACCOUNT_ID");
  ::unsetenv("STS_OPSTEST_PASSWORD");

  sts::BrokerCredentials creds{};
  assert(!sts::try_load_broker_credentials("STS_OPSTEST", creds));
  assert(creds.empty());
  bool threw = false;
  try {
    (void)sts::load_broker_credentials("STS_OPSTEST");
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);

  ::setenv("STS_OPSTEST_CUST_ID", "cust-1", 1);
  threw = false;  // partial secrets fail closed
  try {
    (void)sts::try_load_broker_credentials("STS_OPSTEST", creds);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);

  ::setenv("STS_OPSTEST_FUND_ACCOUNT_ID", "fund-1", 1);
  ::setenv("STS_OPSTEST_PASSWORD", "topsecret-value", 1);
  assert(sts::try_load_broker_credentials("STS_OPSTEST", creds));
  assert(creds.cust_id == "cust-1" && creds.fund_account_id == "fund-1" && creds.password == "topsecret-value");
  const std::string redacted = creds.redacted();
  assert(redacted.find("topsecret-value") == std::string::npos);  // values never printed
  assert(redacted.find("password=set") != std::string::npos);

  ::unsetenv("STS_OPSTEST_CUST_ID");
  ::unsetenv("STS_OPSTEST_FUND_ACCOUNT_ID");
  ::unsetenv("STS_OPSTEST_PASSWORD");
}

}  // namespace

int main() {
  counters_and_gauges();
  structured_log_lines();
  log_levels_filter_debug();
  log_file_names_differ_per_day_and_startup();
  logger_writes_unique_files();
  prune_keeps_newest_and_ignores_strangers();
  oms_feeds_metrics();
  metrics_endpoint_serves_registry();
  metrics_endpoint_disabled_and_bad_listen();
  credentials_come_from_env_only();
  return 0;
}
