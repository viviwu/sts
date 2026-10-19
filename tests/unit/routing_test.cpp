// clang-format off
/**
  ******************************************************************************
  * @file           : routing_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for channel selection, failover and split synthesis.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/broker/routing.hpp"

#include <cassert>
#include <chrono>
#include <string>
#include <vector>

#include "qts/core/clock.hpp"

namespace {

qts::OrderIntent make_intent(const std::string& id, qts::Quantity qty, qts::Price price) {
  return qts::OrderIntent{id, "paper", "test", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, qty, price};
}

class StubChannel : public qts::TradeApi {
 public:
  explicit StubChannel(std::string name)
      : name_(std::move(name)) {}

  std::string_view channel() const noexcept override { return name_; }
  bool connected() const noexcept override { return connected_; }
  void set_connected(bool connected) { connected_ = connected; }

  void submit(const qts::OrderIntent& intent) override { submitted.push_back(intent); }
  void cancel(const qts::ClientOrderId& id) override { cancelled.push_back(id); }
  void set_execution_handler(ExecutionHandler handler) override { execution_handler_ = std::move(handler); }
  void request_account_snapshot(const qts::AccountId& id) override { snapshots_requested.push_back(id); }
  void request_open_orders(const qts::AccountId& id) override { recovery_requested.push_back(id); }
  void set_account_snapshot_handler(AccountSnapshotHandler handler) override { snapshot_handler_ = handler; }
  void set_recovery_complete_handler(RecoveryCompleteHandler handler) override { recovery_handler_ = handler; }
  void request_security_info(const qts::AccountId&, const qts::InstrumentId&) override {}
  void set_security_info_handler(SecurityInfoHandler) override {}
  void request_asset_snapshot(const qts::AccountId&) override {}
  void set_asset_snapshot_handler(AssetSnapshotHandler) override {}

  void emit_report(const qts::ExecutionReport& report) {
    if (execution_handler_) execution_handler_(report);
  }
  void complete_recovery(const qts::AccountId& id, bool ok) {
    if (recovery_handler_) recovery_handler_(id, ok);
  }

  std::vector<qts::OrderIntent> submitted;
  std::vector<std::string> cancelled;
  std::vector<std::string> snapshots_requested;
  std::vector<std::string> recovery_requested;

 private:
  std::string name_;
  bool connected_{true};
  ExecutionHandler execution_handler_;
  AccountSnapshotHandler snapshot_handler_;
  RecoveryCompleteHandler recovery_handler_;
};

qts::RoutingPolicy::Candidate make_candidate(StubChannel& channel, const std::string& name, std::int64_t fee_bps, std::int64_t fill_bps, std::int64_t latency_us, std::int64_t split_weight = 0) {
  return qts::RoutingPolicy::Candidate{name, &channel, qts::ChannelStats{name, fee_bps, fill_bps, latency_us, split_weight}};
}

void fee_selects_cheaper() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500), make_candidate(alt, "alt", 50, 10000, 500)}, qts::RoutingPolicy{}};
  std::vector<qts::ExecutionReport> reports;
  router.set_execution_handler([&reports](const qts::ExecutionReport& r) { reports.push_back(r); });
  router.submit(make_intent("o1", 100, 1'000'0000));
  assert(sim.submitted.size() == 1 && alt.submitted.empty());
}

void latency_weight_flips_selection() {
  StubChannel sim{"sim"}, alt{"alt"};
  // sim is cheaper with near-equal latency: default weights prefer the fee gap.
  qts::Router plain{{make_candidate(sim, "sim", 25, 10000, 500), make_candidate(alt, "alt", 50, 10000, 400)}, qts::RoutingPolicy{}};
  plain.submit(make_intent("o1", 100, 1'000'0000));
  assert(sim.submitted.size() == 1);
  // Cranking the latency weight flips to the fast channel.
  qts::RoutingWeights weights{};
  weights.weight_latency = 1'000'000;
  qts::Router lat{{make_candidate(sim, "sim", 25, 10000, 500), make_candidate(alt, "alt", 50, 10000, 400)}, qts::RoutingPolicy{weights}};
  lat.submit(make_intent("o2", 100, 1'000'0000));
  assert(alt.submitted.size() == 1 && sim.submitted.size() == 1);
}

void fill_weight_flips_selection() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::RoutingWeights weights{};
  weights.weight_fee = 0;
  weights.weight_latency = 0;
  weights.weight_fill = 1;
  qts::Router router{{make_candidate(sim, "sim", 25, 9000, 500), make_candidate(alt, "alt", 50, 10000, 500)}, qts::RoutingPolicy{weights}};
  router.submit(make_intent("o1", 100, 1'000'0000));
  assert(alt.submitted.size() == 1 && sim.submitted.empty());
}

void disconnected_channel_skipped() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500), make_candidate(alt, "alt", 50, 10000, 500)}, qts::RoutingPolicy{}};
  assert(router.connected());
  sim.set_connected(false);
  assert(router.connected());  // alt still up
  router.submit(make_intent("o1", 100, 1'000'0000));
  assert(alt.submitted.size() == 1 && sim.submitted.empty());
  alt.set_connected(false);
  assert(!router.connected());
  sim.set_connected(true);
  router.submit(make_intent("o2", 100, 1'000'0000));
  assert(sim.submitted.size() == 1);
}

void no_channel_synthesizes_rejected() {
  StubChannel sim{"sim"}, alt{"alt"};
  sim.set_connected(false);
  alt.set_connected(false);
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500), make_candidate(alt, "alt", 50, 10000, 500)}, qts::RoutingPolicy{}};
  std::vector<qts::ExecutionReport> reports;
  router.set_execution_handler([&reports](const qts::ExecutionReport& r) { reports.push_back(r); });
  router.submit(make_intent("o1", 100, 1'000'0000));
  assert(sim.submitted.empty() && alt.submitted.empty());
  assert(reports.size() == 1);
  assert(reports[0].status == qts::OrderStatus::rejected);
  assert(reports[0].client_order_id == "o1" && !reports[0].reason.empty());
}

void execution_ids_namespaced_per_channel() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500), make_candidate(alt, "alt", 50, 10000, 500)}, qts::RoutingPolicy{}};
  std::vector<qts::ExecutionReport> reports;
  router.set_execution_handler([&reports](const qts::ExecutionReport& r) { reports.push_back(r); });
  qts::ExecutionReport raw{};
  raw.execution_id = "FILL-7";
  sim.emit_report(raw);
  alt.emit_report(raw);
  assert(reports.size() == 2);
  assert(reports[0].execution_id == "sim:FILL-7");
  assert(reports[1].execution_id == "alt:FILL-7");  // no cross-channel collapse
  qts::ExecutionReport empty{};
  sim.emit_report(empty);
  assert(reports[2].execution_id.empty());  // empty ids pass through untouched
}

void cancel_routed_to_origin_channel() {
  StubChannel sim{"sim"}, alt{"alt"};
  alt.set_connected(false);  // force first order onto sim? No: sim is cheaper anyway.
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500), make_candidate(alt, "alt", 50, 10000, 500)}, qts::RoutingPolicy{}};
  alt.set_connected(true);
  router.submit(make_intent("o1", 100, 1'000'0000));
  assert(sim.submitted.size() == 1);
  sim.set_connected(false);  // origin down: cancel must not leap to alt
  router.cancel("o1");
  assert(sim.cancelled.empty() && alt.cancelled.empty());
  sim.set_connected(true);
  router.cancel("o1");
  assert(sim.cancelled.size() == 1 && alt.cancelled.empty());
  router.cancel("unknown");  // unknown id: silent no-op, no crash
  assert(sim.cancelled.size() == 1 && alt.cancelled.empty());
}

void recovery_aggregated_across_channels() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500), make_candidate(alt, "alt", 50, 10000, 500)}, qts::RoutingPolicy{}};
  std::vector<std::pair<std::string, bool>> completions;
  router.set_recovery_complete_handler([&completions](const qts::AccountId& id, bool ok) { completions.emplace_back(id, ok); });
  router.request_open_orders("paper");
  assert(sim.recovery_requested.size() == 1 && alt.recovery_requested.size() == 1);
  sim.complete_recovery("paper", true);
  assert(completions.empty());  // waits for every fanned-out channel
  alt.complete_recovery("paper", true);
  assert(completions.size() == 1 && completions[0].first == "paper" && completions[0].second);
}

void recovery_failure_propagates() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500), make_candidate(alt, "alt", 50, 10000, 500)}, qts::RoutingPolicy{}};
  std::vector<bool> results;
  router.set_recovery_complete_handler([&results](const qts::AccountId&, bool ok) { results.push_back(ok); });
  router.request_open_orders("paper");
  sim.complete_recovery("paper", true);
  alt.complete_recovery("paper", false);
  assert(results.size() == 1 && !results[0]);  // AND semantics
}

void recovery_with_no_channel_fails_fast() {
  StubChannel sim{"sim"};
  sim.set_connected(false);
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500)}, qts::RoutingPolicy{}};
  std::vector<bool> results;
  router.set_recovery_complete_handler([&results](const qts::AccountId&, bool ok) { results.push_back(ok); });
  router.request_open_orders("paper");
  assert(sim.recovery_requested.empty());
  assert(results.size() == 1 && !results[0]);
}

void tie_keeps_vector_order_and_deterministic() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500), make_candidate(alt, "alt", 25, 10000, 500)}, qts::RoutingPolicy{}};
  router.submit(make_intent("o1", 100, 1'000'0000));
  router.submit(make_intent("o2", 100, 1'000'0000));
  assert(sim.submitted.size() == 2 && alt.submitted.empty());
}

void market_order_uses_protection_for_fee_term() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::Router router{{make_candidate(sim, "sim", 10, 10000, 500), make_candidate(alt, "alt", 90, 10000, 500)}, qts::RoutingPolicy{}};
  qts::OrderIntent intent{"o1", "paper", "test", "TEST", qts::Side::buy, qts::OrderType::market, qts::TimeInForce::day, 100, 0};
  intent.protection_price = 1'000'0000;
  router.submit(intent);
  assert(sim.submitted.size() == 1 && alt.submitted.empty());
}

qts::ExecutionReport make_fill(const std::string& child, qts::Quantity last, qts::Price price, const std::string& exec_id, qts::Quantity cumulative = 0) {
  qts::ExecutionReport report{};
  report.client_order_id = child;
  report.status = qts::OrderStatus::partially_filled;
  report.last_quantity = last;
  report.last_price = price;
  report.execution_id = exec_id;
  report.cumulative_quantity = cumulative;
  return report;
}

qts::ExecutionReport make_terminal(const std::string& child, qts::OrderStatus status) {
  qts::ExecutionReport report{};
  report.client_order_id = child;
  report.status = status;
  return report;
}

void split_pro_rata_by_weight() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500, 7), make_candidate(alt, "alt", 50, 10000, 500, 3)}, qts::RoutingPolicy{}};
  router.submit(make_intent("o1", 100, 1'000'0000));
  assert(sim.submitted.size() == 1 && alt.submitted.size() == 1);
  assert(sim.submitted[0].client_order_id == "o1#0" && sim.submitted[0].quantity == 70);
  assert(alt.submitted[0].client_order_id == "o1#1" && alt.submitted[0].quantity == 30);
  assert(sim.submitted[0].parent_client_order_id == "o1");  // traceable, channels ignore
}

void split_largest_remainder_sums_exactly() {
  StubChannel a{"a"}, b{"b"}, c{"c"};
  qts::Router router{{make_candidate(a, "a", 25, 10000, 500, 1), make_candidate(b, "b", 25, 10000, 500, 1), make_candidate(c, "c", 25, 10000, 500, 1)}, qts::RoutingPolicy{}};
  router.submit(make_intent("o1", 100, 1'000'0000));
  qts::Quantity total = 0;
  for (const auto* channel : {&a, &b, &c}) {
    assert(channel->submitted.size() == 1);
    total += channel->submitted[0].quantity;
  }
  assert(total == 100);
  assert(a.submitted[0].quantity == 34);  // first takes the remainder
}

void split_skips_disconnected_channel() {
  StubChannel sim{"sim"}, alt{"alt"};
  alt.set_connected(false);
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500, 7), make_candidate(alt, "alt", 50, 10000, 500, 3)}, qts::RoutingPolicy{}};
  // Only one weighted channel up: M8-0 pick path, no suffix.
  router.submit(make_intent("o1", 100, 1'000'0000));
  assert(sim.submitted.size() == 1 && alt.submitted.empty());
  assert(sim.submitted[0].client_order_id == "o1");
}

void parent_fill_carries_child_fee() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500, 1), make_candidate(alt, "alt", 50, 10000, 500, 1)}, qts::RoutingPolicy{}};
  std::vector<qts::ExecutionReport> parents;
  router.set_execution_handler([&parents](const qts::ExecutionReport& r) { parents.push_back(r); });
  router.submit(make_intent("o9", 100, 1'000'0000));
  auto fill = make_fill("o9#0", 40, 1'000'0000, "F-FEE");
  fill.fee = 1234;
  sim.emit_report(fill);
  assert(parents.size() == 1 && parents[0].client_order_id == "o9");
  assert(parents[0].last_quantity == 40 && parents[0].fee == 1234);  // whole child fill: exact fee
}

void fills_aggregate_to_parent_filled() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500, 7), make_candidate(alt, "alt", 50, 10000, 500, 3)}, qts::RoutingPolicy{}};
  std::vector<qts::ExecutionReport> parents;
  router.set_execution_handler([&parents](const qts::ExecutionReport& r) { parents.push_back(r); });
  router.submit(make_intent("o1", 100, 1'000'0000));
  sim.emit_report(make_fill("o1#0", 40, 1'000'0000, "F-A"));
  assert(parents.size() == 1 && parents[0].client_order_id == "o1");
  assert(parents[0].status == qts::OrderStatus::partially_filled);
  assert(parents[0].execution_id == "sim:F-A");
  alt.emit_report(make_fill("o1#1", 30, 1'000'0000, "F-B"));
  assert(parents.size() == 2 && parents[1].execution_id == "alt:F-B");
  sim.emit_report(make_fill("o1#0", 30, 1'000'0000, "F-C"));  // 40+30+30 = 100
  assert(parents.size() == 3 && parents[2].status == qts::OrderStatus::filled);
  // Leftover live child withdrawn automatically.
  assert(alt.cancelled.size() == 1 && alt.cancelled[0] == "o1#1");
  // Late terminal noise for the completed parent is dropped.
  alt.emit_report(make_terminal("o1#1", qts::OrderStatus::cancelled));
  assert(parents.size() == 3);
}

void parent_cancel_fans_out_and_completes() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500, 1), make_candidate(alt, "alt", 50, 10000, 500, 1)}, qts::RoutingPolicy{}};
  std::vector<qts::ExecutionReport> parents;
  router.set_execution_handler([&parents](const qts::ExecutionReport& r) { parents.push_back(r); });
  router.submit(make_intent("o1", 100, 1'000'0000));
  sim.emit_report(make_fill("o1#0", 20, 1'000'0000, "F-A"));
  assert(parents.size() == 1 && parents[0].status == qts::OrderStatus::partially_filled);
  router.cancel("o1");
  assert(sim.cancelled.size() == 1 && alt.cancelled.size() == 1);
  sim.emit_report(make_terminal("o1#0", qts::OrderStatus::cancelled));
  assert(parents.size() == 1);  // still waiting on alt
  alt.emit_report(make_terminal("o1#1", qts::OrderStatus::cancelled));
  assert(parents.size() == 2 && parents[1].status == qts::OrderStatus::cancelled);
  assert(parents[1].client_order_id == "o1");
}

void reject_before_live_becomes_rejected() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500, 1), make_candidate(alt, "alt", 50, 10000, 500, 1)}, qts::RoutingPolicy{}};
  std::vector<qts::ExecutionReport> parents;
  router.set_execution_handler([&parents](const qts::ExecutionReport& r) { parents.push_back(r); });
  router.submit(make_intent("o1", 100, 1'000'0000));
  sim.emit_report(make_terminal("o1#0", qts::OrderStatus::rejected));
  assert(parents.empty());  // sibling still live: no verdict yet
  alt.emit_report(make_terminal("o1#1", qts::OrderStatus::rejected));
  assert(parents.size() == 1 && parents[0].status == qts::OrderStatus::rejected);
}

void reject_after_live_becomes_cancelled() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500, 1), make_candidate(alt, "alt", 50, 10000, 500, 1)}, qts::RoutingPolicy{}};
  std::vector<qts::ExecutionReport> parents;
  router.set_execution_handler([&parents](const qts::ExecutionReport& r) { parents.push_back(r); });
  router.submit(make_intent("o1", 100, 1'000'0000));
  qts::ExecutionReport live{};
  live.client_order_id = "o1#1";
  live.status = qts::OrderStatus::live;
  alt.emit_report(live);  // parent goes live first
  assert(parents.size() == 1 && parents[0].status == qts::OrderStatus::live);
  sim.emit_report(make_terminal("o1#0", qts::OrderStatus::rejected));
  alt.emit_report(make_terminal("o1#1", qts::OrderStatus::cancelled));
  assert(parents.size() == 2 && parents[1].status == qts::OrderStatus::cancelled);
}

void replayed_fill_reuses_execution_id() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500, 1), make_candidate(alt, "alt", 50, 10000, 500, 1)}, qts::RoutingPolicy{}};
  std::vector<qts::ExecutionReport> parents;
  router.set_execution_handler([&parents](const qts::ExecutionReport& r) { parents.push_back(r); });
  router.submit(make_intent("o1", 100, 1'000'0000));
  sim.emit_report(make_fill("o1#0", 40, 1'000'0000, "F-A"));
  sim.emit_report(make_fill("o1#0", 40, 1'000'0000, "F-A"));  // channel-level replay
  assert(parents.size() == 2);
  assert(parents[0].execution_id == parents[1].execution_id);  // OMS dedups, no double count
}

void recovery_cumulative_converges_without_double_count() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500, 1), make_candidate(alt, "alt", 50, 10000, 500, 1)}, qts::RoutingPolicy{}};
  std::vector<qts::ExecutionReport> parents;
  router.set_execution_handler([&parents](const qts::ExecutionReport& r) { parents.push_back(r); });
  router.submit(make_intent("o1", 100, 1'000'0000));
  sim.emit_report(make_fill("o1#0", 40, 1'000'0000, ""));     // live, no channel id
  sim.emit_report(make_fill("o1#0", 0, 1'000'0000, "", 40));  // recovery replay, cum 40
  assert(parents.size() == 2);
  assert(parents[0].execution_id == parents[1].execution_id);  // delta 0, same id
  assert(parents[1].last_quantity == 0);
}

void unknown_child_pattern_forwarded_best_effort() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500, 1), make_candidate(alt, "alt", 50, 10000, 500, 1)}, qts::RoutingPolicy{}};
  std::vector<qts::ExecutionReport> parents;
  router.set_execution_handler([&parents](const qts::ExecutionReport& r) { parents.push_back(r); });
  router.submit(make_intent("o1", 100, 1'000'0000));
  qts::ExecutionReport alien = make_fill("o1#9", 10, 1'000'0000, "F-X");  // not a known child
  alt.emit_report(alien);
  assert(parents.size() == 1 && parents[0].client_order_id == "o1");
  assert(parents[0].execution_id == "alt:F-X");
  assert(parents[0].order_id.empty());
}

// M8-2: without a clock the meter stays frozen at its configured seeds.
void meter_freezes_at_seeds_without_clock() {
  StubChannel sim{"sim"};
  qts::Router router{{make_candidate(sim, "sim", 25, 9000, 500)}, qts::RoutingPolicy{}};
  std::vector<qts::ExecutionReport> sink;
  router.set_execution_handler([&sink](const qts::ExecutionReport& r) { sink.push_back(r); });
  router.submit(make_intent("m1", 100, 1'000));
  qts::ExecutionReport ack{};
  ack.client_order_id = "m1";
  ack.status = qts::OrderStatus::live;
  sim.emit_report(ack);
  sim.emit_report(make_terminal("m1", qts::OrderStatus::filled));
  const auto stats = router.channel_stats();
  assert(stats.size() == 1);
  assert(stats[0].fill_rate_bps == 9000 && stats[0].latency_us == 500);
  assert(stats[0].fee_bps == 25);  // contractual fee never measured
}

qts::ExecutionReport make_ack(const std::string& id) {
  qts::ExecutionReport report{};
  report.client_order_id = id;
  report.status = qts::OrderStatus::live;
  return report;
}

// M8-2: ack latency and terminal fill ratio blend into the seeds by alpha.
void meter_blends_latency_and_fill_rate() {
  StubChannel sim{"sim"};
  qts::ManualClock clock{};
  clock.set(qts::Timestamp{std::chrono::seconds{1'000}});
  // alpha 2000 = 20% per sample; seeds fill 5000 / latency 1000us.
  qts::Router router{{make_candidate(sim, "sim", 25, 5000, 1000)}, qts::RoutingPolicy{}, &clock};
  std::vector<qts::ExecutionReport> sink;
  router.set_execution_handler([&sink](const qts::ExecutionReport& r) { sink.push_back(r); });
  router.submit(make_intent("m1", 100, 1'000));
  clock.advance(std::chrono::microseconds{2'000});
  sim.emit_report(make_ack("m1"));  // latency sample 2000: 1000 + 1000*0.2
  assert(router.channel_stats()[0].latency_us == 1'200);
  // Partial fills accumulate without sampling the ratio yet.
  sim.emit_report(make_fill("m1", 40, 1'000, "F-1", 40));
  assert(router.channel_stats()[0].fill_rate_bps == 5000);
  // Terminal 100/100: ratio 10000 blends to 6000.
  sim.emit_report(make_fill("m1", 60, 1'000, "F-2", 100));
  assert(router.channel_stats()[0].fill_rate_bps == 6'000);
  // Cancelled 30/100: ratio 3000 blends 6000 -> 5400.
  router.submit(make_intent("m2", 100, 1'000));
  sim.emit_report(make_ack("m2"));  // zero-latency ack pulls 1200 -> 960
  assert(router.channel_stats()[0].latency_us == 960);
  sim.emit_report(make_fill("m2", 30, 1'000, "F-3", 30));
  sim.emit_report(make_terminal("m2", qts::OrderStatus::cancelled));
  assert(router.channel_stats()[0].fill_rate_bps == 5'400);
}

// M8-2: measured stats steer the next pick (yesterday's tie broken live).
void measured_stats_steer_selection() {
  StubChannel sim{"sim"}, alt{"alt"};
  qts::ManualClock clock{};
  clock.set(qts::Timestamp{std::chrono::seconds{1'000}});
  qts::RoutingWeights weights{};
  weights.weight_fee = 0;  // fees tied at 25: latency decides
  qts::Router router{{make_candidate(sim, "sim", 25, 10000, 500), make_candidate(alt, "alt", 25, 10000, 500)}, qts::RoutingPolicy{weights}, &clock};
  std::vector<qts::ExecutionReport> sink;
  router.set_execution_handler([&sink](const qts::ExecutionReport& r) { sink.push_back(r); });
  router.submit(make_intent("o1", 100, 1'000));
  assert(sim.submitted.size() == 1);  // tie keeps vector order
  clock.advance(std::chrono::microseconds{100'000});
  sim.emit_report(make_ack("o1"));  // sim latency blows out: 500 + 99500*0.2
  assert(router.channel_stats()[0].latency_us == 20'400);
  router.submit(make_intent("o2", 100, 1'000));
  assert(alt.submitted.size() == 1 && sim.submitted.size() == 1);
}

// M8-2: split children meter per channel, not per parent.
void split_children_meter_per_channel() {
  StubChannel first{"a"}, second{"b"};
  qts::ManualClock clock{};
  clock.set(qts::Timestamp{std::chrono::seconds{1'000}});
  qts::Router router{{make_candidate(first, "a", 25, 5000, 1000, 1), make_candidate(second, "b", 25, 5000, 1000, 1)}, qts::RoutingPolicy{}, &clock};
  std::vector<qts::ExecutionReport> sink;
  router.set_execution_handler([&sink](const qts::ExecutionReport& r) { sink.push_back(r); });
  router.submit(make_intent("o1", 100, 1'000));
  clock.advance(std::chrono::microseconds{1'000});
  first.emit_report(make_ack("o1#0"));
  second.emit_report(make_ack("o1#1"));
  const auto stats = router.channel_stats();
  // Both acks sample 1000us against the 1000us seed: unchanged.
  assert(stats[0].latency_us == 1'000 && stats[1].latency_us == 1'000);
  // a's child completes 50/50 (ratio 100% -> 6000); b's cancels empty (0% -> 4000).
  first.emit_report(make_fill("o1#0", 50, 1'000, "F-A", 50));
  second.emit_report(make_terminal("o1#1", qts::OrderStatus::cancelled));
  assert(router.channel_stats()[0].fill_rate_bps == 6'000);
  assert(router.channel_stats()[1].fill_rate_bps == 4'000);
}

}  // namespace

int main() {
  fee_selects_cheaper();
  latency_weight_flips_selection();
  fill_weight_flips_selection();
  disconnected_channel_skipped();
  no_channel_synthesizes_rejected();
  execution_ids_namespaced_per_channel();
  cancel_routed_to_origin_channel();
  recovery_aggregated_across_channels();
  recovery_failure_propagates();
  recovery_with_no_channel_fails_fast();
  tie_keeps_vector_order_and_deterministic();
  market_order_uses_protection_for_fee_term();
  split_pro_rata_by_weight();
  split_largest_remainder_sums_exactly();
  split_skips_disconnected_channel();
  fills_aggregate_to_parent_filled();
  parent_fill_carries_child_fee();
  parent_cancel_fans_out_and_completes();
  reject_before_live_becomes_rejected();
  reject_after_live_becomes_cancelled();
  replayed_fill_reuses_execution_id();
  recovery_cumulative_converges_without_double_count();
  unknown_child_pattern_forwarded_best_effort();
  meter_freezes_at_seeds_without_clock();
  meter_blends_latency_and_fill_rate();
  measured_stats_steer_selection();
  split_children_meter_per_channel();
  return 0;
}
