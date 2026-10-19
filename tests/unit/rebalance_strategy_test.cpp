// clang-format off
/**
  ******************************************************************************
  * @file           : rebalance_strategy_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for target-weight rebalance sizing and gates.
  * @version        : 0.1.0
  * @date           : 04/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/strategy/rebalance_strategy.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

#include "qts/portfolio/portfolio.hpp"

namespace {

qts::RebalanceConfig single_target() {
  qts::RebalanceConfig config{};
  config.strategy_id = "bal-1";
  config.account_id = "paper";
  qts::RebalanceTarget target{};
  target.instrument_id = "TEST";
  target.weight_bps = 5'000;
  config.targets.push_back(target);
  return config;
}

qts::QuoteEvent make_quote(const std::string& instrument, qts::Price last, std::uint64_t sequence) {
  qts::QuoteEvent quote{};
  quote.instrument_id = instrument;
  quote.last_price = last;
  quote.sequence = sequence;
  quote.receive_time = qts::Timestamp{};
  quote.quality = qts::QuoteQuality::valid;
  return quote;
}

void buys_half_of_cash_at_first_print() {
  qts::RebalanceStrategy strategy{single_target()};
  qts::Portfolio book{1'000'000};
  const auto intents = strategy.on_quote(make_quote("TEST", 1'000, 1), book);
  assert(intents.size() == 1);  // goal 500k, holds 0 => buy 500 @ 1000
  assert(intents[0].side == qts::Side::buy);
  assert(intents[0].quantity == 500 && intents[0].limit_price == 1'000);
  assert(intents[0].strategy_id == "bal-1" && intents[0].account_id == "paper");
  assert(intents[0].client_order_id == "rebalance-1" && intents[0].trace_id == "rebalance-1");
}

void sells_down_to_target_from_settled_inventory() {
  qts::RebalanceStrategy strategy{single_target()};
  qts::Portfolio book{0};
  book.fund_settled("bal-1", "TEST", 10'000, 1'000);  // equity 10M, goal 5M
  const auto intents = strategy.on_quote(make_quote("TEST", 1'000, 1), book);
  assert(intents.size() == 1);
  assert(intents[0].side == qts::Side::sell);
  assert(intents[0].quantity == 5'000);
}

void warm_up_gate_holds_until_every_target_prints() {
  qts::RebalanceConfig config = single_target();
  qts::RebalanceTarget second{};
  second.instrument_id = "OTHER";
  second.weight_bps = 5'000;
  config.targets.push_back(second);
  qts::RebalanceStrategy strategy{config};
  qts::Portfolio book{1'000'000};
  assert(strategy.on_quote(make_quote("TEST", 1'000, 1), book).empty());  // OTHER unpriced
  const auto intents = strategy.on_quote(make_quote("OTHER", 1'000, 2), book);
  assert(intents.size() == 1);  // now both priced: OTHER goal 500k => buy 500 @ 1000
  assert(intents[0].instrument_id == "OTHER" && intents[0].quantity == 500);
}

void band_holds_small_drift_and_dust_is_skipped() {
  qts::RebalanceConfig config = single_target();
  config.band_bps = 100;  // 1% of equity
  qts::RebalanceStrategy strategy{config};
  qts::Portfolio book{1'000'000};
  // First print buys 500; feed the fill back so the book matches the goal.
  const auto first = strategy.on_quote(make_quote("TEST", 1'000, 1), book);
  assert(first.size() == 1);
  book.apply_fill(first[0], first[0].quantity, first[0].limit_price);
  // 0.5% drift stays inside the band.
  assert(strategy.on_quote(make_quote("TEST", 1'005, 2), book).empty());
  // Dust: weight 0 with band 0 still skips sub-lot diffs.
  qts::RebalanceConfig dust = single_target();
  dust.targets[0].weight_bps = 1;  // goal 100 on 1M equity
  qts::RebalanceStrategy dusty{dust};
  qts::Portfolio cash{1'000'000};
  assert(dusty.on_quote(make_quote("TEST", 1'000, 1), cash).empty());
}

void sells_clamp_to_sellable_inventory() {
  qts::RebalanceConfig config = single_target();
  config.targets[0].weight_bps = 0;  // goal 0: liquidate TEST
  qts::RebalanceTarget other{};
  other.instrument_id = "OTHER";
  other.weight_bps = 5'000;  // keeps the weight sum in (0, 10000]
  config.targets.push_back(other);
  qts::RebalanceStrategy strategy{config};
  qts::Portfolio book{1'000'000};
  book.fund_settled("bal-1", "TEST", 200, 1'000);
  qts::OrderIntent day_buy{"day-1", "paper", "bal-1", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, 9'900, 1'000};
  book.apply_fill(day_buy, 9'900, 1'000);  // same-day buys are not sellable (T+1)
  // Warm up both symbols first (the first print always holds on the gate).
  assert(strategy.on_quote(make_quote("TEST", 1'000, 1), book).empty());
  assert(strategy.on_quote(make_quote("OTHER", 1'000, 2), book).size() == 1);
  const auto intents = strategy.on_quote(make_quote("TEST", 1'000, 3), book);
  assert(intents.size() == 1);
  assert(intents[0].side == qts::Side::sell && intents[0].quantity == 200);
}

void foreign_quotes_and_bad_prices_are_ignored() {
  qts::RebalanceStrategy strategy{single_target()};
  qts::Portfolio book{1'000'000};
  assert(strategy.on_quote(make_quote("OTHER", 1'000, 1), book).empty());
  assert(strategy.on_quote(make_quote("TEST", 0, 2), book).empty());
  assert(strategy.on_quote(make_quote("TEST", -5, 3), book).empty());
}

void constructor_rejects_bad_configs() {
  bool threw = false;
  try {
    qts::RebalanceConfig bad{};
    qts::RebalanceStrategy strategy{bad};
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    qts::RebalanceConfig bad = single_target();
    bad.targets[0].weight_bps = 10'001;
    qts::RebalanceStrategy strategy{bad};
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    qts::RebalanceConfig bad = single_target();
    bad.targets[0].weight_bps = 6'000;
    qts::RebalanceTarget extra{};
    extra.instrument_id = "OTHER";
    extra.weight_bps = 5'000;  // sums past 100%
    bad.targets.push_back(extra);
    qts::RebalanceStrategy strategy{bad};
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void prefix_and_determinism_match_grid_convention() {
  qts::RebalanceConfig config = single_target();
  config.id_prefix = "bal-1-20261004-9-";
  qts::RebalanceStrategy first{config};
  qts::RebalanceStrategy second{config};
  qts::Portfolio book{1'000'000};
  const auto one = first.on_quote(make_quote("TEST", 1'000, 1), book);
  const auto two = second.on_quote(make_quote("TEST", 1'000, 1), book);
  assert(one.size() == 1 && two.size() == 1);
  assert(one[0].client_order_id == "bal-1-20261004-9-1");
  assert(one[0].client_order_id == two[0].client_order_id);
}

}  // namespace

int main() {
  buys_half_of_cash_at_first_print();
  sells_down_to_target_from_settled_inventory();
  warm_up_gate_holds_until_every_target_prints();
  band_holds_small_drift_and_dust_is_skipped();
  sells_clamp_to_sellable_inventory();
  foreign_quotes_and_bad_prices_are_ignored();
  constructor_rejects_bad_configs();
  prefix_and_determinism_match_grid_convention();
  return 0;
}
