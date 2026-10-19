// clang-format off
/**
  ******************************************************************************
  * @file           : paper_engine_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for paper matching, latency, books and re-matching.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

#include "sts/broker/simulated_broker.hpp"
#include "sts/core/clock.hpp"
#include "sts/oms/order_manager.hpp"

namespace {

sts::OrderIntent make_intent(const std::string& id, sts::Side side = sts::Side::buy, sts::OrderType type = sts::OrderType::limit, sts::Quantity qty = 10, sts::Price price = 100) {
  return sts::OrderIntent{id, "paper", "test", "TEST", side, type, sts::TimeInForce::day, qty, price};
}

struct Capture {
  std::vector<sts::ExecutionReport> reports;
  void attach(sts::SimulatedBroker& broker) {
    broker.set_execution_handler([this](const sts::ExecutionReport& r) { reports.push_back(r); });
  }
};

void crossing_limit_fills_at_reference() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  Capture capture{};
  capture.attach(broker);
  broker.set_reference_price("TEST", 100);

  broker.submit(make_intent("b1"));
  assert(capture.reports.size() == 2);  // live ack + fill
  assert(capture.reports[0].status == sts::OrderStatus::live);
  const auto& fill = capture.reports[1];
  assert(fill.status == sts::OrderStatus::filled);
  assert(fill.last_quantity == 10 && fill.last_price == 100);
  assert(fill.cumulative_quantity == 10 && !fill.execution_id.empty());
}

void non_crossing_limit_stays_live() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  Capture capture{};
  capture.attach(broker);
  broker.set_reference_price("TEST", 100);

  broker.submit(make_intent("b2", sts::Side::buy, sts::OrderType::limit, 10, 90));
  assert(capture.reports.size() == 1);  // live ack only
  assert(capture.reports[0].status == sts::OrderStatus::live);

  auto ask = make_intent("s1", sts::Side::sell, sts::OrderType::limit, 10, 110);
  broker.submit(ask);
  assert(capture.reports.size() == 2);  // sell above ref stays working too
}

void market_fills_without_limit_price() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  Capture capture{};
  capture.attach(broker);
  broker.set_reference_price("TEST", 105);

  auto intent = make_intent("m1", sts::Side::buy, sts::OrderType::market, 5, 0);
  broker.submit(intent);
  assert(capture.reports.size() == 2);
  assert(capture.reports[1].status == sts::OrderStatus::filled);
  assert(capture.reports[1].last_price == 105);
}

void no_reference_price_means_working() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  Capture capture{};
  capture.attach(broker);

  broker.submit(make_intent("w1"));
  assert(capture.reports.size() == 1);
  assert(capture.reports[0].status == sts::OrderStatus::live);
}

void cancel_filled_is_ignored() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  Capture capture{};
  capture.attach(broker);
  broker.set_reference_price("TEST", 100);

  broker.submit(make_intent("f1"));
  assert(capture.reports.size() == 2);
  broker.cancel("f1");  // already filled: no further report
  assert(capture.reports.size() == 2);
  broker.cancel("unknown");  // unknown: no report, no throw
  assert(capture.reports.size() == 2);
}

void duplicate_submit_is_rejected() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  Capture capture{};
  capture.attach(broker);

  broker.submit(make_intent("d1"));
  broker.submit(make_intent("d1"));
  assert(!capture.reports.empty());
  assert(capture.reports.back().status == sts::OrderStatus::rejected);
}

void session_disconnect_throws() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  broker.disconnect();
  assert(!broker.connected());

  bool threw = false;
  try {
    broker.submit(make_intent("x1"));
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);

  broker.reconnect();
  assert(broker.connected());
  Capture capture{};
  capture.attach(broker);
  broker.submit(make_intent("x1"));  // works again
  assert(capture.reports.size() == 1);
}

void snapshot_reflects_fills() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  Capture capture{};
  capture.attach(broker);
  broker.set_starting_cash(1'000'000);
  broker.set_reference_price("TEST", 100);
  broker.submit(make_intent("p1", sts::Side::buy, sts::OrderType::limit, 10, 100));

  bool called = false;
  broker.set_account_snapshot_handler([&](const sts::TradeAccountSnapshot& snapshot) {
    called = true;
    assert(snapshot.account_id == "paper");
    assert(snapshot.available_cash == 1'000'000 - 1'000);
    assert(snapshot.positions.size() == 1);
    assert(snapshot.positions[0].instrument_id == "TEST");
    assert(snapshot.positions[0].total_quantity == 10);
    assert(snapshot.positions[0].average_cost == 100);
    assert(snapshot.positions[0].sellable_quantity == 0);  // T+1: same-day buy
  });
  broker.request_account_snapshot("paper");
  assert(called);

  broker.settle_day();
  bool settled = false;
  broker.set_account_snapshot_handler([&](const sts::TradeAccountSnapshot& snapshot) {
    settled = true;
    assert(snapshot.positions[0].sellable_quantity == 10);
  });
  broker.request_account_snapshot("paper");
  assert(settled);
}

void open_orders_recovery() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  sts::RiskEngine risk{{100, 10'000, true}};
  sts::OrderManager oms{broker, risk, events};  // OMS owns the execution handler

  assert(oms.submit(make_intent("r1")));  // no ref price: stays live
  assert(oms.find("r1")->status == sts::OrderStatus::live);

  bool recovered = false;
  broker.set_recovery_complete_handler([&](const sts::AccountId& account, bool success) { recovered = success && account == "paper"; });
  broker.request_open_orders("paper");
  assert(recovered);
  assert(oms.find("r1")->status == sts::OrderStatus::live);  // still working after replay
}

void latency_delays_delivery() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  sts::ManualClock clock{};
  broker.set_clock(&clock);
  broker.set_latency(std::chrono::seconds{5});
  Capture capture{};
  capture.attach(broker);
  broker.set_reference_price("TEST", 100);

  broker.submit(make_intent("l1"));
  assert(capture.reports.empty() && broker.pending_reports() == 2);  // ack + fill queued
  clock.advance(std::chrono::seconds{3});
  broker.deliver_due();
  assert(capture.reports.empty());  // not due yet
  clock.advance(std::chrono::seconds{2});
  broker.deliver_due();
  assert(capture.reports.size() == 2);  // FIFO: live ack first
  assert(capture.reports[0].status == sts::OrderStatus::live);
  assert(capture.reports[1].status == sts::OrderStatus::filled);
  assert(broker.pending_reports() == 0);
}

void unacked_orders_cannot_cancel() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  sts::ManualClock clock{};
  broker.set_clock(&clock);
  broker.set_latency(std::chrono::seconds{5});
  broker.set_reference_price("TEST", 100);
  sts::RiskEngine risk{{100, 10'000, true}};
  sts::OrderManager oms{broker, risk, events};

  assert(oms.submit(make_intent("l2")));
  assert(oms.find("l2")->status == sts::OrderStatus::pending_new);  // ack still queued
  assert(!oms.cancel("l2"));                                        // like the real world: nothing to cancel yet
  clock.advance(std::chrono::seconds{5});
  broker.deliver_due();
  assert(oms.find("l2")->status == sts::OrderStatus::filled);  // fill arrived first
  assert(!oms.cancel("l2"));                                   // terminal: no cancel
}

void disconnect_holds_pending_reports() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  sts::ManualClock clock{};
  broker.set_clock(&clock);
  broker.set_latency(std::chrono::seconds{5});
  Capture capture{};
  capture.attach(broker);
  broker.set_reference_price("TEST", 100);

  broker.submit(make_intent("l3"));
  broker.disconnect();
  clock.advance(std::chrono::seconds{10});
  broker.deliver_due();
  assert(capture.reports.empty() && broker.pending_reports() == 2);  // held, not dropped
  broker.reconnect();
  broker.deliver_due();
  assert(capture.reports.size() == 2);
}

void slippage_moves_against_taker() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  broker.set_slippage_per_unit(2);
  Capture capture{};
  capture.attach(broker);
  broker.set_reference_price("TEST", 100);

  broker.submit(make_intent("s1", sts::Side::buy));
  assert(capture.reports.back().last_price == 102);  // buyer pays up
  broker.submit(make_intent("s2", sts::Side::sell));
  assert(capture.reports.back().last_price == 98);  // seller gives up
}

void fees_deducted_and_tracked() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  broker.set_starting_cash(1'000'000);
  broker.set_fee_rate_bp(100);  // 1%
  Capture capture{};
  capture.attach(broker);
  broker.set_reference_price("TEST", 100);

  broker.submit(make_intent("f1", sts::Side::buy));  // notional 1000 -> fee 10
  assert(broker.total_fees() == 10);
  broker.submit(make_intent("f2", sts::Side::sell));
  assert(broker.total_fees() == 20);

  bool called = false;
  broker.set_account_snapshot_handler([&](const sts::TradeAccountSnapshot& snapshot) {
    called = true;
    assert(snapshot.available_cash == 1'000'000 - 1'010 + 990);
  });
  broker.request_account_snapshot("paper");
  assert(called);
}

sts::OrderBook asks_only(std::vector<sts::BookLevel> asks) {
  sts::OrderBook book{};
  book.asks = std::move(asks);
  return book;
}

void book_buy_partial_rests_live() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  Capture capture{};
  capture.attach(broker);
  broker.set_order_book("TEST", asks_only({{100, 10}}));

  broker.submit(make_intent("k1", sts::Side::buy, sts::OrderType::limit, 15, 100));
  assert(capture.reports.size() == 2);  // live ack + partial
  assert(capture.reports[0].status == sts::OrderStatus::live);
  const auto& partial = capture.reports[1];
  assert(partial.status == sts::OrderStatus::partially_filled);
  assert(partial.last_quantity == 10 && partial.last_price == 100);
  assert(partial.cumulative_quantity == 10 && !partial.execution_id.empty());

  const auto* book = broker.order_book("TEST");
  assert(book != nullptr && book->asks.empty());  // depth consumed

  broker.cancel("k1");  // remainder cancellable while live
  assert(capture.reports.size() == 3);
  assert(capture.reports[2].status == sts::OrderStatus::cancelled);
  assert(capture.reports[2].cumulative_quantity == 10);
}

void book_limit_outside_touch_rests_untouched() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  Capture capture{};
  capture.attach(broker);
  broker.set_order_book("TEST", asks_only({{100, 10}}));

  broker.submit(make_intent("k2", sts::Side::buy, sts::OrderType::limit, 5, 90));
  assert(capture.reports.size() == 1);  // live ack only
  const auto* book = broker.order_book("TEST");
  assert(book != nullptr && book->asks.size() == 1 && book->asks[0].quantity == 10);
}

void book_market_sweeps_levels_with_vwap() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  Capture capture{};
  capture.attach(broker);
  broker.set_order_book("TEST", asks_only({{100, 10}, {110, 10}}));

  broker.submit(make_intent("k3", sts::Side::buy, sts::OrderType::market, 15, 0));
  assert(capture.reports.size() == 2);
  const auto& fill = capture.reports[1];
  assert(fill.status == sts::OrderStatus::filled);
  assert(fill.last_quantity == 15 && fill.last_price == 103);  // (10*100 + 5*110) / 15
  const auto* book = broker.order_book("TEST");
  assert(book != nullptr && book->asks.size() == 1);
  assert(book->asks[0].price == 110 && book->asks[0].quantity == 5);
}

void book_sell_takes_bids_best_first() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  Capture capture{};
  capture.attach(broker);
  sts::OrderBook book{};
  book.bids = {{105, 4}, {100, 10}};
  broker.set_order_book("TEST", book);

  broker.submit(make_intent("k4", sts::Side::sell, sts::OrderType::limit, 6, 100));
  assert(capture.reports.size() == 2);
  const auto& fill = capture.reports[1];
  assert(fill.status == sts::OrderStatus::filled);
  assert(fill.last_quantity == 6 && fill.last_price == 103);  // (4*105 + 2*100) / 6
  const auto* rest = broker.order_book("TEST");
  assert(rest != nullptr && rest->bids.size() == 1);
  assert(rest->bids[0].price == 100 && rest->bids[0].quantity == 8);
}

void book_empty_side_no_fill() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  Capture capture{};
  capture.attach(broker);
  broker.set_order_book("TEST", asks_only({{100, 10}}));  // no bids: sell rests

  broker.submit(make_intent("k5", sts::Side::sell, sts::OrderType::limit, 5, 90));
  assert(capture.reports.size() == 1);
  assert(capture.reports[0].status == sts::OrderStatus::live);
}

void book_partial_flows_through_oms() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  broker.set_order_book("TEST", asks_only({{100, 10}}));
  sts::RiskEngine risk{{100, 10'000, true}};
  sts::OrderManager oms{broker, risk, events};

  assert(oms.submit(make_intent("k6", sts::Side::buy, sts::OrderType::limit, 15, 100)));
  const auto* found = oms.find("k6");
  assert(found != nullptr && found->status == sts::OrderStatus::partially_filled);
  assert(found->cumulative_quantity == 10 && !found->execution_id.empty());

  assert(oms.cancel("k6"));
  assert(oms.find("k6")->status == sts::OrderStatus::cancelled);
}

void book_top_up_fills_resting_remainder() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  Capture capture{};
  capture.attach(broker);
  broker.set_order_book("TEST", asks_only({{100, 10}}));

  broker.submit(make_intent("t1", sts::Side::buy, sts::OrderType::limit, 15, 100));
  assert(capture.reports.size() == 2);
  assert(capture.reports[1].status == sts::OrderStatus::partially_filled);

  broker.set_order_book("TEST", asks_only({{100, 5}}));  // fresh depth re-matches resting
  assert(capture.reports.size() == 3);
  const auto& second = capture.reports[2];
  assert(second.status == sts::OrderStatus::filled);
  assert(second.last_quantity == 5 && second.cumulative_quantity == 15);
  assert(second.execution_id != capture.reports[1].execution_id);  // distinct fills, no dedup collapse
  const auto* book = broker.order_book("TEST");
  assert(book != nullptr && book->asks.empty());
}

void book_resting_matches_in_submit_order() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  Capture capture{};
  capture.attach(broker);
  broker.set_order_book("TEST", asks_only({}));  // book present but empty: both rest

  broker.submit(make_intent("q1", sts::Side::buy, sts::OrderType::limit, 10, 100));
  broker.submit(make_intent("q2", sts::Side::buy, sts::OrderType::limit, 10, 100));
  assert(capture.reports.size() == 2);

  broker.set_order_book("TEST", asks_only({{100, 15}}));
  assert(capture.reports.size() == 4);
  assert(capture.reports[2].client_order_id == "q1" && capture.reports[2].status == sts::OrderStatus::filled);
  assert(capture.reports[3].client_order_id == "q2" && capture.reports[3].status == sts::OrderStatus::partially_filled);
  assert(capture.reports[3].cumulative_quantity == 5);
}

void book_partial_recovery_converges() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  broker.set_order_book("TEST", asks_only({{100, 10}}));
  sts::RiskEngine risk{{100, 10'000, true}};

  sts::ExecutionReport snapshot{};
  {
    sts::OrderManager oms1{broker, risk, events};
    assert(oms1.submit(make_intent("v1", sts::Side::buy, sts::OrderType::limit, 15, 100)));
    snapshot = *oms1.find("v1");
    assert(snapshot.status == sts::OrderStatus::partially_filled);
  }  // oms1 releases the execution handler

  sts::OrderManager oms2{broker, risk, events};
  assert(oms2.restore_order(make_intent("v1", sts::Side::buy, sts::OrderType::limit, 15, 100), snapshot));
  broker.request_open_orders("paper");
  const auto* found = oms2.find("v1");
  assert(found != nullptr && found->status == sts::OrderStatus::partially_filled);
  assert(found->cumulative_quantity == 10);
}

}  // namespace

int main() {
  crossing_limit_fills_at_reference();
  non_crossing_limit_stays_live();
  market_fills_without_limit_price();
  no_reference_price_means_working();
  cancel_filled_is_ignored();
  duplicate_submit_is_rejected();
  session_disconnect_throws();
  snapshot_reflects_fills();
  open_orders_recovery();
  latency_delays_delivery();
  unacked_orders_cannot_cancel();
  disconnect_holds_pending_reports();
  slippage_moves_against_taker();
  fees_deducted_and_tracked();
  book_buy_partial_rests_live();
  book_limit_outside_touch_rests_untouched();
  book_market_sweeps_levels_with_vwap();
  book_sell_takes_bids_best_first();
  book_empty_side_no_fill();
  book_partial_flows_through_oms();
  book_top_up_fills_resting_remainder();
  book_resting_matches_in_submit_order();
  book_partial_recovery_converges();
  return 0;
}
