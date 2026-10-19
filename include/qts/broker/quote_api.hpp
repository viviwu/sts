// clang-format off
/**
  ******************************************************************************
  * @file           : quote_api.hpp
  * @author         : vivi wu
  * @brief          : Internal market-data port contract (QuoteApi) for行情 adapters.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

#include "qts/core/types.hpp"

namespace qts {

enum class QuoteQuality : std::uint8_t { valid, stale, crossed, out_of_sequence, unavailable };
enum class MarketPhase : std::uint8_t { unknown, opening_auction, continuous, closing_auction, after_hours, halted, closed };

struct QuoteLevel {
  Price price{0};
  Quantity quantity{0};
};

// All prices and quantities use the instrument's configured integer tick/lot.
struct QuoteEvent {
  InstrumentId instrument_id;
  Exchange exchange{Exchange::unknown};
  Price bid_price{0};
  Quantity bid_quantity{0};
  Price ask_price{0};
  Quantity ask_quantity{0};
  Price last_price{0};
  Quantity cumulative_volume{0};
  Price cumulative_turnover{0};
  Price limit_down{0};
  Price limit_up{0};
  std::vector<QuoteLevel> bid_levels;
  std::vector<QuoteLevel> ask_levels;
  std::uint64_t sequence{0};
  Timestamp exchange_time;
  Timestamp receive_time;
  QuoteQuality quality{QuoteQuality::unavailable};
  MarketPhase market_phase{MarketPhase::unknown};
  std::string source_message_id;
};

// Internal market-data port.  Implementations normalize source-specific book,
// trade and heartbeat messages before delivering them to strategies/risk.
class QuoteApi {
 public:
  using QuoteHandler = std::function<void(const QuoteEvent&)>;

  QuoteApi() = default;
  virtual ~QuoteApi() = default;

  QuoteApi(const QuoteApi&) = delete;
  QuoteApi& operator=(const QuoteApi&) = delete;
  QuoteApi(QuoteApi&&) = delete;
  QuoteApi& operator=(QuoteApi&&) = delete;

  // Channel identity (e.g. "ama"); not an exchange venue (dev_scheme.md §1).
  [[nodiscard]] virtual std::string_view channel() const noexcept = 0;
  [[nodiscard]] virtual bool connected() const noexcept = 0;
  virtual void subscribe(const std::vector<InstrumentId>& instruments) = 0;
  virtual void unsubscribe(const std::vector<InstrumentId>& instruments) = 0;
  virtual void set_quote_handler(QuoteHandler handler) = 0;
};

}  // namespace qts
