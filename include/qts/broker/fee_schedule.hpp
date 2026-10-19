// clang-format off
/**
  ******************************************************************************
  * @file           : fee_schedule.hpp
  * @author         : vivi wu
  * @brief          : Integer A-share fee schedule: commission, stamp, transfer.
  * @version        : 0.1.0
  * @date           : 03/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "qts/core/types.hpp"

namespace qts {

// A-share style fill fees, integer only (ADR-003). Rates are per-million
// (2500 = 0.25%% = 万2.5): basis points cannot express retail commissions.
//   commission: both sides, floored per fill at commission_minimum (0 = none)
//   stamp:      sell side only
//   transfer:   both sides
// A zero schedule charges nothing (legacy free-trading default).
struct FeeSchedule {
  std::int64_t commission_ppm{0};
  Price commission_minimum{0};
  std::int64_t stamp_ppm{0};
  std::int64_t transfer_ppm{0};
};

// Overflow-safe (notional * ppm / 1e6) with divide-first ordering.
[[nodiscard]] inline Price fill_fee(const FeeSchedule& schedule, Side side, Price notional) {
  if (notional <= 0) return 0;
  Price fee = 0;
  if (schedule.transfer_ppm > 0) {
    fee += (notional / 1'000'000) * schedule.transfer_ppm + ((notional % 1'000'000) * schedule.transfer_ppm) / 1'000'000;
  }
  if (side == Side::sell && schedule.stamp_ppm > 0) {
    fee += (notional / 1'000'000) * schedule.stamp_ppm + ((notional % 1'000'000) * schedule.stamp_ppm) / 1'000'000;
  }
  if (schedule.commission_ppm > 0) {
    Price commission = (notional / 1'000'000) * schedule.commission_ppm + ((notional % 1'000'000) * schedule.commission_ppm) / 1'000'000;
    if (commission < schedule.commission_minimum) commission = schedule.commission_minimum;
    fee += commission;
  }
  return fee;
}

// Per-instrument fee table (M11-6): exact instrument-id overrides over a
// fallback schedule. Loaded from a dedicated fee file so ops can adjust
// rates without touching the main config.
struct FeeTable {
  FeeSchedule fallback;
  std::unordered_map<InstrumentId, FeeSchedule> overrides;
};

[[nodiscard]] inline const FeeSchedule& schedule_for(const FeeTable& table, const InstrumentId& instrument) {
  const auto found = table.overrides.find(instrument);
  return found == table.overrides.end() ? table.fallback : found->second;
}

}  // namespace qts
