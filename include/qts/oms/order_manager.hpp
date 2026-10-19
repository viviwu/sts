// clang-format off
/**
  ******************************************************************************
  * @file           : order_manager.hpp
  * @author         : vivi wu
  * @brief          : OMS: single source of truth for order state, routing, dedup and recovery.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "qts/broker/trade_api.hpp"
#include "qts/core/event_bus.hpp"
#include "qts/ops/metrics.hpp"
#include "qts/risk/risk_engine.hpp"

namespace qts {

class EventLedger;
enum class LedgerEventType : std::uint8_t;

struct ReconcileDiff {
  ClientOrderId client_order_id;
  std::string reason;
};

// One retained order: the submission intent (carries account + strategy, the
// sub-account dimension) plus the latest broker snapshot. Served to the
// gateway fills/progress queries in submission order (seq).
struct OrderView {
  OrderIntent intent;
  ExecutionReport snapshot;
  std::uint64_t seq{0};
};

class OrderManager {
 public:
  // ledger is optional: when non-null every accepted transition is
  // recorded for crash recovery. The ledger must outlive the OMS.
  OrderManager(TradeApi& gateway, RiskEngine& risk, EventBus& events, EventLedger* ledger = nullptr);
  ~OrderManager();

  OrderManager(const OrderManager&) = delete;
  OrderManager& operator=(const OrderManager&) = delete;
  OrderManager(OrderManager&&) = delete;
  OrderManager& operator=(OrderManager&&) = delete;

  bool submit(const OrderIntent& intent);
  // Reason of the latest submit refusal (risk/duplicate/broker-submit;
  // same string the ledger risk_rejected event carries). Read immediately
  // after a false return, same thread only (the tick thread owns the OMS).
  [[nodiscard]] const std::string& last_reject_reason() const noexcept { return last_reject_reason_; }
  bool cancel(const ClientOrderId& client_order_id);
  // Returns false for an unknown, conflicting or illegal broker report.
  // Callers should retain and alert on rejected reports for reconciliation.
  bool on_execution_report(const ExecutionReport& report);
  [[nodiscard]] const ExecutionReport* find(const ClientOrderId& client_order_id) const;
  // Stored intent for a known order (M11-4, operator replace): null when the
  // order id is unknown. The intent carries the parent linkage needed to
  // rebuild an updated parent from any of its live children.
  [[nodiscard]] const OrderIntent* find_intent(const ClientOrderId& client_order_id) const;

  // Recovery surface (M1-3). restore_order rebuilds book entries without
  // touching the gateway or risk; replay post-snapshot ledger events with
  // on_execution_report; reconcile_open_orders diffs broker truth.
  void set_ledger_versions(std::string config_version, std::string rules_version);
  // Trading-day label stamped on every recorded envelope (session label,
  // like the versions above; "" means unknown). Set once per run by the
  // driver from the wall clock; intraday precision is not claimed.
  void set_trade_date(std::string trade_date);
  // Optional metrics sink (M3-3): when set, submit/cancel/report paths
  // maintain oms_* counters and the open-orders gauge. Must outlive the OMS.
  void set_metrics(MetricsRegistry* metrics);
  // Ledger attach point. Boot recovery constructs the OMS detached
  // (nullptr), replays history without recording duplicates, then attaches
  // the file ledger before live traffic. Must outlive the OMS.
  void set_ledger(EventLedger* ledger);
  [[nodiscard]] std::vector<ExecutionReport> open_orders() const;
  // Every retained order (open + terminal) in submission order. Read-only
  // fan-out for the gateway fills/progress queries; the OMS already retains
  // all records, so this adds no new retention.
  [[nodiscard]] std::vector<OrderView> all_orders() const;
  // Terminal-state predicate shared with the gateway progress counters.
  [[nodiscard]] static bool is_terminal(OrderStatus status) noexcept;
  // Distinct parent ids behind the working set (M11-3): children carry
  // parent_client_order_id, parents never reach the OMS. The gateway fans
  // shutdown cancels out through the execution algorithm per parent so its
  // schedules converge instead of going stale. Empty when every open order
  // is parentless (direct submissions).
  [[nodiscard]] std::vector<ClientOrderId> open_parent_ids() const;
  // Live (unfilled) sell exposure per (strategy, instrument) for T+1
  // reservation: open sell orders sized against the same sellable would
  // otherwise overshoot when their fills land. Sums intent minus cumulative
  // per live record.
  [[nodiscard]] RiskEngine::SellMap live_sell_exposure() const;
  // Live buy exposure as account-level notional for the buying-power gate:
  // concurrent live buys would otherwise overspend the same cash.
  [[nodiscard]] Price live_buy_exposure() const;
  bool restore_order(const OrderIntent& intent, const ExecutionReport& snapshot);
  [[nodiscard]] std::vector<ReconcileDiff> reconcile_open_orders(const std::vector<ExecutionReport>& broker_open) const;
  // New trading day (archforce §3.3): broker execution ids are only unique
  // within a day, so the dedup sets are cleared in step with settlement.
  void settle_day();

 private:
  struct OrderRecord {
    OrderIntent intent;
    ExecutionReport snapshot;
    std::unordered_set<ExecutionId> seen_execution_ids;
    std::uint64_t seq{0};  // submission order (submit + restore share one counter)
  };

  [[nodiscard]] static bool is_transition_allowed(OrderStatus from, OrderStatus target) noexcept;
  void record_event(LedgerEventType type, const OrderIntent& intent, const ExecutionReport& snapshot);
  void note_open_orders() const;

  TradeApi& gateway_;
  RiskEngine& risk_;
  EventBus& events_;
  EventLedger* ledger_;
  MetricsRegistry* metrics_{nullptr};
  std::string ledger_config_version_;
  std::string ledger_rules_version_;
  std::string ledger_trade_date_;
  std::unordered_map<ClientOrderId, OrderRecord> orders_;
  std::uint64_t next_seq_{1};
  std::string last_reject_reason_;  // reason of the latest submit refusal
};

}  // namespace qts
