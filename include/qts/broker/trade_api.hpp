// clang-format off
/**
  ******************************************************************************
  * @file           : trade_api.hpp
  * @author         : vivi wu
  * @brief          : Internal trading port contract (TradeApi): orders, snapshots and recovery.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <functional>
#include <string_view>
#include <vector>

#include "qts/core/types.hpp"

namespace qts {

struct PositionSnapshot {
  InstrumentId instrument_id;
  Quantity total_quantity{0};
  Quantity sellable_quantity{0};  // Product/account rule aware; do not infer from total quantity.
  Quantity frozen_quantity{0};
  Price average_cost{0};
  // ADR-032 (CSP reference): quantity bought today (unsettled under T+1).
  // Zero means the adapter does not report the today/yesterday split.
  Quantity today_quantity{0};
};

struct TradeAccountSnapshot {
  AccountId account_id;
  SecurityAccountId security_account_id;
  Price total_asset{0};
  Price available_cash{0};
  Price withdrawable_cash{0};
  Price frozen_cash{0};
  std::vector<PositionSnapshot> positions;
};

// Spot securities master row (ADR-054): counter-provided tick/lot/band
// reference. instrument_id is normalized "CODE.SUFFIX".
struct SecurityInfoRow {
  InstrumentId instrument_id;
  std::string symbol;
  std::uint16_t security_type{0};  // 1 = stock
  Price price_tick{0};
  Quantity buy_unit{0};
  Quantity sell_unit{0};
  Quantity buy_lower{0};
  Quantity sell_lower{0};
  Price limit_up{0};
  Price limit_down{0};
  Price prev_close{0};
  bool day_trading{false};
  std::uint64_t status{0};
};

// Counter asset summary (total-asset view; fund+share detail stays in the
// account snapshot above).
struct TradeAssetSnapshot {
  AccountId account_id;
  Price total_asset{0};
  Price market_value{0};
  Price leaves_value{0};
  Price frozen{0};
  Price available_t0{0};
  Price available_t1{0};
};

// Internal trading port.  OMS depends on this contract, never on a broker SDK.
// Submission is asynchronous: acceptance, rejection, fills and cancels arrive
// exclusively through ExecutionHandler.
class TradeApi {
 public:
  using ExecutionHandler = std::function<void(const ExecutionReport&)>;
  using AccountSnapshotHandler = std::function<void(const TradeAccountSnapshot&)>;
  using RecoveryCompleteHandler = std::function<void(const AccountId&, bool success)>;
  // Security-info batches (ADR-054): rows accumulate per request; is_last
  // ends one sweep (empty instrument = full-market sweep).
  using SecurityInfoHandler = std::function<void(const AccountId&, const std::vector<SecurityInfoRow>&, bool is_last)>;
  using AssetSnapshotHandler = std::function<void(const TradeAssetSnapshot&)>;

  TradeApi() = default;
  virtual ~TradeApi() = default;

  TradeApi(const TradeApi&) = delete;
  TradeApi& operator=(const TradeApi&) = delete;
  TradeApi(TradeApi&&) = delete;
  TradeApi& operator=(TradeApi&&) = delete;

  // Channel identity (e.g. "simulated", "router", "quantapi"): which TradeAPI
  // endpoint this instance is — not an exchange venue (dev_scheme.md §1 terms).
  [[nodiscard]] virtual std::string_view channel() const noexcept = 0;
  [[nodiscard]] virtual bool connected() const noexcept = 0;
  virtual void submit(const OrderIntent& intent) = 0;
  virtual void cancel(const ClientOrderId& client_order_id) = 0;
  virtual void set_execution_handler(ExecutionHandler handler) = 0;

  // Control/recovery path only. Implementations may fetch asynchronously;
  // these calls must never be made from the order hot path.
  virtual void request_account_snapshot(const AccountId& account_id) = 0;
  virtual void request_open_orders(const AccountId& account_id) = 0;
  virtual void set_account_snapshot_handler(AccountSnapshotHandler handler) = 0;
  virtual void set_recovery_complete_handler(RecoveryCompleteHandler handler) = 0;
  // Reference-data path (ADR-054): single-instrument rows or a full-market
  // sweep when instrument is empty; batches arrive via the drain pump.
  virtual void request_security_info(const AccountId& account_id, const InstrumentId& instrument) = 0;
  virtual void set_security_info_handler(SecurityInfoHandler handler) = 0;
  // Total-asset view for reconciliation (same pump, same fail-closed rules).
  virtual void request_asset_snapshot(const AccountId& account_id) = 0;
  virtual void set_asset_snapshot_handler(AssetSnapshotHandler handler) = 0;
};

}  // namespace qts
