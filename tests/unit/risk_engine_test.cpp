// clang-format off
/**
  ******************************************************************************
  * @file           : risk_engine_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for risk rejections, frequency, positions and switches.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/risk/risk_engine.hpp"

#include <cassert>
#include <chrono>
#include <limits>
#include <unordered_set>
#include <utility>

#include "qts/core/clock.hpp"

namespace {

qts::OrderIntent make_valid() { return qts::OrderIntent{"one", "paper", "test", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, 10, 100}; }

void quantity_limits() {
  qts::RiskEngine risk{{100, 10'000, true}};
  assert(risk.check(make_valid()).allowed);

  auto over_qty = make_valid();
  over_qty.quantity = 101;
  assert(!risk.check(over_qty).allowed);

  auto zero_qty = make_valid();
  zero_qty.quantity = 0;
  assert(!risk.check(zero_qty).allowed);
}

void identity_and_price() {
  qts::RiskEngine risk{{100, 10'000, true}};

  auto no_client_id = make_valid();
  no_client_id.client_order_id.clear();
  assert(!risk.check(no_client_id).allowed);

  auto no_instrument = make_valid();
  no_instrument.instrument_id.clear();
  assert(!risk.check(no_instrument).allowed);

  auto bad_price = make_valid();
  bad_price.limit_price = 0;
  assert(!risk.check(bad_price).allowed);

  // Market orders carry no limit price and must not be rejected for it.
  auto market = make_valid();
  market.type = qts::OrderType::market;
  market.limit_price = 0;
  assert(risk.check(market).allowed);
}

void notional_limits() {
  qts::RiskEngine risk{{100, 500, true}};  // 10 * 100 = 1000 > 500
  assert(!risk.check(make_valid()).allowed);

  // Overflow guard must trigger before the multiplication overflows.
  qts::RiskEngine overflow_risk{{0, std::numeric_limits<qts::Price>::max(), true}};
  auto huge = make_valid();
  huge.quantity = std::numeric_limits<qts::Price>::max() / huge.limit_price + 1;
  assert(!overflow_risk.check(huge).allowed);
}

void kill_switch() {
  qts::RiskEngine risk{{100, 10'000, true}};
  assert(risk.check(make_valid()).allowed);
  risk.set_trading_enabled(false);
  assert(!risk.check(make_valid()).allowed);
  risk.set_trading_enabled(true);
  assert(risk.check(make_valid()).allowed);
}

void strategy_and_account_switches() {
  qts::RiskEngine risk{{100, 10'000, true}};
  risk.set_strategy_blocked("test", true);
  assert(!risk.check(make_valid()).allowed);
  risk.set_strategy_blocked("test", false);
  assert(risk.check(make_valid()).allowed);

  // Blocking another strategy must not affect this order.
  risk.set_strategy_blocked("other", true);
  assert(risk.check(make_valid()).allowed);

  risk.set_account_blocked("paper", true);
  assert(!risk.check(make_valid()).allowed);
  risk.set_account_blocked("paper", false);
  assert(risk.check(make_valid()).allowed);
}

void limits_and_switches_replace_wholesale() {
  qts::RiskEngine risk{{100, 10'000, true}};
  assert(risk.check(make_valid()).allowed);
  // Wholesale limits: tightening the quantity cap rejects, loosening restores.
  qts::RiskLimits tight{1, 10'000, true};
  risk.set_limits(tight);
  assert(!risk.check(make_valid()).allowed);
  risk.set_limits(qts::RiskLimits{100, 10'000, true});
  assert(risk.check(make_valid()).allowed);
  // Wholesale switches: replace clears ids absent from the new sets.
  risk.set_switches({"test"}, {}, {});
  assert(!risk.check(make_valid()).allowed);
  risk.set_switches({}, {}, {});
  assert(risk.check(make_valid()).allowed);
}

void daily_loss_blocks_opens_but_not_closes() {
  qts::RiskLimits limits{100, 10'000, true};
  limits.daily_loss_limit = 1000;
  qts::RiskEngine risk{limits};
  // No loss yet: everything passes.
  assert(risk.check(make_valid()).allowed);
  // Down 1500 with 100 long: new buys blocked, partial/full sells pass.
  risk.set_position("TEST", 100);
  risk.set_day_pnl(-1500);
  auto buy = make_valid();
  buy.quantity = 10;
  assert(!risk.check(buy).allowed);
  auto trim = make_valid();
  trim.side = qts::Side::sell;
  trim.quantity = 40;
  assert(risk.check(trim).allowed);
  auto flip = make_valid();
  flip.side = qts::Side::sell;
  flip.quantity = 150;
  assert(!risk.check(flip).allowed);  // would flip to short: grows |net|
  // Back above the line: opens pass again.
  risk.set_day_pnl(-500);
  assert(risk.check(buy).allowed);
}

void sellable_enforced_only_when_enabled() {
  qts::RiskEngine risk{{100, 10'000, true}};
  auto sell = make_valid();
  sell.side = qts::Side::sell;
  // Default off: sells pass without inventory data.
  assert(risk.check(sell).allowed);
  // Enabled with nothing sellable: sells block, buys still pass.
  qts::RiskLimits strict{100, 10'000, true};
  strict.enforce_sellable = true;
  risk.set_limits(strict);
  assert(!risk.check(sell).allowed);
  assert(risk.check(make_valid()).allowed);
  // Same-day buys are not sellable (T+1): 10 bought, 10 sellable only after settle.
  risk.set_sellable("test", "TEST", 0);
  assert(!risk.check(sell).allowed);
  risk.set_sellable("test", "TEST", 10);
  assert(risk.check(sell).allowed);
  auto oversize = sell;
  oversize.quantity = 11;
  assert(!risk.check(oversize).allowed);
}

// Live (unfilled) sells reserve sellable: two concurrent sells sized
// against the same inventory must not both pass. Absolute set (not
// incremental), so a fresh sync releases fills/cancels with no leak.
void sell_reservation_blocks_concurrent_double_spend() {
  qts::RiskLimits strict{100, 10'000, true};
  strict.enforce_sellable = true;
  qts::RiskEngine risk{strict};
  auto sell = make_valid();
  sell.side = qts::Side::sell;
  sell.quantity = 10;
  risk.set_sellable("test", "TEST", 10);
  assert(risk.check(sell).allowed);
  risk.set_sell_reserved({{std::make_pair(qts::StrategyId{"test"}, qts::InstrumentId{"TEST"}), 6}});
  auto second = sell;
  second.quantity = 5;  // only 4 left unreserved
  assert(!risk.check(second).allowed);
  second.quantity = 4;
  assert(risk.check(second).allowed);
  // Over-reservation floors at zero instead of going negative.
  risk.set_sell_reserved({{std::make_pair(qts::StrategyId{"test"}, qts::InstrumentId{"TEST"}), 50}});
  assert(!risk.check(second).allowed);
  // A fresh sync with no live sells releases everything.
  risk.set_sell_reserved({});
  assert(risk.check(second).allowed);
}

// Sellable is scoped per strategy basket: one strategy's inventory never
// covers another's sells (pod-level discipline, stricter than the broker's
// account gate). Empty strategy sorts into passthrough.
void sellable_is_scoped_per_strategy_basket() {
  qts::RiskLimits strict{10'000, 0, true};
  strict.enforce_sellable = true;
  qts::RiskEngine risk{strict};
  auto sell = make_valid();  // strategy "test"
  sell.side = qts::Side::sell;
  sell.quantity = 10;
  risk.set_sellable("test", "TEST", 10);
  assert(risk.check(sell).allowed);
  auto funded = sell;
  funded.strategy_id = "funded";
  assert(!risk.check(funded).allowed);  // no inventory in its own basket
  risk.set_sellable("funded", "TEST", 10);
  assert(risk.check(funded).allowed);
  // Reservation is scoped too: test's live sells don't touch funded.
  risk.set_sell_reserved({{std::make_pair(qts::StrategyId{"test"}, qts::InstrumentId{"TEST"}), 10}});
  assert(!risk.check(sell).allowed);
  assert(risk.check(funded).allowed);
}

// Buying power: limit buys must fit inside synced cash minus live
// (unfilled) buy exposure. Unknown cash counts as zero once on.
void cash_gate_blocks_overspend_and_reserves_live_buys() {
  qts::RiskLimits strict{10'000, 0, true};
  strict.enforce_cash = true;
  qts::RiskEngine risk{strict};
  auto buy = make_valid();           // buy 10 @ 100 = 1000 notional
  assert(!risk.check(buy).allowed);  // no cash synced yet: fail-closed
  risk.set_cash(5'000);
  assert(risk.check(buy).allowed);
  auto big = buy;
  big.quantity = 51;  // 5100 > 5000
  assert(!risk.check(big).allowed);
  // A live 4000-notional buy leaves only 1000 addressable.
  risk.set_buy_reserved(4'000);
  auto small = buy;  // 1000 fits exactly
  assert(risk.check(small).allowed);
  auto over = buy;
  over.quantity = 11;  // 1100 > 1000 left
  assert(!risk.check(over).allowed);
  // Sells never face the cash gate; market buys skip sizing.
  auto sell = buy;
  sell.side = qts::Side::sell;
  sell.quantity = 9'999;
  assert(risk.check(sell).allowed);
  // Releasing the reservation restores headroom.
  risk.set_buy_reserved(0);
  assert(risk.check(over).allowed);
}

qts::OrderIntent make_ashare() {
  qts::OrderIntent order{"ashare-1", "paper", "test", "SSE.600000", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, 100, 1'000};
  order.exchange = qts::Exchange::sse;
  order.board = qts::AshareBoard::main;
  order.security_account_id = "A123456789";
  return order;
}

void ashare_chain() {
  qts::RiskEngine risk{{1'000'000, 0, true}};

  // Fail-closed: no ruleset configured.
  assert(!risk.check_ashare(make_ashare()).allowed);

  risk.set_ashare_rules({"SSE.600000", qts::Exchange::sse, qts::AshareBoard::main, 1, 100, 100, 1'000'000, true, true, 900, 1'100});
  assert(risk.check_ashare(make_ashare()).allowed);

  // Generic gate still applies first: quantity breach rejects before rules.
  auto over_qty = make_ashare();
  over_qty.quantity = 2'000'000;
  assert(!risk.check_ashare(over_qty).allowed);

  // Venue rule breach surfaces the validator reason.
  auto bad_lot = make_ashare();
  bad_lot.quantity = 101;
  const qts::RiskDecision rejected = risk.check_ashare(bad_lot);
  assert(!rejected.allowed && !rejected.reason.empty());

  // Generic check() stays venue-agnostic: non-A-share flow is unaffected.
  assert(risk.check(make_valid()).allowed);

  risk.clear_ashare_rules();
  assert(!risk.check_ashare(make_ashare()).allowed);
}

void instrument_level_switch() {
  qts::RiskEngine risk{{100, 10'000, true}};
  risk.set_instrument_blocked("TEST", true);
  assert(!risk.check(make_valid()).allowed);
  risk.set_instrument_blocked("TEST", false);
  assert(risk.check(make_valid()).allowed);
}

qts::OrderIntent make_priced(qts::Price price) { return qts::OrderIntent{"band-1", "paper", "test", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, 10, price}; }

void price_band_gate() {
  qts::RiskEngine risk{{1'000, 0, true}};
  risk.set_price_band("TEST", {900, 1'100});
  assert(risk.check(make_priced(1'000)).allowed);
  assert(!risk.check(make_priced(1'101)).allowed);
  assert(!risk.check(make_priced(899)).allowed);

  // Market orders and unbanded instruments are unaffected.
  auto market = make_priced(0);
  market.type = qts::OrderType::market;
  assert(risk.check(market).allowed);
  auto other = make_priced(1'500);
  other.instrument_id = "OTHER";
  assert(risk.check(other).allowed);

  risk.clear_price_band("TEST");
  assert(risk.check(make_priced(1'500)).allowed);
}

void position_cap() {
  qts::RiskEngine risk{{1'000, 0, true}};
  risk.set_position("TEST", 90);
  // Default: no cap configured, anything within quantity limits passes.
  assert(risk.check(make_valid()).allowed);

  risk = qts::RiskEngine{{1'000, 0, true, 100}};
  risk.set_position("TEST", 90);
  assert(risk.check(make_valid()).allowed);  // 90 + 10 == 100
  auto over = make_valid();
  over.quantity = 11;
  assert(!risk.check(over).allowed);  // 90 + 11 > 100
  auto reduce = make_valid();
  reduce.side = qts::Side::sell;
  reduce.quantity = 20;
  assert(risk.check(reduce).allowed);  // 90 - 20 == 70
  auto flip = make_valid();
  flip.side = qts::Side::sell;
  flip.quantity = 200;
  assert(!risk.check(flip).allowed);  // |90 - 200| > 100
}

void instrument_allowlist() {
  qts::RiskEngine risk{{100, 10'000, true}};
  risk.set_instrument_allowlist({"TEST"});
  assert(risk.check(make_valid()).allowed);
  auto other = make_valid();
  other.instrument_id = "OTHER";
  assert(!risk.check(other).allowed);
  risk.set_instrument_allowlist({});
  assert(risk.check(other).allowed);
}

void order_rate_gate() {
  qts::ManualClock clock{};
  qts::RiskEngine risk{{100, 10'000, true}};
  risk.set_rate_limit(2, std::chrono::seconds{60}, &clock);
  assert(risk.check(make_valid()).allowed);
  assert(risk.check(make_valid()).allowed);
  assert(!risk.check(make_valid()).allowed);  // third attempt inside the window
  clock.advance(std::chrono::seconds{61});
  assert(risk.check(make_valid()).allowed);

  // Rejected orders must not consume quota.
  risk.set_trading_enabled(false);
  assert(!risk.check(make_valid()).allowed);
  risk.set_trading_enabled(true);
  clock.advance(std::chrono::seconds{61});
  assert(risk.check(make_valid()).allowed);

  // Null clock disables the gate entirely.
  risk.set_rate_limit(1, std::chrono::seconds{60}, nullptr);
  assert(risk.check(make_valid()).allowed);
  assert(risk.check(make_valid()).allowed);
}

void ashare_inherits_new_gates() {
  qts::RiskEngine risk{{1'000'000, 0, true}};
  risk.set_ashare_rules({"SSE.600000", qts::Exchange::sse, qts::AshareBoard::main, 1, 100, 100, 1'000'000, true, true, 900, 1'100});
  assert(risk.check_ashare(make_ashare()).allowed);
  risk.set_instrument_allowlist({"OTHER"});
  assert(!risk.check_ashare(make_ashare()).allowed);
}

void venue_master_enforces_per_instrument_rules() {
  // Empty master: venue-agnostic legacy path (existing drills unaffected).
  qts::RiskEngine risk{{1'000'000, 0, true}};
  assert(!risk.has_ashare_master());
  assert(risk.check(make_valid()).allowed);

  qts::AshareInstrumentRules rules{"TEST", qts::Exchange::sse, qts::AshareBoard::main, 1, 100, 100, 1'000'000, true, true, 900, 1'100};
  risk.set_ashare_master({{"TEST", rules}});
  assert(risk.has_ashare_master());

  // Unknown instruments fail closed once the master is non-empty.
  auto unknown = make_valid();
  unknown.instrument_id = "GHOST";
  assert(!risk.check(unknown).allowed);

  // Identity: grid-style orders without venue identity are rejected.
  assert(!risk.check(make_valid()).allowed);

  // Fully identified orders pass tick/lot/band gates.
  auto identified = make_valid();
  identified.exchange = qts::Exchange::sse;
  identified.board = qts::AshareBoard::main;
  identified.security_account_id = "A123";
  identified.quantity = 100;
  identified.limit_price = 1'000;
  assert(risk.check(identified).allowed);

  // Lot breach, tick breach and band breach surface validator reasons.
  auto bad_lot = identified;
  bad_lot.quantity = 150;
  assert(!risk.check(bad_lot).allowed);
  auto bad_tick = identified;
  bad_tick.limit_price = 1001;
  // tick is 1 here so 1001 passes; force a coarser tick to prove the gate.
  qts::AshareInstrumentRules coarse = rules;
  coarse.price_tick = 50;
  risk.set_ashare_master({{"TEST", coarse}});
  assert(!risk.check(bad_tick).allowed);
  auto outside_band = identified;
  outside_band.limit_price = 1'200;
  assert(!risk.check(outside_band).allowed);

  risk.clear_ashare_master();
  assert(!risk.has_ashare_master());
  assert(risk.check(make_valid()).allowed);
}

qts::RiskLimits safety_net_limits() {
  qts::RiskLimits limits{100, 10'000, true};
  limits.max_consecutive_rejects = 3;
  limits.max_reject_rate_bps = 3000;
  limits.reject_rate_min_orders = 100;
  limits.max_loss_ratio_bps = 3000;
  return limits;
}

void consecutive_rejects_auto_stop() {
  qts::RiskEngine risk{safety_net_limits()};
  risk.note_submit_rejected("test");
  risk.note_submit_rejected("test");
  assert(risk.check(make_valid()).allowed);
  assert(risk.drain_auto_actions().empty());
  risk.note_submit_rejected("test");  // third consecutive: blocked + queued
  assert(!risk.check(make_valid()).allowed);
  const auto actions = risk.drain_auto_actions();
  assert(actions.size() == 1 && actions[0].kind == qts::AutoActionKind::halt_strategy && actions[0].strategy_id == "test" && !actions[0].reason.empty());
  assert(risk.drain_auto_actions().empty());  // cleared on read
  // An accept clears the streak (after a manual unblock for the probe).
  risk.set_strategy_blocked("test", false);
  risk.note_submit_accepted("test");
  risk.note_submit_rejected("test");
  assert(risk.check(make_valid()).allowed);
}

void broker_rejects_extend_the_streak() {
  qts::RiskLimits limits = safety_net_limits();
  limits.max_consecutive_rejects = 2;
  qts::RiskEngine risk{limits};
  risk.note_submit_accepted("test");  // working order out in the world
  risk.note_broker_reject("test");
  assert(risk.check(make_valid()).allowed);
  risk.note_broker_reject("test");  // second: blocked, no duplicate submit counted
  assert(!risk.check(make_valid()).allowed);
  assert(risk.drain_auto_actions().size() == 1);
}

void global_reject_rate_halts() {
  qts::RiskLimits limits = safety_net_limits();
  limits.max_consecutive_rejects = 0;  // isolate the global gate
  limits.max_reject_rate_bps = 5000;   // 50%
  limits.reject_rate_min_orders = 4;
  qts::RiskEngine risk{limits};
  risk.note_submit_accepted("a");
  risk.note_submit_rejected("b");
  assert(risk.check(make_valid()).allowed);  // 1/2 below the 4-order arming
  risk.note_submit_accepted("a");
  assert(risk.check(make_valid()).allowed);  // 1/3 still below arming
  risk.note_submit_rejected("c");            // 2/4 = 50% >= 50%: halt
  assert(!risk.check(make_valid()).allowed);
  const auto actions = risk.drain_auto_actions();
  assert(actions.size() == 1 && actions[0].kind == qts::AutoActionKind::halt_global && actions[0].strategy_id.empty());
  risk.note_submit_rejected("d");  // already halted: no duplicate action
  assert(risk.drain_auto_actions().empty());
}

void loss_ratio_halts_on_drawdown() {
  qts::RiskEngine risk{safety_net_limits()};
  risk.set_equity_base(1'000'000);
  risk.set_equity(700'001);  // 29.9999%: passes
  assert(risk.check(make_valid()).allowed);
  assert(risk.drain_auto_actions().empty());
  risk.set_equity(700'000);  // exactly 30%: halts
  assert(!risk.check(make_valid()).allowed);
  const auto actions = risk.drain_auto_actions();
  assert(actions.size() == 1 && actions[0].kind == qts::AutoActionKind::halt_global);
  // Undefined ratios never halt: non-positive base, or a gain.
  qts::RiskEngine fresh{safety_net_limits()};
  fresh.set_equity(0);
  assert(fresh.check(make_valid()).allowed);
  fresh.set_equity_base(1'000'000);
  fresh.set_equity(2'000'000);
  assert(fresh.check(make_valid()).allowed);
}

void settle_day_resets_counters_not_blocks() {
  qts::RiskLimits limits = safety_net_limits();
  limits.max_consecutive_rejects = 2;
  limits.daily_loss_limit = 100;
  qts::RiskEngine risk{limits};
  risk.note_submit_rejected("test");
  risk.note_submit_rejected("test");
  assert(!risk.check(make_valid()).allowed);
  risk.set_day_pnl(-10'000);
  assert(!risk.check(make_valid()).allowed);  // loss gate also holds (flat book)
  (void)risk.drain_auto_actions();
  risk.settle_day();
  // Blocks persist (still halted), but the streak and day PnL cleared: after
  // a manual unblock a single reject no longer trips the gate.
  assert(!risk.check(make_valid()).allowed);
  risk.set_strategy_blocked("test", false);
  risk.set_trading_enabled(true);
  risk.note_submit_rejected("test");
  assert(risk.check(make_valid()).allowed);
  assert(risk.drain_auto_actions().empty());
}

void safety_net_all_off() {
  qts::RiskLimits limits{100, 10'000, true};
  limits.max_consecutive_rejects = 0;
  limits.max_reject_rate_bps = 0;
  limits.reject_rate_min_orders = 0;
  limits.max_loss_ratio_bps = 0;
  qts::RiskEngine risk{limits};
  for (int i = 0; i < 10; ++i) risk.note_submit_rejected("test");
  risk.set_equity_base(1'000'000);
  risk.set_equity(1);
  assert(risk.check(make_valid()).allowed);
  assert(risk.drain_auto_actions().empty());
}

}  // namespace

int main() {
  quantity_limits();
  identity_and_price();
  notional_limits();
  kill_switch();
  strategy_and_account_switches();
  limits_and_switches_replace_wholesale();
  sellable_enforced_only_when_enabled();
  sell_reservation_blocks_concurrent_double_spend();
  sellable_is_scoped_per_strategy_basket();
  cash_gate_blocks_overspend_and_reserves_live_buys();
  daily_loss_blocks_opens_but_not_closes();
  ashare_chain();
  instrument_level_switch();
  price_band_gate();
  position_cap();
  instrument_allowlist();
  order_rate_gate();
  ashare_inherits_new_gates();
  venue_master_enforces_per_instrument_rules();
  consecutive_rejects_auto_stop();
  broker_rejects_extend_the_streak();
  global_reject_rate_halts();
  loss_ratio_halts_on_drawdown();
  settle_day_resets_counters_not_blocks();
  safety_net_all_off();
  return 0;
}
