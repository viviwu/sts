// clang-format off
/**
  ******************************************************************************
  * @file           : market_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for market-data config, master data and quality gates.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include <cassert>
#include <chrono>

#include "qts/market/instrument.hpp"

namespace {

qts::AshareInstrumentRules make_rules() { return qts::AshareInstrumentRules{"SSE.600000", qts::Exchange::sse, qts::AshareBoard::main, 1, 100, 100, 1'000'000, true, true, 900, 1'100}; }

void master_find_and_version() {
  qts::InMemoryInstrumentMaster master{"2026-09-24"};
  assert(master.rules_version() == "2026-09-24");
  assert(master.find_rules("SSE.600000") == nullptr);  // fail closed on unknown

  master.upsert(make_rules());
  const auto* found = master.find_rules("SSE.600000");
  assert(found != nullptr && found->minimum_quantity == 100);

  qts::AshareInstrumentRules updated = make_rules();
  updated.price_tick = 10;
  master.upsert(updated);
  assert(master.find_rules("SSE.600000")->price_tick == 10);
}

void calendar_trading_days() {
  qts::StaticTradingCalendar calendar{{20260923, 20260924}};
  assert(calendar.is_trading_day(20260924));
  assert(!calendar.is_trading_day(20260926));  // weekend / holiday
}

qts::QuoteEvent make_quote() {
  qts::QuoteEvent quote{};
  quote.instrument_id = "SSE.600000";
  quote.exchange = qts::Exchange::sse;
  quote.bid_price = 1'000;
  quote.ask_price = 1'001;
  quote.sequence = 7;
  quote.receive_time = qts::Timestamp{std::chrono::seconds{100}};
  quote.quality = qts::QuoteQuality::valid;
  return quote;
}

qts::QuoteQualityInput make_input() {
  qts::QuoteQualityInput in{};
  in.now = qts::Timestamp{std::chrono::seconds{101}};
  return in;
}

void quality_gate() {
  assert(qts::assess_quote_quality(make_quote(), make_input()) == qts::QuoteQuality::valid);

  // Sequence regression and duplication.
  auto regressed = make_quote();
  qts::QuoteQualityInput in = make_input();
  in.has_last_sequence = true;
  in.last_sequence = 7;
  assert(qts::assess_quote_quality(regressed, in) == qts::QuoteQuality::out_of_sequence);

  // Staleness beyond the budget.
  auto stale = make_quote();
  stale.receive_time = qts::Timestamp{std::chrono::seconds{90}};
  assert(qts::assess_quote_quality(stale, make_input()) == qts::QuoteQuality::stale);

  // Receive time in the future: clock anomaly.
  auto future = make_quote();
  future.receive_time = qts::Timestamp{std::chrono::seconds{200}};
  assert(qts::assess_quote_quality(future, make_input()) == qts::QuoteQuality::out_of_sequence);

  // Crossed book.
  auto crossed = make_quote();
  crossed.bid_price = 1'002;
  assert(qts::assess_quote_quality(crossed, make_input()) == qts::QuoteQuality::crossed);

  // Adapter-marked unavailable passes through.
  auto down = make_quote();
  down.quality = qts::QuoteQuality::unavailable;
  assert(qts::assess_quote_quality(down, make_input()) == qts::QuoteQuality::unavailable);
}

}  // namespace

int main() {
  master_find_and_version();
  calendar_trading_days();
  quality_gate();
  return 0;
}
