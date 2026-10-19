// clang-format off
/**
  ******************************************************************************
  * @file           : report_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for the run-report pure function and version checks.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/research/report.hpp"

#include <cassert>
#include <string>
#include <vector>

#include "qts/broker/simulated_broker.hpp"
#include "qts/oms/order_manager.hpp"
#include "qts/strategy/grid_strategy.hpp"

namespace {

qts::LedgerEvent make_applied(const std::string& client, qts::OrderStatus status, qts::Quantity last, qts::Price price, const std::string& exec, qts::Quantity cumulative) {
  qts::LedgerEvent event{};
  event.type = qts::LedgerEventType::execution_applied;
  event.intent = qts::OrderIntent{client, "paper", "test", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, 10, 100};
  event.snapshot = qts::ExecutionReport{"B-" + client, client, status, last, price, "", exec, cumulative, 0};
  event.envelope.config_version = "c";
  event.envelope.rules_version = "r";
  return event;
}

void partials_count_once_and_rejects_histogram() {
  std::vector<qts::LedgerEvent> events;
  events.push_back(make_applied("h1", qts::OrderStatus::partially_filled, 4, 100, "e1", 4));
  events.push_back(make_applied("h1", qts::OrderStatus::filled, 6, 100, "e2", 10));

  qts::LedgerEvent rejected{};
  rejected.type = qts::LedgerEventType::risk_rejected;
  rejected.snapshot = qts::ExecutionReport{"", "h2", qts::OrderStatus::rejected, 0, 0, "trading is disabled", "", 0, 0};
  rejected.envelope.config_version = "c";
  rejected.envelope.rules_version = "r";
  events.push_back(rejected);

  const qts::RunReport report = qts::summarize_ledger(events, "c", "r");
  assert(report.fills == 2);
  assert(report.filled == 1);  // one order despite two fill reports
  assert(report.bought_quantity == 10 && report.gross_buy == 1'000);
  assert(report.rejected == 1);
  assert(report.rejection_reasons.at("trading is disabled") == 1);
  assert(report.versions_match);

  auto tampered = events;
  tampered[0].envelope.rules_version = "r2";  // stale ruleset rerun
  assert(!qts::summarize_ledger(tampered, "c", "r").versions_match);
}

void grid_drill_goldens() {
  qts::EventBus events;
  qts::SimulatedBroker broker{events};
  broker.set_starting_cash(1'000'000);
  qts::RiskEngine risk{{10'000, 0, true}};
  qts::InMemoryEventLedger ledger{};
  qts::OrderManager oms{broker, risk, events, &ledger};
  oms.set_ledger_versions("1", "1");

  qts::GridConfig grid{};
  grid.strategy_id = "grid-1";
  grid.account_id = "paper";
  grid.instrument_id = "TEST";
  grid.lower = 900;
  grid.upper = 1'100;
  grid.grids = 4;
  grid.quantity_per_order = 100;
  qts::GridStrategy strategy{grid, 1'000};

  const std::vector<qts::Price> path{1'000, 950, 900, 950, 1'000, 1'050, 1'100};
  std::uint64_t sequence = 0;
  for (const qts::Price price : path) {
    broker.set_reference_price("TEST", price);
    qts::QuoteEvent quote{};
    quote.instrument_id = "TEST";
    quote.last_price = price;
    quote.sequence = ++sequence;
    for (auto intent : strategy.on_quote(quote, broker.book())) assert(oms.submit(intent));
  }

  const qts::RunReport report = qts::summarize_ledger(ledger.replay(1), "1", "1");
  assert(report.submitted == 6);
  assert(report.filled == 6 && report.cancelled == 0 && report.rejected == 0);
  assert(report.fills == 6);
  assert(report.bought_quantity == 200 && report.sold_quantity == 400);
  assert(report.gross_buy == 185'000 && report.gross_sell == 410'000);
  assert(report.gross_pnl == 225'000);
  assert(report.total_fees == 0 && report.net_pnl == 225'000);  // no fee schedule: net equals gross
  assert(report.max_drawdown == 185'000);
  assert(report.versions_match);
}

}  // namespace

int main() {
  partials_count_once_and_rejects_histogram();
  grid_drill_goldens();
  return 0;
}
