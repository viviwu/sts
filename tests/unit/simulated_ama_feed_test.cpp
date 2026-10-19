// clang-format off
/**
  ******************************************************************************
  * @file           : simulated_ama_feed_test.cpp
  * @author         : vivi wu
  * @brief          : Mock AMA feed unit tests: shape, gaps, faults, determinism.
  * @version        : 0.1.0
  * @date           : 03/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/broker/simulated_ama_feed.hpp"

#include <cassert>
#include <string>
#include <vector>

#include "qts/market/quote_feed.hpp"

namespace {

qts::SimulatedAmaFeed clean_feed() {
  std::vector<qts::SimulatedAmaSymbol> symbols;
  qts::SimulatedAmaSymbol sse{};
  sse.market_type = 101;
  sse.security_code = "600000";
  symbols.push_back(sse);
  qts::SimulatedAmaSymbol szse{};
  szse.market_type = 102;
  szse.security_code = "000001";
  symbols.push_back(szse);
  return qts::SimulatedAmaFeed{symbols, qts::SimulatedAmaConfig{}};
}

void snapshot_shape_matches_adapter_contract() {
  qts::SimulatedAmaFeed feed = clean_feed();
  const std::vector<qts::QuoteEvent> quotes = feed.step();
  assert(quotes.size() == 2);
  assert(quotes[0].instrument_id == "600000.SSE");
  assert(quotes[0].exchange == qts::Exchange::sse);
  assert(quotes[1].instrument_id == "000001.SZSE");
  assert(quotes[1].exchange == qts::Exchange::szse);
  for (const auto& quote : quotes) {
    assert(quote.bid_levels.size() == 10 && quote.ask_levels.size() == 10);
    assert(quote.bid_price < quote.ask_price);
    assert(quote.last_price >= 800 && quote.last_price <= 1200);
    assert(quote.limit_down == 800 && quote.limit_up == 1200);
    assert(quote.cumulative_volume > 0 && quote.cumulative_turnover > 0);
    assert(quote.quality == qts::QuoteQuality::valid);
    assert(quote.market_phase == qts::MarketPhase::continuous);
    assert(!quote.source_message_id.empty());
    assert(!qts::quote_holds_submissions(quote));
  }
  assert(feed.emitted() == 2 && feed.dropped() == 0);
  assert(feed.pending_gaps().empty());
}

void same_seed_replays_identically() {
  qts::SimulatedAmaConfig config{};
  config.seed = 7;
  std::vector<qts::SimulatedAmaSymbol> first_symbols;
  qts::SimulatedAmaSymbol first_sse{};
  first_sse.market_type = 101;
  first_sse.security_code = "600000";
  first_symbols.push_back(first_sse);
  std::vector<qts::SimulatedAmaSymbol> second_symbols = first_symbols;
  qts::SimulatedAmaFeed first{first_symbols, config};
  qts::SimulatedAmaFeed second{second_symbols, config};
  for (int step = 0; step < 20; ++step) {
    const auto left = first.step();
    const auto right = second.step();
    assert(left.size() == right.size() && left.size() == 1);
    assert(left[0].last_price == right[0].last_price);
    assert(left[0].bid_price == right[0].bid_price);
    assert(left[0].cumulative_volume == right[0].cumulative_volume);
  }
  qts::SimulatedAmaConfig other = config;
  other.seed = 8;
  qts::SimulatedAmaFeed third{first_symbols, other};
  bool differs = false;
  qts::SimulatedAmaFeed baseline{first_symbols, config};
  for (int step = 0; step < 20; ++step) {
    if (third.step()[0].last_price != baseline.step()[0].last_price) differs = true;
  }
  assert(differs);
}

void gap_every_reports_backfill_and_keeps_flowing() {
  std::vector<qts::SimulatedAmaSymbol> symbols;
  qts::SimulatedAmaSymbol sse{};
  sse.market_type = 101;
  sse.security_code = "600000";
  symbols.push_back(sse);
  qts::SimulatedAmaConfig config{};
  config.gap_every = 4;
  config.gap_size = 2;
  qts::SimulatedAmaFeed feed{symbols, config};
  for (int step = 0; step < 9; ++step) {
    const auto quotes = feed.step();
    assert(quotes.size() == 1);  // gaps never stop live consumption
  }
  const auto gaps = feed.pending_gaps();
  assert(gaps.size() == 2);
  assert(gaps[0].market_type == 101 && gaps[0].channel_no == 0);
  assert(gaps[0].begin_seq == 4 && gaps[0].end_seq == 5);
  assert(gaps[1].begin_seq == 10 && gaps[1].end_seq == 11);
  // Day boundary resets tick numbering: no false gap after clear.
  feed.settle_day();
  const auto restart = feed.step();
  assert(restart.size() == 1);
  assert(feed.pending_gaps().size() == 2);  // old gaps retained, no new one
}

void phase_windows_hold_submissions() {
  std::vector<qts::SimulatedAmaSymbol> symbols;
  qts::SimulatedAmaSymbol sse{};
  sse.market_type = 101;
  sse.security_code = "600000";
  symbols.push_back(sse);
  qts::SimulatedAmaConfig config{};
  config.auction_ticks = 2;
  config.halt_after = 4;
  config.halt_for = 2;
  qts::SimulatedAmaFeed feed{symbols, config};
  const auto first = feed.step();
  assert(first[0].market_phase == qts::MarketPhase::opening_auction);
  assert(qts::quote_holds_submissions(first[0]));
  const auto second = feed.step();
  assert(second[0].market_phase == qts::MarketPhase::opening_auction);
  const auto third = feed.step();
  assert(third[0].market_phase == qts::MarketPhase::continuous);
  assert(!qts::quote_holds_submissions(third[0]));
  const auto fourth = feed.step();
  const auto fifth = feed.step();
  assert(fourth[0].market_phase == qts::MarketPhase::halted);
  assert(fifth[0].market_phase == qts::MarketPhase::halted);
  assert(qts::quote_holds_submissions(fourth[0]));
  const auto sixth = feed.step();
  assert(sixth[0].market_phase == qts::MarketPhase::continuous);
}

void stale_crossed_corrupt_faults() {
  std::vector<qts::SimulatedAmaSymbol> symbols;
  qts::SimulatedAmaSymbol sse{};
  sse.market_type = 101;
  sse.security_code = "600000";
  symbols.push_back(sse);
  qts::SimulatedAmaConfig config{};
  config.stale_every = 5;
  config.crossed_every = 3;
  config.corrupt_every = 7;
  qts::SimulatedAmaFeed feed{symbols, config};
  bool saw_stale = false;
  bool saw_crossed = false;
  std::size_t quotes = 0;
  for (int step = 0; step < 21; ++step) {
    const auto out = feed.step();
    quotes += out.size();
    for (const auto& quote : out) {
      if (quote.quality == qts::QuoteQuality::stale) {
        saw_stale = true;
        assert(qts::quote_holds_submissions(quote));
      }
      if (quote.quality == qts::QuoteQuality::crossed) {
        saw_crossed = true;
        assert(quote.bid_price > quote.ask_price);
      }
    }
  }
  assert(saw_stale && saw_crossed);
  assert(feed.dropped() == 3);  // snapshots 7, 14, 21 corrupt-dropped
  assert(quotes == 18);
}

void quote_api_handler_and_subscription() {
  qts::SimulatedAmaFeed feed = clean_feed();
  std::size_t delivered = 0;
  feed.set_quote_handler([&](const qts::QuoteEvent&) { ++delivered; });
  assert(std::string{feed.channel()} == "simulated-ama");
  assert(feed.connected());
  feed.subscribe({"600000.SSE"});
  const auto only_sse = feed.step();
  assert(only_sse.size() == 1 && only_sse[0].instrument_id == "600000.SSE");
  assert(delivered == 1);
  feed.unsubscribe({"600000.SSE"});
  const auto both = feed.step();
  assert(both.size() == 2 && delivered == 3);
  feed.disconnect();
  assert(!feed.connected());
  assert(feed.step().empty());
  feed.reconnect();
  assert(feed.step().size() == 2);
}

}  // namespace

int main() {
  snapshot_shape_matches_adapter_contract();
  same_seed_replays_identically();
  gap_every_reports_backfill_and_keeps_flowing();
  phase_windows_hold_submissions();
  stale_crossed_corrupt_faults();
  quote_api_handler_and_subscription();
  return 0;
}
