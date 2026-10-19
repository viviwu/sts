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

#include "qts/broker/simulated_broker.hpp"
#include "qts/oms/order_manager.hpp"
#include "qts/persistence/ledger.hpp"

namespace {

qts::OrderIntent make_intent(const std::string& id, qts::Quantity qty = 10) {
  qts::OrderIntent intent{id, "paper", "test", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, qty, 100};
  intent.trace_id = "trace-" + id;
  return intent;
}

void ledger_assigns_monotonic_ids() {
  qts::InMemoryEventLedger ledger{};
  assert(ledger.latest_event_id() == 0);
  assert(ledger.replay(1).empty());

  qts::LedgerEvent event{};
  event.snapshot.client_order_id = "a";
  assert(ledger.append(event) == 1);
  assert(ledger.append(event) == 2);
  assert(ledger.latest_event_id() == 2);
  assert(ledger.replay(1).size() == 2);
  assert(ledger.replay(2).size() == 1);
  assert(ledger.replay(3).empty());
}

void oms_records_every_transition() {
  qts::EventBus events;
  qts::SimulatedBroker broker{events};  // no reference price: orders stay live
  qts::RiskEngine risk{{100, 10'000, true}};
  qts::InMemoryEventLedger ledger{};
  qts::OrderManager oms{broker, risk, events, &ledger};
  oms.set_ledger_versions("cfg-1", "rules-7");

  assert(oms.submit(make_intent("k1")));
  assert(oms.cancel("k1"));
  // submit + live ack + cancel-request + broker cancel report = 4 events.
  assert(ledger.latest_event_id() == 4);
  const auto events_out = ledger.replay(1);
  assert(events_out.size() == 4);
  assert(events_out[0].type == qts::LedgerEventType::order_submitted);
  assert(events_out[1].type == qts::LedgerEventType::execution_applied);
  assert(events_out[2].type == qts::LedgerEventType::order_cancel_requested);
  assert(events_out[3].type == qts::LedgerEventType::execution_applied);
  for (const auto& e : events_out) {
    assert(e.envelope.trace_id == "trace-k1");
    assert(e.envelope.config_version == "cfg-1");
    assert(e.envelope.rules_version == "rules-7");
  }
  // No-op dedups record nothing.
  assert(oms.on_execution_report({"SIM-k1", "k1", qts::OrderStatus::cancelled, 0, 0, "", ""}));
  assert(ledger.latest_event_id() == 4);
}

void restart_reproduces_state() {
  qts::EventBus events1;
  qts::SimulatedBroker broker1{events1};
  qts::RiskEngine risk1{{100, 10'000, true}};
  qts::InMemoryEventLedger ledger{};
  qts::OrderManager oms1{broker1, risk1, events1, &ledger};
  assert(oms1.submit(make_intent("k2")));  // -> live, recorded as event 1

  // Crash here: snapshot captures the live order at event 1.
  qts::LedgerSnapshot snapshot{};
  snapshot.last_event_id = ledger.latest_event_id();
  snapshot.open_orders = oms1.open_orders();
  assert(snapshot.open_orders.size() == 1);

  // Post-crash broker report arrives and is recorded on the old instance.
  assert(oms1.on_execution_report({"SIM-k2", "k2", qts::OrderStatus::partially_filled, 4, 100, "", "fill-1"}));
  const qts::ExecutionReport* original = oms1.find("k2");
  assert(original->status == qts::OrderStatus::partially_filled);

  // New process: restore from snapshot, replay only newer events.
  qts::EventBus events2;
  qts::SimulatedBroker broker2{events2};
  qts::RiskEngine risk2{{100, 10'000, true}};
  qts::OrderManager oms2{broker2, risk2, events2};
  assert(oms2.restore_order(make_intent("k2"), snapshot.open_orders[0]));
  assert(!oms2.restore_order(make_intent("k2"), snapshot.open_orders[0]));  // duplicate
  for (const auto& e : ledger.replay(snapshot.last_event_id + 1)) {
    if (e.type == qts::LedgerEventType::execution_applied) assert(oms2.on_execution_report(e.snapshot));
  }
  const qts::ExecutionReport* rebuilt = oms2.find("k2");
  assert(rebuilt != nullptr);
  assert(rebuilt->status == original->status);
  assert(rebuilt->cumulative_quantity == original->cumulative_quantity);
  assert(rebuilt->version == original->version);
  assert(rebuilt->order_id == original->order_id);
}

void reconcile_flags_diffs() {
  qts::EventBus events;
  qts::SimulatedBroker broker{events};
  qts::RiskEngine risk{{100, 10'000, true}};
  qts::OrderManager oms{broker, risk, events};
  assert(oms.submit(make_intent("k3")));  // live locally

  // Broker agrees: no diffs.
  assert(oms.reconcile_open_orders({{"SIM-k3", "k3", qts::OrderStatus::live, 0, 0, "", ""}}).empty());

  // Broker filled what OMS thinks is live.
  auto diffs = oms.reconcile_open_orders({{"SIM-k3", "k3", qts::OrderStatus::filled, 10, 100, "", "fill-9", 10, 0}});
  assert(diffs.size() == 1 && diffs[0].client_order_id == "k3");

  // Broker reports an order OMS never saw, and OMS-live k3 is missing there.
  diffs = oms.reconcile_open_orders({{"SIM-ghost", "ghost", qts::OrderStatus::live, 0, 0, "", ""}});
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
