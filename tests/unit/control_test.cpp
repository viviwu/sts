// clang-format off
/**
  ******************************************************************************
  * @file           : control_test.cpp
  * @author         : vivi wu
  * @brief          : Operator control plane tests: parse, fan-out, halt, replace, manual orders, strategy registry.
  * @version        : 0.1.0
  * @date           : 03/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/ops/control.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "sts/broker/simulated_broker.hpp"
#include "sts/execution/execution_api.hpp"
#include "sts/ops/admin_socket.hpp"
#include "sts/persistence/ledger.hpp"
#include "sts/research/ledger_verify.hpp"
#include "sts/strategy/grid_strategy.hpp"

namespace {

sts::OrderIntent make_parent(const std::string& id, sts::Quantity quantity, sts::Price limit) {
  sts::OrderIntent intent{id, "paper", "test", "TEST", sts::Side::buy, sts::OrderType::limit, sts::TimeInForce::day, quantity, limit};
  intent.trace_id = id;
  return intent;
}

struct Harness {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  sts::RiskEngine risk{{1'000'000, 0, true}};
  sts::InMemoryEventLedger ledger{};
  sts::OrderManager oms{broker, risk, events, &ledger};
  sts::TwapAlgorithm execution{sts::TwapConfig{2, std::chrono::seconds{1}}};
  sts::control::ControlContext context{oms, execution, risk};

  Harness() {
    broker.set_starting_cash(10'000'000);
    broker.set_reference_price("TEST", 1'000);
  }

  int submit_due(sts::Timestamp now) {
    int count = 0;
    for (const auto& child : execution.poll(now)) {
      assert(oms.submit(child));
      ++count;
    }
    return count;
  }
};

void parse_format_roundtrip() {
  for (const char* name : {"cancel", "cancel_all", "block_strategy", "unblock_strategy", "block_instrument", "unblock_instrument", "block_account", "unblock_account", "halt", "resume", "replace"}) {
    sts::control::ControlCommand sent{};
    sent.name = name;
    sent.id = "x-1";
    sent.quantity = 100;
    sent.price = 50;
    sts::control::ControlCommand got{};
    std::string error;
    assert(sts::control::parse_command(sts::control::format_command(sent), got, error));
    assert(got.name == sent.name && got.id == sent.id && got.quantity == 100 && got.price == 50);
  }
  sts::control::ControlCommand bare{};
  bare.name = "halt";
  sts::control::ControlCommand got{};
  std::string error;
  assert(sts::control::parse_command(sts::control::format_command(bare), got, error));
  assert(got.name == "halt" && got.id.empty());
}

void parse_rejects_loudly() {
  sts::control::ControlCommand command{};
  std::string error;
  assert(!sts::control::parse_command("not json", command, error) && !error.empty());
  assert(!sts::control::parse_command(R"({"cmd":"explode"})", command, error));
  assert(!sts::control::parse_command(R"({"cmd":"cancel"})", command, error));
  assert(!sts::control::parse_command(R"({"cmd":"block-strategy","id":"x"})", command, error));
  assert(!sts::control::parse_command(R"({"cmd":"replace","id":"x"})", command, error));
  assert(!sts::control::parse_command(R"({"cmd":"replace","id":"x","quantity":0})", command, error));
  // Manual tickets fail closed on every missing/inconsistent field.
  const char* good = R"({"cmd":"order","id":"m-1","account":"paper","instrument":"TEST","side":"buy","type":"limit","quantity":100,"price":900,"exchange":"sse","security":"A1"})";
  assert(sts::control::parse_command(good, command, error));
  assert(!sts::control::parse_command(R"({"cmd":"order","id":"m-1"})", command, error));
  assert(!sts::control::parse_command(R"({"cmd":"order","id":"m-1","account":"paper","instrument":"TEST","side":"hold","type":"limit","quantity":100,"price":900,"exchange":"sse","security":"A1"})",
                                      command, error));
  assert(!sts::control::parse_command(R"({"cmd":"order","id":"m-1","account":"paper","instrument":"TEST","side":"buy","type":"stop","quantity":100,"price":900,"exchange":"sse","security":"A1"})",
                                      command, error));
  assert(!sts::control::parse_command(R"({"cmd":"order","id":"m-1","account":"paper","instrument":"TEST","side":"buy","type":"limit","quantity":0,"price":900,"exchange":"sse","security":"A1"})",
                                      command, error));
  assert(!sts::control::parse_command(R"({"cmd":"order","id":"m-1","account":"paper","instrument":"TEST","side":"buy","type":"limit","quantity":100,"price":0,"exchange":"sse","security":"A1"})",
                                      command, error));
  assert(!sts::control::parse_command(R"({"cmd":"order","id":"m-1","account":"paper","instrument":"TEST","side":"buy","type":"limit","quantity":100,"price":900,"exchange":"nyse","security":"A1"})",
                                      command, error));
  assert(!sts::control::parse_command(
      R"({"cmd":"order","id":"m-1","account":"paper","instrument":"TEST","side":"buy","type":"limit","quantity":100,"price":900,"exchange":"sse","security":"A1","style":"own-best"})", command,
      error));
  assert(
      !sts::control::parse_command(R"({"cmd":"order","id":"m-1","account":"paper","instrument":"TEST","side":"buy","type":"market","quantity":100,"exchange":"sse","security":"A1"})", command, error));
  assert(!sts::control::parse_command(
      R"({"cmd":"order","id":"m-1","account":"paper","instrument":"TEST","side":"buy","type":"market","quantity":100,"price":-1,"exchange":"sse","security":"A1","style":"own-best"})", command,
      error));
}

void manual_order_roundtrip_and_dispatch() {
  // Full ticket survives the inbox encoding.
  sts::control::ControlCommand sent{};
  sent.name = "order";
  sent.id = "m-1";
  sent.account = "paper";
  sent.instrument = "TEST";
  sent.side = "buy";
  sent.order_type = "limit";
  sent.quantity = 100;
  sent.price = 900;
  sent.exchange = "sse";
  sent.security_account = "A1";
  sts::control::ControlCommand got{};
  std::string error;
  assert(sts::control::parse_command(sts::control::format_command(sent), got, error));
  assert(got.name == "order" && got.id == "m-1" && got.account == "paper" && got.instrument == "TEST");
  assert(got.side == "buy" && got.order_type == "limit" && got.quantity == 100 && got.price == 900);
  assert(got.exchange == "sse" && got.security_account == "A1" && got.style.empty());

  // Accepted limit buy rests live (reference 1000, limit 900 never fills).
  Harness harness{};
  const auto accepted = sts::control::apply_command(got, harness.context);
  assert(accepted.ok);
  const sts::OrderIntent* stored = harness.oms.find_intent("m-1");
  assert(stored != nullptr && stored->quantity == 100 && stored->limit_price == 900);
  assert(stored->strategy_id == sts::kPassthroughStrategy && stored->trace_id == "manual-m-1");
  assert(harness.oms.find("m-1")->status == sts::OrderStatus::live);

  // Duplicate ids are refused, never double-submitted.
  assert(!sts::control::apply_command(got, harness.context).ok);

  // Market ticket carries style + protection price instead of a limit.
  sts::control::ControlCommand market{};
  market.name = "order";
  market.id = "m-2";
  market.account = "paper";
  market.instrument = "TEST";
  market.side = "sell";
  market.order_type = "market";
  market.quantity = 100;
  market.price = 950;
  market.exchange = "sse";
  market.security_account = "A1";
  market.style = "own-best";
  assert(sts::control::parse_command(sts::control::format_command(market), market, error));
  assert(sts::control::apply_command(market, harness.context).ok);
  const sts::OrderIntent* mstored = harness.oms.find_intent("m-2");
  assert(mstored != nullptr && mstored->type == sts::OrderType::market);
  assert(mstored->market_order_style == sts::MarketOrderStyle::own_best && mstored->protection_price == 950);

  // Risk refusals stay loud: a blocked instrument never reaches OMS.
  sts::control::ControlCommand blocked{};
  blocked.name = "block_instrument";
  blocked.id = "TEST";
  assert(sts::control::apply_command(blocked, harness.context).ok);
  sts::control::ControlCommand refused = sent;
  refused.id = "m-3";
  assert(!sts::control::apply_command(refused, harness.context).ok);
  // Refusals are recorded, not silent: a rejected snapshot with no live order.
  assert(harness.oms.find("m-3")->status == sts::OrderStatus::rejected);
}

void cancel_child_and_parent_fanout() {
  Harness harness{};
  // Resting TWAP parent: two live slices under one parent id.
  harness.execution.add_parent(make_parent("tw-1", 200, 800));
  assert(harness.submit_due(sts::Timestamp{}) == 1);
  assert(harness.oms.open_parent_ids().size() == 1);

  // Direct child cancel withdraws exactly that child.
  sts::control::ControlCommand cancel_child{};
  cancel_child.name = "cancel";
  cancel_child.id = "tw-1#0";
  const auto single = sts::control::apply_command(cancel_child, harness.context);
  assert(single.ok);
  assert(harness.oms.find("tw-1#0")->status == sts::OrderStatus::pending_cancel || harness.oms.find("tw-1#0")->status == sts::OrderStatus::cancelled);

  // Parent cancel fans out to the remaining live slice.
  sts::control::ControlCommand cancel_parent{};
  cancel_parent.name = "cancel";
  cancel_parent.id = "tw-1";
  const auto fanout = sts::control::apply_command(cancel_parent, harness.context);
  assert(fanout.ok);
  assert(harness.oms.open_orders().empty());

  // Unknown ids never throw, they report refusal.
  sts::control::ControlCommand ghost{};
  ghost.name = "cancel";
  ghost.id = "nope";
  assert(!sts::control::apply_command(ghost, harness.context).ok);
}

void cancel_all_and_switches() {
  Harness harness{};
  harness.execution.add_parent(make_parent("tw-2", 200, 800));
  harness.execution.add_parent(make_parent("tw-3", 200, 800));
  harness.submit_due(sts::Timestamp{});
  assert(harness.oms.open_orders().size() == 2);

  sts::control::ControlCommand all{};
  all.name = "cancel_all";
  const auto done = sts::control::apply_command(all, harness.context);
  assert(done.ok);
  assert(harness.oms.open_orders().empty());

  sts::control::ControlCommand block{};
  block.name = "block_strategy";
  block.id = "test";
  assert(sts::control::apply_command(block, harness.context).ok);
  assert(!harness.risk.check(make_parent("probe", 10, 100)).allowed);
  sts::control::ControlCommand unblock{};
  unblock.name = "unblock_strategy";
  unblock.id = "test";
  assert(sts::control::apply_command(unblock, harness.context).ok);
  assert(harness.risk.check(make_parent("probe", 10, 100)).allowed);

  sts::control::ControlCommand halt{};
  halt.name = "halt";
  assert(sts::control::apply_command(halt, harness.context).ok);
  assert(!harness.risk.check(make_parent("probe", 10, 100)).allowed);
  sts::control::ControlCommand resume{};
  resume.name = "resume";
  assert(sts::control::apply_command(resume, harness.context).ok);
  assert(harness.risk.check(make_parent("probe", 10, 100)).allowed);
}

void cancel_all_sweeps_in_sorted_order() {
  Harness harness{};
  // Live resting buys (limit 800 under the 1000 reference never fills).
  assert(harness.oms.submit(make_parent("zz-1", 200, 800)));
  assert(harness.oms.submit(make_parent("mm-1", 200, 800)));
  assert(harness.oms.submit(make_parent("aa-1", 200, 800)));
  sts::control::ControlCommand all{};
  all.name = "cancel_all";
  assert(sts::control::apply_command(all, harness.context).ok);
  // Deterministic sweep: ledger cancel order must not depend on hash order.
  std::vector<std::string> cancelled;
  for (const auto& event : harness.ledger.replay(1)) {
    if (event.type == sts::LedgerEventType::order_cancel_requested) cancelled.push_back(event.snapshot.client_order_id);
  }
  assert(cancelled.size() == 3);
  assert(cancelled[0] == "aa-1" && cancelled[1] == "mm-1" && cancelled[2] == "zz-1");
}

int dial(const std::string& socket_path) {
  const int sock = ::socket(AF_UNIX, SOCK_STREAM, 0);
  assert(sock >= 0);
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  assert(socket_path.size() < sizeof(address.sun_path));
  std::strncpy(address.sun_path, socket_path.c_str(), sizeof(address.sun_path) - 1);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): sockaddr_un downcast is the POSIX API shape.
  assert(::connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
  return sock;
}

std::string recv_exact(int sock, std::size_t count) {
  std::string out;
  char chunk[256]{0};
  while (out.size() < count) {
    const ssize_t done = ::recv(sock, chunk, std::min(sizeof(chunk) - 1, count - out.size()), 0);
    if (done <= 0) break;
    out.append(chunk, static_cast<std::size_t>(done));
  }
  return out;
}

void await_eof(int sock) {
  timeval window{};
  window.tv_sec = 2;
  ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &window, sizeof(window));
  char byte = 0;
  assert(::recv(sock, &byte, 1, 0) == 0);  // server closes after each reply
  ::close(sock);
}

void query_socket_serves_live_state() {
  Harness harness{};
  assert(harness.oms.submit(make_parent("q-1", 200, 800)));  // resting live order
  const std::string path = (std::filesystem::temp_directory_path() / "sts-query-test.sock").string();
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
  sts::AdminSocket socket{path};
  assert(socket.enabled());
  // No peer yet: polling is a silent no-op.
  socket.poll_once([] { return std::string{"{}"}; });
  // Build the reply from the live book, like the gateway does.
  auto build = [&] {
    std::string ids;
    for (const auto& order : harness.oms.open_orders()) ids += order.client_order_id + ",";
    return std::string{"{\"open\":["} + ids + "]}";
  };
  const int asker = dial(path);
  assert(::send(asker, "state\n", 6, 0) == 6);
  socket.poll_once(build);  // accepts + answers
  socket.poll_once(build);  // drained already: still silent
  assert(recv_exact(asker, 512).find("q-1") != std::string::npos);
  await_eof(asker);
  // Unknown commands are refused loudly.
  const int stranger = dial(path);
  assert(::send(stranger, "halt\n", 5, 0) == 5);
  socket.poll_once(build);
  assert(recv_exact(stranger, 20) == "ERR unknown command\n");
  await_eof(stranger);
  // The client helper surfaces connection failures instead of hanging.
  bool threw = false;
  try {
    (void)sts::query_live_state("/nonexistent-dir/sts-nope.sock");
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
  // Disabled socket: construction without effect, polling silent.
  sts::AdminSocket off{""};
  assert(!off.enabled());
  off.poll_once(build);
  std::filesystem::remove(path, ignored);
}

void replace_replans_through_live_child() {
  Harness harness{};
  harness.execution.add_parent(make_parent("tw-4", 200, 800));
  assert(harness.submit_due(sts::Timestamp{}) == 1);

  sts::control::ControlCommand replace{};
  replace.name = "replace";
  replace.id = "tw-4#0";  // any live child addresses its parent
  replace.quantity = 400;
  const auto result = sts::control::apply_command(replace, harness.context);
  assert(result.ok);
  // Recalled slice withdrawn; the next generation re-emits on poll.
  assert(harness.oms.find("tw-4#0")->status != sts::OrderStatus::live);
  harness.submit_due(sts::Timestamp{});
  const sts::OrderIntent* replanned = harness.oms.find_intent("tw-4#r1#0");
  assert(replanned != nullptr && replanned->quantity == 200);

  // Parentless direct orders are not algorithm children: refused, loudly.
  sts::OrderIntent direct{"direct-1", "paper", "test", "TEST", sts::Side::buy, sts::OrderType::limit, sts::TimeInForce::day, 10, 100};
  direct.trace_id = "direct-1";
  assert(harness.oms.submit(direct));
  sts::control::ControlCommand direct_replace{};
  direct_replace.name = "replace";
  direct_replace.id = "direct-1";
  direct_replace.quantity = 20;
  assert(!sts::control::apply_command(direct_replace, harness.context).ok);
  assert(harness.oms.find_intent("missing") == nullptr);
}

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

void strategy_verbs_roundtrip() {
  sts::control::ControlCommand sent{};
  sent.name = "strategy_start";
  sent.id = "t-grid";
  sent.account = "paper";
  sent.instrument = "TEST";
  sent.strategy_type = "grid";
  sent.quantity = 100;
  sent.lower = 900;
  sent.upper = 1100;
  sent.grids = 4;
  sent.start = 1000;
  sts::control::ControlCommand got{};
  std::string error;
  assert(sts::control::parse_command(sts::control::format_command(sent), got, error));
  assert(got.name == "strategy_start" && got.id == "t-grid" && got.strategy_type == "grid");
  assert(got.account == "paper" && got.instrument == "TEST" && got.quantity == 100);
  assert(got.lower == 900 && got.upper == 1100 && got.grids == 4 && got.start == 1000);
  for (const char* name : {"strategy_stop", "strategy_list"}) {
    sts::control::ControlCommand bare{};
    bare.name = name;
    bare.id = "t-grid";
    assert(sts::control::parse_command(sts::control::format_command(bare), got, error));
    assert(got.name == name);
  }
}

void strategy_ticket_rejects_loudly() {
  sts::control::ControlCommand command{};
  std::string error;
  const char* good = R"({"cmd":"strategy_start","id":"t-1","account":"paper","instrument":"TEST","strategy_type":"grid","quantity":100,"lower":900,"upper":1100,"grids":4,"start":1000})";
  assert(sts::control::parse_command(good, command, error));
  assert(!sts::control::parse_command(R"({"cmd":"strategy_start","id":"t-1"})", command, error));
  assert(!sts::control::parse_command(R"({"cmd":"strategy_start","id":"t-1","account":"paper","instrument":"TEST","strategy_type":" iceberg ","quantity":100,"lower":900,"upper":1100,"grids":4})",
                                      command, error));
  assert(!sts::control::parse_command(R"({"cmd":"strategy_start","id":"t-1","account":"paper","instrument":"TEST","strategy_type":"grid","quantity":0,"lower":900,"upper":1100,"grids":4})", command,
                                      error));
  assert(!sts::control::parse_command(R"({"cmd":"strategy_start","id":"t-1","account":"paper","instrument":"TEST","strategy_type":"grid","quantity":100,"lower":1100,"upper":900,"grids":4})", command,
                                      error));
  assert(!sts::control::parse_command(R"({"cmd":"strategy_start","id":"t-1","account":"paper","instrument":"TEST","strategy_type":"grid","quantity":100,"lower":900,"upper":1100,"grids":0})", command,
                                      error));
  assert(!sts::control::parse_command(R"({"cmd":"strategy_stop"})", command, error));
}

void registry_start_stop_flow() {
  Harness harness{};
  // Strategy verbs need a live registry; the bare context refuses loudly.
  sts::control::ControlCommand start{};
  start.name = "strategy_start";
  start.id = "t-grid";
  start.account = "paper";
  start.instrument = "TEST";
  start.strategy_type = "grid";
  start.quantity = 100;
  start.lower = 900;
  start.upper = 1100;
  start.grids = 4;
  start.start = 1000;
  assert(!sts::control::apply_command(start, harness.context).ok);
  std::vector<std::unique_ptr<sts::Strategy>> live;
  sts::control::StrategyRegistry registry{live, make_test_grid, "test-seed"};
  harness.context.strategies = &registry;
  assert(sts::control::apply_command(start, harness.context).ok);
  assert(live.size() == 1 && registry.active_ids().size() == 1);
  // Duplicates refuse; unknown stops refuse; list names the instance.
  assert(!sts::control::apply_command(start, harness.context).ok);
  sts::control::ControlCommand stop_unknown{};
  stop_unknown.name = "strategy_stop";
  stop_unknown.id = "nope";
  assert(!sts::control::apply_command(stop_unknown, harness.context).ok);
  sts::control::ControlCommand list{};
  list.name = "strategy_list";
  const auto listed = sts::control::apply_command(list, harness.context);
  assert(listed.ok && listed.message.find("t-grid grid paper TEST") != std::string::npos);
  // One open parent under the strategy prefix is withdrawn on stop.
  sts::OrderIntent parent = make_parent("t-grid-r1-test-seed-0", 100, 900);
  parent.parent_client_order_id = "";
  harness.execution.add_parent(parent);
  assert(harness.submit_due(sts::Timestamp{}) == 1);
  sts::control::ControlCommand stop{};
  stop.name = "strategy_stop";
  stop.id = "t-grid";
  const auto stopped = sts::control::apply_command(stop, harness.context);
  assert(stopped.ok && live.empty() && registry.active_ids().empty());
  // Restart mints a fresh generation: no id collision with the withdrawn run.
  assert(sts::control::apply_command(start, harness.context).ok);
  assert(live.size() == 1);
}

}  // namespace

void auto_actions_stop_strategy_and_withdraw() {
  Harness harness{};
  std::vector<std::unique_ptr<sts::Strategy>> live;
  sts::control::StrategyRegistry registry{live, make_test_grid, "test-seed"};
  harness.context.strategies = &registry;
  sts::control::ControlCommand start{};
  start.name = "strategy_start";
  start.id = "t-grid";
  start.account = "paper";
  start.instrument = "TEST";
  start.strategy_type = "grid";
  start.quantity = 100;
  start.lower = 900;
  start.upper = 1100;
  start.grids = 4;
  start.start = 1000;
  assert(sts::control::apply_command(start, harness.context).ok);
  // One live TWAP child under the strategy prefix (limit under the 1000
  // reference never fills, so it rests working).
  sts::OrderIntent parent = make_parent("t-grid-r1-test-seed-0", 200, 800);
  parent.strategy_id = "t-grid";
  harness.execution.add_parent(parent);
  assert(harness.submit_due(sts::Timestamp{}) == 1);
  assert(harness.oms.open_orders().size() == 1);
  // Tighten quantity so fresh submits on the same strategy refuse loudly.
  sts::RiskLimits tight{1, 0, true};
  tight.max_consecutive_rejects = 2;
  harness.risk.set_limits(tight);
  sts::OrderIntent probe = make_parent("probe-1", 200, 800);
  probe.strategy_id = "t-grid";
  assert(!harness.oms.submit(probe));
  assert(sts::control::apply_auto_actions(harness.context).empty());  // streak 1: silent
  probe.client_order_id = "probe-2";
  assert(!harness.oms.submit(probe));  // streak 2: trips
  const std::string summary = sts::control::apply_auto_actions(harness.context);
  assert(!summary.empty() && summary.find("t-grid") != std::string::npos);
  assert(harness.oms.open_orders().empty());  // live child withdrawn with the stop
  assert(registry.active_ids().empty());
  sts::OrderIntent late = make_parent("late", 1, 800);  // passes the tight quantity: only the block may refuse
  late.strategy_id = "t-grid";
  assert(!harness.risk.check(late).allowed);                          // block persists
  assert(sts::control::apply_auto_actions(harness.context).empty());  // drained stays drained
}

void auto_actions_halt_global_withdraws_all() {
  Harness harness{};
  sts::RiskLimits limits{1'000'000, 0, true};
  limits.max_reject_rate_bps = 5000;  // 50%
  limits.reject_rate_min_orders = 4;
  harness.risk.set_limits(limits);
  assert(harness.oms.submit(make_parent("live-1", 200, 800)));
  assert(harness.oms.submit(make_parent("live-2", 200, 800)));
  assert(sts::control::apply_auto_actions(harness.context).empty());
  // Block the venue so fresh submits refuse; distinct strategies keep every
  // per-strategy streak at 1 while the day rate climbs to 2/4 = 50%.
  harness.risk.set_instrument_blocked("TEST", true);
  sts::OrderIntent first = make_parent("r-1", 200, 800);
  first.strategy_id = "s1";
  assert(!harness.oms.submit(first));
  sts::OrderIntent second = make_parent("r-2", 200, 800);
  second.strategy_id = "s2";
  assert(!harness.oms.submit(second));
  const std::string summary = sts::control::apply_auto_actions(harness.context);
  assert(!summary.empty() && summary.find("reject rate") != std::string::npos);
  assert(harness.oms.open_orders().empty());
  harness.risk.set_instrument_blocked("TEST", false);                 // lift the probe block: only the halt may refuse now
  assert(!harness.risk.check(make_parent("late", 10, 100)).allowed);  // globally halted
}

void switch_commands_journal_to_ledger() {
  Harness harness{};
  sts::InMemoryEventLedger ledger{};
  harness.context.ledger = &ledger;
  const char* verbs[] = {"block_strategy", "unblock_strategy", "block_instrument", "unblock_instrument", "block_account", "unblock_account", "halt", "resume"};
  for (const char* verb : verbs) {
    sts::control::ControlCommand command{};
    command.name = verb;
    command.id = "target-1";
    assert(sts::control::apply_command(command, harness.context).ok);
  }
  const auto events = ledger.replay(1);
  assert(events.size() == 8);
  for (std::size_t i = 0; i < 8; ++i) {
    assert(events[i].type == sts::LedgerEventType::control_applied);
    assert(events[i].control.source == "inbox");
    assert(!events[i].control.command.empty());
  }
  assert(events[0].control.command == "block_strategy" && events[0].control.target == "target-1" && events[0].control.blocked_after);
  assert(events[1].control.command == "unblock_strategy" && !events[1].control.blocked_after);
  assert(events[6].control.command == "halt" && events[6].control.target.empty() && events[6].control.blocked_after);
  assert(events[7].control.command == "resume" && !events[7].control.blocked_after);
  // Journaled switches replay cleanly (audit-only, books untouched).
  assert(sts::replay::verify_ledger_replay(events).ok);
  // Ledger-less contexts stay silent: every pre-existing switch test above
  // covers exactly that path and asserts nothing about a ledger.
}

int main() {
  parse_format_roundtrip();
  parse_rejects_loudly();
  manual_order_roundtrip_and_dispatch();
  cancel_child_and_parent_fanout();
  cancel_all_and_switches();
  cancel_all_sweeps_in_sorted_order();
  query_socket_serves_live_state();
  replace_replans_through_live_child();
  strategy_verbs_roundtrip();
  strategy_ticket_rejects_loudly();
  registry_start_stop_flow();
  auto_actions_stop_strategy_and_withdraw();
  auto_actions_halt_global_withdraws_all();
  switch_commands_journal_to_ledger();
  return 0;
}
