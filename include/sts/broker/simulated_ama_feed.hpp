// clang-format off
/**
  ******************************************************************************
  * @file           : simulated_ama_feed.hpp
  * @author         : vivi wu
  * @brief          : Deterministic AMA-shaped mock feed: snapshots plus tick gaps.
  * @version        : 0.1.0
  * @date           : 03/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "sts/broker/ama_snapshot.hpp"
#include "sts/broker/ama_tick.hpp"
#include "sts/broker/quote_api.hpp"

namespace sts {

// Mock AMA market-data source (M7-1 end-to-end, zero vendor dependency).
//
// Generates AmaSnapshotFixture records deterministically (seed-pinned walk),
// normalizes them through the production AmaSnapshotNormalizer, and observes
// a per-(market, channel) appl_seq through the production
// AmaTickSequenceTracker. The emitted QuoteEvent stream is therefore shaped
// exactly like the real adapter output (L10, limits, phases, quality), while
// tick gaps surface as QueryMDTick-shaped AmaTickGap ranges for backfill.
//
// Faults and phase windows are opt-in (all zero = clean continuous stream).
// Corrupt fixtures exercise the normalizer fail-closed path: nothing is
// emitted and the sequence counters do not advance for that symbol.
struct SimulatedAmaSymbol {
  int market_type{101};  // 101 SSE / 102 SZSE / 2 BSE
  std::string security_code;
  Price start{1'000};
  Price step{50};
  Price low{800};
  Price high{1'200};
  Price limit_down{800};
  Price limit_up{1'200};
};

struct SimulatedAmaConfig {
  std::uint64_t seed{42};
  std::uint64_t auction_ticks{0};  // leading snapshots per symbol as auction
  std::uint64_t halt_after{0};     // 1-based snapshot index where halt starts
  std::uint64_t halt_for{0};       // halt window length in snapshots
  std::uint64_t gap_every{0};      // every Nth tick skips gap_size seq numbers
  std::uint64_t gap_size{2};
  std::uint64_t stale_every{0};
  std::uint64_t crossed_every{0};
  std::uint64_t corrupt_every{0};
  std::int64_t orig_time_base{20261003093000000LL};  // 2026-10-03 09:30:00.000
  std::int64_t orig_time_step_ms{500};
  std::int32_t szse_channel{5};
};

class SimulatedAmaFeed final : public QuoteApi {
 public:
  explicit SimulatedAmaFeed(std::vector<SimulatedAmaSymbol> symbols, SimulatedAmaConfig config = {});

  std::string_view channel() const noexcept override;
  bool connected() const noexcept override;
  void subscribe(const std::vector<InstrumentId>& instruments) override;
  void unsubscribe(const std::vector<InstrumentId>& instruments) override;
  void set_quote_handler(QuoteHandler handler) override;

  void disconnect();
  void reconnect();
  // Day boundary: tick sequences reset, same beat as settle_day.
  void settle_day();

  // One snapshot per configured symbol, in config order. Gaps/faults apply
  // per emission; corrupt fixtures are dropped (fail-closed) and reported
  // in dropped(). The quote handler, when set, receives every emitted event.
  std::vector<QuoteEvent> step();
  // Single-symbol step for focused tests; nullopt when the symbol is unknown
  // or its fixture was corrupt-dropped.
  std::optional<QuoteEvent> next_for(const std::string& security_code);

  [[nodiscard]] std::vector<AmaTickGap> pending_gaps() const { return gaps_; }
  [[nodiscard]] std::size_t emitted() const noexcept { return emitted_; }
  [[nodiscard]] std::size_t dropped() const noexcept { return dropped_; }
  [[nodiscard]] std::size_t snapshots() const noexcept { return snapshots_; }
  [[nodiscard]] const AmaTickSequenceTracker& tracker() const noexcept { return tracker_; }

 private:
  struct SymbolState {
    SimulatedAmaSymbol spec;
    Price current{0};
    Quantity cumulative_volume{0};
    Price cumulative_turnover{0};
    std::int64_t orig_time{0};
    std::uint64_t index{0};  // snapshots generated for this symbol
  };

  SymbolState* find(const std::string& security_code);
  std::int32_t channel_for(int market_type) const;
  std::string phase_for(const SymbolState& state) const;
  AmaSnapshotFixture build_fixture(SymbolState& state);
  std::optional<QuoteEvent> emit_one(SymbolState& state);

  std::vector<SymbolState> symbols_;
  SimulatedAmaConfig config_;
  AmaSnapshotNormalizer normalizer_;
  AmaTickSequenceTracker tracker_;
  std::unordered_map<std::int64_t, std::int64_t> next_seq_;         // (market,channel) -> next appl_seq
  std::unordered_map<std::int64_t, std::uint64_t> tick_emissions_;  // per-channel emission count for gap pacing
  std::vector<AmaTickGap> gaps_;
  std::unordered_set<InstrumentId> subscriptions_;
  QuoteHandler handler_;
  bool connected_{true};
  std::size_t emitted_{0};
  std::size_t dropped_{0};
  std::size_t snapshots_{0};
  std::mt19937_64 rng_;
  std::uniform_int_distribution<int> draw_{-1, 1};
};

}  // namespace sts
