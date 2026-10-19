// clang-format off
/**
  ******************************************************************************
  * @file           : order_manager_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for OMS transitions, dedup, conflicts and cancel flows.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/oms/order_manager.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>

#include "qts/broker/simulated_broker.hpp"
#include "qts/broker/trade_api.hpp"
#include "qts/persistence/ledger.hpp"

namespace {

// Gateway that never reports: orders stay PendingNew so transition rules
// can be exercised deterministically.
class QuietGateway : public qts::TradeApi {
 public:
  std::string_view channel() const noexcept override { return "quiet"; }
  bool connected() const noexcept override { return true; }
  void submit(const qts::OrderIntent&) override {}
  void cancel(const qts::ClientOrderId&) override {}
  void set_execution_handler(ExecutionHandler handler) override { handler_ = std::move(handler); }
  void request_account_snapshot(const qts::AccountId&) override {}
  void request_open_orders(const qts::AccountId&) override {}
  void set_account_snapshot_handler(AccountSnapshotHandler) override {}
  void set_recovery_complete_handler(RecoveryCompleteHandler) override {}
  void request_security_info(const qts::AccountId&, const qts::InstrumentId&) override {}
  void set_security_info_handler(SecurityInfoHandler) override {}
  void request_asset_snapshot(const qts::AccountId&) override {}
  void set_asset_snapshot_handler(AssetSnapshotHandler) override {}

 private:
  ExecutionHandler handler_;
};

// Gateway whose transport always fails: submit must surface as Rejected.
class ThrowingGateway final : public QuietGateway {
 public:
  void submit(const qts::OrderIntent&) override { throw std::runtime_error("transport down"); }
};

qts::OrderIntent make_intent(const std::string& id) { return qts::OrderIntent{id, "paper", "test", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, 10, 100}; }

qts::ExecutionReport make_report(const std::string& order_id, const std::string& client_id, qts::OrderStatus status, qts::Quantity last_qty, const std::string& exec_id, qts::Quantity cumulative = 0) {
  return qts::ExecutionReport{order_id, client_id, status, last_qty, 100, "", exec_id, cumulative, 0};
}

void happy_path_and_dedup() {
  qts::EventBus events;
  qts::SimulatedBroker broker{events};
  qts::RiskEngine risk{{100, 10'000, true}};
  qts::OrderManager oms{broker, risk, events};

  assert(oms.submit(make_intent("two")));
  assert(oms.find("two")->status == qts::OrderStatus::live);
  assert(!oms.submit(make_intent("two")));  // duplicate client order id

  assert(oms.on_execution_report(make_report("SIM-two", "two", qts::OrderStatus::partially_filled, 4, "fill-1")));
  assert(oms.find("two")->cumulative_quantity == 4);
  // Replayed execution id must not double-count.
  assert(oms.on_execution_report(make_report("SIM-two", "two", qts::OrderStatus::partially_filled, 4, "fill-1")));
  assert(oms.find("two")->cumulative_quantity == 4);
  // Conflicting broker order id must be rejected for reconciliation.
  assert(!oms.on_execution_report(make_report("OTHER", "two", qts::OrderStatus::filled, 6, "fill-2")));
  assert(oms.cancel("two"));
  assert(oms.find("two")->status == qts::OrderStatus::cancelled);
}

void live_sell_exposure_tracks_unfilled_remainder() {
  qts::EventBus events;
  QuietGateway gateway;
  qts::RiskEngine risk{{10'000, 0, true}};
  qts::OrderManager oms{gateway, risk, events};
  auto sell = make_intent("exp-1");
  sell.side = qts::Side::sell;
  sell.quantity = 60;
  assert(oms.submit(sell));
  auto exposure = oms.live_sell_exposure();
  const auto key = std::make_pair(qts::StrategyId{"test"}, qts::InstrumentId{"TEST"});
  assert(exposure.size() == 1 && exposure[key] == 60);
  // Partial fills shrink the remainder; buys never count toward it.
  assert(oms.on_execution_report(make_report("SIM-exp-1", "exp-1", qts::OrderStatus::partially_filled, 20, "fill-1", 20)));
  assert(oms.live_sell_exposure()[key] == 40);
  assert(oms.submit(make_intent("exp-2")));  // buy side
  assert(oms.live_sell_exposure()[key] == 40);
  // Terminal fill clears the reservation input.
  assert(oms.on_execution_report(make_report("SIM-exp-1", "exp-1", qts::OrderStatus::filled, 40, "fill-2", 60)));
  assert(oms.live_sell_exposure().empty());
}

void live_buy_exposure_sums_unfilled_notional() {
  qts::EventBus events;
  QuietGateway gateway;
  qts::RiskEngine risk{{10'000, 0, true}};
  qts::OrderManager oms{gateway, risk, events};
  assert(oms.live_buy_exposure() == 0);
  auto first = make_intent("buy-1");
  first.quantity = 60;
  first.limit_price = 100;  // 6000 notional
  assert(oms.submit(first));
  auto second = make_intent("buy-2");
  second.quantity = 40;
  second.limit_price = 100;  // +4000
  assert(oms.submit(second));
  assert(oms.live_buy_exposure() == 10'000);
  // Partial fill releases proportionally; sells never count.
  assert(oms.on_execution_report(make_report("SIM-buy-1", "buy-1", qts::OrderStatus::partially_filled, 20, "fill-1", 20)));
  assert(oms.live_buy_exposure() == 8'000);
  auto sell = make_intent("sell-1");
  sell.side = qts::Side::sell;
  sell.quantity = 1'000;
  assert(oms.submit(sell));
  assert(oms.live_buy_exposure() == 8'000);
}

void risk_rejection_never_reaches_broker() {
  qts::EventBus events;
  int published = 0;
  events.subscribe_execution([&](const qts::ExecutionReport&) { ++published; });
  qts::SimulatedBroker broker{events};
  qts::RiskEngine risk{{100, 10'000, false}};  // kill switch off
  qts::OrderManager oms{broker, risk, events};

  assert(!oms.submit(make_intent("risk-blocked")));
  const auto* snapshot = oms.find("risk-blocked");
  assert(snapshot != nullptr && snapshot->status == qts::OrderStatus::rejected);
  assert(published == 1);
}

void submission_exception_becomes_rejected() {
  qts::EventBus events;
  ThrowingGateway broker{};
  qts::RiskEngine risk{{100, 10'000, true}};
  qts::OrderManager oms{broker, risk, events};

  assert(!oms.submit(make_intent("transport-fail")));
  const auto* snapshot = oms.find("transport-fail");
  assert(snapshot != nullptr && snapshot->status == qts::OrderStatus::rejected);
  assert(snapshot->version == 1);
}

void illegal_transitions_are_rejected() {
  qts::EventBus events;
  QuietGateway broker{};
  qts::RiskEngine risk{{100, 10'000, true}};
  qts::OrderManager oms{broker, risk, events};

  assert(oms.submit(make_intent("t1")));
  // PendingNew -> PendingCancel is not a legal transition.
  assert(!oms.on_execution_report(make_report("", "t1", qts::OrderStatus::pending_cancel, 0, "")));
  // PendingNew -> Live is legal.
  assert(oms.on_execution_report(make_report("B-t1", "t1", qts::OrderStatus::live, 0, "")));
  // Live -> Rejected is not legal (rejection only happens pre-broker).
  assert(!oms.on_execution_report(make_report("B-t1", "t1", qts::OrderStatus::rejected, 0, "")));
  // Live -> Filled, then Filled is terminal: no further transitions.
  assert(oms.on_execution_report(make_report("B-t1", "t1", qts::OrderStatus::filled, 10, "fill-1", 10)));
  assert(!oms.on_execution_report(make_report("B-t1", "t1", qts::OrderStatus::cancelled, 0, "cxl-1")));
  // Repeated terminal acknowledgement without new fill is harmless.
  assert(oms.on_execution_report(make_report("B-t1", "t1", qts::OrderStatus::filled, 0, "", 10)));
  assert(oms.find("t1")->cumulative_quantity == 10);
}

void pending_cancel_cannot_go_live() {
  qts::EventBus events;
  QuietGateway broker{};
  qts::RiskEngine risk{{100, 10'000, true}};
  qts::OrderManager oms{broker, risk, events};

  assert(oms.submit(make_intent("t2")));
  assert(oms.on_execution_report(make_report("B-t2", "t2", qts::OrderStatus::live, 0, "")));
  assert(oms.cancel("t2"));
  // PendingCancel -> Live moves backwards and must be rejected.
  assert(!oms.on_execution_report(make_report("B-t2", "t2", qts::OrderStatus::live, 0, "")));
}

void illegal_cancel_and_unknown_reports() {
  qts::EventBus events;
  QuietGateway broker{};
  qts::RiskEngine risk{{100, 10'000, true}};
  qts::OrderManager oms{broker, risk, events};

  assert(!oms.cancel("unknown"));  // no such order
  assert(oms.submit(make_intent("t3")));
  assert(!oms.cancel("t3"));  // PendingNew is not cancellable (needs Live/PartiallyFilled)
  assert(!oms.on_execution_report(make_report("B-x", "unknown", qts::OrderStatus::live, 0, "")));
  assert(!oms.on_execution_report(make_report("B-t3", "t3", qts::OrderStatus::live, -1, "")));  // negative fill
}

void settle_day_clears_execution_dedup() {
  qts::EventBus events;
  qts::SimulatedBroker broker{events};
  qts::RiskEngine risk{{100, 10'000, true}};
  qts::OrderManager oms{broker, risk, events};

  assert(oms.submit(make_intent("sd1")));
  assert(oms.on_execution_report(make_report("SIM-sd1", "sd1", qts::OrderStatus::partially_filled, 4, "day1-fill")));
  const std::uint64_t version_before = oms.find("sd1")->version;
  // Same execution id replays are deduped within the day.
  assert(oms.on_execution_report(make_report("SIM-sd1", "sd1", qts::OrderStatus::partially_filled, 4, "day1-fill")));
  assert(oms.find("sd1")->version == version_before);
  // Execution ids are day-scoped (archforce §3.3): after settlement the
  // same id is a new fill again.
  oms.settle_day();
  assert(oms.on_execution_report(make_report("SIM-sd1", "sd1", qts::OrderStatus::partially_filled, 4, "day1-fill")));
  assert(oms.find("sd1")->version == version_before + 1);
  assert(oms.find("sd1")->cumulative_quantity == 8);
}

void open_parent_ids_groups_working_children() {
  qts::EventBus events;
  qts::SimulatedBroker broker{events};
  qts::RiskEngine risk{{10000, 0, true}};
  qts::OrderManager oms{broker, risk, events};
  assert(oms.open_parent_ids().empty());

  // Resting limit children (buy below the reference never crosses).
  broker.set_reference_price("TEST", 1'000);
  auto child_a = make_intent("a#0");
  child_a.quantity = 100;
  child_a.limit_price = 800;
  child_a.parent_client_order_id = "parent-a";
  auto child_b = make_intent("b#0");
  child_b.quantity = 100;
  child_b.limit_price = 800;
  child_b.parent_client_order_id = "parent-a";
  auto lone = make_intent("lone");
  lone.quantity = 100;
  lone.limit_price = 800;
  assert(oms.submit(child_a) && oms.submit(child_b) && oms.submit(lone));
  const auto parents = oms.open_parent_ids();
  assert(parents.size() == 1 && parents[0] == "parent-a");  // parentless excluded
}

void empty_strategy_normalizes_to_passthrough() {
  qts::EventBus events;
  qts::SimulatedBroker broker{events};
  qts::RiskEngine risk{{100, 10'000, true}};
  qts::InMemoryEventLedger ledger{};
  qts::OrderManager oms{broker, risk, events, &ledger};
  auto manual = make_intent("manual-1");
  manual.strategy_id.clear();  // manual pass-through order
  assert(oms.submit(manual));
  const auto stored = ledger.replay(1);
  assert(!stored.empty());
  // Every recorded leg carries the normalized basket, including the broker leg.
  for (const auto& event : stored) {
    assert(event.intent.strategy_id == qts::kPassthroughStrategy);
  }
}

void all_orders_reports_submission_order() {
  qts::EventBus events;
  qts::SimulatedBroker broker{events};
  qts::RiskEngine risk{{10'000, 0, true}};
  qts::OrderManager oms{broker, risk, events};
  assert(oms.submit(make_intent("seq-b")));
  assert(oms.submit(make_intent("seq-a")));
  assert(oms.cancel("seq-b"));
  // Restored orders join the tail in restore order (ledger order).
  qts::OrderIntent restored = make_intent("seq-c");
  qts::ExecutionReport snapshot{"", "seq-c", qts::OrderStatus::live, 0, 0, "", "", 0, 0, "", 0};
  assert(oms.restore_order(restored, snapshot));
  const auto views = oms.all_orders();
  assert(views.size() == 3);
  assert(views[0].intent.client_order_id == "seq-b" && views[0].seq == 1);
  assert(views[1].intent.client_order_id == "seq-a" && views[1].seq == 2);
  assert(views[2].intent.client_order_id == "seq-c" && views[2].seq == 3);
  assert(views[0].intent.strategy_id == "test");
  assert(qts::OrderManager::is_terminal(qts::OrderStatus::filled));
  assert(!qts::OrderManager::is_terminal(qts::OrderStatus::live));
}

}  // namespace

int main() {
  happy_path_and_dedup();
  risk_rejection_never_reaches_broker();
  submission_exception_becomes_rejected();
  illegal_transitions_are_rejected();
  pending_cancel_cannot_go_live();
  illegal_cancel_and_unknown_reports();
  settle_day_clears_execution_dedup();
  empty_strategy_normalizes_to_passthrough();
  open_parent_ids_groups_working_children();
  live_sell_exposure_tracks_unfilled_remainder();
  live_buy_exposure_sums_unfilled_notional();
  all_orders_reports_submission_order();
  return 0;
}
