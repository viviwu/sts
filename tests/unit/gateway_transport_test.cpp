// clang-format off
/**
  ******************************************************************************
  * @file           : gateway_transport_test.cpp
  * @author         : vivi wu
  * @brief          : GatewayTransport tests: req/reply protocol, auth, orders, pushes, drops.
  * @version        : 0.1.0
  * @date           : 10/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/gateway/gateway_transport.hpp"

#include <cassert>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

#include "nlohmann/json.hpp"
#include "sts/broker/simulated_broker.hpp"
#include "sts/execution/execution_api.hpp"
#include "sts/persistence/ledger.hpp"
#include "sts/strategy/grid_strategy.hpp"

namespace {

std::string socket_path() { return (std::filesystem::temp_directory_path() / "sts-gateway-transport-test.sock").string(); }

std::unique_ptr<sts::Strategy> make_test_grid(const sts::StrategyConfig& configured, const std::string& id_prefix) {
  if (configured.type != "grid") throw std::runtime_error("unknown strategy type: " + configured.type);
  sts::GridConfig grid{};
  grid.strategy_id = configured.id;
  grid.account_id = configured.account;
  grid.instrument_id = configured.instrument;
  grid.lower = configured.lower;
  grid.upper = configured.upper;
  grid.grids = configured.grids;
  grid.quantity_per_order = configured.quantity;
  grid.id_prefix = id_prefix;
  return std::make_unique<sts::GridStrategy>(grid, configured.start);
}

struct Harness {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  sts::RiskEngine risk{{1'000'000, 0, true}};
  sts::InMemoryEventLedger ledger{};
  sts::OrderManager oms{broker, risk, events, &ledger};
  sts::ImmediateAlgorithm execution{};
  sts::Portfolio book{10'000'000};
  std::vector<std::unique_ptr<sts::Strategy>> live;
  sts::control::StrategyRegistry registry{live, make_test_grid, "gateway-seed"};
  sts::control::ControlContext context{oms, execution, risk, &registry};
  sts::gateway::GatewayService front{context, book};
  sts::gateway::GatewayTransport transport{front, socket_path()};

  Harness() { broker.set_starting_cash(10'000'000); }
};

nlohmann::json ask(sts::gateway::GatewayClient& client, sts::gateway::GatewayTransport& transport, nlohmann::json request) {
  client.send(request.dump());
  transport.poll_once();
  return nlohmann::json::parse(client.recv_line());
}

void basic_ops_need_no_login() {
  Harness harness{};
  sts::gateway::GatewayClient client{socket_path()};
  // Paper-open mode (no login accounts): socket permissions are the access
  // control. Login refuses as disabled; whoami reports the default admin.
  assert(ask(client, harness.transport, {{"req", 1}, {"op", "ping"}})["ok"] == true);
  const auto cash = ask(client, harness.transport, {{"req", 2}, {"op", "cash"}});
  assert(cash["ok"] == true && cash["data"]["cash"] == 10'000'000 && cash["data"]["shared"] == true);
  assert(ask(client, harness.transport, {{"req", 3}, {"op", "login"}})["ok"] == false);
  assert(ask(client, harness.transport, {{"req", 4}, {"op", "logout"}})["ok"] == true);
  const auto me = ask(client, harness.transport, {{"req", 5}, {"op", "whoami"}});
  assert(me["ok"] == true && me["data"]["operator"] == "admin" && me["data"]["authed"] == true);
}

sts::gateway::GatewayAuth live_auth() {
  sts::gateway::GatewayAuth auth{};
  auth.login_accounts = {"live"};
  auth.verify = [](const sts::AccountId& account, const std::string& password) { return account == "live" && password == "live-pw"; };
  return auth;
}

struct AuthHarness {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  sts::RiskEngine risk{{1'000'000, 0, true}};
  sts::InMemoryEventLedger ledger{};
  sts::OrderManager oms{broker, risk, events, &ledger};
  sts::ImmediateAlgorithm execution{};
  sts::Portfolio book{10'000'000};
  std::vector<std::unique_ptr<sts::Strategy>> live;
  sts::control::StrategyRegistry registry{live, make_test_grid, "gateway-seed"};
  sts::control::ControlContext context{oms, execution, risk, &registry};
  sts::gateway::GatewayService front{context, book, {"paper", "live"}};
  sts::gateway::GatewayTransport transport{front, socket_path(), "", live_auth()};

  AuthHarness() { broker.set_starting_cash(10'000'000); }
};

void account_login_and_bound_account() {
  AuthHarness harness{};
  sts::gateway::GatewayClient client{socket_path()};
  // Ping passes pre-login; global views refuse on a guarded server.
  assert(ask(client, harness.transport, {{"req", 1}, {"op", "ping"}})["ok"] == true);
  assert(ask(client, harness.transport, {{"req", 2}, {"op", "cash"}})["ok"] == false);
  // Paper accounts stay open; live accounts need their password.
  assert(ask(client, harness.transport, {{"req", 3}, {"op", "cash"}, {"account", "paper"}})["ok"] == true);
  assert(ask(client, harness.transport, {{"req", 4}, {"op", "cash"}, {"account", "live"}})["ok"] == false);
  // One failure shape for unknown accounts and wrong passwords.
  assert(ask(client, harness.transport, {{"req", 5}, {"op", "login"}, {"account", "nobody"}, {"password", "x"}})["ok"] == false);
  assert(ask(client, harness.transport, {{"req", 6}, {"op", "login"}, {"account", "live"}, {"password", "wrong"}})["ok"] == false);
  const auto seen = ask(client, harness.transport, {{"req", 7}, {"op", "login"}, {"account", "live"}, {"password", "live-pw"}});
  assert(seen["ok"] == true && seen["data"]["account"] == "live");
  const auto me = ask(client, harness.transport, {{"req", 8}, {"op", "whoami"}});
  assert(me["ok"] == true && me["data"]["account"] == "live" && me["data"]["authed"] == true);
  // Bound sessions default to their own account and refuse strangers.
  const auto mine = ask(client, harness.transport, {{"req", 9}, {"op", "cash"}});
  assert(mine["ok"] == true && mine["data"]["cash"] == 10'000'000);
  assert(ask(client, harness.transport, {{"req", 10}, {"op", "cash"}, {"account", "paper"}})["ok"] == false);
  const nlohmann::json ticket{{"req", 11},     {"op", "submit"},  {"id", "f-1"},     {"account", "paper"}, {"strategy", "test"}, {"instrument", "TEST"},
                              {"side", "buy"}, {"type", "limit"}, {"quantity", 100}, {"price", 900},       {"exchange", "sse"},  {"security", "A1"}};
  assert(ask(client, harness.transport, ticket)["ok"] == false);  // stranger account
  nlohmann::json own = ticket;
  own["req"] = 12;
  own["account"] = "live";
  assert(ask(client, harness.transport, own)["ok"] == true);
  // Id-attributed ops honor ownership too.
  assert(ask(client, harness.transport, {{"req", 13}, {"op", "find_order"}, {"id", "f-1"}})["ok"] == true);
  assert(ask(client, harness.transport, {{"req", 14}, {"op", "cancel"}, {"id", "f-1"}})["ok"] == true);
  const nlohmann::json stranger_start{
      {"req", 15},  {"op", "strategy_start"}, {"id", "t-1"}, {"strategy_type", "grid"}, {"account", "paper"}, {"instrument", "TEST"}, {"quantity", 100}, {"lower", 900}, {"upper", 1100},
      {"grids", 4}, {"start", 1000}};
  assert(ask(client, harness.transport, stranger_start)["ok"] == false);  // stranger account
  nlohmann::json own_start = stranger_start;
  own_start["req"] = 16;
  own_start["id"] = "t-2";
  own_start["account"] = "live";
  assert(ask(client, harness.transport, own_start)["ok"] == true);
  assert(ask(client, harness.transport, {{"req", 17}, {"op", "strategy_stop"}, {"id", "t-2"}})["ok"] == true);
  // Logout is idempotent; the session no longer reads live data.
  assert(ask(client, harness.transport, {{"req", 18}, {"op", "logout"}})["ok"] == true);
  assert(ask(client, harness.transport, {{"req", 19}, {"op", "cash"}, {"account", "live"}})["ok"] == false);
  assert(ask(client, harness.transport, {{"req", 20}, {"op", "cash"}, {"account", "paper"}})["ok"] == true);
}


void order_flow() {
  Harness harness{};
  sts::gateway::GatewayClient client{socket_path()};
  const nlohmann::json ticket{{"req", 10},     {"op", "submit"},  {"id", "f-1"},     {"account", "paper"}, {"strategy", "test"}, {"instrument", "TEST"},
                              {"side", "buy"}, {"type", "limit"}, {"quantity", 100}, {"price", 900},       {"exchange", "sse"},  {"security", "A1"}};
  assert(ask(client, harness.transport, ticket)["ok"] == true);
  assert(ask(client, harness.transport, ticket)["ok"] == false);  // duplicate id
  const auto opens = ask(client, harness.transport, {{"req", 11}, {"op", "open_orders"}});
  assert(opens["ok"] == true && opens["data"]["orders"].size() == 1);
  assert(opens["data"]["orders"][0]["client_order_id"] == "f-1");
  const auto found = ask(client, harness.transport, {{"req", 12}, {"op", "find_order"}, {"id", "f-1"}});
  assert(found["ok"] == true && found["data"]["order"]["status"] == "live");
  assert(ask(client, harness.transport, {{"req", 13}, {"op", "cancel"}, {"id", "f-1"}})["ok"] == true);
  assert(ask(client, harness.transport, {{"req", 14}, {"op", "open_orders"}})["data"]["orders"].empty());
  // Broken tickets and unknown ops refuse with the req echoed.
  nlohmann::json bad_side = ticket;
  bad_side["req"] = 15;
  bad_side["id"] = "f-2";
  bad_side["side"] = "hold";
  const auto refused = ask(client, harness.transport, bad_side);
  assert(refused["ok"] == false && refused["req"] == 15);
  assert(ask(client, harness.transport, {{"req", 16}, {"op", "explode"}})["ok"] == false);
  client.send("{not json");
  harness.transport.poll_once();
  const auto malformed = nlohmann::json::parse(client.recv_line());
  assert(malformed["ok"] == false && malformed["req"] == 0);
  // Settled inventory is visible per strategy basket.
  harness.book.fund_settled("test", "TEST", 500, 950);
  const auto positions = ask(client, harness.transport, {{"req", 17}, {"op", "positions_for"}, {"strategy", "test"}});
  assert(positions["ok"] == true && positions["data"]["positions"][0]["qty"] == 500);
}

void strategy_flow() {
  Harness harness{};
  sts::gateway::GatewayClient client{socket_path()};
  const nlohmann::json start{
      {"req", 20},  {"op", "strategy_start"}, {"id", "t-grid"}, {"strategy_type", "grid"}, {"account", "paper"}, {"instrument", "TEST"}, {"quantity", 100}, {"lower", 900}, {"upper", 1100},
      {"grids", 4}, {"start", 1000}};
  assert(ask(client, harness.transport, start)["ok"] == true);
  const auto listed = ask(client, harness.transport, {{"req", 21}, {"op", "strategy_list"}});
  assert(listed["ok"] == true && listed["data"]["strategies"].get<std::string>().find("t-grid") != std::string::npos);
  assert(ask(client, harness.transport, {{"req", 22}, {"op", "strategy_stop"}, {"id", "nope"}})["ok"] == false);
  assert(ask(client, harness.transport, {{"req", 23}, {"op", "strategy_stop"}, {"id", "t-grid"}})["ok"] == true);
}

void pushes_fan_out() {
  Harness harness{};
  sts::gateway::GatewayClient client{socket_path()};
  const auto quote_sub = ask(client, harness.transport, {{"req", 30}, {"op", "subscribe_quotes"}});
  assert(quote_sub["ok"] == true && quote_sub["data"]["sub"] != 0);
  const std::uint64_t quote_id = quote_sub["data"]["sub"].get<std::uint64_t>();
  sts::QuoteEvent quote{};
  quote.instrument_id = "TEST";
  quote.last_price = 1000;
  harness.front.on_quote(quote);
  harness.transport.poll_once();
  const auto push = nlohmann::json::parse(client.recv_line());
  assert(push["push"] == "quote" && push["quote"]["instrument"] == "TEST" && push["quote"]["last"] == 1000);
  // Reports ride the same push path without a req id.
  const auto report_sub = ask(client, harness.transport, {{"req", 31}, {"op", "subscribe_reports"}});
  assert(report_sub["ok"] == true);
  sts::ExecutionReport report{};
  report.client_order_id = "f-9";
  report.status = sts::OrderStatus::filled;
  harness.front.on_execution_report(report);
  harness.transport.poll_once();
  const auto report_push = nlohmann::json::parse(client.recv_line());
  assert(report_push["push"] == "report" && report_push["report"]["client_order_id"] == "f-9");
  // Unsubscribed peers go quiet; unknown subs refuse.
  assert(ask(client, harness.transport, {{"req", 32}, {"op", "unsubscribe"}, {"sub", quote_id}})["ok"] == true);
  assert(ask(client, harness.transport, {{"req", 33}, {"op", "unsubscribe"}, {"sub", quote_id}})["ok"] == false);
  harness.front.on_quote(quote);
  harness.transport.poll_once();
  bool timed_out = false;
  try {
    (void)client.recv_line(200);
  } catch (const std::runtime_error&) {
    timed_out = true;
  }
  assert(timed_out);
}

void strategy_pushes_fan_out() {
  Harness harness{};
  sts::gateway::GatewayClient client{socket_path()};
  const auto sub = ask(client, harness.transport, {{"req", 34}, {"op", "subscribe_strategies"}});
  assert(sub["ok"] == true && sub["data"]["sub"] != 0);
  const nlohmann::json start{
      {"req", 35},  {"op", "strategy_start"}, {"id", "t-grid"}, {"strategy_type", "grid"}, {"account", "paper"}, {"instrument", "TEST"}, {"quantity", 100}, {"lower", 900}, {"upper", 1100},
      {"grids", 4}, {"start", 1000}};
  client.send(start.dump());
  harness.transport.poll_once();
  const auto started = nlohmann::json::parse(client.recv_line());
  assert(started["push"] == "strategy" && started["strategy"]["strategy"] == "t-grid" && started["strategy"]["event"] == "started");
  const auto started_reply = nlohmann::json::parse(client.recv_line());
  assert(started_reply["ok"] == true && started_reply["req"] == 35);
  client.send(nlohmann::json({{"req", 36}, {"op", "strategy_stop"}, {"id", "t-grid"}}).dump());
  harness.transport.poll_once();
  const auto stopped_push = nlohmann::json::parse(client.recv_line());
  assert(stopped_push["push"] == "strategy" && stopped_push["strategy"]["event"] == "stopped");
  const auto stopped = nlohmann::json::parse(client.recv_line());
  assert(stopped["ok"] == true && stopped["req"] == 36);
}

void disconnect_cleans_up() {
  Harness harness{};
  sts::gateway::GatewayClient client{socket_path()};
  assert(ask(client, harness.transport, {{"req", 40}, {"op", "subscribe_quotes"}})["ok"] == true);
  {
    // A second peer subscribes then vanishes without unsubscribing.
    sts::gateway::GatewayClient leaver{socket_path()};
    assert(ask(leaver, harness.transport, {{"req", 41}, {"op", "subscribe_quotes"}})["ok"] == true);
  }  // leaver closes here
  harness.transport.poll_once();  // reaps the dead peer and its subscription
  sts::QuoteEvent quote{};
  quote.instrument_id = "TEST";
  harness.front.on_quote(quote);
  harness.transport.poll_once();
  // The surviving peer still gets exactly one push (no dup, no crash).
  const auto push = nlohmann::json::parse(client.recv_line());
  assert(push["push"] == "quote");
  bool timed_out = false;
  try {
    (void)client.recv_line(200);
  } catch (const std::runtime_error&) {
    timed_out = true;
  }
  assert(timed_out);
}

void remote_login_replaces_and_strategy_survives() {
  AuthHarness harness{};
  sts::gateway::GatewayClient first{socket_path()};
  sts::gateway::GatewayClient second{socket_path()};
  // First client logs in and starts a strategy on the server.
  assert(ask(first, harness.transport, {{"req", 100}, {"op", "login"}, {"account", "live"}, {"password", "live-pw"}})["ok"] == true);
  const nlohmann::json start{{"req", 101},           {"op", "strategy_start"}, {"id", "t-rep"}, {"strategy_type", "grid"}, {"account", "live"},
                             {"instrument", "TEST"}, {"quantity", 100},        {"lower", 900},  {"upper", 1100},           {"grids", 4},
                             {"start", 1000}};
  assert(ask(first, harness.transport, start)["ok"] == true);
  // A remote login on the same account replaces the first session.
  assert(ask(second, harness.transport, {{"req", 102}, {"op", "login"}, {"account", "live"}, {"password", "live-pw"}})["ok"] == true);
  bool first_kicked = false;
  try {
    (void)ask(first, harness.transport, {{"req", 103}, {"op", "whoami"}});
  } catch (const std::runtime_error&) {
    first_kicked = true;
  }
  assert(first_kicked);
  // The strategy keeps running on the server under the new session.
  const auto listed = ask(second, harness.transport, {{"req", 104}, {"op", "strategy_list"}});
  assert(listed["ok"] == true);
  const auto progress = ask(second, harness.transport, {{"req", 105}, {"op", "strategy_progress"}, {"id", "t-rep"}});
  assert(progress["ok"] == true && progress["data"]["strategy"] == "t-rep");
}

void batch_replies_stay_ordered() {
  Harness harness{};
  sts::gateway::GatewayClient client{socket_path()};
  // Two pipelined requests get two ordered replies from one poll.
  client.send(R"({"req": 50, "op": "ping"})");
  client.send(R"({"req": 51, "op": "ping"})");
  harness.transport.poll_once();
  const auto first = nlohmann::json::parse(client.recv_line());
  const auto second = nlohmann::json::parse(client.recv_line());
  assert(first["req"] == 50 && first["ok"] == true);
  assert(second["req"] == 51 && second["ok"] == true);
}

void poll_tells_quiet_from_gone() {
  Harness harness{};
  sts::gateway::GatewayClient client{socket_path()};
  std::string frame;
  assert(!client.poll_line(frame, 200));  // nobody speaks: quiet, not gone
  client.send(R"({"req": 60, "op": "ping"})");
  harness.transport.poll_once();
  assert(client.poll_line(frame, 2000));
  assert(nlohmann::json::parse(frame)["req"] == 60);
}

void fills_and_progress_over_socket() {
  Harness harness{};
  sts::gateway::GatewayClient client{socket_path()};
  const nlohmann::json ticket{{"req", 80},     {"op", "submit"},  {"id", "f-1"},     {"account", "paper"}, {"strategy", "test"}, {"instrument", "TEST"},
                              {"side", "buy"}, {"type", "limit"}, {"quantity", 100}, {"price", 900},       {"exchange", "sse"},  {"security", "A1"}};
  assert(ask(client, harness.transport, ticket)["ok"] == true);
  nlohmann::json second = ticket;
  second["req"] = 81;
  second["id"] = "f-2";
  assert(ask(client, harness.transport, second)["ok"] == true);
  sts::ExecutionReport filled{"SIM-f-2", "f-2", sts::OrderStatus::filled, 100, 900, "", "e-1", 100, 0};
  assert(harness.oms.on_execution_report(filled));
  const auto fills = ask(client, harness.transport, {{"req", 82}, {"op", "fills"}, {"account", "paper"}, {"strategy", "test"}});
  assert(fills["ok"] == true && fills["data"]["fills"].size() == 1 && fills["data"]["truncated"] == false);
  assert(fills["data"]["fills"][0]["client_order_id"] == "f-2" && fills["data"]["fills"][0]["account"] == "paper");
  assert(fills["data"]["fills"][0]["strategy"] == "test" && fills["data"]["fills"][0]["seq"] == 2);
  const auto progress = ask(client, harness.transport, {{"req", 83}, {"op", "strategy_progress"}, {"id", "test"}});
  assert(progress["ok"] == true && progress["data"]["submitted"] == 2 && progress["data"]["open"] == 1 && progress["data"]["filled"] == 1);
  assert(progress["data"]["running"] == false);
  assert(ask(client, harness.transport, {{"req", 84}, {"op", "strategy_progress"}, {"id", "ghost"}})["ok"] == false);
}

std::uint16_t tcp_test_port(const std::string& endpoint) {
  const std::size_t mark = endpoint.rfind(':');
  assert(mark != std::string::npos);
  return static_cast<std::uint16_t>(std::stoul(endpoint.substr(mark + 1)));
}

void tcp_loopback_and_dual_stack() {
  Harness harness{};
  // Unix-only, TCP-only (ephemeral port) and dual-stack transports share
  // one protocol: same frames, same auth gate.
  sts::gateway::GatewayTransport tcp_only{harness.front, "", "127.0.0.1:0"};
  assert(tcp_only.enabled() && !tcp_only.tcp_endpoint().empty());
  const std::uint16_t port = tcp_test_port(tcp_only.tcp_endpoint());
  assert(port != 0);
  sts::gateway::GatewayClient tcp_client{"127.0.0.1", port};
  assert(ask(tcp_client, tcp_only, {{"req", 90}, {"op", "ping"}})["ok"] == true);
  const auto cash = ask(tcp_client, tcp_only, {{"req", 91}, {"op", "cash"}});
  assert(cash["ok"] == true && cash["data"]["cash"] == 10'000'000);

  sts::gateway::GatewayTransport dual{harness.front, socket_path(), "127.0.0.1:0"};
  assert(dual.enabled() && !dual.tcp_endpoint().empty());
  sts::gateway::GatewayClient unix_client{socket_path()};
  sts::gateway::GatewayClient dual_tcp{"127.0.0.1", tcp_test_port(dual.tcp_endpoint())};
  assert(ask(unix_client, dual, {{"req", 92}, {"op", "ping"}})["ok"] == true);
  assert(ask(dual_tcp, dual, {{"req", 93}, {"op", "ping"}})["ok"] == true);
}

void tcp_bad_bind_fails_closed() {
  Harness harness{};
  for (const std::string bad : {"no-colon", ":19001", "127.0.0.1:", "127.0.0.1:99999", "127.0.0.1:abc"}) {
    bool threw = false;
    try {
      sts::gateway::GatewayTransport doomed{harness.front, "", bad};
    } catch (const std::runtime_error&) {
      threw = true;
    }
    assert(threw);
  }
  // TCP client refuses dead servers loudly.
  bool threw = false;
  try {
    sts::gateway::GatewayClient nowhere{"127.0.0.1", 1};  // nothing listens on port 1
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
}

void settle_report_and_confirm_over_socket() {
  Harness harness{};
  sts::gateway::GatewayClient client{socket_path()};
  // Unwired server refuses both settlement ops loudly.
  assert(ask(client, harness.transport, {{"req", 70}, {"op", "settle_report"}})["ok"] == false);
  // Wire a matching snapshot plus the attestation ledger.
  sts::InMemoryEventLedger attestations{};
  harness.front.set_snapshot_provider([] {
    sts::TradeAccountSnapshot snapshot{};
    snapshot.account_id = "paper";
    snapshot.available_cash = 10'000'000;
    return sts::gateway::SnapshotBatch{{snapshot}, "2026-10-11T00:00:00Z"};
  });
  harness.front.set_attestation_ledger(&attestations);
  const auto report = ask(client, harness.transport, {{"req", 71}, {"op", "settle_report"}});
  assert(report["ok"] == true);
  assert(report["data"]["accounts"].size() == 1 && report["data"]["accounts"][0]["clean"] == true);
  assert(report["data"]["snapshot_time"] == "2026-10-11T00:00:00Z");
  const std::string snapshot_time = report["data"]["snapshot_time"].get<std::string>();
  const auto watermark = report["data"]["ledger_watermark"].get<std::uint64_t>();
  const auto confirmed =
      ask(client, harness.transport, {{"req", 72}, {"op", "settle_confirm"}, {"snapshot_time", snapshot_time}, {"watermark", watermark}, {"cash_dirty", 0}, {"position_dirty", 0}, {"note", "close"}});
  assert(confirmed["ok"] == true);
  assert(attestations.latest_event_id() == 1);
  // Refusals stay loud: unknown op shape, future watermark, missing time.
  assert(ask(client, harness.transport, {{"req", 73}, {"op", "settle_confirm"}, {"snapshot_time", snapshot_time}, {"watermark", 99}})["ok"] == false);
}

}  // namespace

int main() {
  basic_ops_need_no_login();
  account_login_and_bound_account();
  order_flow();
  strategy_flow();
  fills_and_progress_over_socket();
  tcp_loopback_and_dual_stack();
  tcp_bad_bind_fails_closed();
  pushes_fan_out();
  strategy_pushes_fan_out();
  settle_report_and_confirm_over_socket();
  disconnect_cleans_up();
  remote_login_replaces_and_strategy_survives();
  batch_replies_stay_ordered();
  poll_tells_quiet_from_gone();
  std::error_code ignored;
  std::filesystem::remove(socket_path(), ignored);
  return 0;
}
