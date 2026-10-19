// clang-format off
/**
  ******************************************************************************
  * @file           : strategy_api.hpp
  * @author         : vivi wu
  * @brief          : Strategy port: normalized quotes in, OrderIntents out.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <string_view>
#include <vector>

#include "qts/broker/quote_api.hpp"
#include "qts/core/types.hpp"
#include "qts/portfolio/portfolio.hpp"

namespace qts {

// Strategy boundary (M0 contract): strategies observe normalized quotes and
// the strategy-side book, and express intent as OrderIntent. They never
// touch TradeApi, OMS or risk.
class Strategy {
 public:
  Strategy() = default;
  virtual ~Strategy() = default;

  Strategy(const Strategy&) = delete;
  Strategy& operator=(const Strategy&) = delete;
  Strategy(Strategy&&) = delete;
  Strategy& operator=(Strategy&&) = delete;

  [[nodiscard]] virtual std::string_view name() const noexcept = 0;
  virtual std::vector<OrderIntent> on_quote(const QuoteEvent& quote, const Portfolio& book) = 0;
};

}  // namespace qts
