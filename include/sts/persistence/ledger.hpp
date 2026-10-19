// clang-format off
/**
  ******************************************************************************
  * @file           : ledger.hpp
  * @author         : vivi wu
  * @brief          : Event-ledger interface, in-memory drill ledger and recovery snapshot.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "sts/core/event.hpp"
#include "sts/core/types.hpp"

namespace sts {

// Ledger event kinds. Every OMS state transition produces exactly one
// recordable event; no-op dedups produce none. funding_deposited carries
// startup funding (opening cash/inventory, M11-7): it is authoritative for
// positions replay but never touches the OMS state machine.
// reconcile_attested carries a human settlement confirmation (M14): an
// attestation, not a state transition — replay verifies its shape and
// otherwise ignores it, and the projections skip it like funding.
// control_applied carries an operator switch command (M15: halt/resume and
// the block/unblock verbs): audit-only on replay, like attestations. Order
// and strategy lifecycle verbs need no entry — their order events already
// trail them.
enum class LedgerEventType : std::uint8_t { order_submitted, execution_applied, order_cancel_requested, risk_rejected, funding_deposited, reconcile_attested, control_applied };

// Startup funding deposit (M11-7): one event per opening position plus at
// most one cash leg (quantity == 0). Empty strategy sorts into the
// passthrough basket on replay, matching broker-side funding. Cash is the
// run's starting cash; replay takes the first non-zero cash leg and falls
// back to the caller's default when the stream carries none (pre-funding
// ledgers).
struct FundingDeposit {
  StrategyId strategy;
  InstrumentId instrument;
  Quantity quantity{0};
  Price average_cost{0};
  Price cash{0};
};

// Operator switch command record (M15): halt/resume and the six
// block/unblock verbs. source names the entry path ("inbox" for the file
// inbox; gateway sessions will carry the operator id). Only successful
// applications are recorded — refusals stay log-only, like before.
struct ControlRecord {
  std::string command;
  std::string target;  // strategy/account/instrument id, empty for halt/resume
  bool blocked_after{false};
  std::string source;
};

// Settlement attestation (M14, option A): a human confirms having reviewed
// a settle report. broker_snapshot_time identifies the reviewed snapshot;
// ledger_watermark pins the ledger prefix covered; the dirty counts repeat
// what the report showed (cash/position comparable rows only — broker-only
// informational fields never count); disclosed_fees repeats the STS-side
// fee total shown. The server records the claim verbatim; auditing the
// claim against the report is out-of-band by design.
struct ReconcileAttestation {
  std::string operator_id;
  std::string broker_snapshot_time;
  std::uint64_t ledger_watermark{0};
  int cash_dirty{0};
  int position_dirty{0};
  Price disclosed_fees{0};
  std::string note;
};

// Self-contained recovery unit: envelope for correlation, intent for
// rebuild (requested quantity, trace id), snapshot for fast-forward.
struct LedgerEvent {
  EventEnvelope envelope;
  LedgerEventType type{LedgerEventType::order_submitted};
  OrderIntent intent;
  ExecutionReport snapshot;
  FundingDeposit funding;
  ReconcileAttestation attestation;
  ControlRecord control;
};

// Crash-recovery anchor: reload open orders, then replay ledger events
// with event_id greater than last_event_id, then reconcile with the
// broker. Written by the recovery driver, not by the ledger itself.
struct LedgerSnapshot {
  std::uint64_t last_event_id{0};
  std::string config_version;
  std::string rules_version;
  std::vector<ExecutionReport> open_orders;
};

// Append-only event log. The ledger assigns envelope.event_id from its own
// generator (construct with an explicit start to reproduce replay runs).
// File-backed JSONL implementation is FileEventLedger (file_ledger.hpp);
// M1 uses the in-memory implementation below.
class EventLedger {
 public:
  EventLedger() = default;
  virtual ~EventLedger() = default;

  EventLedger(const EventLedger&) = delete;
  EventLedger& operator=(const EventLedger&) = delete;
  EventLedger(EventLedger&&) = delete;
  EventLedger& operator=(EventLedger&&) = delete;

  // Stores the event, stamps envelope.event_id, returns the assigned id.
  virtual std::uint64_t append(LedgerEvent event) = 0;
  // All events with event_id >= from_event_id, in append order.
  [[nodiscard]] virtual std::vector<LedgerEvent> replay(std::uint64_t from_event_id) const = 0;
  [[nodiscard]] virtual std::uint64_t latest_event_id() const = 0;
};

class InMemoryEventLedger final : public EventLedger {
 public:
  explicit InMemoryEventLedger(std::uint64_t start_event_id = 1)
      : ids_{start_event_id} {}

  std::uint64_t append(LedgerEvent event) override {
    std::lock_guard<std::mutex> lock{mutex_};
    const std::uint64_t assigned_id = ids_.next();
    event.envelope.event_id = assigned_id;
    events_.push_back(std::move(event));
    return assigned_id;
  }

  std::vector<LedgerEvent> replay(std::uint64_t from_event_id) const override {
    std::lock_guard<std::mutex> lock{mutex_};
    std::vector<LedgerEvent> out;
    for (const auto& event : events_) {
      if (event.envelope.event_id >= from_event_id) out.push_back(event);
    }
    return out;
  }

  std::uint64_t latest_event_id() const override {
    std::lock_guard<std::mutex> lock{mutex_};
    return events_.empty() ? 0 : events_.back().envelope.event_id;
  }

 private:
  mutable std::mutex mutex_;
  EventIdGenerator ids_;
  std::vector<LedgerEvent> events_;
};

}  // namespace sts
