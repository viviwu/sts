// clang-format off
/**
  ******************************************************************************
  * @file           : execution.cpp
  * @author         : vivi wu
  * @brief          : Execution algorithms: immediate, TWAP, POV/VWAP and iceberg.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include "sts/execution/execution_api.hpp"

namespace sts {

namespace {

bool is_terminal_report(OrderStatus status) noexcept { return status == OrderStatus::filled || status == OrderStatus::cancelled || status == OrderStatus::rejected; }

// First-generation children are "<parent>#<seq>", later ones
// "<parent>#r<gen>#<seq>" so a replace never reuses an id.
std::string child_id(const ClientOrderId& parent, int generation, int seq) {
  std::string text = parent + "#";
  if (generation > 0) text += "r" + std::to_string(generation) + "#";
  return text + std::to_string(seq);
}

void check_parent(const OrderIntent& parent, const char* owner) {
  if (parent.client_order_id.empty()) throw std::invalid_argument(std::string(owner) + " parent needs a client order id");
  if (parent.quantity <= 0) throw std::invalid_argument(std::string(owner) + " parent quantity must be positive");
}

void prune_live(std::vector<ClientOrderId>& live, const ExecutionReport& report) {
  if (!is_terminal_report(report.status)) return;
  live.erase(std::remove(live.begin(), live.end(), report.client_order_id), live.end());
}

}  // namespace

OrderIntent ImmediateAlgorithm::make_child(const OrderIntent& parent, int generation) {
  OrderIntent child = parent;
  child.parent_client_order_id = parent.client_order_id;
  child.client_order_id = child_id(parent.client_order_id, generation, 0);
  return child;
}

void ImmediateAlgorithm::add_parent(const OrderIntent& parent) {
  check_parent(parent, "immediate");
  const int generation = generations_[parent.client_order_id];
  pending_.push(QueuedChild{parent.client_order_id, make_child(parent, generation)});
}

std::vector<OrderIntent> ImmediateAlgorithm::poll(Timestamp /*now*/) {
  std::vector<OrderIntent> due;
  while (!pending_.empty()) {
    live_[pending_.front().parent] = pending_.front().child.client_order_id;
    due.push_back(pending_.front().child);
    pending_.pop();
  }
  return due;
}

bool ImmediateAlgorithm::empty() const noexcept { return pending_.empty() && live_.empty(); }

void ImmediateAlgorithm::on_execution_report(const ExecutionReport& report) {
  if (!is_terminal_report(report.status)) return;
  for (auto it = live_.begin(); it != live_.end();) {
    it = (it->second == report.client_order_id) ? live_.erase(it) : std::next(it);
  }
}

void ImmediateAlgorithm::drop_queued(const ClientOrderId& parent_id) {
  std::queue<QueuedChild> rest;
  bool found = false;
  while (!pending_.empty()) {
    if (pending_.front().parent == parent_id) {
      found = true;
    } else {
      rest.push(pending_.front());
    }
    pending_.pop();
  }
  pending_ = std::move(rest);
  if (!found) throw std::invalid_argument("immediate cancel: unknown parent " + parent_id);
}

std::vector<ClientOrderId> ImmediateAlgorithm::cancel_parent(const ClientOrderId& parent_id) {
  if (parent_id.empty()) throw std::invalid_argument("immediate cancel: empty parent id");
  const auto live = live_.find(parent_id);
  if (live != live_.end()) {
    const ClientOrderId child = live->second;
    live_.erase(live);
    return {child};
  }
  drop_queued(parent_id);  // throws on unknown; never submitted, nothing to cancel
  return {};
}

std::vector<ClientOrderId> ImmediateAlgorithm::replace_parent(const OrderIntent& updated_parent) {
  check_parent(updated_parent, "immediate");
  const auto cancelled = cancel_parent(updated_parent.client_order_id);
  const int generation = ++generations_[updated_parent.client_order_id];
  pending_.push(QueuedChild{updated_parent.client_order_id, make_child(updated_parent, generation)});
  return cancelled;
}

TwapAlgorithm::TwapAlgorithm(TwapConfig config)
    : config_{config} {
  if (config_.slices <= 0) throw std::invalid_argument("twap slices must be positive");
  if (config_.interval <= std::chrono::nanoseconds{0}) throw std::invalid_argument("twap interval must be positive");
}

void TwapAlgorithm::add_parent(const OrderIntent& parent) {
  check_parent(parent, "twap");
  ParentSchedule schedule{};
  schedule.parent = parent;
  // Tiny parents emit one unit per slice rather than zero-quantity children.
  schedule.slices = static_cast<int>(std::min<Quantity>(parent.quantity, config_.slices));
  schedule.base = parent.quantity / schedule.slices;
  schedule.remainder = static_cast<int>(parent.quantity % schedule.slices);
  pending_.push(schedule);
}

std::vector<OrderIntent> TwapAlgorithm::poll(Timestamp now) {
  std::vector<OrderIntent> due;
  // Anchor every unscheduled parent at this poll: output is a pure function
  // of parent order and poll times.
  std::queue<ParentSchedule> rest;
  while (!pending_.empty()) {
    ParentSchedule schedule = pending_.front();
    pending_.pop();
    if (!schedule.started) {
      schedule.started = true;
      schedule.start = now;
    }
    while (schedule.emitted < schedule.slices) {
      const Timestamp due_at = schedule.start + schedule.emitted * config_.interval;
      if (due_at > now) break;
      OrderIntent child = schedule.parent;
      child.parent_client_order_id = schedule.parent.client_order_id;
      child.client_order_id = child_id(schedule.parent.client_order_id, schedule.generation, schedule.emitted);
      child.quantity = schedule.base + (schedule.emitted < schedule.remainder ? 1 : 0);
      schedule.live_children.push_back(child.client_order_id);
      due.push_back(child);
      ++schedule.emitted;
    }
    // Retain completed schedules while children are live (see prune_terminal).
    if (schedule.emitted < schedule.slices || !schedule.live_children.empty()) rest.push(schedule);
  }
  pending_ = std::move(rest);
  return due;
}

bool TwapAlgorithm::empty() const noexcept { return pending_.empty(); }

void TwapAlgorithm::prune_terminal(const ExecutionReport& report) {
  std::queue<ParentSchedule> rest;
  while (!pending_.empty()) {
    ParentSchedule schedule = pending_.front();
    pending_.pop();
    prune_live(schedule.live_children, report);
    // Retain completed schedules while children are live so cancel_parent
    // can still fan out to them; drop only once fully reported.
    if (schedule.emitted < schedule.slices || !schedule.live_children.empty()) rest.push(schedule);
  }
  pending_ = std::move(rest);
}

void TwapAlgorithm::on_execution_report(const ExecutionReport& report) { prune_terminal(report); }

std::vector<ClientOrderId> TwapAlgorithm::drop_schedules(const ClientOrderId& parent_id, bool missing_ok) {
  std::vector<ClientOrderId> cancelled;
  std::queue<ParentSchedule> rest;
  bool found = false;
  while (!pending_.empty()) {
    ParentSchedule schedule = pending_.front();
    pending_.pop();
    if (schedule.parent.client_order_id == parent_id) {
      found = true;
      cancelled.insert(cancelled.end(), schedule.live_children.begin(), schedule.live_children.end());
    } else {
      rest.push(schedule);
    }
  }
  pending_ = std::move(rest);
  if (!found && !missing_ok) throw std::invalid_argument("twap cancel: unknown parent " + parent_id);
  return cancelled;
}

std::vector<ClientOrderId> TwapAlgorithm::cancel_parent(const ClientOrderId& parent_id) {
  if (parent_id.empty()) throw std::invalid_argument("twap cancel: empty parent id");
  return drop_schedules(parent_id, false);
}

std::vector<ClientOrderId> TwapAlgorithm::replace_parent(const OrderIntent& updated_parent) {
  check_parent(updated_parent, "twap");
  // Re-plan from scratch: emitted count restarts, generation keeps ids unique.
  int generation = 0;
  {
    std::queue<ParentSchedule> scan = pending_;
    while (!scan.empty()) {
      if (scan.front().parent.client_order_id == updated_parent.client_order_id) {
        generation = scan.front().generation + 1;
      }
      scan.pop();
    }
  }
  const auto cancelled = drop_schedules(updated_parent.client_order_id, false);
  ParentSchedule schedule{};
  schedule.parent = updated_parent;
  schedule.generation = generation;
  schedule.slices = static_cast<int>(std::min<Quantity>(updated_parent.quantity, config_.slices));
  schedule.base = updated_parent.quantity / schedule.slices;
  schedule.remainder = static_cast<int>(updated_parent.quantity % schedule.slices);
  pending_.push(schedule);
  return cancelled;
}

PovAlgorithm::PovAlgorithm(PovConfig config)
    : config_{config} {
  if (config_.participation_bps <= 0 || config_.participation_bps > 10'000) {
    throw std::invalid_argument("pov participation must be in (0, 10000]");
  }
}

void PovAlgorithm::add_parent(const OrderIntent& parent) {
  check_parent(parent, "pov");
  ParentState state{};
  state.parent = parent;
  state.remaining = parent.quantity;
  pending_.push(state);
}

void PovAlgorithm::on_quote(const QuoteEvent& quote) {
  std::queue<ParentState> rest;
  while (!pending_.empty()) {
    ParentState state = pending_.front();
    pending_.pop();
    if (state.remaining > 0 && quote.instrument_id == state.parent.instrument_id && quote.cumulative_volume > 0) {
      if (!state.seen) {
        state.seen = true;
      } else if (quote.cumulative_volume > state.last_cumulative) {
        const Quantity bar = quote.cumulative_volume - state.last_cumulative;
        state.budget_bps_points += bar * config_.participation_bps;
      }
      state.last_cumulative = quote.cumulative_volume;
    }
    rest.push(state);
  }
  pending_ = std::move(rest);
}

std::vector<OrderIntent> PovAlgorithm::poll(Timestamp /*now*/) {
  std::vector<OrderIntent> due;
  std::queue<ParentState> rest;
  while (!pending_.empty()) {
    ParentState state = pending_.front();
    pending_.pop();
    auto units = static_cast<Quantity>(state.budget_bps_points / 10'000);
    units = std::min(units, state.remaining);
    if (units > 0) {
      OrderIntent child = state.parent;
      child.parent_client_order_id = state.parent.client_order_id;
      child.client_order_id = child_id(state.parent.client_order_id, state.generation, state.emitted);
      child.quantity = units;
      state.live_children.push_back(child.client_order_id);
      due.push_back(child);
      ++state.emitted;
      state.remaining -= units;
      state.budget_bps_points -= units * 10'000;
    }
    // Retain filled-down states while children are live so cancel fans out.
    if (state.remaining > 0 || !state.live_children.empty()) rest.push(state);
  }
  pending_ = std::move(rest);
  return due;
}

bool PovAlgorithm::empty() const noexcept { return pending_.empty(); }

void PovAlgorithm::prune_terminal(const ExecutionReport& report) {
  std::queue<ParentState> rest;
  while (!pending_.empty()) {
    ParentState state = pending_.front();
    pending_.pop();
    prune_live(state.live_children, report);
    if (state.remaining > 0 || !state.live_children.empty()) rest.push(state);
  }
  pending_ = std::move(rest);
}

void PovAlgorithm::on_execution_report(const ExecutionReport& report) { prune_terminal(report); }

std::vector<ClientOrderId> PovAlgorithm::drop_states(const ClientOrderId& parent_id, bool missing_ok) {
  std::vector<ClientOrderId> cancelled;
  std::queue<ParentState> rest;
  bool found = false;
  while (!pending_.empty()) {
    ParentState state = pending_.front();
    pending_.pop();
    if (state.parent.client_order_id == parent_id) {
      found = true;
      cancelled.insert(cancelled.end(), state.live_children.begin(), state.live_children.end());
    } else {
      rest.push(state);
    }
  }
  pending_ = std::move(rest);
  if (!found && !missing_ok) throw std::invalid_argument("pov cancel: unknown parent " + parent_id);
  return cancelled;
}

std::vector<ClientOrderId> PovAlgorithm::cancel_parent(const ClientOrderId& parent_id) {
  if (parent_id.empty()) throw std::invalid_argument("pov cancel: empty parent id");
  return drop_states(parent_id, false);
}

std::vector<ClientOrderId> PovAlgorithm::replace_parent(const OrderIntent& updated_parent) {
  check_parent(updated_parent, "pov");
  int generation = 0;
  {
    std::queue<ParentState> scan = pending_;
    while (!scan.empty()) {
      if (scan.front().parent.client_order_id == updated_parent.client_order_id) {
        generation = scan.front().generation + 1;
      }
      scan.pop();
    }
  }
  const auto cancelled = drop_states(updated_parent.client_order_id, false);
  ParentState state{};
  state.parent = updated_parent;
  state.generation = generation;
  state.remaining = updated_parent.quantity;
  pending_.push(state);
  return cancelled;
}

VwapAlgorithm::VwapAlgorithm(VwapConfig config)
    : config_{std::move(config)} {
  if (config_.weights_bps.empty()) throw std::invalid_argument("vwap curve must not be empty");
  for (const int weight : config_.weights_bps) {
    if (weight <= 0) throw std::invalid_argument("vwap weights must be positive");
    total_weight_ += weight;
  }
  if (config_.interval <= std::chrono::nanoseconds{0}) throw std::invalid_argument("vwap interval must be positive");
}

void VwapAlgorithm::add_parent(const OrderIntent& parent) {
  check_parent(parent, "vwap");
  ParentSchedule schedule{};
  schedule.parent = parent;
  pending_.push(schedule);
}

std::vector<OrderIntent> VwapAlgorithm::poll(Timestamp now) {
  std::vector<OrderIntent> due;
  std::queue<ParentSchedule> rest;
  while (!pending_.empty()) {
    ParentSchedule schedule = pending_.front();
    pending_.pop();
    if (!schedule.started) {
      schedule.started = true;
      schedule.start = now;
    }
    const auto slice_count = static_cast<int>(config_.weights_bps.size());
    while (schedule.emitted < slice_count) {
      const Timestamp due_at = schedule.start + schedule.emitted * config_.interval;
      if (due_at > now) break;
      const bool last = schedule.emitted == slice_count - 1;
      const Quantity quantity =
          last ? schedule.parent.quantity - schedule.emitted_quantity : schedule.parent.quantity * config_.weights_bps[static_cast<std::vector<int>::size_type>(schedule.emitted)] / total_weight_;
      const int seq = schedule.emitted;
      ++schedule.emitted;
      if (quantity <= 0 && !last) continue;  // tiny parents skip dust slices; the last slice absorbs them
      OrderIntent child = schedule.parent;
      child.parent_client_order_id = schedule.parent.client_order_id;
      child.client_order_id = child_id(schedule.parent.client_order_id, schedule.generation, seq);
      child.quantity = quantity;
      schedule.live_children.push_back(child.client_order_id);
      due.push_back(child);
      schedule.emitted_quantity += quantity;
    }
    // Retain completed curves while children are live so cancel fans out.
    if (schedule.emitted < slice_count || !schedule.live_children.empty()) rest.push(schedule);
  }
  pending_ = std::move(rest);
  return due;
}

bool VwapAlgorithm::empty() const noexcept { return pending_.empty(); }

void VwapAlgorithm::prune_terminal(const ExecutionReport& report) {
  std::queue<ParentSchedule> rest;
  while (!pending_.empty()) {
    ParentSchedule schedule = pending_.front();
    pending_.pop();
    prune_live(schedule.live_children, report);
    const auto slice_count = static_cast<int>(config_.weights_bps.size());
    if (schedule.emitted < slice_count || !schedule.live_children.empty()) rest.push(schedule);
  }
  pending_ = std::move(rest);
}

void VwapAlgorithm::on_execution_report(const ExecutionReport& report) { prune_terminal(report); }

std::vector<ClientOrderId> VwapAlgorithm::drop_schedules(const ClientOrderId& parent_id, bool missing_ok) {
  std::vector<ClientOrderId> cancelled;
  std::queue<ParentSchedule> rest;
  bool found = false;
  while (!pending_.empty()) {
    ParentSchedule schedule = pending_.front();
    pending_.pop();
    if (schedule.parent.client_order_id == parent_id) {
      found = true;
      cancelled.insert(cancelled.end(), schedule.live_children.begin(), schedule.live_children.end());
    } else {
      rest.push(schedule);
    }
  }
  pending_ = std::move(rest);
  if (!found && !missing_ok) throw std::invalid_argument("vwap cancel: unknown parent " + parent_id);
  return cancelled;
}

std::vector<ClientOrderId> VwapAlgorithm::cancel_parent(const ClientOrderId& parent_id) {
  if (parent_id.empty()) throw std::invalid_argument("vwap cancel: empty parent id");
  return drop_schedules(parent_id, false);
}

std::vector<ClientOrderId> VwapAlgorithm::replace_parent(const OrderIntent& updated_parent) {
  check_parent(updated_parent, "vwap");
  int generation = 0;
  {
    std::queue<ParentSchedule> scan = pending_;
    while (!scan.empty()) {
      if (scan.front().parent.client_order_id == updated_parent.client_order_id) {
        generation = scan.front().generation + 1;
      }
      scan.pop();
    }
  }
  const auto cancelled = drop_schedules(updated_parent.client_order_id, false);
  ParentSchedule schedule{};
  schedule.parent = updated_parent;
  schedule.generation = generation;
  pending_.push(schedule);
  return cancelled;
}

IcebergAlgorithm::IcebergAlgorithm(IcebergConfig config)
    : config_{config} {
  if (config_.display_quantity <= 0) throw std::invalid_argument("iceberg display must be positive");
}

void IcebergAlgorithm::add_parent(const OrderIntent& parent) {
  check_parent(parent, "iceberg");
  ParentState state{};
  state.parent = parent;
  state.remaining = parent.quantity;
  pending_.push(state);
}

void IcebergAlgorithm::on_execution_report(const ExecutionReport& report) {
  std::queue<ParentState> rest;
  while (!pending_.empty()) {
    ParentState state = pending_.front();
    pending_.pop();
    apply_report_to_state(state, report);
    if (state.remaining > 0) rest.push(state);
  }
  pending_ = std::move(rest);
}

void IcebergAlgorithm::apply_report_to_state(ParentState& state, const ExecutionReport& report) {
  if (!state.active || report.client_order_id != state.active_child || state.remaining <= 0) return;
  const bool duplicate = !report.execution_id.empty() && !state.seen_execution_ids.insert(report.execution_id).second;
  if (!duplicate) {
    const Quantity cumulative = report.cumulative_quantity > 0 ? report.cumulative_quantity : state.active_filled + report.last_quantity;
    const Quantity capped = cumulative > state.active_quantity ? state.active_quantity : cumulative;
    if (capped > state.active_filled) {
      state.remaining -= capped - state.active_filled;
      state.active_filled = capped;
    }
  }
  if (is_terminal_report(report.status) || state.active_filled >= state.active_quantity) {
    state.active = false;
  }
}

std::vector<OrderIntent> IcebergAlgorithm::poll(Timestamp /*now*/) {
  std::vector<OrderIntent> due;
  std::queue<ParentState> rest;
  while (!pending_.empty()) {
    ParentState state = pending_.front();
    pending_.pop();
    if (!state.active && state.remaining > 0) {
      OrderIntent child = state.parent;
      child.parent_client_order_id = state.parent.client_order_id;
      child.client_order_id = child_id(state.parent.client_order_id, 0, state.emitted);
      child.quantity = state.remaining > config_.display_quantity ? config_.display_quantity : state.remaining;
      ++state.emitted;
      state.active = true;
      state.active_child = child.client_order_id;
      state.active_quantity = child.quantity;
      state.active_filled = 0;
      state.seen_execution_ids.clear();
      due.push_back(child);
    }
    if (state.remaining > 0) rest.push(state);
  }
  pending_ = std::move(rest);
  return due;
}

bool IcebergAlgorithm::empty() const noexcept { return pending_.empty(); }

std::vector<ClientOrderId> IcebergAlgorithm::cancel_parent(const ClientOrderId& parent_id) {
  if (parent_id.empty()) throw std::invalid_argument("iceberg cancel: empty parent id");
  std::vector<ClientOrderId> cancelled;
  std::queue<ParentState> rest;
  bool found = false;
  while (!pending_.empty()) {
    ParentState state = pending_.front();
    pending_.pop();
    if (state.parent.client_order_id == parent_id) {
      found = true;
      if (state.active) cancelled.push_back(state.active_child);
    } else {
      rest.push(state);
    }
  }
  pending_ = std::move(rest);
  if (!found) throw std::invalid_argument("iceberg cancel: unknown parent " + parent_id);
  return cancelled;
}

std::vector<ClientOrderId> IcebergAlgorithm::replace_parent(const OrderIntent& updated_parent) {
  check_parent(updated_parent, "iceberg");
  // Only the unexecuted remainder is re-planned: the live peak is recalled
  // for cancel, fills already recorded stay recorded, and remaining
  // already includes the recalled peak's unfilled portion. The live peak
  // itself cannot be edited in place (no native amend); the next peak
  // uses the updated params. Emitted count continues so child ids stay
  // unique without generations.
  std::vector<ClientOrderId> cancelled;
  std::queue<ParentState> rest;
  bool found = false;
  while (!pending_.empty()) {
    ParentState state = pending_.front();
    pending_.pop();
    if (state.parent.client_order_id == updated_parent.client_order_id) {
      found = true;
      if (state.active) {
        cancelled.push_back(state.active_child);
        state.active = false;
      }
      const Quantity remaining = state.remaining;
      const int emitted = state.emitted;
      state = ParentState{};
      state.parent = updated_parent;
      state.remaining = remaining;
      state.emitted = emitted;
    }
    rest.push(state);
  }
  pending_ = std::move(rest);
  if (!found) throw std::invalid_argument("iceberg replace: unknown parent " + updated_parent.client_order_id);
  return cancelled;
}

}  // namespace sts
