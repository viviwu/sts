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
#include "sts/market/ashare.hpp"

#include <cassert>

namespace {

sts::AshareInstrumentRules base_rules() { return sts::AshareInstrumentRules{"SSE.600000", sts::Exchange::sse, sts::AshareBoard::main, 1, 100, 100, 1'000'000, true, true, 900, 1'100}; }

sts::OrderIntent base_order() {
  sts::OrderIntent order{"ashare-1", "paper", "test", "SSE.600000", sts::Side::buy, sts::OrderType::limit, sts::TimeInForce::day, 100, 1'000};
  order.exchange = sts::Exchange::sse;
  order.board = sts::AshareBoard::main;
  order.security_account_id = "A123456789";
  return order;
}

void valid_limit() { assert(sts::AshareOrderValidator::validate(base_order(), base_rules()).valid); }

void quantity_rules() {
  auto order = base_order();
  order.quantity = 101;  // violates 100-share lot step
  assert(!sts::AshareOrderValidator::validate(order, base_rules()).valid);

  order.quantity = 50;  // below minimum
  assert(!sts::AshareOrderValidator::validate(order, base_rules()).valid);
}

void price_rules() {
  auto order = base_order();
  order.limit_price = 1'101;  // outside [900, 1100] band
  assert(!sts::AshareOrderValidator::validate(order, base_rules()).valid);

  sts::AshareInstrumentRules tick_rules = base_rules();
  tick_rules.price_tick = 100;
  order.limit_price = 1'050;  // violates 100-unit tick
  assert(!sts::AshareOrderValidator::validate(order, tick_rules).valid);
}

void identity_rules() {
  auto order = base_order();
  order.security_account_id.clear();  // A-share requires shareholder account
  assert(!sts::AshareOrderValidator::validate(order, base_rules()).valid);

  order = base_order();
  order.exchange = sts::Exchange::szse;  // mismatch with instrument rules
  assert(!sts::AshareOrderValidator::validate(order, base_rules()).valid);

  order = base_order();
  order.instrument_id = "SSE.600001";
  assert(!sts::AshareOrderValidator::validate(order, base_rules()).valid);
}

void market_order_rules() {
  auto order = base_order();
  order.type = sts::OrderType::market;
  order.limit_price = 0;
  order.market_order_style = sts::MarketOrderStyle::best_five_ioc;
  order.protection_price = 1'100;
  assert(sts::AshareOrderValidator::validate(order, base_rules()).valid);

  // No style selected.
  order.market_order_style = sts::MarketOrderStyle::none;
  assert(!sts::AshareOrderValidator::validate(order, base_rules()).valid);

  // No protection price: allowed on the main board (fills zero per spec).
  order.market_order_style = sts::MarketOrderStyle::best_five_ioc;
  order.protection_price = 0;
  assert(sts::AshareOrderValidator::validate(order, base_rules()).valid);

  // Venue disallows market orders at all.
  sts::AshareInstrumentRules no_market = base_rules();
  no_market.market_orders_allowed = false;
  order.protection_price = 1'100;
  assert(!sts::AshareOrderValidator::validate(order, no_market).valid);
}

void market_protection_price_by_board() {
  auto order = base_order();
  order.type = sts::OrderType::market;
  order.limit_price = 0;
  order.market_order_style = sts::MarketOrderStyle::best_five_ioc;
  order.protection_price = 0;

  // STAR and ChiNext require a protection price (archforce §3.1).
  sts::AshareInstrumentRules star = base_rules();
  star.board = sts::AshareBoard::star;
  order.board = sts::AshareBoard::star;
  assert(!sts::AshareOrderValidator::validate(order, star).valid);
  order.protection_price = 1'100;
  assert(sts::AshareOrderValidator::validate(order, star).valid);

  sts::AshareInstrumentRules chinext = base_rules();
  chinext.exchange = sts::Exchange::szse;
  chinext.board = sts::AshareBoard::chinext;
  chinext.instrument_id = "SZSE.300000";
  order.exchange = sts::Exchange::szse;
  order.board = sts::AshareBoard::chinext;
  order.instrument_id = "SZSE.300000";
  order.protection_price = 0;
  assert(!sts::AshareOrderValidator::validate(order, chinext).valid);
  order.protection_price = 1'100;
  assert(sts::AshareOrderValidator::validate(order, chinext).valid);
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
