// clang-format off
/**
  ******************************************************************************
  * @file           : routing.cpp
  * @author         : vivi wu
  * @brief          : Router implementation: selection, failover, split synthesis and recovery.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/broker/routing.hpp"

#include <algorithm>
#include <chrono>
#include <string>

namespace sts {

RoutingPolicy::RoutingPolicy(RoutingWeights weights)
    : weights_(weights) {}

std::optional<std::size_t> RoutingPolicy::pick(const OrderIntent& intent, const std::vector<Candidate>& channels) const {
  const Price unit_price = intent.limit_price > 0 ? intent.limit_price : intent.protection_price;
  // Divide-first: quantity * (unit/10'000) stays in int64 for any risk-capped
  // size; truncation only dulls sub-yuan fee differences.
  const std::int64_t fee_base = intent.quantity * (unit_price / 10'000);
  std::optional<std::size_t> best;
  std::int64_t best_score = 0;
  for (std::size_t i = 0; i < channels.size(); ++i) {
    if (channels[i].api == nullptr || !channels[i].api->connected()) continue;
    const std::int64_t fee_points = (fee_base * channels[i].stats.fee_bps) / 10'000;
    const std::int64_t score = ((weights_.weight_fee * fee_points) + (weights_.weight_latency * channels[i].stats.latency_us)) - (weights_.weight_fill * channels[i].stats.fill_rate_bps);
    if (!best.has_value() || score < best_score) {
      best = i;
      best_score = score;
    }
  }
  return best;
}

Router::Router(std::vector<RoutingPolicy::Candidate> channels, RoutingPolicy policy, const Clock* clock, std::int64_t alpha_bps)
    : channels_(std::move(channels)),
      policy_(policy),
      clock_(clock),
      alpha_bps_(alpha_bps) {
  if (alpha_bps_ < 1) alpha_bps_ = 1;
  if (alpha_bps_ > 10'000) alpha_bps_ = 10'000;
  meters_.reserve(channels_.size());
  for (const auto& candidate : channels_) {
    ChannelMeter meter{};
    meter.fill_rate_bps = candidate.stats.fill_rate_bps;
    meter.latency_us = candidate.stats.latency_us;
    meters_.push_back(std::move(meter));
  }
}

std::vector<ChannelStats> Router::channel_stats() const {
  std::vector<ChannelStats> out;
  out.reserve(channels_.size());
  for (std::size_t i = 0; i < channels_.size(); ++i) {
    ChannelStats stats = channels_[i].stats;
    if (i < meters_.size()) {
      stats.fill_rate_bps = meters_[i].fill_rate_bps;
      stats.latency_us = meters_[i].latency_us;
    }
    out.push_back(std::move(stats));
  }
  return out;
}

bool Router::is_terminal_report(OrderStatus status) noexcept { return status == OrderStatus::filled || status == OrderStatus::cancelled || status == OrderStatus::rejected; }

void Router::refresh_stats() {
  for (std::size_t i = 0; i < channels_.size() && i < meters_.size(); ++i) {
    channels_[i].stats.fill_rate_bps = meters_[i].fill_rate_bps;
    channels_[i].stats.latency_us = meters_[i].latency_us;
  }
}

void Router::meter_submit(std::size_t index, const ClientOrderId& order_id, Quantity quantity) {
  if (clock_ == nullptr || index >= meters_.size() || order_id.empty() || quantity <= 0) return;
  ChannelMeter::Flight flight{};
  flight.ordered = quantity;
  flight.submitted = clock_->now();
  meters_[index].flights.insert_or_assign(order_id, flight);
}

void Router::meter_report(const ExecutionReport& report) {
  if (clock_ == nullptr || report.client_order_id.empty()) return;
  for (auto& meter : meters_) {
    const auto found = meter.flights.find(report.client_order_id);
    if (found == meter.flights.end()) continue;
    ChannelMeter::Flight& flight = found->second;
    const Timestamp now = clock_->now();
    if (!flight.acked) {
      flight.acked = true;
      std::int64_t sample = std::chrono::duration_cast<std::chrono::microseconds>(now - flight.submitted).count();
      if (sample < 0) sample = 0;
      meter.latency_us += (sample - meter.latency_us) * alpha_bps_ / 10'000;
    }
    if (report.last_quantity > 0) flight.filled += report.last_quantity;
    // Completion is terminal status or cumulative convergence (some
    // channels finish on a partially_filled last slice); either way the
    // flight closes here, so a trailing terminal never samples twice.
    const bool converged = flight.ordered > 0 && flight.filled >= flight.ordered;
    if (!is_terminal_report(report.status) && !converged) return;
    std::int64_t ratio = 0;
    if (flight.ordered > 0) {
      ratio = flight.filled * 10'000 / flight.ordered;
      if (ratio < 0) ratio = 0;
      if (ratio > 10'000) ratio = 10'000;
    }
    meter.fill_rate_bps += (ratio - meter.fill_rate_bps) * alpha_bps_ / 10'000;
    meter.flights.erase(found);
    return;
  }
}

std::string_view Router::channel() const noexcept { return "router"; }

bool Router::connected() const noexcept {
  return std::any_of(channels_.begin(), channels_.end(), [](const RoutingPolicy::Candidate& channel) { return channel.api != nullptr && channel.api->connected(); });
}

void Router::submit(const OrderIntent& intent) {
  // Measured stats drive selection: refresh from the meters first so fresh
  // samples (not yesterday's seeds) rank the channels.
  refresh_stats();
  // M8-1 split: 2+ weighted channels connected. Anything else is M8-0 pick.
  std::vector<std::size_t> participants;
  std::int64_t total_weight = 0;
  for (std::size_t i = 0; i < channels_.size(); ++i) {
    if (channels_[i].api != nullptr && channels_[i].api->connected() && channels_[i].stats.split_weight > 0) {
      participants.push_back(i);
      total_weight += channels_[i].stats.split_weight;
    }
  }
  if (participants.size() >= 2 && total_weight > 0 && intent.quantity > 0) {
    submit_split(intent, participants, total_weight);
    return;
  }
  const auto picked = policy_.pick(intent, channels_);
  if (!picked.has_value()) {
    // Fail loud, never silent: OMS turns Rejected into a terminal state.
    if (execution_handler_) {
      ExecutionReport report{};
      report.client_order_id = intent.client_order_id;
      report.status = OrderStatus::rejected;
      report.reason = "no channel available";
      execution_handler_(report);
    }
    return;
  }
  order_channels_.insert_or_assign(intent.client_order_id, *picked);
  meter_submit(*picked, intent.client_order_id, intent.quantity);
  channels_[*picked].api->submit(intent);
}

void Router::submit_split(const OrderIntent& intent, const std::vector<std::size_t>& participants, std::int64_t total_weight) {
  // Largest remainder keeps the shares exact; zero-share children are skipped
  // but numbering stays deterministic for identical inputs.
  struct Share {
    std::size_t channel_index{0};
    Quantity base{0};
    std::int64_t remainder{0};
  };
  std::vector<Share> shares;
  Quantity assigned = 0;
  for (const auto index : participants) {
    const std::int64_t weight = channels_[index].stats.split_weight;
    const Quantity base = intent.quantity * weight / total_weight;
    shares.push_back(Share{index, base, intent.quantity * weight % total_weight});
    assigned += base;
  }
  std::sort(shares.begin(), shares.end(), [](const Share& left, const Share& right) {
    if (left.remainder != right.remainder) return left.remainder > right.remainder;
    return left.channel_index < right.channel_index;
  });
  Quantity leftover = intent.quantity - assigned;
  for (auto& share : shares) {
    if (leftover <= 0) break;
    ++share.base;
    --leftover;
  }

  SplitState state{};
  state.parent = intent;
  std::size_t child_no = 0;
  for (const auto& share : shares) {
    if (share.base <= 0) continue;
    OrderIntent child = intent;
    child.quantity = share.base;
    child.client_order_id = intent.client_order_id + "#" + std::to_string(child_no);
    child.parent_client_order_id = intent.client_order_id;
    SplitChild entry{};
    entry.channel_index = share.channel_index;
    entry.child_id = child.client_order_id;
    entry.target = share.base;
    state.children.push_back(entry);
    child_to_parent_.insert_or_assign(entry.child_id, intent.client_order_id);
    meter_submit(share.channel_index, entry.child_id, share.base);
    channels_[share.channel_index].api->submit(child);
    ++child_no;
  }
  splits_.insert_or_assign(intent.client_order_id, std::move(state));
}

void Router::cancel(const ClientOrderId& client_order_id) {
  const auto split_it = splits_.find(client_order_id);
  if (split_it != splits_.end()) {
    SplitState& state = split_it->second;
    if (state.view == ParentView::kDone) return;
    state.cancel_requested = true;
    for (const auto& child : state.children) {
      if (child.terminal || child.channel_index >= channels_.size()) continue;
      TradeApi* api = channels_[child.channel_index].api;
      if (api != nullptr && api->connected()) api->cancel(child.child_id);
    }
    return;
  }
  // Cancelling a bare child id degrades to cancelling its parent.
  const auto child_it = child_to_parent_.find(client_order_id);
  if (child_it != child_to_parent_.end()) {
    cancel(child_it->second);
    return;
  }
  const auto routed = order_channels_.find(client_order_id);
  if (routed == order_channels_.end()) return;  // unknown id: nothing to withdraw
  const std::size_t index = routed->second;
  if (index < channels_.size() && channels_[index].api != nullptr && channels_[index].api->connected()) {
    channels_[index].api->cancel(client_order_id);
  }
}

void Router::set_execution_handler(ExecutionHandler handler) {
  execution_handler_ = std::move(handler);
  for (const auto& channel : channels_) {
    if (channel.api == nullptr) continue;
    const std::string name = channel.channel;
    channel.api->set_execution_handler([this, name](const ExecutionReport& report) { on_channel_report(name, report); });
  }
}

void Router::request_account_snapshot(const AccountId& account_id) {
  for (const auto& channel : channels_) {
    if (channel.api != nullptr && channel.api->connected()) channel.api->request_account_snapshot(account_id);
  }
}

void Router::request_open_orders(const AccountId& account_id) {
  std::size_t fanned = 0;
  for (const auto& channel : channels_) {
    if (channel.api != nullptr && channel.api->connected()) {
      channel.api->request_open_orders(account_id);
      ++fanned;
    }
  }
  pending_recovery_ = fanned;
  recovery_remaining_ = fanned;
  recovery_ok_ = true;
  recovery_account_ = account_id;
  if (fanned == 0 && recovery_complete_handler_) recovery_complete_handler_(account_id, false);
}

void Router::set_account_snapshot_handler(AccountSnapshotHandler handler) {
  for (const auto& channel : channels_) {
    if (channel.api != nullptr) channel.api->set_account_snapshot_handler(handler);
  }
}

void Router::request_security_info(const AccountId& account_id, const InstrumentId& instrument) {
  for (const auto& channel : channels_) {
    if (channel.api != nullptr && channel.api->connected()) channel.api->request_security_info(account_id, instrument);
  }
}

void Router::set_security_info_handler(SecurityInfoHandler handler) {
  for (const auto& channel : channels_) {
    if (channel.api != nullptr) channel.api->set_security_info_handler(handler);
  }
}

void Router::request_asset_snapshot(const AccountId& account_id) {
  for (const auto& channel : channels_) {
    if (channel.api != nullptr && channel.api->connected()) channel.api->request_asset_snapshot(account_id);
  }
}

void Router::set_asset_snapshot_handler(AssetSnapshotHandler handler) {
  for (const auto& channel : channels_) {
    if (channel.api != nullptr) channel.api->set_asset_snapshot_handler(handler);
  }
}

void Router::set_recovery_complete_handler(RecoveryCompleteHandler handler) {
  recovery_complete_handler_ = std::move(handler);
  for (const auto& channel : channels_) {
    if (channel.api == nullptr) continue;
    channel.api->set_recovery_complete_handler([this](const AccountId& account_id, bool success) { on_channel_recovery(account_id, success); });
  }
}

bool Router::split_child_parent(const ClientOrderId& child_id, ClientOrderId& parent_out) {
  const std::size_t hash = child_id.rfind('#');
  if (hash == std::string::npos || hash + 1 >= child_id.size()) return false;
  for (std::size_t offset = hash + 1; offset < child_id.size(); ++offset) {
    if (child_id[offset] < '0' || child_id[offset] > '9') return false;
  }
  parent_out = child_id.substr(0, hash);
  return true;
}

void Router::on_channel_report(const std::string& channel, const ExecutionReport& report) {
  meter_report(report);
  const auto child_it = child_to_parent_.find(report.client_order_id);
  if (child_it != child_to_parent_.end()) {
    const auto split_it = splits_.find(child_it->second);
    if (split_it != splits_.end() && split_it->second.view != ParentView::kDone) {
      on_split_report(channel, report.client_order_id, report);
      return;
    }
    // Completed parent: post-terminal noise is dropped.
    if (split_it != splits_.end()) return;
  }
  // Best-effort fallback: an unknown "<parent>#<n>" for a live split parent
  // (e.g. router restart) is forwarded rewritten; OMS reconciles the rest.
  ClientOrderId parent_out;
  if (split_child_parent(report.client_order_id, parent_out)) {
    const auto split_it = splits_.find(parent_out);
    if (split_it != splits_.end() && split_it->second.view != ParentView::kDone && execution_handler_) {
      ExecutionReport rewritten = report;
      rewritten.client_order_id = parent_out;
      rewritten.order_id.clear();
      if (!rewritten.execution_id.empty()) rewritten.execution_id = channel + ":" + rewritten.execution_id;
      execution_handler_(rewritten);
      return;
    }
    if (split_it != splits_.end()) return;
  }
  if (!execution_handler_) return;
  ExecutionReport namespaced = report;
  if (!namespaced.execution_id.empty()) namespaced.execution_id = channel + ":" + namespaced.execution_id;
  execution_handler_(namespaced);
}

void Router::on_split_report(const std::string& channel, const ClientOrderId& child_id, const ExecutionReport& report) {
  if (!execution_handler_) return;
  SplitState& state = splits_[child_to_parent_[child_id]];
  SplitChild* child = find_split_child(state, child_id);
  if (child == nullptr || child->terminal) return;  // unknown or post-terminal duplicate: drop

  const bool attempted_fill = report.last_quantity > 0 || report.cumulative_quantity > 0;
  if (!attempted_fill && (report.status == OrderStatus::live || report.status == OrderStatus::pending_new)) {
    on_split_ack(state, channel, child_id, report);
    return;
  }
  if (attempted_fill) {
    on_split_fill(state, *child, channel, child_id, report);
    return;
  }
  on_split_terminal(state, *child, report);
}

Router::SplitChild* Router::find_split_child(SplitState& state, const ClientOrderId& child_id) {
  for (auto& entry : state.children) {
    if (entry.child_id == child_id) return &entry;
  }
  return nullptr;
}

void Router::on_split_ack(SplitState& state, const std::string& channel, const ClientOrderId& child_id, const ExecutionReport& report) {
  // Channel ack: only the first one moves the parent view.
  if (state.view != ParentView::kNew) return;
  state.view = ParentView::kLive;
  ExecutionReport parent{};
  parent.client_order_id = state.parent.client_order_id;
  parent.status = OrderStatus::live;
  parent.execution_id = report.execution_id.empty() ? state.parent.client_order_id + ":live#" + child_id : channel + ":" + report.execution_id;
  execution_handler_(parent);
}

void Router::on_split_fill(SplitState& state, SplitChild& child, const std::string& channel, const ClientOrderId& child_id, const ExecutionReport& report) {
  Quantity computed = child.filled;
  if (report.cumulative_quantity > 0) {
    computed = std::max(computed, std::min(report.cumulative_quantity, child.target));
  } else {
    computed = std::min(child.filled + report.last_quantity, child.target);
  }
  const Quantity delta = computed - child.filled;
  child.filled = computed;
  Quantity total = 0;
  for (const auto& entry : state.children) total += entry.filled;
  if (total >= state.parent.quantity) {
    emit_parent_terminal(state, OrderStatus::filled, total);
    return;
  }
  if (state.view != ParentView::kPartial) state.view = ParentView::kPartial;
  // Fee follows the filled portion: whole child fills carry the exact fee,
  // capped partials pro-rate (divide-last is safe: fees are small).
  Price fee = 0;
  if (report.last_quantity > 0) {
    fee = (report.fee * delta) / report.last_quantity;
  }
  emit_parent_fill(state, channel, child_id, delta, report.last_price, report.execution_id, computed, fee);
  if (report.status == OrderStatus::filled || report.status == OrderStatus::cancelled) {
    child.terminal = true;
    check_split_completion(state);
  }
}

void Router::on_split_terminal(SplitState& state, SplitChild& child, const ExecutionReport& report) {
  // Non-fill terminal reports.
  if (report.status == OrderStatus::rejected) {
    child.terminal = true;
    Quantity total = 0;
    bool any_live = false;
    for (const auto& entry : state.children) {
      total += entry.filled;
      if (!entry.terminal) any_live = true;
    }
    if (total == 0 && !any_live && !state.cancel_requested && state.view == ParentView::kNew) {
      emit_parent_terminal(state, OrderStatus::rejected, 0);
      return;
    }
    check_split_completion(state);
    return;
  }
  if (report.status == OrderStatus::cancelled || report.status == OrderStatus::filled) {
    if (report.status == OrderStatus::filled) child.filled = std::max(child.filled, child.target);
    child.terminal = true;
    check_split_completion(state);
  }
}

void Router::emit_parent_fill(
    SplitState& state, const std::string& channel, const ClientOrderId& child_id, Quantity delta, Price price, const std::string& raw_execution_id, Quantity child_total, Price fee) {
  ExecutionReport parent{};
  parent.client_order_id = state.parent.client_order_id;
  parent.status = OrderStatus::partially_filled;
  parent.last_quantity = delta;
  parent.last_price = price;
  parent.fee = fee;
  parent.execution_id = raw_execution_id.empty() ? channel + ":" + child_id + "@" + std::to_string(child_total) : channel + ":" + raw_execution_id;
  execution_handler_(parent);
}

void Router::emit_parent_terminal(SplitState& state, OrderStatus status, Quantity total) {
  state.view = ParentView::kDone;
  ExecutionReport parent{};
  parent.client_order_id = state.parent.client_order_id;
  parent.status = status;
  if (status == OrderStatus::filled) {
    parent.execution_id = state.parent.client_order_id + ":filled@" + std::to_string(total);
    // Leftover live children can no longer add quantity: withdraw them.
    for (const auto& child : state.children) {
      if (child.terminal || child.channel_index >= channels_.size()) continue;
      TradeApi* api = channels_[child.channel_index].api;
      if (api != nullptr && api->connected()) api->cancel(child.child_id);
    }
  } else if (status == OrderStatus::rejected) {
    parent.execution_id = state.parent.client_order_id + ":rejected";
  } else {
    parent.execution_id = state.parent.client_order_id + ":cancelled";
  }
  execution_handler_(parent);
}

void Router::check_split_completion(SplitState& state) {
  if (state.view == ParentView::kDone) return;
  Quantity total = 0;
  for (const auto& entry : state.children) {
    total += entry.filled;
    if (!entry.terminal) return;  // still working somewhere
  }
  if (total >= state.parent.quantity) {
    emit_parent_terminal(state, OrderStatus::filled, total);
  } else {
    emit_parent_terminal(state, OrderStatus::cancelled, total);
  }
}

void Router::on_channel_recovery(const AccountId& account_id, bool success) {
  (void)account_id;
  if (pending_recovery_ == 0 || recovery_remaining_ == 0) return;  // spurious: ignore
  recovery_ok_ = recovery_ok_ && success;
  --recovery_remaining_;
  if (recovery_remaining_ == 0) {
    pending_recovery_ = 0;
    if (recovery_complete_handler_) recovery_complete_handler_(recovery_account_, recovery_ok_);
  }
}

}  // namespace sts
