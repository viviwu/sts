// clang-format off
/**
  ******************************************************************************
  * @file           : quote_feed_test.cpp
  * @author         : vivi wu
  * @brief          : Simulated L1 feed realism, fault injection and hold gate.
  * @version        : 0.1.0
  * @date           : 03/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/market/quote_feed.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

void l1_fields_are_populated() {
  qts::SimulatedQuoteFeed feed{42, 1'000, 50, 800, 1'200};
  const qts::QuoteEvent first = feed.next("TEST", 1);
  assert(first.last_price >= 800 && first.last_price <= 1'200);
  assert(first.bid_price < first.last_price && first.ask_price > first.last_price);
  assert(first.bid_quantity == 100 && first.ask_quantity == 100);
  assert(first.bid_levels.size() == 1 && first.ask_levels.size() == 1);
  assert(first.cumulative_volume == 100);
  assert(first.cumulative_turnover == 100 * first.last_price);
  assert(first.quality == qts::QuoteQuality::valid);
  assert(first.market_phase == qts::MarketPhase::continuous);
  assert(!qts::quote_holds_submissions(first));

  const qts::QuoteEvent second = feed.next("TEST", 2);
  assert(second.cumulative_volume == 200);
  assert(second.cumulative_turnover == 200 * second.last_price || second.last_price != first.last_price);
}

void multi_symbol_state_is_independent() {
  qts::SimulatedQuoteFeed feed{7, 1'000, 50, 800, 1'200};
  const qts::QuoteEvent a1 = feed.next("AAA", 1);
  const qts::QuoteEvent b1 = feed.next("BBB", 2);
  assert(a1.cumulative_volume == 100 && b1.cumulative_volume == 100);
  const qts::QuoteEvent a2 = feed.next("AAA", 3);
  assert(a2.cumulative_volume == 200);
  assert(feed.current_for("AAA") == a2.last_price);
  assert(feed.current_for("BBB") == b1.last_price);
}

void crossed_and_stale_hold_submissions() {
  qts::SimulatedQuoteFeed feed{1, 1'000, 50, 800, 1'200};
  qts::SimulatedQuoteFeed::Faults faults{};
  faults.crossed_every = 3;
  faults.stale_every = 5;
  feed.set_faults(faults);
  bool saw_crossed = false;
  bool saw_stale = false;
  for (std::uint64_t i = 1; i <= 15; ++i) {
    const qts::QuoteEvent quote = feed.next("TEST", i);
    if (quote.quality == qts::QuoteQuality::crossed) {
      saw_crossed = true;
      assert(quote.bid_price > quote.ask_price);
      assert(qts::quote_holds_submissions(quote));
    }
    if (quote.quality == qts::QuoteQuality::stale) {
      saw_stale = true;
      assert(qts::quote_holds_submissions(quote));
    }
  }
  assert(saw_crossed && saw_stale);
}

void halt_and_auction_hold_submissions() {
  qts::SimulatedQuoteFeed feed{9, 1'000, 50, 800, 1'200};
  qts::SimulatedQuoteFeed::Faults faults{};
  faults.auction_ticks = 2;
  faults.halt_after = 4;
  faults.halt_for = 2;
  feed.set_faults(faults);
  const qts::QuoteEvent t1 = feed.next("TEST", 1);
  const qts::QuoteEvent t2 = feed.next("TEST", 2);
  assert(t1.market_phase == qts::MarketPhase::opening_auction);
  assert(t2.market_phase == qts::MarketPhase::opening_auction);
  assert(qts::quote_holds_submissions(t1));
  const qts::QuoteEvent t3 = feed.next("TEST", 3);
  assert(t3.market_phase == qts::MarketPhase::continuous);
  assert(!qts::quote_holds_submissions(t3));
  const qts::QuoteEvent t4 = feed.next("TEST", 4);
  const qts::QuoteEvent t5 = feed.next("TEST", 5);
  assert(t4.market_phase == qts::MarketPhase::halted);
  assert(t5.market_phase == qts::MarketPhase::halted);
  assert(qts::quote_holds_submissions(t4));
  const qts::QuoteEvent t6 = feed.next("TEST", 6);
  assert(t6.market_phase == qts::MarketPhase::continuous);
  assert(!qts::quote_holds_submissions(t6));
}

void legacy_path_stays_clean() {
  // No faults configured: every quote tradeable, drill counts unaffected.
  qts::SimulatedQuoteFeed feed{42, 1'000, 50, 800, 1'200};
  for (std::uint64_t i = 1; i <= 7; ++i) {
    const qts::QuoteEvent quote = feed.next("TEST", i);
    assert(!qts::quote_holds_submissions(quote));
  }
}

std::string write_quotes_file(const std::string& tag, const std::string& body) {
  static unsigned counter = 0;
  const auto path = std::filesystem::temp_directory_path() / ("qts-quotes-" + tag + "-" + std::to_string(counter++) + ".csv");
  std::ofstream out{path};
  assert(static_cast<bool>(out));
  out << body;
  out.close();
  return path.string();
}

void file_feed_replays_rows_in_order() {
  const std::string path = write_quotes_file("order", "# comment\n\nTEST,1000,950,1050,100,100\nTEST,1050,1000,1100,200,200\n");
  qts::FileQuoteFeed feed{path};
  assert(feed.size() == 2);
  assert(!feed.exhausted());
  const auto first = feed.next();
  assert(first.has_value());
  assert(first->instrument_id == "TEST" && first->last_price == 1'000 && first->sequence == 1);
  assert(first->bid_levels.size() == 1 && first->ask_levels.size() == 1);
  assert(!qts::quote_holds_submissions(*first));
  const auto second = feed.next();
  assert(second.has_value() && second->last_price == 1'050 && second->sequence == 2);
  assert(second->cumulative_volume == 400);
  assert(!feed.next().has_value());
  assert(feed.exhausted());
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

void file_feed_rejects_loudly() {
  bool missing = false;
  try {
    qts::FileQuoteFeed feed{"/nonexistent/qts-quotes-missing.csv"};
  } catch (const std::runtime_error&) {
    missing = true;
  }
  assert(missing);
  for (const auto& body : {"TEST,1000,950\n", "TEST,1000,1050,950,100,100\n", "TEST,abc,950,1050,100,100\n", "TEST,1000,950,1050,0,100\n"}) {
    const std::string path = write_quotes_file("bad", body);
    bool bad = false;
    try {
      qts::FileQuoteFeed feed{path};
    } catch (const std::runtime_error&) {
      bad = true;
    }
    assert(bad);
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }
  const std::string empty = write_quotes_file("empty", "# only a comment\n");
  bool none = false;
  try {
    qts::FileQuoteFeed feed{empty};
  } catch (const std::runtime_error&) {
    none = true;
  }
  assert(none);
  std::error_code ec;
  std::filesystem::remove(empty, ec);
}

}  // namespace

int main() {
  l1_fields_are_populated();
  multi_symbol_state_is_independent();
  crossed_and_stale_hold_submissions();
  halt_and_auction_hold_submissions();
  legacy_path_stays_clean();
  file_feed_replays_rows_in_order();
  file_feed_rejects_loudly();
  return 0;
}
