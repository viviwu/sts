// clang-format off
/**
  ******************************************************************************
  * @file           : grid_strategy.hpp
  * @author         : vivi wu
  * @brief          : Grid signal strategy emitting OrderIntents only (never trades directly).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "qts/core/types.hpp"
#include "qts/strategy/strategy_api.hpp"

namespace qts {

struct GridConfig {
  StrategyId strategy_id;
  AccountId account_id;
  InstrumentId instrument_id;
  Exchange exchange{Exchange::unknown};
  AshareBoard board{AshareBoard::unknown};
  SecurityAccountId security_account_id;
  Price lower{0};
  Price upper{0};
  int grids{0};  // intervals; levels = grids + 1
  Quantity quantity_per_order{0};
  // Parent-order id prefix for cross-restart uniqueness ("<prefix><seq>").
  // Empty keeps the legacy "grid-<seq>" ids (deterministic unit/replay
  // runs); the gateway sets a per-run prefix (strategy/date/pid) so
  // restarted processes never re-mint ids already in the recovered book.
  std::string id_prefix;
};

// Spot grid drill strategy. Each level is an independent buy-low/sell-high
// flip-flop with one-grid profit target:
//   buy_armed:  fires a limit buy at L when price <= L, then arms the sell;
//   sell_armed: fires a limit sell at L + step when price >= L + step.
// Levels below the start price begin buy-armed, levels at/above begin
// sell-armed (the latter assumes starting inventory; production sizing must
// consult the portfolio, which lands in M2). The top level's sell target
// sits above the range and never fires; likewise the bottom buy always
// re-arms on deep dips. Client order ids are sequential and deterministic.
class GridStrategy final : public Strategy {
 public:
  GridStrategy(GridConfig config, Price start_price);

  [[nodiscard]] std::string_view name() const noexcept override { return "grid"; }
  // NOTE: position-aware sizing is future work; the drill grid assumes
  // inventory (shorts allowed in paper) and ignores the book.
  std::vector<OrderIntent> on_quote(const QuoteEvent& quote, const Portfolio& book) override;

  // For tests and drills: current grid lines, low to high.
  [[nodiscard]] const std::vector<Price>& levels() const { return levels_; }

 private:
  enum class LevelState : std::uint8_t { buy_armed, sell_armed };

  OrderIntent make_order(Side side, Price price);

  GridConfig config_;
  Price step_{0};
  std::vector<Price> levels_;
  std::vector<LevelState> states_;
  std::uint64_t sequence_{0};
};

}  // namespace qts
