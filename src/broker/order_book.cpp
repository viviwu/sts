// clang-format off
/**
  ******************************************************************************
  * @file           : order_book.cpp
  * @author         : vivi wu
  * @brief          : Order-book depth take implementation (sweep pricing, per-fill ids).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/broker/order_book.hpp"

#include <algorithm>

namespace sts {

BookFill OrderBook::take(Side side, OrderType type, Price limit, Quantity quantity) {
  std::vector<BookLevel>& levels = (side == Side::buy) ? asks : bids;
  Quantity remaining = quantity;
  Quantity filled = 0;
  Price notional = 0;
  for (auto& level : levels) {
    if (remaining == 0) break;
    if (level.quantity <= 0) continue;
    if (type == OrderType::limit) {
      if (side == Side::buy && level.price > limit) break;
      if (side == Side::sell && level.price < limit) break;
    }
    const Quantity taken = std::min(remaining, level.quantity);
    level.quantity -= taken;
    remaining -= taken;
    filled += taken;
    notional += taken * level.price;
  }
  levels.erase(std::remove_if(levels.begin(), levels.end(), [](const BookLevel& level) { return level.quantity <= 0; }), levels.end());
  return BookFill{filled, filled > 0 ? notional / filled : 0};
}

}  // namespace sts
