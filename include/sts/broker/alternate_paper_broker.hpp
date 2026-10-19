// clang-format off
/**
  ******************************************************************************
  * @file           : alternate_paper_broker.hpp
  * @author         : vivi wu
  * @brief          : Second paper channel: tick/lot admission, session gate, queued drain, seq sidecar.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>

#include "sts/broker/fee_schedule.hpp"
#include "sts/broker/trade_api.hpp"
#include "sts/core/event_bus.hpp"
#include "sts/portfolio/portfolio.hpp"

namespace sts {

// Second self-developed paper channel (M3-2). Matching semantics are
// identical to SimulatedBroker (fill at reference when the order crosses),
// but admission and delivery differ on purpose:
//
// - tick 50 / lot 200 enforced per order (vs no admission checks);
// - an explicit trading-session gate (vs connected-only);
// - reports are never delivered inline: they queue behind drain(), which
//   models the serial-callback requirement (archforce §3.4) and makes
//   handler reentrancy impossible by construction;
// - each accepted order takes a monotonic client_seq persisted to a
//   sidecar file, so restarted processes never reuse broker order ids
//   (archforce §3.2 pattern); the order id is "ALT-<seq>".
class AlternatePaperBroker final : public TradeApi {
 public:
  static constexpr Price kPriceTick = 50;
  static constexpr Quantity kLotSize = 200;

  // seq_path persists the next client sequence across restarts.
  explicit AlternatePaperBroker(EventBus& events, std::string seq_path);

  void set_starting_cash(Price cash);
  void set_fee_schedule(FeeSchedule schedule);
  void set_fee_override(InstrumentId instrument, FeeSchedule schedule);
  void fund_settled_position(const StrategyId& strategy, InstrumentId instrument, Quantity quantity, Price avg_cost);
  void set_reference_price(InstrumentId instrument, Price price);
  void clear_reference_price(const InstrumentId& instrument);
  void set_session_open(bool open);
  void disconnect();
  void reconnect();
  void settle_day();
  // Delivers all queued reports FIFO via the execution handler. Submit and
  // cancel only enqueue; the handler never runs reentrantly inside them,
  // only here in drain order.
  void drain();
  [[nodiscard]] std::size_t pending_reports() const { return pending_.size(); }
  [[nodiscard]] const Portfolio& book() const { return book_; }

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
  };

  void require_tradable() const;
  void enqueue(const ExecutionReport& report);
  void emit_live(const WorkingOrder& order);
  void emit_fill(WorkingOrder& order, Price reference);
  void emit_cancelled(const WorkingOrder& order);
  void emit_rejected(const OrderIntent& intent, const std::string& reason);
  std::uint64_t next_seq();
  void persist_seq() const;
  void load_seq();

  // Mandatory injected dependency; the class is non-copyable and the
  // reference can never be null, while a pointer would add nullable state.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  EventBus& events_;
  std::string seq_path_;
  ExecutionHandler execution_handler_;
  AccountSnapshotHandler account_snapshot_handler_;
  RecoveryCompleteHandler recovery_complete_handler_;
  SecurityInfoHandler security_info_handler_;
  AssetSnapshotHandler asset_snapshot_handler_;
  bool connected_{true};
  bool session_open_{true};
  FeeTable fee_table_;
  std::unordered_map<ClientOrderId, WorkingOrder> orders_;
  std::unordered_map<InstrumentId, Price> reference_prices_;
  Portfolio book_{10'000'000};
  std::deque<ExecutionReport> pending_;
  std::uint64_t next_seq_{1};
};

}  // namespace sts
