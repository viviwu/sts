// clang-format off
/**
  ******************************************************************************
  * @file           : ledger_verify_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for offline verification (legal, tampered, truncated streams).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/research/ledger_verify.hpp"

#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

#include "qts/broker/simulated_broker.hpp"
#include "qts/oms/order_manager.hpp"
#include "qts/persistence/ledger.hpp"

namespace {

qts::OrderIntent make_intent(const std::string& id, qts::Quantity qty = 100, qts::Price price = 1000) {
  qts::OrderIntent intent{id, "paper", "test", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, qty, price};
  intent.trace_id = "trace-" + id;
  return intent;
}

void empty_ledger_verifies() {
  const qts::replay::VerifyResult result = qts::replay::verify_ledger_replay({});
  assert(result.ok && result.error.empty());
  assert(result.events == 0 && result.orders == 0 && result.open == 0);
}

std::vector<qts::LedgerEvent> filled_order_events() {
  qts::EventBus events;
  qts::SimulatedBroker broker{events};
  broker.set_starting_cash(1'000'000);
  broker.set_reference_price("TEST", 1000);
  qts::RiskEngine risk{{1000, 10'000'000, true}};
  qts::InMemoryEventLedger ledger{};
  qts::OrderManager oms{broker, risk, events, &ledger};
  assert(oms.submit(make_intent("v1")));
  assert(oms.submit(make_intent("v2")));
  const auto stored = ledger.replay(1);
  assert(stored.size() == 6);  // submit + live ack + fill per order
  assert(oms.open_orders().empty());
  return stored;
}

void filled_orders_verify() {
  const auto stored = filled_order_events();
  const qts::replay::VerifyResult result = qts::replay::verify_ledger_replay(stored);
  assert(result.ok && result.error.empty());
  assert(result.events == 6 && result.orders == 2 && result.open == 0);
}

void cancel_flow_verifies() {
  qts::EventBus events;
  qts::SimulatedBroker broker{events};  // no reference price: order stays live
  qts::RiskEngine risk{{1000, 10'000'000, true}};
  qts::InMemoryEventLedger ledger{};
  qts::OrderManager oms{broker, risk, events, &ledger};
  assert(oms.submit(make_intent("c1")));
  assert(oms.cancel("c1"));
  const qts::replay::VerifyResult result = qts::replay::verify_ledger_replay(ledger.replay(1));
  assert(result.ok && result.error.empty());
  assert(result.orders == 1 && result.open == 0);
}

void illegal_transition_fails() {
  auto stored = filled_order_events();
  // Forge a live report after the terminal fill: filled -> live is illegal.
  qts::LedgerEvent forged{};
  forged.type = qts::LedgerEventType::execution_applied;
  forged.intent = stored[0].intent;
  forged.snapshot = stored.back().snapshot;
  forged.snapshot.status = qts::OrderStatus::live;
  forged.snapshot.last_quantity = 0;
  forged.snapshot.last_price = 0;
  forged.snapshot.execution_id = "forged-1";  // fresh id: must face the transition check, not the dedup set
  forged.envelope.event_id = stored.back().envelope.event_id + 1;
  stored.push_back(forged);
  const qts::replay::VerifyResult result = qts::replay::verify_ledger_replay(stored);
  assert(!result.ok && !result.error.empty());
}

void truncated_stream_fails() {
  auto stored = filled_order_events();
  // Drop the order_submitted events: reports arrive for unknown orders.
  std::vector<qts::LedgerEvent> truncated;
  for (const auto& e : stored) {
    if (e.type != qts::LedgerEventType::order_submitted) truncated.push_back(e);
  }
  assert(truncated.size() == 4);
  const qts::replay::VerifyResult result = qts::replay::verify_ledger_replay(truncated);
  assert(!result.ok && !result.error.empty());
}

void duplicate_submit_fails() {
  auto stored = filled_order_events();
  // Replay the first submit twice: the second restore must refuse.
  std::vector<qts::LedgerEvent> doubled;
  doubled.push_back(stored[0]);
  for (const auto& e : stored) doubled.push_back(e);
  for (std::size_t i = 0; i < doubled.size(); ++i) doubled[i].envelope.event_id = i + 1;
  const qts::replay::VerifyResult result = qts::replay::verify_ledger_replay(doubled);
  assert(!result.ok && !result.error.empty());
}

// M11-7: funding legs verify cleanly, count toward neither orders nor
// opens, and keep strict id ordering enforced around them.
void funding_verifies_without_touching_orders() {
  auto stored = filled_order_events();
  qts::LedgerEvent cash{};
  cash.type = qts::LedgerEventType::funding_deposited;
  cash.envelope.trace_id = "funding";
  cash.funding.cash = 10'000'000;
  qts::LedgerEvent deposit{};
  deposit.type = qts::LedgerEventType::funding_deposited;
  deposit.envelope.trace_id = "funding";
  deposit.funding.instrument = "TEST";
  deposit.funding.quantity = 10'000;
  deposit.funding.average_cost = 1'000;
  std::vector<qts::LedgerEvent> funded;
  funded.push_back(cash);
  funded.push_back(deposit);
  for (const auto& e : stored) funded.push_back(e);
  for (std::size_t i = 0; i < funded.size(); ++i) funded[i].envelope.event_id = i + 1;
  const qts::replay::VerifyResult result = qts::replay::verify_ledger_replay(funded);
  assert(result.ok && result.error.empty());
  assert(result.events == 8 && result.orders == 2 && result.open == 0);
}

}  // namespace

int main() {
  empty_ledger_verifies();
  filled_orders_verify();
  cancel_flow_verifies();
  illegal_transition_fails();
  truncated_stream_fails();
  duplicate_submit_fails();
  funding_verifies_without_touching_orders();
  return 0;
}
