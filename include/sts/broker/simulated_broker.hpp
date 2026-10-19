// clang-format off
/**
  ******************************************************************************
  * @file           : simulated_broker.hpp
  * @author         : vivi wu
  * @brief          : Deterministic paper matching channel (reference-price fills, sync delivery).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <chrono>
#include <deque>
#include <string>
#include <unordered_map>

#include "sts/broker/fee_schedule.hpp"
#include "sts/broker/order_book.hpp"
#include "sts/broker/trade_api.hpp"
#include "sts/core/clock.hpp"
#include "sts/core/event_bus.hpp"
#include "sts/portfolio/portfolio.hpp"

namespace sts {

// Deterministic single-process paper engine. Reports are normally delivered
// inline, preserving fully reproducible tests and replay. An optional
// latency model queues reports behind a Clock: matching, cash and positions
// still move at event time (broker truth), while OMS only learns on
// delivery -- cancels against unacked orders behave like the real world.
// Call deliver_due() after advancing the clock (Replayer does this per
// bar); with zero latency everything delivers inline as before.
// Slippage is a fixed per-unit penalty against the taker; fees are charged
// in basis points per fill and tracked via total_fees() for reporting.
class SimulatedBroker final : public TradeApi {
 public:
  explicit SimulatedBroker(EventBus& events);

  // Paper controls. Call before trading in tests and demos.
  void set_starting_cash(Price cash);
  void fund_settled_position(const StrategyId& strategy, InstrumentId instrument, Quantity quantity, Price avg_cost);
  void set_reference_price(InstrumentId instrument, Price price);
  void clear_reference_price(const InstrumentId& instrument);
  // Standing depth for book-aware matching (M6-0, opt-in per instrument).
  // When a book is set, taker orders match against depth at submit time:
  // full consumption fills, partial consumption leaves the remainder live,
  // zero consumption just acks. Without a book the legacy reference-price
  // path applies, so existing drills are unaffected.
  void set_order_book(InstrumentId instrument, OrderBook book);
  void clear_order_book(const InstrumentId& instrument);
  [[nodiscard]] const OrderBook* order_book(const InstrumentId& instrument) const;
  void set_clock(const Clock* clock);  // must outlive the broker; null disables latency
  void set_latency(std::chrono::nanoseconds latency);
  void set_slippage_per_unit(Price slippage);
  void set_fee_rate_bp(int fee_rate_bp);        // legacy: commission-only, no minimum
  void set_fee_schedule(FeeSchedule schedule);  // default for all instruments
  void set_fee_override(InstrumentId instrument, FeeSchedule schedule);
  void disconnect();
  void reconnect();
  // New trading day: same-day buys become sellable in snapshots.
  void settle_day();
  // Delivers queued reports due at the clock time. No clock: flushes all.
  // While disconnected reports are held for reconnect.
  void deliver_due();
  [[nodiscard]] Price total_fees() const { return book_.total_fees(); }
  [[nodiscard]] const Portfolio& book() const { return book_; }
  [[nodiscard]] std::size_t pending_reports() const { return pending_.size(); }

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

 private:
  struct WorkingOrder {
    OrderIntent intent;
    OrderId order_id;
    OrderStatus state{OrderStatus::live};
    Quantity filled{0};    // book-path partial fills; legacy path fills at once
    int fill_seq{0};       // per-fill suffix so execution ids stay unique
    std::uint64_t seq{0};  // submit order: resting orders re-match FIFO
  };

  struct PendingReport {
    Timestamp deliver_at;
    ExecutionReport report;
  };

  void require_connected() const;
  void deliver(const ExecutionReport& report);
  void emit_now(const ExecutionReport& report);
  void emit_live(const WorkingOrder& order);
  void emit_fill(WorkingOrder& order, Price reference);
  void emit_book_fill(WorkingOrder& order);
  // Re-match every working order against fresh depth in submit order.
  // Called after each book update so resting remainders fill when depth
  // arrives; each new fill carries the next per-order execution-id suffix.
  void match_resting(const InstrumentId& instrument, OrderBook& book);
  void emit_cancelled(const WorkingOrder& order);

  // Mandatory injected dependency; the class is non-copyable and the
  // reference can never be null, while a pointer would add nullable state.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  EventBus& events_;
  ExecutionHandler execution_handler_;
  AccountSnapshotHandler account_snapshot_handler_;
  RecoveryCompleteHandler recovery_complete_handler_;
  SecurityInfoHandler security_info_handler_;
  AssetSnapshotHandler asset_snapshot_handler_;
  bool connected_{true};
  std::unordered_map<ClientOrderId, WorkingOrder> orders_;
  std::uint64_t next_seq_{0};
  std::unordered_map<InstrumentId, Price> reference_prices_;
  std::unordered_map<InstrumentId, OrderBook> books_;
  Portfolio book_{10'000'000};
  const Clock* clock_{nullptr};
  std::chrono::nanoseconds latency_{0};
  Price slippage_per_unit_{0};
  FeeTable fee_table_;
  std::deque<PendingReport> pending_;
};

}  // namespace sts
