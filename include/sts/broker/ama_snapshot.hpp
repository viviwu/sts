// clang-format off
/**
  ******************************************************************************
  * @file           : ama_snapshot.hpp
  * @author         : vivi wu
  * @brief          : AMA snapshot normalizer: MDSnapshot to QuoteEvent (precision, phase, sequence).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include "sts/broker/quote_api.hpp"
#include "sts/market/instrument.hpp"

namespace sts {

// AMA snapshot normalizer (M7-0, ADR-027).
//
// Converts a recorded MDSnapshot into the internal QuoteEvent. The input is
// a local fixture struct with the exact vendor field shapes (int64 scaled
// prices/volumes, YYYYMMDDHHMMSSsss exchange time), so no vendor header is
// referenced here or in tests; wiring the real SDK later is a field-for-field
// copy at the call site (see resources/broker-mappings/archforce-ama.md §2).
//
// Fail-closed: unknown markets, empty codes, corrupt timestamps and negative
// mapped numerics yield nullopt (nothing is emitted). Stale/crossed/future
// snapshots are still emitted with the quality stamp from
// assess_quote_quality, so downstream keeps the same contract as paper feeds.
struct AmaSnapshotFixture {
  static constexpr std::size_t kLevels = 10;

  int market_type{0};  // 101 SSE / 102 SZSE / 2 NEEQ(BSE); cf. MarketType
  std::string security_code;
  std::int64_t orig_time{0};  // YYYYMMDDHHMMSSsss, exchange time, ms precision
  std::string trading_phase_code;
  std::array<std::int64_t, kLevels> bid_price{};
  std::array<std::int64_t, kLevels> bid_volume{};
  std::array<std::int64_t, kLevels> offer_price{};
  std::array<std::int64_t, kLevels> offer_volume{};
  std::int64_t last_price{0};
  std::int64_t total_volume_trade{0};
  std::int64_t total_value_trade{0};
  std::int64_t high_limited{0};
  std::int64_t low_limited{0};
  std::int32_t channel_no{0};
};

class AmaSnapshotNormalizer {
 public:
  // Vendor fixed-point scales (see archforce-ama.md §3.1).
  static constexpr std::int64_t kAmaPriceScale = 1'000'000;  // Price6
  static constexpr std::int64_t kAmaQtyScale = 100;          // Qty2
  static constexpr std::int64_t kAmaAmountScale = 100'000;   // Amt5
  // Internal Price unit: 1/10,000 yuan.
  static constexpr std::int64_t kPriceScale = 10'000;

  // receive_time must be stamped by the adapter at callback enqueue time;
  // now anchors the staleness check. Returns nullopt only for fail-closed
  // inputs; otherwise the event carries its assessed quality.
  std::optional<QuoteEvent> normalize(const AmaSnapshotFixture& snap, Timestamp receive_time, Timestamp now, std::chrono::nanoseconds max_staleness = std::chrono::seconds{3});

 private:
  static bool map_market(int market_type, Exchange& exchange, std::string& suffix);
  static MarketPhase map_phase(int market_type, const std::string& phase_code);
  static bool parse_orig_time(std::int64_t orig_time, Timestamp& out);

  std::unordered_map<InstrumentId, std::uint64_t> sequences_;
};

}  // namespace sts
