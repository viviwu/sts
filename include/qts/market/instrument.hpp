// clang-format off
/**
  ******************************************************************************
  * @file           : instrument.hpp
  * @author         : vivi wu
  * @brief          : Versioned instrument master: injectable per-symbol trading rules.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "qts/broker/quote_api.hpp"
#include "qts/market/ashare.hpp"

namespace qts {

// Versioned instrument master. Rules are injected (never hard-coded in
// OMS/risk) and carry rules_version so EventEnvelope can record exactly
// which ruleset validated each decision.
class InstrumentMaster {
 public:
  InstrumentMaster() = default;
  virtual ~InstrumentMaster() = default;

  InstrumentMaster(const InstrumentMaster&) = delete;
  InstrumentMaster& operator=(const InstrumentMaster&) = delete;
  InstrumentMaster(InstrumentMaster&&) = delete;
  InstrumentMaster& operator=(InstrumentMaster&&) = delete;

  // Returns nullptr for unknown instruments; callers fail closed.
  [[nodiscard]] virtual const AshareInstrumentRules* find_rules(const InstrumentId& instrument_id) const = 0;
  [[nodiscard]] virtual std::string_view rules_version() const noexcept = 0;
};

class InMemoryInstrumentMaster final : public InstrumentMaster {
 public:
  explicit InMemoryInstrumentMaster(std::string version)
      : version_{std::move(version)} {}

  void upsert(AshareInstrumentRules rules) { rules_.insert_or_assign(rules.instrument_id, std::move(rules)); }

  const AshareInstrumentRules* find_rules(const InstrumentId& instrument_id) const override {
    const auto found = rules_.find(instrument_id);
    return found == rules_.end() ? nullptr : &found->second;
  }

  std::string_view rules_version() const noexcept override { return version_; }

 private:
  std::string version_;
  std::unordered_map<InstrumentId, AshareInstrumentRules> rules_;
};

// Trading calendar. Dates are YYYYMMDD integers: no chrono calendar types
// (C++20) are required, keeping the C++17 baseline intact.
class TradingCalendar {
 public:
  TradingCalendar() = default;
  virtual ~TradingCalendar() = default;

  TradingCalendar(const TradingCalendar&) = delete;
  TradingCalendar& operator=(const TradingCalendar&) = delete;
  TradingCalendar(TradingCalendar&&) = delete;
  TradingCalendar& operator=(TradingCalendar&&) = delete;

  [[nodiscard]] virtual bool is_trading_day(int yyyymmdd) const = 0;
};

class StaticTradingCalendar final : public TradingCalendar {
 public:
  explicit StaticTradingCalendar(std::unordered_set<int> trading_days)
      : trading_days_{std::move(trading_days)} {}

  bool is_trading_day(int yyyymmdd) const override { return trading_days_.find(yyyymmdd) != trading_days_.end(); }

 private:
  std::unordered_set<int> trading_days_;
};

struct QuoteQualityInput {
  bool has_last_sequence{false};
  std::uint64_t last_sequence{0};
  Timestamp now;
  std::chrono::nanoseconds max_staleness{std::chrono::seconds{3}};
};

// Market-data quality gate: sequence regression/duplication, staleness,
// time anomaly and crossed books. Priority: out_of_sequence > stale >
// crossed > valid. An adapter-marked unavailable quote stays unavailable.
[[nodiscard]] inline QuoteQuality assess_quote_quality(const QuoteEvent& quote, const QuoteQualityInput& input) {
  if (quote.quality == QuoteQuality::unavailable) return QuoteQuality::unavailable;
  if (input.has_last_sequence && quote.sequence <= input.last_sequence) return QuoteQuality::out_of_sequence;
  if (quote.receive_time > input.now) return QuoteQuality::out_of_sequence;
  if (input.now - quote.receive_time > input.max_staleness) return QuoteQuality::stale;
  if (quote.bid_price > 0 && quote.ask_price > 0 && quote.bid_price > quote.ask_price) {
    return QuoteQuality::crossed;
  }
  return QuoteQuality::valid;
}

}  // namespace qts
