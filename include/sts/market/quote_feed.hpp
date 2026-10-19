// clang-format off
/**
  ******************************************************************************
  * @file           : quote_feed.hpp
  * @author         : vivi wu
  * @brief          : Deterministic simulated quote walk feeding long-run demos.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "sts/broker/quote_api.hpp"

namespace sts {

// Deterministic simulated quote walk (M9-0, ADR-034). Ticks a symmetric
// random walk around start_price in unit steps, reflected at [low, high].
// Backed by std::mt19937_64 so the same seed always yields the same path
// on every platform; the gateway feeds each quote through the regular
// strategy -> execution -> risk -> OMS -> SimulatedBroker path.
//
// Realism pack (M11-0, no vendor dependency): emits a full L1 quote
// (bid/ask + quantities, one-level depth, cumulative volume/turnover,
// limit bands, exchange/receive timestamps) so downstream quality and
// phase gates can be exercised without a real feed. Fault injection and
// phase windows are opt-in (all zero = legacy valid/continuous path, so
// existing drills stay byte-identical).
//
// Multi-instrument: per-symbol walk state is kept in a map; the legacy
// single-symbol constructor seeds the default symbol lazily on first use.
class SimulatedQuoteFeed {
 public:
  struct Faults {
    std::uint64_t stale_every{0};    // every Nth tick marked stale (0 = off)
    std::uint64_t crossed_every{0};  // every Nth tick crossed book (0 = off)
    std::uint64_t halt_after{0};     // first halted tick (1-based, 0 = off)
    std::uint64_t halt_for{0};       // halt window length in ticks
    std::uint64_t auction_ticks{0};  // leading ticks as opening_auction
  };

  SimulatedQuoteFeed(std::uint64_t seed, Price start_price, Price step, Price low, Price high);

  QuoteEvent next(const InstrumentId& instrument_id, std::uint64_t sequence);

  [[nodiscard]] Price current() const noexcept { return current_; }
  [[nodiscard]] Price current_for(const InstrumentId& instrument_id) const;

  void set_tick(Price tick) noexcept { step_ = tick; }
  void set_price_limits(Price limit_down, Price limit_up) noexcept;
  void set_phase(MarketPhase phase) noexcept { forced_phase_ = phase; }
  void set_faults(Faults faults) noexcept { faults_ = faults; }

 private:
  struct SymbolState {
    Price current{0};
    Quantity cumulative_volume{0};
    bool seeded{false};
  };

  SymbolState& state_for(const InstrumentId& instrument_id);

  Price start_{0};
  Price current_;
  Price step_;
  Price low_;
  Price high_;
  Price limit_down_{0};
  Price limit_up_{0};
  MarketPhase forced_phase_{MarketPhase::continuous};
  Faults faults_{};
  std::uint64_t emitted_{0};
  std::mt19937_64 rng_;
  std::uniform_int_distribution<int> draw_{-1, 1};
  std::unordered_map<InstrumentId, SymbolState> symbols_;
};

// Submission-hold predicate shared by the gateway and tests: any
// non-valid quality or non-continuous phase must not open new risk.
// Cancels, queries and market-data stay live while held.
[[nodiscard]] inline bool quote_holds_submissions(const QuoteEvent& quote) noexcept {
  if (quote.quality != QuoteQuality::valid) return true;
  return quote.market_phase != MarketPhase::continuous;
}

// File-backed quote source: one CSV row per tick, replayed in file order.
//
// Line format (fail-closed, no silent skips):
//   instrument,last,bid,ask,bid_qty,ask_qty
// Prices/quantities are integers in the instrument's tick/lot (ADR-003).
// Blank lines and '#' comments are ignored; every other line must parse
// with bid <= last <= ask and positive quantities, else the constructor
// throws. Sequences are assigned 1..N in file order so the same file
// always yields the same stream; next() returns nullopt at EOF.
class FileQuoteFeed {
 public:
  explicit FileQuoteFeed(const std::string& path);

  [[nodiscard]] std::optional<QuoteEvent> next();
  [[nodiscard]] bool exhausted() const noexcept { return cursor_ >= quotes_.size(); }
  [[nodiscard]] std::size_t size() const noexcept { return quotes_.size(); }

 private:
  std::vector<QuoteEvent> quotes_;
  std::size_t cursor_{0};
};

}  // namespace sts
