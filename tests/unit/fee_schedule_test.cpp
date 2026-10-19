// clang-format off
/**
  ******************************************************************************
  * @file           : fee_schedule_test.cpp
  * @author         : vivi wu
  * @brief          : A-share fee schedule math and paper-channel application.
  * @version        : 0.1.0
  * @date           : 03/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/broker/fee_schedule.hpp"

#include <cassert>
#include <limits>

#include "sts/broker/alternate_paper_broker.hpp"
#include "sts/broker/simulated_broker.hpp"
#include "sts/core/event_bus.hpp"

namespace {

void zero_schedule_charges_nothing() {
  const sts::FeeSchedule free{};
  assert(sts::fill_fee(free, sts::Side::buy, 1'000'000) == 0);
  assert(sts::fill_fee(free, sts::Side::sell, 1'000'000) == 0);
  assert(sts::fill_fee(free, sts::Side::buy, 0) == 0);
}

void commission_with_minimum() {
  sts::FeeSchedule schedule{};
  schedule.commission_ppm = 2500;       // 万2.5
  schedule.commission_minimum = 50000;  // 5 yuan floor per fill
  // 100 shares @ 10 yuan = 1,000,000 units notional -> 2500 raw, floored to 50000.
  assert(sts::fill_fee(schedule, sts::Side::buy, 1'000'000) == 50000);
  // 10,000 shares @ 10 yuan = 100,000,000 -> 250,000 raw, above the floor.
  assert(sts::fill_fee(schedule, sts::Side::buy, 100'000'000) == 250'000);
}

void stamp_is_sell_only_and_transfer_is_both() {
  sts::FeeSchedule schedule{};
  schedule.stamp_ppm = 500;
  schedule.transfer_ppm = 100;
  // 1,000,000 notional: buy pays transfer 100 only, sell adds stamp 500.
  assert(sts::fill_fee(schedule, sts::Side::buy, 1'000'000) == 100);
  assert(sts::fill_fee(schedule, sts::Side::sell, 1'000'000) == 600);
}

void large_notionals_do_not_overflow() {
  sts::FeeSchedule schedule{};
  schedule.commission_ppm = 2500;
  schedule.stamp_ppm = 500;
  schedule.transfer_ppm = 100;
  const sts::Price huge = std::numeric_limits<sts::Price>::max() / 2;
  const sts::Price fee = sts::fill_fee(schedule, sts::Side::sell, huge);
  assert(fee > 0 && fee < huge);  // divide-first ordering, no wrap
}

sts::OrderIntent make_order(const std::string& id, sts::Side side, sts::Quantity quantity, sts::Price price) {
  sts::OrderIntent intent{id, "paper", "test", "TEST", side, sts::OrderType::limit, sts::TimeInForce::day, quantity, price};
  intent.trace_id = id;
  return intent;
}

void simulated_applies_buy_sell_mix() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  broker.set_starting_cash(100'000'000);
  sts::FeeSchedule schedule{};
  schedule.commission_ppm = 2500;
  schedule.commission_minimum = 50000;
  schedule.stamp_ppm = 500;
  schedule.transfer_ppm = 100;
  broker.set_fee_schedule(schedule);
  broker.set_reference_price("TEST", 100'000);  // 10 yuan
  // Buy 100 @ 10 yuan = 10,000,000 notional: commission 25,000->floor 50,000 + transfer 1,000.
  broker.submit(make_order("b1", sts::Side::buy, 100, 100'000));
  assert(broker.total_fees() == 51000);
  // Sell 100 @ 10 yuan: commission floor 50,000 + transfer 1,000 + stamp 5,000.
  broker.submit(make_order("s1", sts::Side::sell, 100, 100'000));
  assert(broker.total_fees() == 51000 + 56000);
}

void alternate_applies_schedule() {
  sts::EventBus events;
  sts::AlternatePaperBroker broker{events, "/tmp/sts-fee-test.seq"};
  broker.set_starting_cash(100'000'000);
  broker.set_session_open(true);
  sts::FeeSchedule schedule{};
  schedule.commission_ppm = 2500;
  schedule.commission_minimum = 50000;
  broker.set_fee_schedule(schedule);
  broker.set_reference_price("TEST", 100'000);
  // Alternate lot is 200: 200 @ 10 yuan = 20,000,000 -> commission 50,000 exactly.
  broker.submit(make_order("a1", sts::Side::buy, 200, 100'000));
  broker.drain();
  assert(broker.book().total_fees() == 50000);
}

}  // namespace

void schedule_for_prefers_overrides() {
  sts::FeeTable table{};
  table.fallback.commission_ppm = 2500;
  sts::FeeSchedule vip{};
  vip.commission_ppm = 2000;
  table.overrides.insert_or_assign("600000.SSE", vip);
  assert(sts::schedule_for(table, "600000.SSE").commission_ppm == 2000);
  assert(sts::schedule_for(table, "000001.SZSE").commission_ppm == 2500);
}

int main() {
  schedule_for_prefers_overrides();
  zero_schedule_charges_nothing();
  commission_with_minimum();
  stamp_is_sell_only_and_transfer_is_both();
  large_notionals_do_not_overflow();
  simulated_applies_buy_sell_mix();
  alternate_applies_schedule();
  return 0;
}
