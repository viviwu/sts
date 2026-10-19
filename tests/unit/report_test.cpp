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
#include "sts/research/report.hpp"

#include <cassert>
#include <string>
#include <vector>

#include "sts/broker/simulated_broker.hpp"
#include "sts/oms/order_manager.hpp"
#include "sts/strategy/grid_strategy.hpp"

namespace {

sts::LedgerEvent make_applied(const std::string& client, sts::OrderStatus status, sts::Quantity last, sts::Price price, const std::string& exec, sts::Quantity cumulative) {
  sts::LedgerEvent event{};
  event.type = sts::LedgerEventType::execution_applied;
  event.intent = sts::OrderIntent{client, "paper", "test", "TEST", sts::Side::buy, sts::OrderType::limit, sts::TimeInForce::day, 10, 100};
  event.snapshot = sts::ExecutionReport{"B-" + client, client, status, last, price, "", exec, cumulative, 0};
  event.envelope.config_version = "c";
  event.envelope.rules_version = "r";
  return event;
}

void partials_count_once_and_rejects_histogram() {
  std::vector<sts::LedgerEvent> events;
  events.push_back(make_applied("h1", sts::OrderStatus::partially_filled, 4, 100, "e1", 4));
  events.push_back(make_applied("h1", sts::OrderStatus::filled, 6, 100, "e2", 10));

  sts::LedgerEvent rejected{};
  rejected.type = sts::LedgerEventType::risk_rejected;
  rejected.snapshot = sts::ExecutionReport{"", "h2", sts::OrderStatus::rejected, 0, 0, "trading is disabled", "", 0, 0};
  rejected.envelope.config_version = "c";
  rejected.envelope.rules_version = "r";
  events.push_back(rejected);

  const sts::RunReport report = sts::summarize_ledger(events, "c", "r");
  assert(report.fills == 2);
  assert(report.filled == 1);  // one order despite two fill reports
  assert(report.bought_quantity == 10 && report.gross_buy == 1'000);
  assert(report.rejected == 1);
  assert(report.rejection_reasons.at("trading is disabled") == 1);
  assert(report.versions_match);

  auto tampered = events;
  tampered[0].envelope.rules_version = "r2";  // stale ruleset rerun
  assert(!sts::summarize_ledger(tampered, "c", "r").versions_match);
}

void grid_drill_goldens() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  broker.set_starting_cash(1'000'000);
  sts::RiskEngine risk{{10'000, 0, true}};
  sts::InMemoryEventLedger ledger{};
  sts::OrderManager oms{broker, risk, events, &ledger};
  oms.set_ledger_versions("1", "1");

  sts::GridConfig grid{};
  grid.strategy_id = "grid-1";
  grid.account_id = "paper";
  grid.instrument_id = "TEST";
  grid.lower = 900;
  grid.upper = 1'100;
  grid.grids = 4;
  grid.quantity_per_order = 100;
  sts::GridStrategy strategy{grid, 1'000};

  const std::vector<sts::Price> path{1'000, 950, 900, 950, 1'000, 1'050, 1'100};
  std::uint64_t sequence = 0;
  for (const sts::Price price : path) {
    broker.set_reference_price("TEST", price);
    sts::QuoteEvent quote{};
    quote.instrument_id = "TEST";
    quote.last_price = price;
    quote.sequence = ++sequence;
    for (auto intent : strategy.on_quote(quote, broker.book())) assert(oms.submit(intent));
  }

  const sts::RunReport report = sts::summarize_ledger(ledger.replay(1), "1", "1");
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
