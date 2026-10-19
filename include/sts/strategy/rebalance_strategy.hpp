// clang-format off
/**
  ******************************************************************************
  * @file           : rebalance_strategy.hpp
  * @author         : vivi wu
  * @brief          : Target-weight rebalance strategy emitting OrderIntents only.
  * @version        : 0.1.0
  * @date           : 04/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "sts/core/types.hpp"
#include "sts/strategy/strategy_api.hpp"

namespace sts {

// One target line: goal weight in basis points of the strategy's equity
// (cash + own-basket notionals); weights sum to (0, 10000], the rest is an
// implicit cash buffer. Identity and lot come from the instrument master
// (unknown/100 when the master is absent, mirroring the grid fallback).
struct RebalanceTarget {
  InstrumentId instrument_id;
  Exchange exchange{Exchange::unknown};
  AshareBoard board{AshareBoard::unknown};
  SecurityAccountId security_account_id;
  int weight_bps{0};
  Quantity lot_step{100};
};

struct RebalanceConfig {
  StrategyId strategy_id;
  AccountId account_id;
  std::vector<RebalanceTarget> targets;
  int band_bps{0};  // per-symbol notional deadband; 0 trades any dust-filtered drift
  // Parent-order id prefix for cross-restart uniqueness ("<prefix><seq>").
  // Empty keeps the deterministic "rebalance-<seq>" ids; the gateway sets a
  // per-run prefix, same convention as GridConfig::id_prefix.
  std::string id_prefix;
};

// Target-weight rebalancer. History-free: it reads only the triggering
// quote plus the strategy-side book (own baskets — other strategies never
// pollute the math; shared account cash is the documented exception).
// At most one intent per quote, evaluated for the quoted symbol only:
//
//   warm-up:  no intent until every target has printed at least once
//             (fail-closed on partial universes);
//   drift:    |goal - current| within band_bps of equity => hold;
//   sizing:   |diff| / price floored to lot_step (dust skipped);
//             sells additionally clamp to own-basket sellable (T+1 safe).
class RebalanceStrategy final : public Strategy {
 public:
  explicit RebalanceStrategy(RebalanceConfig config);

  [[nodiscard]] std::string_view name() const noexcept override { return "rebalance"; }
  std::vector<OrderIntent> on_quote(const QuoteEvent& quote, const Portfolio& book) override;

 private:
  OrderIntent make_order(const RebalanceTarget& target, Side side, Quantity quantity, Price price);

  RebalanceConfig config_;
  std::unordered_map<InstrumentId, Price> last_prices_;
  std::uint64_t sequence_{0};
};

}  // namespace sts
