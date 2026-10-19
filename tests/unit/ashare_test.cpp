// clang-format off
/**
  ******************************************************************************
  * @file           : ashare_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for A-share order and rule validation.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/market/ashare.hpp"

#include <cassert>

namespace {

qts::AshareInstrumentRules base_rules() { return qts::AshareInstrumentRules{"SSE.600000", qts::Exchange::sse, qts::AshareBoard::main, 1, 100, 100, 1'000'000, true, true, 900, 1'100}; }

qts::OrderIntent base_order() {
  qts::OrderIntent order{"ashare-1", "paper", "test", "SSE.600000", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, 100, 1'000};
  order.exchange = qts::Exchange::sse;
  order.board = qts::AshareBoard::main;
  order.security_account_id = "A123456789";
  return order;
}

void valid_limit() { assert(qts::AshareOrderValidator::validate(base_order(), base_rules()).valid); }

void quantity_rules() {
  auto order = base_order();
  order.quantity = 101;  // violates 100-share lot step
  assert(!qts::AshareOrderValidator::validate(order, base_rules()).valid);

  order.quantity = 50;  // below minimum
  assert(!qts::AshareOrderValidator::validate(order, base_rules()).valid);
}

void price_rules() {
  auto order = base_order();
  order.limit_price = 1'101;  // outside [900, 1100] band
  assert(!qts::AshareOrderValidator::validate(order, base_rules()).valid);

  qts::AshareInstrumentRules tick_rules = base_rules();
  tick_rules.price_tick = 100;
  order.limit_price = 1'050;  // violates 100-unit tick
  assert(!qts::AshareOrderValidator::validate(order, tick_rules).valid);
}

void identity_rules() {
  auto order = base_order();
  order.security_account_id.clear();  // A-share requires shareholder account
  assert(!qts::AshareOrderValidator::validate(order, base_rules()).valid);

  order = base_order();
  order.exchange = qts::Exchange::szse;  // mismatch with instrument rules
  assert(!qts::AshareOrderValidator::validate(order, base_rules()).valid);

  order = base_order();
  order.instrument_id = "SSE.600001";
  assert(!qts::AshareOrderValidator::validate(order, base_rules()).valid);
}

void market_order_rules() {
  auto order = base_order();
  order.type = qts::OrderType::market;
  order.limit_price = 0;
  order.market_order_style = qts::MarketOrderStyle::best_five_ioc;
  order.protection_price = 1'100;
  assert(qts::AshareOrderValidator::validate(order, base_rules()).valid);

  // No style selected.
  order.market_order_style = qts::MarketOrderStyle::none;
  assert(!qts::AshareOrderValidator::validate(order, base_rules()).valid);

  // No protection price: allowed on the main board (fills zero per spec).
  order.market_order_style = qts::MarketOrderStyle::best_five_ioc;
  order.protection_price = 0;
  assert(qts::AshareOrderValidator::validate(order, base_rules()).valid);

  // Venue disallows market orders at all.
  qts::AshareInstrumentRules no_market = base_rules();
  no_market.market_orders_allowed = false;
  order.protection_price = 1'100;
  assert(!qts::AshareOrderValidator::validate(order, no_market).valid);
}

void market_protection_price_by_board() {
  auto order = base_order();
  order.type = qts::OrderType::market;
  order.limit_price = 0;
  order.market_order_style = qts::MarketOrderStyle::best_five_ioc;
  order.protection_price = 0;

  // STAR and ChiNext require a protection price (archforce §3.1).
  qts::AshareInstrumentRules star = base_rules();
  star.board = qts::AshareBoard::star;
  order.board = qts::AshareBoard::star;
  assert(!qts::AshareOrderValidator::validate(order, star).valid);
  order.protection_price = 1'100;
  assert(qts::AshareOrderValidator::validate(order, star).valid);

  qts::AshareInstrumentRules chinext = base_rules();
  chinext.exchange = qts::Exchange::szse;
  chinext.board = qts::AshareBoard::chinext;
  chinext.instrument_id = "SZSE.300000";
  order.exchange = qts::Exchange::szse;
  order.board = qts::AshareBoard::chinext;
  order.instrument_id = "SZSE.300000";
  order.protection_price = 0;
  assert(!qts::AshareOrderValidator::validate(order, chinext).valid);
  order.protection_price = 1'100;
  assert(qts::AshareOrderValidator::validate(order, chinext).valid);
}

}  // namespace

int main() {
  valid_limit();
  quantity_rules();
  price_rules();
  identity_rules();
  market_order_rules();
  market_protection_price_by_board();
  return 0;
}
