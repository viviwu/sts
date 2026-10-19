// clang-format off
/**
  ******************************************************************************
  * @file           : order_book.hpp
  * @author         : vivi wu
  * @brief          : Opt-in depth simulation: price-level takes for limit and market orders.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <vector>

#include "qts/core/types.hpp"

namespace qts {

// Standing depth for one instrument (M6-0). Bids must be sorted best-first
// (descending), asks best-first (ascending); take() walks from the front,
// so unsorted input matches in the wrong order. Pure value type: take()
// consumes depth deterministically, keeping replay reproducible.
struct BookLevel {
  Price price{0};
  Quantity quantity{0};
};

struct BookFill {
  Quantity filled{0};
  Price average_price{0};  // VWAP of consumed levels; 0 when filled == 0
};

struct OrderBook {
  std::vector<BookLevel> bids;
  std::vector<BookLevel> asks;

  // Taker match: buys consume asks low-up, sells consume bids high-down.
  // Limit orders only touch levels within the limit; market orders sweep
  // everything. Returns filled quantity + VWAP; any leftover rests with the
  // caller (stays live). Emptied levels are removed.
  BookFill take(Side side, OrderType type, Price limit, Quantity quantity);
};

}  // namespace qts
