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

#include "sts/broker/simulated_broker.hpp"
#include "sts/execution/execution_api.hpp"
#include "sts/market/instrument.hpp"
#include "sts/oms/order_manager.hpp"
#include "sts/persistence/ledger.hpp"
#include "sts/strategy/grid_strategy.hpp"

namespace {

sts::GridConfig grid_config() {
  sts::GridConfig config{};
  config.strategy_id = "grid-1";
  config.account_id = "paper";
  config.instrument_id = "TEST";
  config.exchange = sts::Exchange::unknown;
  config.lower = 900;
  config.upper = 1'100;
  config.grids = 4;
  config.quantity_per_order = 100;
  return config;
}

sts::QuoteEvent make_quote(sts::Price last, std::uint64_t sequence) {
  sts::QuoteEvent quote{};
  quote.instrument_id = "TEST";
  quote.last_price = last;
  quote.sequence = sequence;
  quote.receive_time = sts::Timestamp{};
  quote.quality = sts::QuoteQuality::valid;
  return quote;
}

void bad_config_throws() {
  bool threw = false;
  try {
    sts::GridConfig bad = grid_config();
    bad.grids = 0;
    sts::GridStrategy strategy{bad, 1'000};
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void foreign_quotes_ignored() {
  sts::GridStrategy strategy{grid_config(), 1'000};
  const sts::Portfolio book{0};
  auto other = make_quote(500, 1);
  other.instrument_id = "OTHER";
  assert(strategy.on_quote(other, book).empty());
  assert(strategy.on_quote(make_quote(0, 2), book).empty());
  assert(strategy.levels().size() == 5);
}

void grid_drill_end_to_end() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  broker.set_starting_cash(1'000'000);
  sts::RiskEngine risk{{10'000, 0, true}};
  sts::InMemoryEventLedger ledger{};
  sts::OrderManager oms{broker, risk, events, &ledger};
  sts::GridStrategy strategy{grid_config(), 1'000};
  sts::ImmediateAlgorithm execution{};

  // Scripted path: 1000 -> 950 -> 900 -> 950 -> 1000 -> 1050 -> 1100.
  const std::vector<sts::Price> path{1'000, 950, 900, 950, 1'000, 1'050, 1'100};
  int submitted = 0;
  std::uint64_t sequence = 0;
  for (const sts::Price price : path) {
    broker.set_reference_price("TEST", price);
    auto quote = make_quote(price, ++sequence);
    execution.on_quote(quote);  // no-op except for volume-tracking algorithms
    for (auto intent : strategy.on_quote(quote, broker.book())) {
      execution.add_parent(intent);
    }
    for (auto child : execution.poll(sts::Timestamp{})) {
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
  broker.set_account_snapshot_handler([&](const sts::TradeAccountSnapshot& snapshot) {
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
