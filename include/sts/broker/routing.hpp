// clang-format off
/**
  ******************************************************************************
  * @file           : routing.hpp
  * @author         : vivi wu
  * @brief          : Smart router: weighted selection, failover and pro-rata splits over channels.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "sts/broker/trade_api.hpp"
#include "sts/core/clock.hpp"

namespace sts {

// Smart routing, selection stage (M8-0, ADR-030).
//
// The Router implements TradeApi and fans out to N channel adapters, so OMS
// keeps holding a single TradeApi and never changes ("multi-adapter switch
// changes no strategy semantics", M3 acceptance). This stage does selection
// (1 intent -> 1 channel) plus failover for new orders; splitting lives in
// the execution layer (M8-1), measurement in M8-2.

// Per-channel statistics. Fee is contractual (config file); fill rate and
// latency start as yesterday's aggregates (config file) and move to online
// measurement in M8-2. Integer only (ADR-003): fee in basis points,
// fill rate in 0..10000 (100.00%), latency in microseconds. A positive
// split_weight opts the channel into M8-1 pro-rata splits.
struct ChannelStats {
  std::string channel;
  std::int64_t fee_bps{0};
  std::int64_t fill_rate_bps{0};
  std::int64_t latency_us{0};
  std::int64_t split_weight{0};
};

// Online per-channel measurement (M8-2, ADR-030): EWMA fill-rate and
// ack-latency over real order flow, seeded by the configured
// yesterday-aggregates. Fill ratio samples per terminal order
// (filled/ordered in bps, clamped); latency samples per first ack in
// microseconds. No clock (unit tests) or no samples yet (fresh start,
// post-restart recovery noise) keeps the seeds — measurement never
// invents data, it only blends observed samples with weight alpha_bps.
struct ChannelMeter {
  std::int64_t fill_rate_bps{0};
  std::int64_t latency_us{0};
  struct Flight {
    Quantity ordered{0};
    Quantity filled{0};
    Timestamp submitted;
    bool acked{false};
  };
  std::unordered_map<ClientOrderId, Flight> flights;
};

struct RoutingWeights {
  std::int64_t weight_fee{1};
  std::int64_t weight_latency{1};
  std::int64_t weight_fill{1};
};

// Weighted least-cost selection. Score (all int64, divide-first, min wins):
//   fee_points = quantity * unit_price / 10'000 * fee_bps / 10'000
//   score = w_fee * fee_points + w_lat * latency_us - w_fill * fill_rate_bps
// unit_price is limit_price, falling back to protection_price for market
// orders (0 prices simply contribute no fee term). Ties keep vector order;
// disconnected channels are skipped; no channel returns nullopt.
class RoutingPolicy {
 public:
  explicit RoutingPolicy(RoutingWeights weights = RoutingWeights{});

  struct Candidate {
    std::string channel;
    TradeApi* api{nullptr};
    ChannelStats stats{};
  };

  [[nodiscard]] std::optional<std::size_t> pick(const OrderIntent& intent, const std::vector<Candidate>& channels) const;

 private:
  RoutingWeights weights_;
};

// Selecting TradeApi over a fixed channel set (non-owning pointers; the
// caller/gateway owns channel lifetimes and must outlive the router).
//
// - submit routes to the picked channel and remembers order -> channel for
//   cancel. With no channel available it synthesizes a Rejected report through
//   the execution handler instead of dropping silently.
// - execution ids are namespaced "<channel>:<id>" on forward, because OMS
//   dedups on the bare execution_id string (order_manager.cpp) and two
//   channels must never collapse each other's ids.
// - account snapshot requests fan out to every connected channel and forward
//   as-is; open-order recovery completes once per request_open_orders call
//   after every fanned-out channel reports (single-flight; success is AND).
// - M8-1 split (ADR-031): when 2+ channels carry split_weight and are
//   connected at submit, the intent is divided pro-rata (largest remainder)
//   into children "<parent>#<i>" sent straight to channels. OMS only ever sees
//   one coherent parent stream synthesized here: fills accumulate into
//   parent partials, completion yields exactly one terminal (Filled once the
//   summed fills reach the parent quantity, else Cancelled, or Rejected when
//   a pre-live child rejects with nothing filled). Synthesized reports carry
//   an empty order_id (never collide with OMS state) and deterministic
//   execution ids ("<channel>:<raw>" or "<child>@<total>") so recovery replays
//   dedup instead of double-counting. Parent cancel fans out to live
//   children; a child stranded on a dead channel never completes optimistically
//   (reconnect replay + reconcile alarms apply). Unknown "<parent>#<n>"
//   reports for a live split parent are forwarded rewritten (best effort,
//   e.g. router restart); reports for completed parents are dropped.
class Router final : public TradeApi {
 public:
  // clock == nullptr disables measurement (stats stay at their seeds);
  // alpha_bps blends each sample (1..10000, clamped; default 2000 = 20%).
  Router(std::vector<RoutingPolicy::Candidate> channels, RoutingPolicy policy, const Clock* clock = nullptr, std::int64_t alpha_bps = 2000);

  std::string_view channel() const noexcept override;
  bool connected() const noexcept override;
  void submit(const OrderIntent& intent) override;
  void cancel(const ClientOrderId& client_order_id) override;
  void set_execution_handler(ExecutionHandler handler) override;
  void request_account_snapshot(const AccountId& account_id) override;
  void request_open_orders(const AccountId& account_id) override;
  void set_account_snapshot_handler(AccountSnapshotHandler handler) override;
  void set_recovery_complete_handler(RecoveryCompleteHandler handler) override;
  void request_security_info(const AccountId& account_id, const InstrumentId& instrument) override;
  void set_security_info_handler(SecurityInfoHandler handler) override;
  void request_asset_snapshot(const AccountId& account_id) override;
  void set_asset_snapshot_handler(AssetSnapshotHandler handler) override;

  // Current per-channel stats: contractual fee plus measured (or seed)
  // fill-rate/latency. Read-only snapshot for tests and ops surfaces.
  [[nodiscard]] std::vector<ChannelStats> channel_stats() const;

 private:
  enum class ParentView : std::uint8_t { kNew, kLive, kPartial, kDone };
  struct SplitChild {
    std::size_t channel_index{0};
    ClientOrderId child_id;
    Quantity target{0};
    Quantity filled{0};
    bool terminal{false};
  };
  struct SplitState {
    OrderIntent parent;
    std::vector<SplitChild> children;
    ParentView view{ParentView::kNew};
    bool cancel_requested{false};
  };

  void on_channel_report(const std::string& channel, const ExecutionReport& report);
  void on_channel_recovery(const AccountId& account_id, bool success);
  void submit_split(const OrderIntent& intent, const std::vector<std::size_t>& participants, std::int64_t total_weight);
  void on_split_report(const std::string& channel, const ClientOrderId& child_id, const ExecutionReport& report);
  static SplitChild* find_split_child(SplitState& state, const ClientOrderId& child_id);
  void on_split_ack(SplitState& state, const std::string& channel, const ClientOrderId& child_id, const ExecutionReport& report);
  void on_split_fill(SplitState& state, SplitChild& child, const std::string& channel, const ClientOrderId& child_id, const ExecutionReport& report);
  void on_split_terminal(SplitState& state, SplitChild& child, const ExecutionReport& report);
  void emit_parent_fill(
      SplitState& state, const std::string& channel, const ClientOrderId& child_id, Quantity delta, Price price, const std::string& raw_execution_id, Quantity child_total, Price fee);
  void emit_parent_terminal(SplitState& state, OrderStatus status, Quantity total);
  void check_split_completion(SplitState& state);
  static bool split_child_parent(const ClientOrderId& child_id, ClientOrderId& parent_out);

  std::vector<RoutingPolicy::Candidate> channels_;
  RoutingPolicy policy_;
  const Clock* clock_{nullptr};
  std::int64_t alpha_bps_{2000};
  std::vector<ChannelMeter> meters_;

  void refresh_stats();
  void meter_submit(std::size_t index, const ClientOrderId& order_id, Quantity quantity);
  void meter_report(const ExecutionReport& report);
  [[nodiscard]] static bool is_terminal_report(OrderStatus status) noexcept;
  ExecutionHandler execution_handler_;
  RecoveryCompleteHandler recovery_complete_handler_;
  std::unordered_map<ClientOrderId, std::size_t> order_channels_;
  std::unordered_map<ClientOrderId, SplitState> splits_;
  std::unordered_map<ClientOrderId, ClientOrderId> child_to_parent_;
  std::size_t pending_recovery_{0};
  std::size_t recovery_remaining_{0};
  bool recovery_ok_{true};
  AccountId recovery_account_;
};

}  // namespace sts
