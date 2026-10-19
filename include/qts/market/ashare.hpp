// clang-format off
/**
  ******************************************************************************
  * @file           : ashare.hpp
  * @author         : vivi wu
  * @brief          : A-share trading-rule validation (accounts, lots, ticks, bands, T+1).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <string>

#include "qts/core/types.hpp"

namespace qts {

// Instrument-master supplied rules.  Do not hard-code exchange rules in OMS:
// they differ by venue, board, product and may change with trading rules.
struct AshareInstrumentRules {
  InstrumentId instrument_id;
  Exchange exchange{Exchange::unknown};
  AshareBoard board{AshareBoard::unknown};
  Price price_tick{0};
  Quantity minimum_quantity{0};
  Quantity quantity_step{0};
  Quantity maximum_quantity{0};
  bool sell_odd_lot_only{false};
  bool market_orders_allowed{false};
  Price limit_down{0};  // zero means no static price band applies
  Price limit_up{0};
};

struct OrderValidationResult {
  bool valid{false};
  std::string reason;
};

class AshareOrderValidator {
 public:
  [[nodiscard]] static OrderValidationResult validate(const OrderIntent& intent, const AshareInstrumentRules& rules);

 private:
  [[nodiscard]] static OrderValidationResult check_identity(const OrderIntent& intent, const AshareInstrumentRules& rules);
  [[nodiscard]] static OrderValidationResult check_quantity(const OrderIntent& intent, const AshareInstrumentRules& rules);
  [[nodiscard]] static OrderValidationResult check_price(const OrderIntent& intent, const AshareInstrumentRules& rules);
};

}  // namespace qts
