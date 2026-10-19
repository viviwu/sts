// clang-format off
/**
  ******************************************************************************
  * @file           : execution_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for execution algorithms and parent lifecycles.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include <cassert>
#include <chrono>
#include <string>
#include <vector>

#include "sts/execution/execution_api.hpp"

namespace {

sts::OrderIntent make_parent(const std::string& id, sts::Quantity quantity) {
  sts::OrderIntent intent{};
  intent.client_order_id = id;
  intent.account_id = "paper";
  intent.strategy_id = "grid-1";
  intent.instrument_id = "TEST";
  intent.side = sts::Side::buy;
  intent.type = sts::OrderType::limit;
  intent.quantity = quantity;
  intent.limit_price = 1'000;
  intent.trace_id = "trace-" + id;
  return intent;
}

void immediate_emits_one_linked_child_per_parent() {
  sts::ImmediateAlgorithm algo{};
  assert(algo.empty());
  assert(algo.poll(sts::Timestamp{}).empty());

  algo.add_parent(make_parent("grid-1", 100));
  algo.add_parent(make_parent("grid-2", 200));
  assert(!algo.empty());

  const auto children = algo.poll(sts::Timestamp{});
  assert(children.size() == 2);
  assert(!algo.empty());  // emitted children stay live until terminally reported

  // Identity: derived child id + parent linkage, trace inherited.
  assert(children[0].client_order_id == "grid-1#0");
  assert(children[0].parent_client_order_id == "grid-1");
  assert(children[0].trace_id == "trace-grid-1");
  assert(children[1].client_order_id == "grid-2#0");
  assert(children[1].parent_client_order_id == "grid-2");

  // Economics untouched: only the identity columns change.
  assert(children[0].quantity == 100 && children[0].limit_price == 1'000);
  assert(children[0].side == sts::Side::buy && children[0].instrument_id == "TEST");
  assert(children[1].quantity == 200);

  // Terminal reports retire the live children.
  for (const auto& child : children) {
    sts::ExecutionReport report{};
    report.client_order_id = child.client_order_id;
    report.status = sts::OrderStatus::filled;
    report.cumulative_quantity = child.quantity;
    report.execution_id = "fill-" + child.client_order_id;
    algo.on_execution_report(report);
  }
  assert(algo.empty());
}

void immediate_is_deterministic_across_instances() {
  sts::ImmediateAlgorithm first{};
  sts::ImmediateAlgorithm second{};
  for (const auto& id : std::vector<std::string>{"a", "b", "c"}) {
    first.add_parent(make_parent(id, 100));
    second.add_parent(make_parent(id, 100));
  }
  const auto left = first.poll(sts::Timestamp{});
  const auto right = second.poll(sts::Timestamp{});
  assert(left.size() == right.size());
  for (std::size_t i = 0; i < left.size(); ++i) {
    assert(left[i].client_order_id == right[i].client_order_id);
    assert(left[i].parent_client_order_id == right[i].parent_client_order_id);
    assert(left[i].trace_id == right[i].trace_id);
  }
}

sts::Timestamp at(std::int64_t seconds) { return sts::Timestamp{std::chrono::seconds{seconds}}; }

sts::ExecutionReport make_child_report(const std::string& child_id, sts::OrderStatus status, sts::Quantity cumulative, const std::string& execution_id) {
  sts::ExecutionReport report{};
  report.order_id = "SIM-" + child_id;
  report.client_order_id = child_id;
  report.status = status;
  report.last_quantity = 0;
  report.cumulative_quantity = cumulative;
  report.execution_id = execution_id;
  return report;
}

void twap_splits_evenly_over_intervals() {
  sts::TwapConfig config{};
  config.slices = 4;
  config.interval = std::chrono::seconds{10};
  sts::TwapAlgorithm algo{config};
  algo.add_parent(make_parent("p1", 100));

  // Schedule anchors at the first poll: slice 0 is due immediately.
  auto due = algo.poll(at(1'000));
  assert(due.size() == 1 && due[0].quantity == 25);
  assert(due[0].client_order_id == "p1#0");
  assert(due[0].parent_client_order_id == "p1");
  assert(!algo.empty());

  // Early poll emits nothing; on-time poll emits exactly one slice.
  assert(algo.poll(at(1'005)).empty());
  due = algo.poll(at(1'010));
  assert(due.size() == 1 && due[0].client_order_id == "p1#1" && due[0].quantity == 25);
  due = algo.poll(at(1'020));
  assert(due.size() == 1 && due[0].client_order_id == "p1#2");
  assert(!algo.empty());
  due = algo.poll(at(1'030));
  assert(due.size() == 1 && due[0].client_order_id == "p1#3");
  // Fully emitted but children live: retained until terminally reported.
  assert(!algo.empty());
  for (const char* id : {"p1#0", "p1#1", "p1#2", "p1#3"}) {
    algo.on_execution_report(make_child_report(id, sts::OrderStatus::filled, 25, std::string{"fill-"} + id));
  }
  assert(algo.empty());
}

void twap_remainder_goes_to_early_slices() {
  sts::TwapConfig config{};
  config.slices = 3;
  config.interval = std::chrono::seconds{10};
  sts::TwapAlgorithm algo{config};
  algo.add_parent(make_parent("p2", 100));

  // Sparse polling catches up: slice 0 at anchor, the rest at once.
  const auto first = algo.poll(at(5'000));
  assert(first.size() == 1 && first[0].quantity == 34);
  const auto late = algo.poll(at(5'000 + 20));
  assert(late.size() == 2);
  assert(late[0].quantity == 33 && late[1].quantity == 33);
  assert(late[0].client_order_id == "p2#1" && late[1].client_order_id == "p2#2");
  // Cancel after full emission still fans out to every live child.
  assert(!algo.empty());
  const auto cancelled = algo.cancel_parent("p2");
  assert((cancelled == std::vector<std::string>{"p2#0", "p2#1", "p2#2"}));
  assert(algo.empty());
}

void twap_tiny_parent_emits_unit_slices() {
  sts::TwapConfig config{};
  config.slices = 4;
  config.interval = std::chrono::seconds{10};
  sts::TwapAlgorithm algo{config};
  algo.add_parent(make_parent("p3", 2));

  // Quantity < slices: two unit children, never zero-quantity.
  auto due = algo.poll(at(0));
  assert(due.size() == 1 && due[0].quantity == 1);
  due = algo.poll(at(10));
  assert(due.size() == 1 && due[0].quantity == 1);
  const auto cancelled = algo.cancel_parent("p3");
  assert((cancelled == std::vector<std::string>{"p3#0", "p3#1"}));
  assert(algo.empty());
}

void twap_is_deterministic_across_instances() {
  auto run = []() {
    sts::TwapConfig config{};
    config.slices = 3;
    config.interval = std::chrono::seconds{10};
    sts::TwapAlgorithm algo{config};
    algo.add_parent(make_parent("q", 100));
    std::vector<std::string> ids;
    for (std::int64_t t = 0; t <= 30; t += 5) {
      for (const auto& child : algo.poll(at(t))) ids.push_back(child.client_order_id);
    }
    return ids;
  };
  assert(run() == run());
  assert(run().size() == 3);
}

sts::QuoteEvent make_volume_quote(const std::string& instrument, sts::Quantity cumulative) {
  sts::QuoteEvent quote{};
  quote.instrument_id = instrument;
  quote.last_price = 1'000;
  quote.cumulative_volume = cumulative;
  quote.quality = sts::QuoteQuality::valid;
  return quote;
}

void pov_tracks_market_volume() {
  sts::PovConfig config{};
  config.participation_bps = 10'000;  // 100%: one unit per market unit
  sts::PovAlgorithm algo{config};
  algo.add_parent(make_parent("v1", 100));

  // First quote only sets the baseline: no retroactive budget.
  algo.on_quote(make_volume_quote("TEST", 1'000));
  assert(algo.poll(at(0)).empty());
  assert(!algo.empty());

  // +50 printed -> 50 units due; +100 more -> remainder filled.
  algo.on_quote(make_volume_quote("TEST", 1'050));
  auto due = algo.poll(at(1));
  assert(due.size() == 1 && due[0].quantity == 50);
  assert(due[0].client_order_id == "v1#0");
  assert(due[0].parent_client_order_id == "v1");
  algo.on_quote(make_volume_quote("TEST", 1'150));
  due = algo.poll(at(2));
  assert(due.size() == 1 && due[0].quantity == 50 && due[0].client_order_id == "v1#1");
  algo.on_execution_report(make_child_report("v1#0", sts::OrderStatus::filled, 50, "fill-v1-0"));
  algo.on_execution_report(make_child_report("v1#1", sts::OrderStatus::filled, 100, "fill-v1-1"));
  assert(algo.empty());
}

void pov_partial_participation_and_resync() {
  sts::PovConfig config{};
  config.participation_bps = 1'000;  // 10%
  sts::PovAlgorithm algo{config};
  algo.add_parent(make_parent("v2", 100));

  algo.on_quote(make_volume_quote("TEST", 0));  // unset volume: ignored
  algo.on_quote(make_volume_quote("TEST", 500));
  assert(algo.poll(at(0)).empty());
  algo.on_quote(make_volume_quote("TEST", 1'500));  // +1000 -> budget 100
  auto due = algo.poll(at(1));
  assert(due.size() == 1 && due[0].quantity == 100);
  const auto cancelled = algo.cancel_parent("v2");
  assert((cancelled == std::vector<std::string>{"v2#0"}));
  assert(algo.empty());

  // Regression in cumulative volume resyncs without negative accrual.
  sts::PovAlgorithm late{config};
  late.add_parent(make_parent("v3", 10));
  late.on_quote(make_volume_quote("TEST", 2'000));
  late.on_quote(make_volume_quote("TEST", 1'900));  // reset/regress: no budget
  assert(late.poll(at(0)).empty());
  assert(!late.empty());
}

void pov_is_deterministic_across_instances() {
  auto run = []() {
    sts::PovConfig config{};
    config.participation_bps = 5'000;
    sts::PovAlgorithm algo{config};
    algo.add_parent(make_parent("w", 60));
    algo.on_quote(make_volume_quote("TEST", 100));
    algo.on_quote(make_volume_quote("TEST", 220));  // +120 -> 60 units
    std::vector<sts::Quantity> quantities;
    for (const auto& child : algo.poll(at(0))) quantities.push_back(child.quantity);
    return quantities;
  };
  assert(run() == run());
  assert(run() == std::vector<sts::Quantity>{60});
}

void vwap_follows_curve_with_remainder_on_last() {
  sts::VwapConfig config{};
  config.weights_bps = {5000, 3000, 2000};
  config.interval = std::chrono::seconds{10};
  sts::VwapAlgorithm algo{config};
  algo.add_parent(make_parent("s1", 101));

  auto due = algo.poll(at(0));
  assert(due.size() == 1 && due[0].quantity == 50 && due[0].client_order_id == "s1#0");
  due = algo.poll(at(10));
  assert(due.size() == 1 && due[0].quantity == 30 && due[0].client_order_id == "s1#1");
  assert(!algo.empty());
  due = algo.poll(at(20));
  assert(due.size() == 1 && due[0].quantity == 21 && due[0].client_order_id == "s1#2");
  algo.on_execution_report(make_child_report("s1#0", sts::OrderStatus::filled, 50, "fill-s1-0"));
  algo.on_execution_report(make_child_report("s1#1", sts::OrderStatus::filled, 80, "fill-s1-1"));
  algo.on_execution_report(make_child_report("s1#2", sts::OrderStatus::filled, 101, "fill-s1-2"));
  assert(algo.empty());
}

void vwap_tiny_parent_skips_dust_slices() {
  sts::VwapConfig config{};
  config.weights_bps = {5000, 3000, 2000};
  config.interval = std::chrono::seconds{10};
  sts::VwapAlgorithm algo{config};
  algo.add_parent(make_parent("s2", 1));

  // Leading dust slices are skipped; the last slice carries the unit.
  assert(algo.poll(at(0)).empty());
  assert(algo.poll(at(10)).empty());
  const auto due = algo.poll(at(20));
  assert(due.size() == 1 && due[0].quantity == 1 && due[0].client_order_id == "s2#2");
  const auto cancelled = algo.cancel_parent("s2");
  assert((cancelled == std::vector<std::string>{"s2#2"}));
  assert(algo.empty());
}

void iceberg_refreshes_peak_on_full_fill() {
  sts::IcebergConfig config{};
  config.display_quantity = 100;
  sts::IcebergAlgorithm algo{config};
  algo.add_parent(make_parent("h1", 250));

  // Only the peak is visible; partial fills do not refresh.
  auto due = algo.poll(at(0));
  assert(due.size() == 1 && due[0].quantity == 100 && due[0].client_order_id == "h1#0");
  assert(algo.poll(at(0)).empty());  // active child outstanding
  algo.on_execution_report(make_child_report("h1#0", sts::OrderStatus::partially_filled, 40, "fill-1"));
  assert(algo.poll(at(0)).empty());

  // Duplicate delivery of the same execution is ignored.
  algo.on_execution_report(make_child_report("h1#0", sts::OrderStatus::partially_filled, 40, "fill-1"));
  assert(algo.poll(at(0)).empty());

  // Full consumption releases the next peak; the tail peak is the remainder.
  algo.on_execution_report(make_child_report("h1#0", sts::OrderStatus::filled, 100, "fill-2"));
  due = algo.poll(at(0));
  assert(due.size() == 1 && due[0].quantity == 100 && due[0].client_order_id == "h1#1");
  algo.on_execution_report(make_child_report("h1#1", sts::OrderStatus::filled, 100, "fill-3"));
  due = algo.poll(at(0));
  assert(due.size() == 1 && due[0].quantity == 50 && due[0].client_order_id == "h1#2");
  algo.on_execution_report(make_child_report("h1#2", sts::OrderStatus::filled, 50, "fill-4"));
  assert(algo.empty());
}

void iceberg_cancel_returns_remainder_to_hidden() {
  sts::IcebergConfig config{};
  config.display_quantity = 100;
  sts::IcebergAlgorithm algo{config};
  algo.add_parent(make_parent("h2", 150));

  auto due = algo.poll(at(0));
  assert(due.size() == 1 && due[0].quantity == 100);
  // Active child cancelled after a 30-unit partial: 120 hidden, next peak 100.
  algo.on_execution_report(make_child_report("h2#0", sts::OrderStatus::partially_filled, 30, "fill-1"));
  algo.on_execution_report(make_child_report("h2#0", sts::OrderStatus::cancelled, 30, "cxl-1"));
  due = algo.poll(at(0));
  assert(due.size() == 1 && due[0].quantity == 100 && due[0].client_order_id == "h2#1");
  assert(!algo.empty());
}

void immediate_cancel_before_and_after_poll() {
  sts::ImmediateAlgorithm algo{};
  algo.add_parent(make_parent("c1", 100));
  // Cancel before poll: never submitted, nothing for OMS to cancel.
  assert(algo.cancel_parent("c1").empty());
  assert(algo.poll(at(0)).empty());

  algo.add_parent(make_parent("c2", 100));
  auto due = algo.poll(at(0));
  assert(due.size() == 1);
  // Cancel after poll: the live child goes to OMS.cancel.
  const auto cancelled = algo.cancel_parent("c2");
  assert(cancelled == std::vector<std::string>{"c2#0"});
  assert(algo.empty());
}

void immediate_replace_bumps_generation() {
  sts::ImmediateAlgorithm algo{};
  algo.add_parent(make_parent("r1", 100));
  assert(algo.poll(at(0)).size() == 1);

  auto updated = make_parent("r1", 200);
  const auto cancelled = algo.replace_parent(updated);
  assert(cancelled == std::vector<std::string>{"r1#0"});
  const auto due = algo.poll(at(0));
  assert(due.size() == 1 && due[0].client_order_id == "r1#r1#0" && due[0].quantity == 200);
  assert(!algo.empty());  // replacement child live until reported
  sts::ExecutionReport report{};
  report.client_order_id = "r1#r1#0";
  report.status = sts::OrderStatus::filled;
  report.cumulative_quantity = 200;
  report.execution_id = "fill-r1";
  algo.on_execution_report(report);
  assert(algo.empty());
}

void twap_cancel_and_replace() {
  sts::TwapConfig config{};
  config.slices = 4;
  config.interval = std::chrono::seconds{10};
  sts::TwapAlgorithm algo{config};
  algo.add_parent(make_parent("t1", 100));

  auto due = algo.poll(at(0));
  assert(due.size() == 1 && due[0].client_order_id == "t1#0");
  // Terminal reports prune the live set; only outstanding slices cancel.
  algo.on_execution_report(make_child_report("t1#0", sts::OrderStatus::filled, 25, "fill-1"));
  due = algo.poll(at(10));
  assert(due.size() == 1 && due[0].client_order_id == "t1#1");
  const auto cancelled = algo.cancel_parent("t1");
  assert(cancelled == std::vector<std::string>{"t1#1"});

  // Replace re-plans from scratch with a new generation suffix.
  algo.add_parent(make_parent("t2", 80));
  assert(algo.poll(at(100)).size() == 1);
  auto updated = make_parent("t2", 40);
  const auto recancelled = algo.replace_parent(updated);
  assert(recancelled == std::vector<std::string>{"t2#0"});
  due = algo.poll(at(110));
  assert(due.size() == 1 && due[0].client_order_id == "t2#r1#0" && due[0].quantity == 10);
}

void iceberg_replace_recalls_live_peak() {
  sts::IcebergConfig config{};
  config.display_quantity = 100;
  sts::IcebergAlgorithm algo{config};
  algo.add_parent(make_parent("h3", 250));

  auto due = algo.poll(at(0));
  assert(due.size() == 1 && due[0].quantity == 100);
  algo.on_execution_report(make_child_report("h3#0", sts::OrderStatus::partially_filled, 40, "fill-1"));

  // Replace recalls the live peak; its 60 unfilled units return to hidden.
  auto updated = make_parent("h3", 250);
  updated.limit_price = 1'010;
  const auto cancelled = algo.replace_parent(updated);
  assert(cancelled == std::vector<std::string>{"h3#0"});
  due = algo.poll(at(0));
  assert(due.size() == 1 && due[0].client_order_id == "h3#1");
  assert(due[0].quantity == 100 && due[0].limit_price == 1'010);
  assert(!algo.empty());
}

}  // namespace

int main() {
  immediate_emits_one_linked_child_per_parent();
  immediate_is_deterministic_across_instances();
  twap_splits_evenly_over_intervals();
  twap_remainder_goes_to_early_slices();
  twap_tiny_parent_emits_unit_slices();
  twap_is_deterministic_across_instances();
  pov_tracks_market_volume();
  pov_partial_participation_and_resync();
  pov_is_deterministic_across_instances();
  vwap_follows_curve_with_remainder_on_last();
  vwap_tiny_parent_skips_dust_slices();
  iceberg_refreshes_peak_on_full_fill();
  iceberg_cancel_returns_remainder_to_hidden();
  immediate_cancel_before_and_after_poll();
  immediate_replace_bumps_generation();
  twap_cancel_and_replace();
  iceberg_replace_recalls_live_peak();
  return 0;
}
