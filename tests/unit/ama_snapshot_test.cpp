// clang-format off
/**
  ******************************************************************************
  * @file           : ama_snapshot_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for AMA snapshot normalization and quality gates.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/broker/ama_snapshot.hpp"

#include <cassert>
#include <chrono>
#include <optional>

namespace {

sts::AmaSnapshotFixture make_snapshot() {
  sts::AmaSnapshotFixture snap{};
  snap.market_type = 101;
  snap.security_code = "600000";
  snap.orig_time = 20260928093000000;  // 2026-09-28 09:30:00.000
  snap.trading_phase_code = "T";
  for (std::size_t i = 0; i < sts::AmaSnapshotFixture::kLevels; ++i) {
    snap.bid_price[i] = 1'000'000'000 - static_cast<std::int64_t>(i) * 100'000;  // 1000.00 - i*0.01
    snap.bid_volume[i] = (static_cast<std::int64_t>(i) + 1) * 100 * 100;         // (i+1) lots in Qty2
    snap.offer_price[i] = 1'000'100'000 + static_cast<std::int64_t>(i) * 100'000;
    snap.offer_volume[i] = (static_cast<std::int64_t>(i) + 1) * 200 * 100;
  }
  snap.last_price = 1'000'050'000;
  snap.total_volume_trade = 1'234'500 * 100;
  snap.total_value_trade = 1'234'567'890LL * 10;  // Amt5; /10 keeps all digits
  snap.high_limited = 1'100'000'000;
  snap.low_limited = 900'000'000;
  snap.channel_no = 0;  // Shanghai: absent
  return snap;
}

sts::Timestamp make_now() { return sts::Timestamp{std::chrono::seconds{1'790'586'600}}; }  // ~2026-09-28

void sse_happy_path() {
  sts::AmaSnapshotNormalizer normalizer;
  const sts::Timestamp now = make_now();
  const auto event = normalizer.normalize(make_snapshot(), now - std::chrono::seconds{1}, now);
  assert(event.has_value());
  assert(event->instrument_id == "600000.SSE");
  assert(event->exchange == sts::Exchange::sse);
  assert(event->bid_price == 1'000'0000);
  assert(event->bid_quantity == 100);
  assert(event->ask_price == 1'000'1000);
  assert(event->ask_quantity == 200);
  assert(event->last_price == 1'000'0500);
  assert(event->cumulative_volume == 1'234'500);
  assert(event->cumulative_turnover == 1'234'567'890);
  assert(event->limit_up == 1'100'0000);
  assert(event->limit_down == 900'0000);
  assert(event->bid_levels.size() == 10 && event->ask_levels.size() == 10);
  assert(event->bid_levels[9].price == 9'991'000);
  assert(event->ask_levels[9].price == 10'010'000);
  assert(event->sequence == 1);
  assert(event->quality == sts::QuoteQuality::valid);
  assert(event->market_phase == sts::MarketPhase::continuous);
  assert(event->source_message_id == "101:600000:20260928093000000");
  // 2026-09-28 09:30:00.000 treated as UTC civil (vendor Beijing wall time).
  assert(event->exchange_time == sts::Timestamp{std::chrono::nanoseconds{1'790'587'800'000'000'000}});
}

void szse_phase_and_bse_market() {
  sts::AmaSnapshotNormalizer normalizer;
  const sts::Timestamp now = make_now();
  auto shenzhen = make_snapshot();
  shenzhen.market_type = 102;
  shenzhen.security_code = "000001";
  shenzhen.trading_phase_code = "O";
  const auto sz = normalizer.normalize(shenzhen, now - std::chrono::seconds{1}, now);
  assert(sz.has_value());
  assert(sz->instrument_id == "000001.SZSE");
  assert(sz->exchange == sts::Exchange::szse);
  assert(sz->market_phase == sts::MarketPhase::opening_auction);

  auto beijing = make_snapshot();
  beijing.market_type = 2;  // kNEEQ, not the ATP trade id 104
  beijing.security_code = "920000";
  const auto bj = normalizer.normalize(beijing, now - std::chrono::seconds{1}, now);
  assert(bj.has_value());
  assert(bj->instrument_id == "920000.BSE");
  assert(bj->exchange == sts::Exchange::bse);
}

void phase_table_spot_checks() {
  sts::AmaSnapshotNormalizer normalizer;
  const sts::Timestamp now = make_now();
  auto snap = make_snapshot();
  snap.market_type = 101;
  snap.trading_phase_code = "C";
  assert(normalizer.normalize(snap, now - std::chrono::seconds{1}, now)->market_phase == sts::MarketPhase::opening_auction);
  snap.trading_phase_code = "U";
  assert(normalizer.normalize(snap, now - std::chrono::seconds{1}, now)->market_phase == sts::MarketPhase::closing_auction);
  snap.trading_phase_code = "P";
  assert(normalizer.normalize(snap, now - std::chrono::seconds{1}, now)->market_phase == sts::MarketPhase::halted);
  snap.trading_phase_code = "E";
  assert(normalizer.normalize(snap, now - std::chrono::seconds{1}, now)->market_phase == sts::MarketPhase::closed);
  snap.market_type = 102;
  snap.trading_phase_code = "A";
  assert(normalizer.normalize(snap, now - std::chrono::seconds{1}, now)->market_phase == sts::MarketPhase::after_hours);
  snap.trading_phase_code = "H";
  assert(normalizer.normalize(snap, now - std::chrono::seconds{1}, now)->market_phase == sts::MarketPhase::halted);
  snap.trading_phase_code = "";
  assert(normalizer.normalize(snap, now - std::chrono::seconds{1}, now)->market_phase == sts::MarketPhase::unknown);
}

void fail_closed_inputs() {
  sts::AmaSnapshotNormalizer normalizer;
  const sts::Timestamp now = make_now();
  const sts::Timestamp receive = now - std::chrono::seconds{1};
  auto snap = make_snapshot();

  auto hkex = snap;
  hkex.market_type = 103;
  assert(!normalizer.normalize(hkex, receive, now).has_value());
  auto cfets = snap;
  cfets.market_type = 104;
  assert(!normalizer.normalize(cfets, receive, now).has_value());
  auto usa = snap;
  usa.market_type = 12;
  assert(!normalizer.normalize(usa, receive, now).has_value());

  auto no_code = snap;
  no_code.security_code.clear();
  assert(!normalizer.normalize(no_code, receive, now).has_value());

  auto bad_month = snap;
  bad_month.orig_time = 20261328093000000;
  assert(!normalizer.normalize(bad_month, receive, now).has_value());
  auto bad_day = snap;
  bad_day.orig_time = 20260900093000000;
  assert(!normalizer.normalize(bad_day, receive, now).has_value());
  auto bad_hour = snap;
  bad_hour.orig_time = 20260928243000000;
  assert(!normalizer.normalize(bad_hour, receive, now).has_value());
  auto zero_time = snap;
  zero_time.orig_time = 0;
  assert(!normalizer.normalize(zero_time, receive, now).has_value());

  auto negative = snap;
  negative.bid_price[0] = -1;
  assert(!normalizer.normalize(negative, receive, now).has_value());

  // Leap day is a real calendar date.
  auto leap = snap;
  leap.orig_time = 20240229093000000;
  assert(normalizer.normalize(leap, receive, now).has_value());
  auto non_leap = snap;
  non_leap.orig_time = 20230229093000000;
  assert(!normalizer.normalize(non_leap, receive, now).has_value());
}

void precision_truncation() {
  sts::AmaSnapshotNormalizer normalizer;
  const sts::Timestamp now = make_now();
  auto snap = make_snapshot();
  snap.bid_price[0] = 1'000'000'001;  // Price6 not divisible by 100: truncate
  snap.bid_volume[0] = 150;           // 1.5 shares: truncate to 1
  snap.total_value_trade = 1'234'567'895;
  const auto event = normalizer.normalize(snap, now - std::chrono::seconds{1}, now);
  assert(event.has_value());
  assert(event->bid_price == 1'000'0000);
  assert(event->bid_quantity == 1);
  assert(event->cumulative_turnover == 123'456'789);
}

void sequence_monotonic_per_instrument() {
  sts::AmaSnapshotNormalizer normalizer;
  const sts::Timestamp now = make_now();
  const sts::Timestamp receive = now - std::chrono::seconds{1};
  auto snap = make_snapshot();
  assert(normalizer.normalize(snap, receive, now)->sequence == 1);
  assert(normalizer.normalize(snap, receive, now)->sequence == 2);
  auto other = snap;
  other.security_code = "600001";
  assert(normalizer.normalize(other, receive, now)->sequence == 1);  // independent counter
  // Later exchange time sorts after earlier one.
  auto later = snap;
  later.orig_time = 20260928093001000;
  const auto first = normalizer.normalize(snap, receive, now);
  const auto second = normalizer.normalize(later, receive, now);
  assert(first.has_value() && second.has_value());
  assert(first->exchange_time < second->exchange_time);
}

void quality_stale_future_crossed() {
  sts::AmaSnapshotNormalizer normalizer;
  const sts::Timestamp now = make_now();
  auto snap = make_snapshot();

  const auto stale = normalizer.normalize(snap, now - std::chrono::seconds{90}, now);
  assert(stale.has_value() && stale->quality == sts::QuoteQuality::stale);

  const auto future = normalizer.normalize(snap, now + std::chrono::seconds{1}, now);
  assert(future.has_value() && future->quality == sts::QuoteQuality::out_of_sequence);

  auto crossed = snap;
  crossed.bid_price[0] = crossed.offer_price[0] + 100;
  const auto cross = normalizer.normalize(crossed, now - std::chrono::seconds{1}, now);
  assert(cross.has_value() && cross->quality == sts::QuoteQuality::crossed);
}

void zero_limits_mean_no_band() {
  sts::AmaSnapshotNormalizer normalizer;
  const sts::Timestamp now = make_now();
  auto snap = make_snapshot();
  snap.high_limited = 0;  // FPGA-style missing field stays a valid zero
  snap.low_limited = 0;
  const auto event = normalizer.normalize(snap, now - std::chrono::seconds{1}, now);
  assert(event.has_value());
  assert(event->limit_up == 0 && event->limit_down == 0);
  assert(event->quality == sts::QuoteQuality::valid);
}

}  // namespace

int main() {
  sse_happy_path();
  szse_phase_and_bse_market();
  phase_table_spot_checks();
  fail_closed_inputs();
  precision_truncation();
  sequence_monotonic_per_instrument();
  quality_stale_future_crossed();
  zero_limits_mean_no_band();
  return 0;
}
