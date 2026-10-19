// clang-format off
/**
  ******************************************************************************
  * @file           : rebalance_strategy.cpp
  * @author         : vivi wu
  * @brief          : Target-weight rebalance implementation: warm-up gate plus drift sizing.
  * @version        : 0.1.0
  * @date           : 04/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/strategy/rebalance_strategy.hpp"

#include <algorithm>
#include <stdexcept>

namespace qts {

RebalanceStrategy::RebalanceStrategy(RebalanceConfig config)
    : config_{std::move(config)} {
  if (config_.strategy_id.empty() || config_.account_id.empty()) {
    throw std::invalid_argument("rebalance strategy requires strategy/account ids");
  }
  if (config_.targets.empty()) throw std::invalid_argument("rebalance strategy requires at least one target");
  if (config_.band_bps < 0) throw std::invalid_argument("rebalance band_bps must be non-negative");
  int total_weight = 0;
  for (const auto& target : config_.targets) {
    if (target.instrument_id.empty()) throw std::invalid_argument("rebalance target needs an instrument id");
    if (target.weight_bps < 0 || target.weight_bps > 10'000) throw std::invalid_argument("rebalance weight_bps must be within 0..10000");
    if (target.lot_step <= 0) throw std::invalid_argument("rebalance lot_step must be positive");
    total_weight += target.weight_bps;
  }
  if (total_weight <= 0 || total_weight > 10'000) throw std::invalid_argument("rebalance weights must sum to (0, 10000]");
}

std::vector<OrderIntent> RebalanceStrategy::on_quote(const QuoteEvent& quote, const Portfolio& book) {
  std::vector<OrderIntent> intents;
  if (quote.last_price <= 0) return intents;
  const RebalanceTarget* target = nullptr;
  for (const auto& entry : config_.targets) {
    if (entry.instrument_id == quote.instrument_id) {
      target = &entry;
      break;
    }
  }
  if (target == nullptr) return intents;
  last_prices_[quote.instrument_id] = quote.last_price;
  for (const auto& entry : config_.targets) {
    if (last_prices_.find(entry.instrument_id) == last_prices_.end()) return intents;  // warm-up gate
  }
  Price equity = book.cash();
  for (const auto& entry : config_.targets) {
    const PositionView held = book.position_for(config_.strategy_id, entry.instrument_id);
    equity += held.total * last_prices_[entry.instrument_id];
  }
  if (equity <= 0) return intents;
  const Price price = quote.last_price;
  const PositionView ours = book.position_for(config_.strategy_id, target->instrument_id);
  const auto goal = static_cast<Price>(static_cast<std::int64_t>(target->weight_bps) * equity / 10'000);
  const Price diff = goal - ours.total * price;
  const auto band = static_cast<Price>(static_cast<std::int64_t>(config_.band_bps) * equity / 10'000);
  if (diff >= -band && diff <= band) return intents;
  const Side side = diff > 0 ? Side::buy : Side::sell;
  Quantity quantity = (diff > 0 ? diff : -diff) / price;
  quantity -= quantity % target->lot_step;
  if (side == Side::sell) {
    quantity = std::min(quantity, ours.sellable);
    quantity -= quantity % target->lot_step;
  }
  if (quantity < target->lot_step) return intents;
  intents.push_back(make_order(*target, side, quantity, price));
  return intents;
}

OrderIntent RebalanceStrategy::make_order(const RebalanceTarget& target, Side side, Quantity quantity, Price price) {
  ++sequence_;
  const std::string tag = config_.id_prefix.empty() ? "rebalance-" + std::to_string(sequence_) : config_.id_prefix + std::to_string(sequence_);
  OrderIntent intent{};
  intent.client_order_id = tag;
  intent.account_id = config_.account_id;
  intent.strategy_id = config_.strategy_id;
  intent.instrument_id = target.instrument_id;
  intent.side = side;
  intent.type = OrderType::limit;
  intent.time_in_force = TimeInForce::day;
  intent.quantity = quantity;
  intent.limit_price = price;
  intent.exchange = target.exchange;
  intent.board = target.board;
  intent.security_account_id = target.security_account_id;
  intent.trace_id = tag;
  return intent;
}

}  // namespace qts
