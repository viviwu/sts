// clang-format off
/**
  ******************************************************************************
  * @file           : ledger_verify.hpp
  * @author         : vivi wu
  * @brief          : Offline ledger verifier: strict replay plus double-rebuild convergence.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include "sts/broker/simulated_broker.hpp"
#include "sts/core/event_bus.hpp"
#include "sts/oms/order_manager.hpp"
#include "sts/persistence/ledger.hpp"
#include "sts/risk/risk_engine.hpp"

namespace sts::replay {

// Offline ledger verification (sts_replay verify): rebuilds OMS state from
// a stored event stream without touching any broker. Every event must apply
// cleanly through the real state machine (unknown orders, duplicate
// submits and illegal transitions fail the run), and two independent
// rebuilds must converge field-by-field -- the M2 "same stream replays
// identically" acceptance, made executable.
//
// Rebuild rules mirror the recovery path: order_submitted / risk_rejected
// restore the order, execution_applied goes through on_execution_report,
// order_cancel_requested is informational (the terminal broker report that
// follows carries the state; live -> cancelled is a legal transition).
// funding_deposited never touches the OMS (startup inventory, not an order)
// and counts toward neither orders nor opens. No submits ever reach the
// broker, so matching, risk limits and cash are irrelevant to the rebuild.
//
struct VerifyResult {
  bool ok{false};
  int events{0};
  int orders{0};
  int open{0};
  std::string error;
};

namespace detail {

const char* event_name(LedgerEventType type);

}  // namespace detail

// feed_recovery_event applies ONE stored event to a live OMS (boot recovery).
// Declared here so detail::rebuild_once can drive it; defined after detail.
[[nodiscard]] inline bool feed_recovery_event(OrderManager& oms, const LedgerEvent& event, std::string& error);

namespace detail {

inline const char* status_name(OrderStatus status) {
  switch (status) {
    case OrderStatus::pending_new:
      return "pending_new";
    case OrderStatus::live:
      return "live";
    case OrderStatus::partially_filled:
      return "partially_filled";
    case OrderStatus::filled:
      return "filled";
    case OrderStatus::pending_cancel:
      return "pending_cancel";
    case OrderStatus::cancelled:
      return "cancelled";
    case OrderStatus::rejected:
      return "rejected";
  }
  return "unknown";
}

inline const char* event_name(LedgerEventType type) {
  switch (type) {
    case LedgerEventType::order_submitted:
      return "order_submitted";
    case LedgerEventType::execution_applied:
      return "execution_applied";
    case LedgerEventType::order_cancel_requested:
      return "order_cancel_requested";
    case LedgerEventType::risk_rejected:
      return "risk_rejected";
    case LedgerEventType::funding_deposited:
      return "funding_deposited";
    case LedgerEventType::reconcile_attested:
      return "reconcile_attested";
    case LedgerEventType::control_applied:
      return "control_applied";
  }
  return "unknown";
}

inline bool same_snapshot(const ExecutionReport& lhs, const ExecutionReport& rhs, std::string& field) {
  if (lhs.client_order_id != rhs.client_order_id) {
    field = "client_order_id";
    return false;
  }
  if (lhs.order_id != rhs.order_id) {
    field = "order_id";
    return false;
  }
  if (lhs.status != rhs.status) {
    field = "status";
    return false;
  }
  if (lhs.last_quantity != rhs.last_quantity) {
    field = "last_quantity";
    return false;
  }
  if (lhs.last_price != rhs.last_price) {
    field = "last_price";
    return false;
  }
  if (lhs.cumulative_quantity != rhs.cumulative_quantity) {
    field = "cumulative_quantity";
    return false;
  }
  if (lhs.version != rhs.version) {
    field = "version";
    return false;
  }
  if (lhs.reason != rhs.reason) {
    field = "reason";
    return false;
  }
  if (lhs.execution_id != rhs.execution_id) {
    field = "execution_id";
    return false;
  }
  if (lhs.broker_order_id != rhs.broker_order_id) {
    field = "broker_order_id";
    return false;
  }
  if (lhs.reject_code != rhs.reject_code) {
    field = "reject_code";
    return false;
  }
  if (lhs.fee != rhs.fee) {
    field = "fee";
    return false;
  }
  return true;
}

// Replays events into a fresh OMS. Returns true with the open-order set on
// success; on failure returns false with a human-readable reason.
inline bool rebuild_once(const std::vector<LedgerEvent>& events, std::vector<ExecutionReport>& open_out, int& orders_out, std::string& error) {
  EventBus bus;
  SimulatedBroker broker{bus};
  RiskEngine risk{{0, 0, true}};  // never consulted: restores bypass risk
  OrderManager oms{broker, risk, bus};
  std::unordered_set<ClientOrderId> seen;
  std::uint64_t previous_id = 0;
  bool first = true;

  for (const auto& event : events) {
    if (!first && event.envelope.event_id <= previous_id) {
      error = "event ids not strictly increasing at event " + std::to_string(event.envelope.event_id);
      return false;
    }
    first = false;
    previous_id = event.envelope.event_id;

    switch (event.type) {
      case LedgerEventType::order_submitted:
      case LedgerEventType::risk_rejected:
        if (!feed_recovery_event(oms, event, error)) return false;
        seen.insert(event.intent.client_order_id);
        break;
      case LedgerEventType::execution_applied:
      case LedgerEventType::order_cancel_requested:
        if (!feed_recovery_event(oms, event, error)) return false;
        break;
      case LedgerEventType::funding_deposited:
        if (!feed_recovery_event(oms, event, error)) return false;
        break;
      case LedgerEventType::reconcile_attested:
        if (!feed_recovery_event(oms, event, error)) return false;
        break;
      case LedgerEventType::control_applied:
        if (!feed_recovery_event(oms, event, error)) return false;
        break;
    }
  }

  open_out = oms.open_orders();
  std::sort(open_out.begin(), open_out.end(), [](const ExecutionReport& lhs, const ExecutionReport& rhs) { return lhs.client_order_id < rhs.client_order_id; });
  orders_out = static_cast<int>(seen.size());
  return true;
}

}  // namespace detail

// feed_recovery_event applies ONE stored event to a live OMS (boot
// recovery); detail::rebuild_once above drives it twice for the
// determinism check. Returns false plus a reason on duplicates, unknown
// orders, or illegal transitions.
[[nodiscard]] inline bool feed_recovery_event(OrderManager& oms, const LedgerEvent& event, std::string& error) {
  const std::string where = "event " + std::to_string(event.envelope.event_id) + " (" + detail::event_name(event.type) + " " + event.snapshot.client_order_id + ")";
  switch (event.type) {
    case LedgerEventType::order_submitted:
    case LedgerEventType::risk_rejected:
      if (!oms.restore_order(event.intent, event.snapshot)) {
        error = where + ": cannot restore order (duplicate or id mismatch)";
        return false;
      }
      return true;
    case LedgerEventType::execution_applied:
      if (!oms.on_execution_report(event.snapshot)) {
        error = where + ": report rejected (unknown order, conflict or illegal transition from stored state)";
        return false;
      }
      return true;
    case LedgerEventType::order_cancel_requested:
      if (oms.find(event.snapshot.client_order_id) == nullptr) {
        error = where + ": cancel for unknown order";
        return false;
      }
      return true;
    case LedgerEventType::funding_deposited:
      return true;  // startup inventory: authoritative for positions replay, not an order
    case LedgerEventType::reconcile_attested:
      return true;  // settlement attestation: audit-only, never touches books
    case LedgerEventType::control_applied:
      return true;  // switch command: audit-only, risk state is not replayed
  }
  error = where + ": unknown event type";
  return false;
}

[[nodiscard]] inline VerifyResult verify_ledger_replay(const std::vector<LedgerEvent>& events) {
  VerifyResult result{};
  result.events = static_cast<int>(events.size());

  std::vector<ExecutionReport> first_open;
  std::vector<ExecutionReport> second_open;
  int first_orders = 0;
  int second_orders = 0;
  std::string error;
  if (!detail::rebuild_once(events, first_open, first_orders, error)) {
    result.error = "first rebuild failed: " + error;
    return result;
  }
  if (!detail::rebuild_once(events, second_open, second_orders, error)) {
    result.error = "second rebuild failed: " + error;
    return result;
  }
  if (first_orders != second_orders) {
    result.error = "rebuilds disagree on order count";
    return result;
  }
  if (first_open.size() != second_open.size()) {
    result.error = "rebuilds disagree on open-order count";
    return result;
  }
  for (std::size_t i = 0; i < first_open.size(); ++i) {
    std::string field;
    if (!detail::same_snapshot(first_open[i], second_open[i], field)) {
      result.error = "rebuilds disagree on " + first_open[i].client_order_id + " field " + field;
      return result;
    }
  }

  result.ok = true;
  result.orders = first_orders;
  result.open = static_cast<int>(first_open.size());
  return result;
}

}  // namespace sts::replay
