// clang-format off
/**
  ******************************************************************************
  * @file           : execution_api.hpp
  * @author         : vivi wu
  * @brief          : Execution-algorithm port plus parent/child lifecycle (cancel/replace).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <chrono>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "sts/broker/quote_api.hpp"
#include "sts/core/types.hpp"

namespace sts {

// Execution boundary (M4 contract): algorithms turn strategy-level parent
// intents into child OrderIntents over time. Only children travel the
// risk -> OMS -> broker path; each child carries parent_client_order_id and
// inherits the parent trace_id so orders, fills and risk decisions stay
// correlated (invariant 4). Scheduling is clock-driven: callers add parents
// as they arrive and drain due children via poll(). poll() must be
// deterministic: same parents in the same order + same poll times => same
// children out, so replay and paper stay consistent (M2 contract).
class ExecutionAlgorithm {
 public:
  ExecutionAlgorithm() = default;
  virtual ~ExecutionAlgorithm() = default;

  ExecutionAlgorithm(const ExecutionAlgorithm&) = delete;
  ExecutionAlgorithm& operator=(const ExecutionAlgorithm&) = delete;
  ExecutionAlgorithm(ExecutionAlgorithm&&) = delete;
  ExecutionAlgorithm& operator=(ExecutionAlgorithm&&) = delete;

  virtual void add_parent(const OrderIntent& parent) = 0;
  [[nodiscard]] virtual std::vector<OrderIntent> poll(Timestamp now) = 0;
  [[nodiscard]] virtual bool empty() const noexcept = 0;
  // Market-data feed for volume-tracking algorithms (POV/VWAP). The default
  // ignores quotes; tracking algorithms accrue volume budgets here and emit
  // only via poll(), so output stays a pure function of quote sequence and
  // poll times.
  virtual void on_quote(const QuoteEvent& /*quote*/) {}
  // Fill feedback for stateful algorithms (iceberg refresh). Default
  // ignores; live-path forwarding lands with parent/child cancel
  // propagation in M4-4.
  virtual void on_execution_report(const ExecutionReport& /*report*/) {}
  // Parent lifecycle (M4-4): cancel fans out to live children, replace
  // swaps the parent plan. Returned child ids go to OMS.cancel, which
  // returns false for already-terminal children, so callers must tolerate
  // failures. No channel here offers native amend (archforce mapping), so
  // replace is cancel + re-plan under a new generation suffix, never an
  // in-place edit: first-generation children are "<parent>#<seq>",
  // later ones "<parent>#r<gen>#<seq>".
  virtual std::vector<ClientOrderId> cancel_parent(const ClientOrderId& /*parent_id*/) { return {}; }
  virtual std::vector<ClientOrderId> replace_parent(const OrderIntent& /*updated_parent*/) { throw std::logic_error("replace not supported"); }
};

// M4-0 pass-through: each parent emits exactly one child at the next poll.
// The child keeps every parent field; only the identity changes:
// client_order_id becomes "<parent>#<per-parent sequence>" (here always
// "#0") and parent_client_order_id records the parent. Timing is ignored,
// so wiring strategy -> execution -> risk/OMS preserves current drill
// counts while exercising the parent/child linkage end to end.
class ImmediateAlgorithm final : public ExecutionAlgorithm {
 public:
  void add_parent(const OrderIntent& parent) override;
  [[nodiscard]] std::vector<OrderIntent> poll(Timestamp now) override;
  [[nodiscard]] bool empty() const noexcept override;
  void on_execution_report(const ExecutionReport& report) override;
  std::vector<ClientOrderId> cancel_parent(const ClientOrderId& parent_id) override;
  std::vector<ClientOrderId> replace_parent(const OrderIntent& updated_parent) override;

 private:
  struct QueuedChild {
    ClientOrderId parent;
    OrderIntent child;
  };

  [[nodiscard]] static OrderIntent make_child(const OrderIntent& parent, int generation);
  void drop_queued(const ClientOrderId& parent_id);

  std::queue<QueuedChild> pending_;
  std::unordered_map<ClientOrderId, ClientOrderId> live_;  // parent -> emitted child
  std::unordered_map<ClientOrderId, int> generations_;     // parent -> last used generation
};

// M4-1: time-weighted average price. Each parent is split into a fixed
// number of equal-ish children released one per interval on the caller's
// clock. The schedule anchors at the first poll after add_parent; sparse
// polling emits all due children at once (catch-up), so output depends only
// on parent order and poll times, never on wall-clock gaps.
struct TwapConfig {
  int slices{4};
  std::chrono::nanoseconds interval{std::chrono::seconds{1}};
};

class TwapAlgorithm final : public ExecutionAlgorithm {
 public:
  explicit TwapAlgorithm(TwapConfig config = {});
  void add_parent(const OrderIntent& parent) override;
  [[nodiscard]] std::vector<OrderIntent> poll(Timestamp now) override;
  [[nodiscard]] bool empty() const noexcept override;
  void on_execution_report(const ExecutionReport& report) override;
  std::vector<ClientOrderId> cancel_parent(const ClientOrderId& parent_id) override;
  std::vector<ClientOrderId> replace_parent(const OrderIntent& updated_parent) override;

 private:
  struct ParentSchedule {
    OrderIntent parent;
    int slices{0};
    Quantity base{0};
    int remainder{0};
    int emitted{0};
    int generation{0};
    Timestamp start{Timestamp{}};
    bool started{false};
    std::vector<ClientOrderId> live_children;
  };

  void prune_terminal(const ExecutionReport& report);
  std::vector<ClientOrderId> drop_schedules(const ClientOrderId& parent_id, bool missing_ok);

  TwapConfig config_;
  std::queue<ParentSchedule> pending_;
};

// M4-2: percentage of volume. Each parent accrues a budget of
// participation_bps ten-thousandths of the market volume printed on its
// instrument (via QuoteEvent::cumulative_volume deltas) and converts budget
// into limit children at poll time. Budgets accrue per parent from the
// quotes seen after add_parent; late parents earn no retroactive budget.
// Decreases/resets of cumulative volume resync without negative accrual.
struct PovConfig {
  int participation_bps{1000};
};

class PovAlgorithm final : public ExecutionAlgorithm {
 public:
  explicit PovAlgorithm(PovConfig config = {});
  void add_parent(const OrderIntent& parent) override;
  void on_quote(const QuoteEvent& quote) override;
  [[nodiscard]] std::vector<OrderIntent> poll(Timestamp now) override;
  [[nodiscard]] bool empty() const noexcept override;
  void on_execution_report(const ExecutionReport& report) override;
  std::vector<ClientOrderId> cancel_parent(const ClientOrderId& parent_id) override;
  std::vector<ClientOrderId> replace_parent(const OrderIntent& updated_parent) override;

 private:
  struct ParentState {
    OrderIntent parent;
    Quantity remaining{0};
    std::int64_t budget_bps_points{0};
    int emitted{0};
    int generation{0};
    Quantity last_cumulative{0};
    bool seen{false};
    std::vector<ClientOrderId> live_children;
  };

  void prune_terminal(const ExecutionReport& report);
  std::vector<ClientOrderId> drop_states(const ClientOrderId& parent_id, bool missing_ok);

  PovConfig config_;
  std::queue<ParentState> pending_;
};

// M4-2: scheduled VWAP. Children follow an explicit intraday volume curve
// (weights in ten-thousandths, e.g. {5000, 3000, 2000}); slice i releases
// at anchor + i * interval with the same catch-up rule as TWAP, and the
// last slice absorbs integer remainder. The curve stands in for a
// history-calibrated profile until research volumes land (post-M4).
struct VwapConfig {
  std::vector<int> weights_bps{5000, 3000, 2000};
  std::chrono::nanoseconds interval{std::chrono::seconds{1}};
};

class VwapAlgorithm final : public ExecutionAlgorithm {
 public:
  explicit VwapAlgorithm(VwapConfig config = {});
  void add_parent(const OrderIntent& parent) override;
  [[nodiscard]] std::vector<OrderIntent> poll(Timestamp now) override;
  [[nodiscard]] bool empty() const noexcept override;
  void on_execution_report(const ExecutionReport& report) override;
  std::vector<ClientOrderId> cancel_parent(const ClientOrderId& parent_id) override;
  std::vector<ClientOrderId> replace_parent(const OrderIntent& updated_parent) override;

 private:
  struct ParentSchedule {
    OrderIntent parent;
    int emitted{0};
    int generation{0};
    Quantity emitted_quantity{0};
    Timestamp start{Timestamp{}};
    bool started{false};
    std::vector<ClientOrderId> live_children;
  };

  void prune_terminal(const ExecutionReport& report);
  std::vector<ClientOrderId> drop_schedules(const ClientOrderId& parent_id, bool missing_ok);

  VwapConfig config_;
  std::int64_t total_weight_{0};
  std::queue<ParentSchedule> pending_;
};

// M4-3: iceberg. Only display_quantity is visible at a time; the next peak
// releases on poll after the active child is fully consumed. Consumption is
// tracked from execution reports (cumulative preferred, last_quantity +
// execution-id dedup otherwise); partially cancelled children return their
// remainder to the hidden quantity. Live-path report forwarding lands in
// M4-4 with cancel propagation; until then the algorithm is driven by tests
// and replay harnesses.
struct IcebergConfig {
  Quantity display_quantity{100};
};

class IcebergAlgorithm final : public ExecutionAlgorithm {
 public:
  explicit IcebergAlgorithm(IcebergConfig config = {});
  void add_parent(const OrderIntent& parent) override;
  void on_execution_report(const ExecutionReport& report) override;
  [[nodiscard]] std::vector<OrderIntent> poll(Timestamp now) override;
  [[nodiscard]] bool empty() const noexcept override;
  std::vector<ClientOrderId> cancel_parent(const ClientOrderId& parent_id) override;
  std::vector<ClientOrderId> replace_parent(const OrderIntent& updated_parent) override;

 private:
  struct ParentState {
    OrderIntent parent;
    Quantity remaining{0};
    int emitted{0};
    bool active{false};
    ClientOrderId active_child;
    Quantity active_quantity{0};
    Quantity active_filled{0};
    std::unordered_set<ExecutionId> seen_execution_ids;
  };

  static void apply_report_to_state(ParentState& state, const ExecutionReport& report);

  IcebergConfig config_;
  std::queue<ParentState> pending_;
};

}  // namespace sts
