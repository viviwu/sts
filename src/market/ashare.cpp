// clang-format off
/**
  ******************************************************************************
  * @file           : ashare.cpp
  * @author         : vivi wu
  * @brief          : A-share rule-check implementation against injected instrument rules.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/market/ashare.hpp"

namespace sts {

OrderValidationResult AshareOrderValidator::validate(const OrderIntent& intent, const AshareInstrumentRules& rules) {
  OrderValidationResult identity = check_identity(intent, rules);
  if (!identity.valid) return identity;
  OrderValidationResult quantity = check_quantity(intent, rules);
  if (!quantity.valid) return quantity;
  return check_price(intent, rules);
}

OrderValidationResult AshareOrderValidator::check_identity(const OrderIntent& intent, const AshareInstrumentRules& rules) {
  if (intent.exchange != rules.exchange || intent.instrument_id != rules.instrument_id) {
    return {false, "instrument or exchange does not match A-share rules"};
  }
  if (intent.security_account_id.empty()) return {false, "missing security account"};
  return {true, ""};
}

OrderValidationResult AshareOrderValidator::check_quantity(const OrderIntent& intent, const AshareInstrumentRules& rules) {
  if (intent.quantity <= 0 || rules.minimum_quantity <= 0 || rules.quantity_step <= 0) {
    return {false, "invalid quantity or instrument rules"};
  }
  if (rules.maximum_quantity > 0 && intent.quantity > rules.maximum_quantity) {
    return {false, "quantity exceeds instrument maximum"};
  }
  const bool odd_lot_sale = intent.side == Side::sell && rules.sell_odd_lot_only && intent.quantity < rules.minimum_quantity;
  if (!odd_lot_sale && intent.quantity < rules.minimum_quantity) return {false, "quantity below minimum"};
  if (!odd_lot_sale && intent.quantity % rules.quantity_step != 0) return {false, "quantity violates lot step"};
  return {true, ""};
}

OrderValidationResult AshareOrderValidator::check_price(const OrderIntent& intent, const AshareInstrumentRules& rules) {
  if (intent.type == OrderType::limit) {
    if (intent.limit_price <= 0 || rules.price_tick <= 0 || intent.limit_price % rules.price_tick != 0) {
      return {false, "limit price violates price tick"};
    }
    if ((rules.limit_down > 0 && intent.limit_price < rules.limit_down) || (rules.limit_up > 0 && intent.limit_price > rules.limit_up)) {
      return {false, "limit price outside price band"};
    }
    return {true, ""};
  }
  if (!rules.market_orders_allowed || intent.market_order_style == MarketOrderStyle::none) {
    return {false, "market order style is unavailable"};
  }
  // archforce §3.1: only STAR/ChiNext market orders must carry a
  // protection price; main-board market orders specify zero.
  if ((rules.board == AshareBoard::star || rules.board == AshareBoard::chinext) && intent.protection_price <= 0) {
    return {false, "market order requires protection price"};
  }
  return {true, ""};
}

}  // namespace sts
