// clang-format off
/**
  ******************************************************************************
  * @file           : e2e_paper_test.cpp
  * @author         : vivi wu
  * @brief          : End-to-end paper drill test: grid, risk, OMS, ledger and cash assertions.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

#include "qts/broker/simulated_broker.hpp"
#include "qts/execution/execution_api.hpp"
#include "qts/market/instrument.hpp"
#include "qts/oms/order_manager.hpp"
#include "qts/persistence/ledger.hpp"
#include "qts/strategy/grid_strategy.hpp"

namespace {

qts::GridConfig grid_config() {
  qts::GridConfig config{};
  config.strategy_id = "grid-1";
  config.account_id = "paper";
  config.instrument_id = "TEST";
  config.exchange = qts::Exchange::unknown;
  config.lower = 900;
  config.upper = 1'100;
  config.grids = 4;
  config.quantity_per_order = 100;
  return config;
}

qts::QuoteEvent make_quote(qts::Price last, std::uint64_t sequence) {
  qts::QuoteEvent quote{};
  quote.instrument_id = "TEST";
  quote.last_price = last;
  quote.sequence = sequence;
  quote.receive_time = qts::Timestamp{};
  quote.quality = qts::QuoteQuality::valid;
  return quote;
}

void bad_config_throws() {
  bool threw = false;
  try {
    qts::GridConfig bad = grid_config();
    bad.grids = 0;
    qts::GridStrategy strategy{bad, 1'000};
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void foreign_quotes_ignored() {
  qts::GridStrategy strategy{grid_config(), 1'000};
  const qts::Portfolio book{0};
  auto other = make_quote(500, 1);
  other.instrument_id = "OTHER";
  assert(strategy.on_quote(other, book).empty());
  assert(strategy.on_quote(make_quote(0, 2), book).empty());
  assert(strategy.levels().size() == 5);
}

void grid_drill_end_to_end() {
  qts::EventBus events;
  qts::SimulatedBroker broker{events};
  broker.set_starting_cash(1'000'000);
  qts::RiskEngine risk{{10'000, 0, true}};
  qts::InMemoryEventLedger ledger{};
  qts::OrderManager oms{broker, risk, events, &ledger};
  qts::GridStrategy strategy{grid_config(), 1'000};
  qts::ImmediateAlgorithm execution{};

  // Scripted path: 1000 -> 950 -> 900 -> 950 -> 1000 -> 1050 -> 1100.
  const std::vector<qts::Price> path{1'000, 950, 900, 950, 1'000, 1'050, 1'100};
  int submitted = 0;
  std::uint64_t sequence = 0;
  for (const qts::Price price : path) {
    broker.set_reference_price("TEST", price);
    auto quote = make_quote(price, ++sequence);
    execution.on_quote(quote);  // no-op except for volume-tracking algorithms
    for (auto intent : strategy.on_quote(quote, broker.book())) {
      execution.add_parent(intent);
    }
    for (auto child : execution.poll(qts::Timestamp{})) {
      assert(oms.submit(child));
      ++submitted;
    }
  }
  assert(submitted == 6);                  // 2 grid buys + 4 take-profit sells
  assert(ledger.latest_event_id() == 18);  // submit + ack + fill per order

  // Everything filled: no open orders on either side, reconcile is clean.
  assert(oms.open_orders().empty());
  broker.request_open_orders("paper");
  assert(oms.reconcile_open_orders({}).empty());

  bool called = false;
  broker.set_account_snapshot_handler([&](const qts::TradeAccountSnapshot& snapshot) {
    called = true;
    // Buys 100@950 + 100@900, sells 100@(950+1000+1050+1100).
    assert(snapshot.available_cash == 1'225'000);
    assert(snapshot.positions.size() == 1);
    assert(snapshot.positions[0].total_quantity == -200);
  });
  broker.request_account_snapshot("paper");
  assert(called);

  for (const auto& event : ledger.replay(1)) {
    assert(!event.envelope.trace_id.empty());  // every event traceable
  }
}

}  // namespace

int main() {
  bad_config_throws();
  foreign_quotes_ignored();
  grid_drill_end_to_end();
  return 0;
}
