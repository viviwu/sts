// clang-format off
/**
  ******************************************************************************
  * @file           : reconcile_test.cpp
  * @author         : vivi wu
  * @brief          : Settlement diff tests: clean books, cash and position gaps.
  * @version        : 0.1.0
  * @date           : 11/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/research/reconcile.hpp"

#include <cassert>
#include <string>

namespace {

sts::OrderIntent buy(const std::string& id, sts::Quantity qty, sts::Price price) {
  return sts::OrderIntent{id, "paper", "test", "TEST", sts::Side::buy, sts::OrderType::limit, sts::TimeInForce::day, qty, price};
}

sts::TradeAccountSnapshot broker_snapshot(sts::Price cash) {
  sts::TradeAccountSnapshot snapshot{};
  snapshot.account_id = "paper";
  snapshot.total_asset = cash;
  snapshot.available_cash = cash;
  snapshot.withdrawable_cash = cash;
  return snapshot;
}

void matching_books_are_clean() {
  sts::Portfolio book{1'000'000};
  book.apply_fill(buy("b1", 100, 900), 100, 900);
  auto broker = broker_snapshot(book.cash());
  sts::PositionSnapshot position{};
  position.instrument_id = "TEST";
  position.total_quantity = 100;
  position.sellable_quantity = 0;
  position.average_cost = 900;
  broker.positions.push_back(position);
  const auto diff = sts::reconcile::diff_books(book, broker);
  assert(diff.clean() && diff.cash_dirty() == 0 && diff.position_dirty() == 0);
  assert(diff.sts_fees_disclosed == 0);
  // Broker-only fields disclose without judging.
  broker.total_asset = 5;
  broker.frozen_cash = 7;
  const auto disclosed = sts::reconcile::diff_books(book, broker);
  assert(disclosed.clean());
}

void cash_gap_is_dirty() {
  sts::Portfolio book{1'000'000};
  const auto diff = sts::reconcile::diff_books(book, broker_snapshot(999'000));
  assert(!diff.clean() && diff.cash_dirty() == 1 && diff.position_dirty() == 0);
  assert(diff.cash[0].field == "available_cash" && diff.cash[0].sts_value == 1'000'000 && diff.cash[0].broker_value == 999'000);
}

void position_gaps_cover_both_sides() {
  sts::Portfolio book{1'000'000};
  book.apply_fill(buy("b1", 100, 900), 100, 900);
  auto broker = broker_snapshot(book.cash());
  sts::PositionSnapshot ghost{};
  ghost.instrument_id = "GHOST";
  ghost.total_quantity = 50;
  ghost.sellable_quantity = 50;
  ghost.average_cost = 100;
  broker.positions.push_back(ghost);  // broker-only holding
  auto diff = sts::reconcile::diff_books(book, broker);
  assert(!diff.clean() && diff.position_dirty() == 2);                            // TEST missing at broker, GHOST missing in book
  assert(diff.positions.size() == 2 && diff.positions[0].instrument == "GHOST");  // sorted universe
  // Same instrument, wrong average: still dirty, frozen disclosed.
  sts::PositionSnapshot wrong{};
  wrong.instrument_id = "TEST";
  wrong.total_quantity = 100;
  wrong.average_cost = 950;
  wrong.frozen_quantity = 10;
  auto broker2 = broker_snapshot(book.cash());
  broker2.positions.push_back(wrong);
  const auto diff2 = sts::reconcile::diff_books(book, broker2);
  assert(!diff2.clean() && diff2.position_dirty() == 1);
  assert(diff2.positions[0].broker_frozen == 10 && diff2.positions[0].sts_avg == 900);
}

void fees_are_disclosed_not_judged() {
  sts::Portfolio book{1'000'000};
  book.apply_fill(buy("b1", 100, 900), 100, 900);
  book.apply_fee(250);
  auto broker = broker_snapshot(book.cash());
  sts::PositionSnapshot position{};
  position.instrument_id = "TEST";
  position.total_quantity = 100;
  position.average_cost = 900;
  broker.positions.push_back(position);
  const auto diff = sts::reconcile::diff_books(book, broker);
  assert(diff.clean() && diff.sts_fees_disclosed == 250);
}

}  // namespace

int main() {
  matching_books_are_clean();
  cash_gap_is_dirty();
  position_gaps_cover_both_sides();
  fees_are_disclosed_not_judged();
  return 0;
}
