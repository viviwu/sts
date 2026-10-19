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
#include "qts/research/reconcile.hpp"

#include <cassert>
#include <string>

namespace {

qts::OrderIntent buy(const std::string& id, qts::Quantity qty, qts::Price price) {
  return qts::OrderIntent{id, "paper", "test", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, qty, price};
}

qts::TradeAccountSnapshot broker_snapshot(qts::Price cash) {
  qts::TradeAccountSnapshot snapshot{};
  snapshot.account_id = "paper";
  snapshot.total_asset = cash;
  snapshot.available_cash = cash;
  snapshot.withdrawable_cash = cash;
  return snapshot;
}

void matching_books_are_clean() {
  qts::Portfolio book{1'000'000};
  book.apply_fill(buy("b1", 100, 900), 100, 900);
  auto broker = broker_snapshot(book.cash());
  qts::PositionSnapshot position{};
  position.instrument_id = "TEST";
  position.total_quantity = 100;
  position.sellable_quantity = 0;
  position.average_cost = 900;
  broker.positions.push_back(position);
  const auto diff = qts::reconcile::diff_books(book, broker);
  assert(diff.clean() && diff.cash_dirty() == 0 && diff.position_dirty() == 0);
  assert(diff.qts_fees_disclosed == 0);
  // Broker-only fields disclose without judging.
  broker.total_asset = 5;
  broker.frozen_cash = 7;
  const auto disclosed = qts::reconcile::diff_books(book, broker);
  assert(disclosed.clean());
}

void cash_gap_is_dirty() {
  qts::Portfolio book{1'000'000};
  const auto diff = qts::reconcile::diff_books(book, broker_snapshot(999'000));
  assert(!diff.clean() && diff.cash_dirty() == 1 && diff.position_dirty() == 0);
  assert(diff.cash[0].field == "available_cash" && diff.cash[0].qts_value == 1'000'000 && diff.cash[0].broker_value == 999'000);
}

void position_gaps_cover_both_sides() {
  qts::Portfolio book{1'000'000};
  book.apply_fill(buy("b1", 100, 900), 100, 900);
  auto broker = broker_snapshot(book.cash());
  qts::PositionSnapshot ghost{};
  ghost.instrument_id = "GHOST";
  ghost.total_quantity = 50;
  ghost.sellable_quantity = 50;
  ghost.average_cost = 100;
  broker.positions.push_back(ghost);  // broker-only holding
  auto diff = qts::reconcile::diff_books(book, broker);
  assert(!diff.clean() && diff.position_dirty() == 2);                            // TEST missing at broker, GHOST missing in book
  assert(diff.positions.size() == 2 && diff.positions[0].instrument == "GHOST");  // sorted universe
  // Same instrument, wrong average: still dirty, frozen disclosed.
  qts::PositionSnapshot wrong{};
  wrong.instrument_id = "TEST";
  wrong.total_quantity = 100;
  wrong.average_cost = 950;
  wrong.frozen_quantity = 10;
  auto broker2 = broker_snapshot(book.cash());
  broker2.positions.push_back(wrong);
  const auto diff2 = qts::reconcile::diff_books(book, broker2);
  assert(!diff2.clean() && diff2.position_dirty() == 1);
  assert(diff2.positions[0].broker_frozen == 10 && diff2.positions[0].qts_avg == 900);
}

void fees_are_disclosed_not_judged() {
  qts::Portfolio book{1'000'000};
  book.apply_fill(buy("b1", 100, 900), 100, 900);
  book.apply_fee(250);
  auto broker = broker_snapshot(book.cash());
  qts::PositionSnapshot position{};
  position.instrument_id = "TEST";
  position.total_quantity = 100;
  position.average_cost = 900;
  broker.positions.push_back(position);
  const auto diff = qts::reconcile::diff_books(book, broker);
  assert(diff.clean() && diff.qts_fees_disclosed == 250);
}

}  // namespace

int main() {
  matching_books_are_clean();
  cash_gap_is_dirty();
  position_gaps_cover_both_sides();
  fees_are_disclosed_not_judged();
  return 0;
}
