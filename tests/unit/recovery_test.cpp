// clang-format off
/**
  ******************************************************************************
  * @file           : recovery_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for snapshot restore, replay and reconcile diffs.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include <cassert>
#include <string>

#include "sts/broker/simulated_broker.hpp"
#include "sts/oms/order_manager.hpp"
#include "sts/persistence/ledger.hpp"

namespace {

sts::OrderIntent make_intent(const std::string& id, sts::Quantity qty = 10) {
  sts::OrderIntent intent{id, "paper", "test", "TEST", sts::Side::buy, sts::OrderType::limit, sts::TimeInForce::day, qty, 100};
  intent.trace_id = "trace-" + id;
  return intent;
}

void ledger_assigns_monotonic_ids() {
  sts::InMemoryEventLedger ledger{};
  assert(ledger.latest_event_id() == 0);
  assert(ledger.replay(1).empty());

  sts::LedgerEvent event{};
  event.snapshot.client_order_id = "a";
  assert(ledger.append(event) == 1);
  assert(ledger.append(event) == 2);
  assert(ledger.latest_event_id() == 2);
  assert(ledger.replay(1).size() == 2);
  assert(ledger.replay(2).size() == 1);
  assert(ledger.replay(3).empty());
}

void oms_records_every_transition() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};  // no reference price: orders stay live
  sts::RiskEngine risk{{100, 10'000, true}};
  sts::InMemoryEventLedger ledger{};
  sts::OrderManager oms{broker, risk, events, &ledger};
  oms.set_ledger_versions("cfg-1", "rules-7");

  assert(oms.submit(make_intent("k1")));
  assert(oms.cancel("k1"));
  // submit + live ack + cancel-request + broker cancel report = 4 events.
  assert(ledger.latest_event_id() == 4);
  const auto events_out = ledger.replay(1);
  assert(events_out.size() == 4);
  assert(events_out[0].type == sts::LedgerEventType::order_submitted);
  assert(events_out[1].type == sts::LedgerEventType::execution_applied);
  assert(events_out[2].type == sts::LedgerEventType::order_cancel_requested);
  assert(events_out[3].type == sts::LedgerEventType::execution_applied);
  for (const auto& e : events_out) {
    assert(e.envelope.trace_id == "trace-k1");
    assert(e.envelope.config_version == "cfg-1");
    assert(e.envelope.rules_version == "rules-7");
  }
  // No-op dedups record nothing.
  assert(oms.on_execution_report({"SIM-k1", "k1", sts::OrderStatus::cancelled, 0, 0, "", ""}));
  assert(ledger.latest_event_id() == 4);
}

void restart_reproduces_state() {
  sts::EventBus events1;
  sts::SimulatedBroker broker1{events1};
  sts::RiskEngine risk1{{100, 10'000, true}};
  sts::InMemoryEventLedger ledger{};
  sts::OrderManager oms1{broker1, risk1, events1, &ledger};
  assert(oms1.submit(make_intent("k2")));  // -> live, recorded as event 1

  // Crash here: snapshot captures the live order at event 1.
  sts::LedgerSnapshot snapshot{};
  snapshot.last_event_id = ledger.latest_event_id();
  snapshot.open_orders = oms1.open_orders();
  assert(snapshot.open_orders.size() == 1);

  // Post-crash broker report arrives and is recorded on the old instance.
  assert(oms1.on_execution_report({"SIM-k2", "k2", sts::OrderStatus::partially_filled, 4, 100, "", "fill-1"}));
  const sts::ExecutionReport* original = oms1.find("k2");
  assert(original->status == sts::OrderStatus::partially_filled);

  // New process: restore from snapshot, replay only newer events.
  sts::EventBus events2;
  sts::SimulatedBroker broker2{events2};
  sts::RiskEngine risk2{{100, 10'000, true}};
  sts::OrderManager oms2{broker2, risk2, events2};
  assert(oms2.restore_order(make_intent("k2"), snapshot.open_orders[0]));
  assert(!oms2.restore_order(make_intent("k2"), snapshot.open_orders[0]));  // duplicate
  for (const auto& e : ledger.replay(snapshot.last_event_id + 1)) {
    if (e.type == sts::LedgerEventType::execution_applied) assert(oms2.on_execution_report(e.snapshot));
  }
  const sts::ExecutionReport* rebuilt = oms2.find("k2");
  assert(rebuilt != nullptr);
  assert(rebuilt->status == original->status);
  assert(rebuilt->cumulative_quantity == original->cumulative_quantity);
  assert(rebuilt->version == original->version);
  assert(rebuilt->order_id == original->order_id);
}

void reconcile_flags_diffs() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  sts::RiskEngine risk{{100, 10'000, true}};
  sts::OrderManager oms{broker, risk, events};
  assert(oms.submit(make_intent("k3")));  // live locally

  // Broker agrees: no diffs.
  assert(oms.reconcile_open_orders({{"SIM-k3", "k3", sts::OrderStatus::live, 0, 0, "", ""}}).empty());

  // Broker filled what OMS thinks is live.
  auto diffs = oms.reconcile_open_orders({{"SIM-k3", "k3", sts::OrderStatus::filled, 10, 100, "", "fill-9", 10, 0}});
  assert(diffs.size() == 1 && diffs[0].client_order_id == "k3");

  // Broker reports an order OMS never saw, and OMS-live k3 is missing there.
  diffs = oms.reconcile_open_orders({{"SIM-ghost", "ghost", sts::OrderStatus::live, 0, 0, "", ""}});
  assert(diffs.size() == 2);
}

}  // namespace

int main() {
  ledger_assigns_monotonic_ids();
  oms_records_every_transition();
  restart_reproduces_state();
  reconcile_flags_diffs();
  return 0;
}
