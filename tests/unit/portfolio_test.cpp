// clang-format off
/**
  ******************************************************************************
  * @file           : portfolio_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for positions, cash, PnL and T+1 sellable.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/portfolio/portfolio.hpp"

#include <cassert>
#include <string>

namespace {

sts::OrderIntent buy(const std::string& id, sts::Quantity qty, sts::Price price) {
  return sts::OrderIntent{id, "paper", "test", "TEST", sts::Side::buy, sts::OrderType::limit, sts::TimeInForce::day, qty, price};
}

sts::OrderIntent sell(const std::string& id, sts::Quantity qty, sts::Price price) {
  auto intent = buy(id, qty, price);
  intent.side = sts::Side::sell;
  return intent;
}

void average_cost_weights_fills() {
  sts::Portfolio book{1'000'000};
  book.apply_fill(buy("b1", 10, 100), 10, 100);
  book.apply_fill(buy("b2", 10, 200), 10, 200);
  const auto position = book.position("TEST");
  assert(position.total == 20);
  assert(position.average_cost == 150);  // (1000 + 2000) / 20
  assert(book.cash() == 1'000'000 - 3'000);
}

void sells_realize_pnl_and_reset_on_flat() {
  sts::Portfolio book{1'000'000};
  book.apply_fill(buy("b1", 10, 100), 10, 100);
  book.apply_fill(sell("s1", 4, 150), 4, 150);
  assert(book.realized_pnl() == 4 * 50);
  assert(book.position("TEST").total == 6);
  book.apply_fill(sell("s2", 6, 120), 6, 120);
  assert(book.realized_pnl() == 4 * 50 + 6 * 20);
  const auto flat = book.position("TEST");
  assert(flat.total == 0 && flat.average_cost == 0);
  assert(book.cash() == 1'000'000 - 1'000 + 600 + 720);
}

void same_day_buys_are_not_sellable() {
  sts::Portfolio book{1'000'000};
  book.apply_fill(buy("b1", 100, 10), 100, 10);
  auto position = book.position("TEST");
  assert(position.total == 100 && position.sellable == 0);  // T+1
  book.settle_day();
  assert(book.position("TEST").sellable == 100);
  assert(book.position("UNKNOWN").total == 0);  // unknown instrument is empty
}

void unrealized_needs_marks() {
  sts::Portfolio book{1'000'000};
  assert(book.unrealized_pnl("TEST") == 0);
  book.apply_fill(buy("b1", 10, 100), 10, 100);
  book.update_mark("TEST", 130);
  assert(book.unrealized_pnl("TEST") == 10 * 30);
}

void fees_hit_cash_and_accumulate() {
  sts::Portfolio book{1'000'000};
  book.apply_fee(0);  // no-op
  assert(book.total_fees() == 0 && book.cash() == 1'000'000);
  book.apply_fee(25);
  assert(book.total_fees() == 25 && book.cash() == 1'000'000 - 25);
}

void zero_fill_is_ignored() {
  sts::Portfolio book{1'000'000};
  book.apply_fill(buy("b1", 10, 100), 0, 100);
  assert(book.position("TEST").total == 0 && book.cash() == 1'000'000);
}

sts::OrderIntent buy_as(const std::string& id, const std::string& strategy, sts::Quantity qty, sts::Price price) {
  auto intent = buy(id, qty, price);
  intent.strategy_id = strategy;
  return intent;
}

void baskets_split_by_strategy_and_aggregate_exactly() {
  sts::Portfolio book{1'000'000};
  book.apply_fill(buy_as("a1", "alpha", 10, 100), 10, 100);
  book.apply_fill(buy_as("b1", "beta", 10, 200), 10, 200);
  // Per-basket books are independent lots.
  assert(book.position_for("alpha", "TEST").total == 10);
  assert(book.position_for("alpha", "TEST").average_cost == 100);
  assert(book.position_for("beta", "TEST").average_cost == 200);
  // Account view aggregates exactly: (1000 + 2000) / 20.
  const auto account = book.position("TEST");
  assert(account.total == 20 && account.average_cost == 150);
  assert(book.cash() == 1'000'000 - 3'000);
  // Realized PnL attributes to the selling basket only.
  auto sell_alpha = buy_as("a2", "alpha", 10, 150);
  sell_alpha.side = sts::Side::sell;
  book.apply_fill(sell_alpha, 10, 150);
  assert(book.realized_for("alpha") == 10 * 50);
  assert(book.realized_for("beta") == 0);
  assert(book.realized_pnl() == 10 * 50);
  assert(book.position("TEST").total == 10);  // beta's lot remains
}

void empty_strategy_falls_into_passthrough() {
  sts::Portfolio book{1'000'000};
  auto manual = buy("m1", 10, 100);
  manual.strategy_id.clear();
  book.apply_fill(manual, 10, 100);
  assert(book.position_for(sts::kPassthroughStrategy, "TEST").total == 10);
  assert(book.position_for("", "TEST").total == 0);
  assert(book.position("TEST").total == 10);
}

void funded_inventory_is_settled_and_cash_neutral() {
  sts::Portfolio book{1'000'000};
  book.fund_settled("grid-1", "TEST", 1000, 950);
  const auto position = book.position("TEST");
  assert(position.total == 1000 && position.sellable == 1000);
  assert(position.average_cost == 950);
  assert(book.cash() == 1'000'000);  // holdings are existing inventory
  assert(book.position_for("grid-1", "TEST").total == 1000);
  // Same-day buys still reduce sellable after funding.
  book.apply_fill(buy("b1", 100, 1000), 100, 1000);
  const auto after = book.position("TEST");
  assert(after.total == 1100 && after.sellable == 1000);
}

void equity_marks_or_falls_back_to_cost() {
  sts::Portfolio book{1'000'000};
  assert(book.equity() == 1'000'000);            // cash only
  book.apply_fill(buy("b1", 10, 100), 10, 100);  // cash 999000, 10 @ 100
  assert(book.equity() == 1'000'000);            // unmarked: cost basis back
  book.update_mark("TEST", 150);
  assert(book.equity() == 1'000'000 + 500);  // marked: 10 * (150 - 100)
  book.apply_fill(sell("s1", 4, 150), 4, 150);
  assert(book.equity() == 1'000'000 + 500);  // sale at mark: equity unchanged
}

}  // namespace

int main() {
  average_cost_weights_fills();
  sells_realize_pnl_and_reset_on_flat();
  same_day_buys_are_not_sellable();
  unrealized_needs_marks();
  fees_hit_cash_and_accumulate();
  zero_fill_is_ignored();
  baskets_split_by_strategy_and_aggregate_exactly();
  empty_strategy_falls_into_passthrough();
  funded_inventory_is_settled_and_cash_neutral();
  equity_marks_or_falls_back_to_cost();
  return 0;
}
