// clang-format off
/**
  ******************************************************************************
  * @file           : portfolio.cpp
  * @author         : vivi wu
  * @brief          : Portfolio book implementation (fills, fees, T+1 sellable, settlement).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/portfolio/portfolio.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace sts {

void Portfolio::apply_fill(const OrderIntent& intent, Quantity last_quantity, Price last_price) {
  if (last_quantity <= 0 || last_price < 0) return;
  Holding& holding = holdings_[BasketKey{basket_strategy(intent), intent.instrument_id}];
  const Price notional = last_quantity * last_price;
  if (intent.side == Side::buy) {
    holding.cost_basis += notional;
    holding.total += last_quantity;
    holding.day_buy += last_quantity;
    cash_ -= notional;
  } else {
    const Price average = holding.total > 0 ? holding.cost_basis / holding.total : 0;
    holding.realized += last_quantity * (last_price - average);
    if (holding.total > 0) {
      holding.cost_basis -= average * last_quantity;
      holding.cost_basis = std::max(holding.cost_basis, Price{0});
    }
    holding.total -= last_quantity;
    if (holding.total == 0) {
      holding.cost_basis = 0;
      holding.day_buy = 0;
    }
    cash_ += notional;
  }
}

void Portfolio::apply_fee(Price fee) {
  if (fee <= 0) return;
  total_fees_ += fee;
  cash_ -= fee;
}

void Portfolio::fund_settled(const StrategyId& strategy, InstrumentId instrument, Quantity quantity, Price avg_cost) {
  if (quantity <= 0 || avg_cost < 0) return;
  const StrategyId basket = strategy.empty() ? StrategyId{kPassthroughStrategy} : strategy;
  Holding& holding = holdings_[BasketKey{basket, instrument}];
  holding.total += quantity;
  holding.cost_basis += quantity * avg_cost;
}

void Portfolio::update_mark(InstrumentId instrument, Price mark) {
  for (auto& entry : holdings_) {
    if (entry.first.instrument == instrument) entry.second.mark = mark;
  }
}

void Portfolio::settle_day() {
  for (auto& entry : holdings_) entry.second.day_buy = 0;
}

PositionView Portfolio::view_of(const Holding& holding, const InstrumentId& instrument) {
  PositionView view{};
  view.instrument_id = instrument;
  view.total = holding.total;
  const Quantity sellable = holding.total - holding.day_buy;
  view.sellable = sellable > 0 ? sellable : 0;
  view.average_cost = holding.total > 0 ? holding.cost_basis / holding.total : 0;
  return view;
}

PositionView Portfolio::position(const InstrumentId& instrument) const {
  Quantity total = 0;
  Quantity day_buy = 0;
  Price basis = 0;
  for (const auto& entry : holdings_) {
    if (entry.first.instrument != instrument) continue;
    total += entry.second.total;
    day_buy += entry.second.day_buy;
    basis += entry.second.cost_basis;
  }
  Holding aggregate{};
  aggregate.total = total;
  aggregate.day_buy = day_buy;
  aggregate.cost_basis = basis;
  return view_of(aggregate, instrument);
}

std::unordered_map<InstrumentId, PositionView> Portfolio::all_positions() const {
  std::unordered_map<InstrumentId, PositionView> out;
  for (const auto& entry : holdings_) {
    if (entry.second.total == 0) continue;
    if (out.find(entry.first.instrument) != out.end()) continue;
    out.emplace(entry.first.instrument, position(entry.first.instrument));
  }
  return out;
}

Price Portfolio::realized_pnl() const {
  Price total = 0;
  for (const auto& entry : holdings_) total += entry.second.realized;
  return total;
}

namespace {

Price saturating_add(Price running, Price term) {
  if (term > 0 && running > std::numeric_limits<Price>::max() - term) return std::numeric_limits<Price>::max();
  if (term < 0 && running < std::numeric_limits<Price>::min() - term) return std::numeric_limits<Price>::min();
  return running + term;
}

// True when total*unit overflows Price. Each branch pins the divisor sign
// first, so there is no divide-by-zero and no sign confusion for shorts.
bool multiplication_overflows(Quantity total, Price unit) {
  if (unit > 0) return total > std::numeric_limits<Price>::max() / unit || total < std::numeric_limits<Price>::min() / unit;
  if (unit < 0) return total < std::numeric_limits<Price>::max() / unit || total > std::numeric_limits<Price>::min() / unit;
  return false;
}

}  // namespace

Price Portfolio::equity() const {
  Price total = cash_;
  for (const auto& entry : holdings_) {
    const Holding& holding = entry.second;
    if (holding.total == 0) continue;
    const Price average = holding.cost_basis / holding.total;
    const Price unit = holding.mark > 0 ? holding.mark : average;
    if (multiplication_overflows(holding.total, unit)) {
      return (holding.total > 0) == (unit > 0) ? std::numeric_limits<Price>::max() : std::numeric_limits<Price>::min();
    }
    total = saturating_add(total, holding.total * unit);
  }
  return total;
}

Price Portfolio::unrealized_pnl(const InstrumentId& instrument) const {
  Price total = 0;
  for (const auto& entry : holdings_) {
    if (entry.first.instrument != instrument || entry.second.total == 0) continue;
    const Holding& holding = entry.second;
    const Price average = holding.cost_basis / holding.total;
    total += holding.total * (holding.mark - average);
  }
  return total;
}

PositionView Portfolio::position_for(const StrategyId& strategy, const InstrumentId& instrument) const {
  const auto iter = holdings_.find(BasketKey{strategy, instrument});
  if (iter == holdings_.end()) {
    PositionView view{};
    view.instrument_id = instrument;
    return view;
  }
  return view_of(iter->second, instrument);
}

std::unordered_map<InstrumentId, PositionView> Portfolio::all_positions_for(const StrategyId& strategy) const {
  std::unordered_map<InstrumentId, PositionView> out;
  for (const auto& entry : holdings_) {
    if (entry.first.strategy != strategy || entry.second.total == 0) continue;
    out.emplace(entry.first.instrument, view_of(entry.second, entry.first.instrument));
  }
  return out;
}

Price Portfolio::realized_for(const StrategyId& strategy) const {
  Price total = 0;
  for (const auto& entry : holdings_) {
    if (entry.first.strategy == strategy) total += entry.second.realized;
  }
  return total;
}

std::vector<StrategyId> Portfolio::strategies() const {
  std::unordered_map<StrategyId, bool> seen;
  std::vector<StrategyId> out;
  for (const auto& entry : holdings_) {
    if (entry.second.total == 0 && entry.second.realized == 0) continue;
    if (seen.find(entry.first.strategy) == seen.end()) {
      seen.emplace(entry.first.strategy, true);
      out.push_back(entry.first.strategy);
    }
  }
  return out;
}

}  // namespace sts
