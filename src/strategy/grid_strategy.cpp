// clang-format off
/**
  ******************************************************************************
  * @file           : grid_strategy.cpp
  * @author         : vivi wu
  * @brief          : Grid strategy implementation: per-level flip signals with take-profit.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/strategy/grid_strategy.hpp"

#include <stdexcept>

namespace sts {

GridStrategy::GridStrategy(GridConfig config, Price start_price)
    : config_{std::move(config)} {
  if (config_.strategy_id.empty() || config_.account_id.empty() || config_.instrument_id.empty()) {
    throw std::invalid_argument("grid strategy requires strategy/account/instrument ids");
  }
  if (config_.grids < 1 || config_.lower >= config_.upper || config_.quantity_per_order <= 0) {
    throw std::invalid_argument("grid strategy requires 0 < lower < upper and grids >= 1");
  }
  step_ = (config_.upper - config_.lower) / static_cast<Price>(config_.grids);
  if (step_ <= 0) throw std::invalid_argument("grid step must be positive");
  levels_.reserve(static_cast<std::size_t>(config_.grids) + 1);
  states_.reserve(static_cast<std::size_t>(config_.grids) + 1);
  for (int i = 0; i <= config_.grids; ++i) {
    const Price level = config_.lower + (step_ * static_cast<Price>(i));
    levels_.push_back(level);
    states_.push_back(level < start_price ? LevelState::buy_armed : LevelState::sell_armed);
  }
}

std::vector<OrderIntent> GridStrategy::on_quote(const QuoteEvent& quote, const Portfolio& /*book*/) {
  std::vector<OrderIntent> intents;
  if (quote.instrument_id != config_.instrument_id || quote.last_price <= 0) return intents;
  const Price price = quote.last_price;
  for (std::size_t i = 0; i < levels_.size(); ++i) {
    const Price level = levels_[i];
    if (states_[i] == LevelState::buy_armed) {
      if (price <= level) {
        intents.push_back(make_order(Side::buy, level));
        states_[i] = LevelState::sell_armed;
      }
    } else if (price >= level + step_) {
      intents.push_back(make_order(Side::sell, level + step_));
      states_[i] = LevelState::buy_armed;
    }
  }
  return intents;
}

OrderIntent GridStrategy::make_order(Side side, Price price) {
  ++sequence_;
  const std::string tag = config_.id_prefix.empty() ? "grid-" + std::to_string(sequence_) : config_.id_prefix + std::to_string(sequence_);
  OrderIntent intent{};
  intent.client_order_id = tag;
  intent.account_id = config_.account_id;
  intent.strategy_id = config_.strategy_id;
  intent.instrument_id = config_.instrument_id;
  intent.side = side;
  intent.type = OrderType::limit;
  intent.time_in_force = TimeInForce::day;
  intent.quantity = config_.quantity_per_order;
  intent.limit_price = price;
  intent.exchange = config_.exchange;
  intent.board = config_.board;
  intent.security_account_id = config_.security_account_id;
  intent.trace_id = tag;
  return intent;
}

}  // namespace sts
