// clang-format off
/**
  ******************************************************************************
  * @file           : portfolio.hpp
  * @author         : vivi wu
  * @brief          : Position, cash, average-cost, realized and floating PnL book.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "sts/core/types.hpp"

namespace sts {

struct PositionView {
  InstrumentId instrument_id;
  Quantity total{0};
  Quantity sellable{0};  // total minus same-day buys (T+1); floored at zero
  Price average_cost{0};
};

// Strategy-side book. Fed with each applied execution exactly once in
// ledger order (not idempotent by design; replay drivers own dedup).
// Cash and positions move on fills; marks are informational only.
// A-share T+1 approximation: same-day buys are not sellable until
// settle_day(). Sells are assumed to come from sellable inventory;
// sellable-aware risk gating is a later step, not enforced here.
//
// Basket model: one Portfolio operates the account, internally keeping one
// basket book per (strategy, instrument) — a strategy runs a basket of
// positions, and the account views below aggregate baskets exactly.
// Orders without a strategy normalize to kPassthroughStrategy, so every
// position is attributable. Cash and fees stay account-level (shared).
class Portfolio {
 public:
  explicit Portfolio(Price starting_cash)
      : cash_{starting_cash} {}

  void apply_fill(const OrderIntent& intent, Quantity last_quantity, Price last_price);
  void apply_fee(Price fee);
  // Opening inventory (M11-1): settled positions funded at startup
  // (day_buy = 0, immediately sellable). Cash is untouched: holdings are
  // existing inventory, not new buys.
  void fund_settled(const StrategyId& strategy, InstrumentId instrument, Quantity quantity, Price avg_cost);
  void update_mark(InstrumentId instrument, Price mark);
  void settle_day();  // new trading day: same-day buys become sellable

  [[nodiscard]] Price cash() const { return cash_; }
  [[nodiscard]] Price realized_pnl() const;
  [[nodiscard]] Price total_fees() const { return total_fees_; }
  // Net-asset equity (M13 loss-ratio base): cash plus every holding at its
  // mark, falling back to average cost when unmarked (a mark of 0 means no
  // quote yet, not a worthless position). Saturates on overflow; shorts
  // subtract through signed math.
  [[nodiscard]] Price equity() const;
  [[nodiscard]] PositionView position(const InstrumentId& instrument) const;
  [[nodiscard]] std::unordered_map<InstrumentId, PositionView> all_positions() const;
  [[nodiscard]] Price unrealized_pnl(const InstrumentId& instrument) const;

  // Per-strategy basket views (empty strategy sorts like any other id).
  [[nodiscard]] PositionView position_for(const StrategyId& strategy, const InstrumentId& instrument) const;
  [[nodiscard]] std::unordered_map<InstrumentId, PositionView> all_positions_for(const StrategyId& strategy) const;
  [[nodiscard]] Price realized_for(const StrategyId& strategy) const;
  [[nodiscard]] std::vector<StrategyId> strategies() const;

 private:
  struct Holding {
    Quantity total{0};
    Quantity day_buy{0};
    Price cost_basis{0};  // sum of buy notionals held; average = basis / total
    Price mark{0};
    Price realized{0};  // sell PnL booked inside this basket
  };

  struct BasketKey {
    StrategyId strategy;
    InstrumentId instrument;
    bool operator==(const BasketKey& other) const { return strategy == other.strategy && instrument == other.instrument; }
  };

  struct BasketKeyHash {
    std::size_t operator()(const BasketKey& key) const noexcept { return std::hash<std::string>{}(key.strategy + '\x1f' + key.instrument); }
  };

  static StrategyId basket_strategy(const OrderIntent& intent) { return intent.strategy_id.empty() ? kPassthroughStrategy : intent.strategy_id; }
  [[nodiscard]] static PositionView view_of(const Holding& holding, const InstrumentId& instrument);

  std::unordered_map<BasketKey, Holding, BasketKeyHash> holdings_;
  Price cash_{0};
  Price total_fees_{0};
};

}  // namespace sts
