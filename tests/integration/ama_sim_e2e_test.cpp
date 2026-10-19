// clang-format off
/**
  ******************************************************************************
  * @file           : ama_sim_e2e_test.cpp
  * @author         : vivi wu
  * @brief          : AMA mock end-to-end: snapshots drive strategy to settlement.
  * @version        : 0.1.0
  * @date           : 03/10/26
  ******************************************************************************
  */
// clang-format on
#include <cassert>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "qts/broker/simulated_ama_feed.hpp"
#include "qts/broker/simulated_broker.hpp"
#include "qts/execution/execution_api.hpp"
#include "qts/market/quote_feed.hpp"
#include "qts/oms/order_manager.hpp"
#include "qts/persistence/ledger.hpp"
#include "qts/research/report.hpp"
#include "qts/strategy/grid_strategy.hpp"

namespace {

qts::GridConfig grid_for(const std::string& strategy, const std::string& instrument, qts::Exchange exchange) {
  qts::GridConfig config{};
  config.strategy_id = strategy;
  config.account_id = "paper";
  config.instrument_id = instrument;
  config.exchange = exchange;
  config.board = qts::AshareBoard::main;
  config.security_account_id = "A123";
  config.lower = 900;
  config.upper = 1'100;
  config.grids = 4;
  config.quantity_per_order = 100;
  config.id_prefix = strategy + "-";  // distinct per strategy: children never collide across grids
  return config;
}

qts::AshareInstrumentRules rules_for(const std::string& instrument, qts::Exchange exchange) {
  return qts::AshareInstrumentRules{instrument, exchange, qts::AshareBoard::main, 1, 100, 100, 1'000'000, true, true, 800, 1'200};
}

void ama_snapshots_drive_the_full_chain() {
  std::vector<qts::SimulatedAmaSymbol> symbols;
  qts::SimulatedAmaSymbol sse{};
  sse.market_type = 101;
  sse.security_code = "600000";
  symbols.push_back(sse);
  qts::SimulatedAmaSymbol szse{};
  szse.market_type = 102;
  szse.security_code = "000001";
  symbols.push_back(szse);
  qts::SimulatedAmaConfig feed_config{};
  feed_config.seed = 42;
  feed_config.auction_ticks = 2;
  feed_config.halt_after = 8;
  feed_config.halt_for = 2;
  feed_config.gap_every = 6;
  feed_config.gap_size = 2;
  qts::SimulatedAmaFeed feed{symbols, feed_config};

  qts::EventBus events;
  qts::SimulatedBroker broker{events};
  broker.set_starting_cash(10'000'000);
  qts::RiskEngine risk{{10'000, 0, true}};
  risk.set_ashare_master({{"600000.SSE", rules_for("600000.SSE", qts::Exchange::sse)}, {"000001.SZSE", rules_for("000001.SZSE", qts::Exchange::szse)}});
  risk.set_instrument_allowlist({"600000.SSE", "000001.SZSE"});
  risk.set_price_band("600000.SSE", {800, 1'200});
  risk.set_price_band("000001.SZSE", {800, 1'200});
  qts::InMemoryEventLedger ledger{};
  qts::OrderManager oms{broker, risk, events, &ledger};
  qts::GridStrategy grid_sse{grid_for("grid-sse", "600000.SSE", qts::Exchange::sse), 1'000};
  qts::GridStrategy grid_szse{grid_for("grid-szse", "000001.SZSE", qts::Exchange::szse), 1'000};
  qts::ImmediateAlgorithm execution{};

  int submitted = 0;
  int held = 0;
  std::unordered_set<qts::InstrumentId> known;
  constexpr int kSteps = 30;
  for (int step = 0; step < kSteps; ++step) {
    const std::vector<qts::QuoteEvent> quotes = feed.step();
    assert(!quotes.empty());
    for (const auto& quote : quotes) {
      broker.set_reference_price(quote.instrument_id, quote.last_price);
      // AMA-shaped quotes obey the same hold contract as the gateway.
      if (qts::quote_holds_submissions(quote)) {
        ++held;
        continue;
      }
      qts::GridStrategy& strategy = (quote.instrument_id == "600000.SSE") ? grid_sse : grid_szse;
      for (const auto& intent : strategy.on_quote(quote, broker.book())) {
        execution.add_parent(intent);
      }
      for (const auto& child : execution.poll(qts::Timestamp{})) {
        assert(oms.submit(child));
        ++submitted;
      }
    }
    for (const auto& entry : broker.book().all_positions()) known.insert(entry.first);
    for (const auto& instrument : known) {
      const qts::PositionView view = broker.book().position(instrument);
      risk.set_position(instrument, view.total);
      for (const auto& basket : {"grid-sse", "grid-szse"}) {
        risk.set_sellable(basket, instrument, broker.book().position_for(basket, instrument).sellable);
      }
    }
  }

  // Auction (2x2) + halt (2x2) quotes hold; gaps never stop live consumption.
  assert(held >= 8);
  assert(submitted > 0);
  assert(feed.emitted() == static_cast<std::size_t>(kSteps * 2));
  assert(!feed.pending_gaps().empty());
  const qts::AmaTickGap first_gap = feed.pending_gaps().front();
  assert(first_gap.market_type == 101 && first_gap.channel_no == 0);
  assert(first_gap.begin_seq == 6 && first_gap.end_seq == 7);

  // Reference-price fills complete synchronously: nothing left working.
  assert(oms.open_orders().empty());
  assert(oms.reconcile_open_orders({}).empty());

  const auto stream = ledger.replay(1);
  assert(!stream.empty());
  for (const auto& event : stream) {
    assert(!event.envelope.trace_id.empty());
  }
  const qts::RunReport report = qts::summarize_ledger(stream, "", "");
  assert(report.fills > 0);
  assert(report.filled > 0);
}

}  // namespace

int main() {
  ama_snapshots_drive_the_full_chain();
  return 0;
}
