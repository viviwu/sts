// clang-format off
/**
  ******************************************************************************
  * @file           : admin_view.hpp
  * @author         : vivi wu
  * @brief          : Offline ledger projections (orders, positions, reconcile) for sts_admin.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "sts/persistence/ledger.hpp"
#include "sts/portfolio/portfolio.hpp"

namespace sts::admin {

// Offline ledger views (M9-2, ADR-034). The admin CLI is a read-only
// projector over a JSONL ledger file: zero IPC with the gateway, safe to
// run while the gateway appends (append-only reads the tail).
struct OrderView {
  ClientOrderId client_order_id;
  OrderStatus status{OrderStatus::pending_new};
  Quantity cumulative_quantity{0};
  std::uint64_t version{0};
  std::uint64_t last_event_id{0};
};

// Latest snapshot per client_order_id, in first-seen order.
[[nodiscard]] std::vector<OrderView> summarize_orders(const std::vector<LedgerEvent>& events);

// Open (non-terminal) orders from the same stream.
[[nodiscard]] std::vector<OrderView> open_orders(const std::vector<LedgerEvent>& events);

// Positions rebuilt by feeding funding deposits (settled) then every fill
// through a fresh Portfolio. Starting cash rides the stream when the
// gateway funded it (M11-7); the argument is the fallback for pre-funding
// ledgers (pass the gateway's configured value).
[[nodiscard]] Portfolio rebuild_positions(const std::vector<LedgerEvent>& events, Price starting_cash);

}  // namespace sts::admin
