// clang-format off
/**
  ******************************************************************************
  * @file           : ama_snapshot.cpp
  * @author         : vivi wu
  * @brief          : AMA snapshot normalization implementation.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/broker/ama_snapshot.hpp"

#include <algorithm>
#include <array>

namespace qts {
namespace {

// Roma civil-date algorithm: days since 1970-01-01 for a proleptic
// Gregorian date. C++17 has no chrono calendar types (ADR-009).
constexpr std::int64_t days_from_civil(std::int64_t year, std::int64_t month, std::int64_t day) {
  const std::int64_t era = (year >= 0 ? year : (year - 399)) / 400;
  const std::int64_t yoe = year - (era * 400);
  const std::int64_t doy = (((153 * (month > 2 ? (month - 3) : (month + 9)) + 2) / 5) + day) - 1;
  const std::int64_t doe = ((yoe * 365) + (yoe / 4) - (yoe / 100)) + doy;
  return (era * 146097) + doe - 719468;
}

constexpr bool is_leap(std::int64_t year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

}  // namespace

bool AmaSnapshotNormalizer::map_market(int market_type, Exchange& exchange, std::string& suffix) {
  // NEEQ(2) is the AMA id for Beijing; ATP trade numbering differs (see
  // archforce-ama.md §2.1), so this table must not be shared with trade code.
  switch (market_type) {
    case 101:
      exchange = Exchange::sse;
      suffix = "SSE";
      return true;
    case 102:
      exchange = Exchange::szse;
      suffix = "SZSE";
      return true;
    case 2:
      exchange = Exchange::bse;
      suffix = "BSE";
      return true;
    default:
      return false;  // HKEx/CFETS/USA: no Exchange enumerator, refuse loudly
  }
}

MarketPhase AmaSnapshotNormalizer::map_phase(int market_type, const std::string& phase_code) {
  if (phase_code.empty()) return MarketPhase::unknown;
  const char first = phase_code[0];
  if (market_type == 101) {
    // ama_struct.h MDSnapshot comment: S/C/T/E/P/M/N/U.
    switch (first) {
      case 'C':
        return MarketPhase::opening_auction;
      case 'T':
        return MarketPhase::continuous;
      case 'U':
        return MarketPhase::closing_auction;
      case 'P':
      case 'M':
      case 'N':
        return MarketPhase::halted;
      case 'E':
        return MarketPhase::closed;
      default:
        return MarketPhase::unknown;
    }
  }
  if (market_type == 102) {
    // S/O/T/B/C/E/H/A/V.
    switch (first) {
      case 'O':
        return MarketPhase::opening_auction;
      case 'T':
        return MarketPhase::continuous;
      case 'C':
        return MarketPhase::closing_auction;
      case 'H':
      case 'B':
      case 'V':
        return MarketPhase::halted;
      case 'E':
        return MarketPhase::closed;
      case 'A':
        return MarketPhase::after_hours;
      default:
        return MarketPhase::unknown;
    }
  }
  return MarketPhase::unknown;
}

bool AmaSnapshotNormalizer::parse_orig_time(std::int64_t orig_time, Timestamp& out) {
  if (orig_time <= 0) return false;
  std::int64_t rest = orig_time;
  const std::int64_t millis = rest % 1000;
  rest /= 1000;
  const std::int64_t second = rest % 100;
  rest /= 100;
  const std::int64_t minute = rest % 100;
  rest /= 100;
  const std::int64_t hour = rest % 100;
  rest /= 100;
  const std::int64_t day = rest % 100;
  rest /= 100;
  const std::int64_t month = rest % 100;
  const std::int64_t year = rest / 100;
  if (year < 2000 || year > 2100 || month < 1 || month > 12 || hour > 23 || minute > 59 || second > 59 || millis > 999) {
    return false;
  }
  constexpr std::array<std::int64_t, 12> kMonthDays{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  std::int64_t max_day = kMonthDays.at(static_cast<std::size_t>(month) - 1);
  if (month == 2 && is_leap(year)) max_day = 29;
  if (day < 1 || day > max_day) return false;
  const std::int64_t days = days_from_civil(year, month, day);
  const std::int64_t total_millis = ((((((days * 24) + hour) * 60) + minute) * 60) * 1000) + (second * 1000) + millis;
  out = Timestamp{std::chrono::nanoseconds{total_millis * 1'000'000}};
  return true;
}

std::optional<QuoteEvent> AmaSnapshotNormalizer::normalize(const AmaSnapshotFixture& snap, Timestamp receive_time, Timestamp now, std::chrono::nanoseconds max_staleness) {
  Exchange exchange{Exchange::unknown};
  std::string suffix;
  if (!map_market(snap.market_type, exchange, suffix)) return std::nullopt;
  if (snap.security_code.empty()) return std::nullopt;
  Timestamp exchange_time{};
  if (!parse_orig_time(snap.orig_time, exchange_time)) return std::nullopt;

  // Integer truncation toward zero; any negative mapped field means a
  // corrupt snapshot (FPGA-missing fields arrive as 0, which stays valid).
  constexpr std::int64_t kPriceDiv = kAmaPriceScale / kPriceScale;      // 100
  constexpr std::int64_t kTurnoverDiv = kAmaAmountScale / kPriceScale;  // 10
  auto non_negative = [](const std::array<std::int64_t, AmaSnapshotFixture::kLevels>& values) { return std::all_of(values.begin(), values.end(), [](std::int64_t value) { return value >= 0; }); };
  if (!non_negative(snap.bid_price) || !non_negative(snap.bid_volume) || !non_negative(snap.offer_price) || !non_negative(snap.offer_volume) || snap.last_price < 0 || snap.total_volume_trade < 0 ||
      snap.total_value_trade < 0 || snap.high_limited < 0 || snap.low_limited < 0) {
    return std::nullopt;
  }

  QuoteEvent event{};
  event.instrument_id = snap.security_code + "." + suffix;
  event.exchange = exchange;
  event.bid_price = snap.bid_price[0] / kPriceDiv;
  event.bid_quantity = snap.bid_volume[0] / kAmaQtyScale;
  event.ask_price = snap.offer_price[0] / kPriceDiv;
  event.ask_quantity = snap.offer_volume[0] / kAmaQtyScale;
  event.last_price = snap.last_price / kPriceDiv;
  event.cumulative_volume = snap.total_volume_trade / kAmaQtyScale;
  event.cumulative_turnover = snap.total_value_trade / kTurnoverDiv;
  event.limit_down = snap.low_limited / kPriceDiv;
  event.limit_up = snap.high_limited / kPriceDiv;
  event.bid_levels.reserve(AmaSnapshotFixture::kLevels);
  event.ask_levels.reserve(AmaSnapshotFixture::kLevels);
  for (std::size_t i = 0; i < AmaSnapshotFixture::kLevels; ++i) {
    event.bid_levels.push_back(QuoteLevel{snap.bid_price.at(i) / kPriceDiv, snap.bid_volume.at(i) / kAmaQtyScale});
    event.ask_levels.push_back(QuoteLevel{snap.offer_price.at(i) / kPriceDiv, snap.offer_volume.at(i) / kAmaQtyScale});
  }
  event.sequence = ++sequences_[event.instrument_id];
  event.exchange_time = exchange_time;
  event.receive_time = receive_time;
  event.market_phase = map_phase(snap.market_type, snap.trading_phase_code);
  event.source_message_id = std::to_string(snap.market_type) + ":" + snap.security_code + ":" + std::to_string(snap.orig_time);
  event.quality = QuoteQuality::valid;  // adapter-marked, then confirmed by the gate below

  QuoteQualityInput quality_input{};
  quality_input.has_last_sequence = event.sequence > 1;
  quality_input.last_sequence = event.sequence - 1;
  quality_input.now = now;
  quality_input.max_staleness = max_staleness;
  event.quality = assess_quote_quality(event, quality_input);
  return event;
}

}  // namespace qts
