// clang-format off
/**
  ******************************************************************************
  * @file           : types.hpp
  * @author         : vivi wu
  * @brief          : Core domain types: fixed-point price/quantity, intents, reports, enums.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace qts {

using Price = std::int64_t;     // 最小价格单位，例如 1/10,000 元
using Quantity = std::int64_t;  // 最小可交易数量单位
using OrderId = std::string;
using ClientOrderId = std::string;
using AccountId = std::string;
using SecurityAccountId = std::string;  // A 股股东代码；不等同于资金账户
using StrategyId = std::string;
using InstrumentId = std::string;
using ExecutionId = std::string;
using TraceId = std::string;
using Timestamp = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;

// Every order belongs to exactly one strategy. Orders submitted without an
// explicit strategy (e.g. manual pass-through / direct-execution orders)
// are normalized to this basket at the OMS entry point; the projection
// layers apply the same default defensively.
inline constexpr char kPassthroughStrategy[] = "passthrough";

enum class Side : std::uint8_t { buy, sell };
enum class OrderType : std::uint8_t { market, limit };
enum class TimeInForce : std::uint8_t { day, immediate_or_cancel, fill_or_kill };
enum class OrderStatus : std::uint8_t { pending_new, live, partially_filled, filled, pending_cancel, cancelled, rejected };
enum class Exchange : std::uint8_t { unknown, sse, szse, bse };
enum class AshareBoard : std::uint8_t { unknown, main, star, chinext, beijing_select };
enum class TradingBusiness : std::uint8_t { cash, margin_financing, margin_short_sale };
enum class MarketOrderStyle : std::uint8_t { none, best_five_ioc, best_five_to_limit, own_best, counterparty_best };

struct OrderIntent {
  ClientOrderId client_order_id;
  AccountId account_id;
  StrategyId strategy_id;
  InstrumentId instrument_id;
  Side side{Side::buy};
  OrderType type{OrderType::limit};
  TimeInForce time_in_force{TimeInForce::day};
  Quantity quantity{0};
  Price limit_price{0};
  TraceId trace_id;
  // Domestic equity fields. They are optional at the cross-asset core level,
  // and mandatory when an A-share ruleset is selected.
  Exchange exchange{Exchange::unknown};
  AshareBoard board{AshareBoard::unknown};
  SecurityAccountId security_account_id;
  TradingBusiness trading_business{TradingBusiness::cash};
  MarketOrderStyle market_order_style{MarketOrderStyle::none};
  Price protection_price{0};
  // M4 execution linkage: child orders carry the parent's client order id.
  // Empty means a parent (strategy-level) order. Appended last so existing
  // positional initializers keep working.
  ClientOrderId parent_client_order_id;
};

struct ExecutionReport {
  OrderId order_id;
  ClientOrderId client_order_id;
  OrderStatus status{OrderStatus::pending_new};
  Quantity last_quantity{0};
  Price last_price{0};
  std::string reason;
  // Broker execution identifier.  A non-empty value is used by OMS to make
  // execution reports idempotent across reconnects and replays.
  ExecutionId execution_id;
  // Cumulative filled quantity as reported by the broker.  Zero means that
  // this adapter only supplied last_quantity for the report.
  Quantity cumulative_quantity{0};
  std::uint64_t version{0};
  // ADR-032 (CSP reference): broker-side business order id (柜台委托号,
  // e.g. QuantAPI ClOrdNo).  Opaque to OMS; adapters use it for cancel
  // (OrigClOrdNo) and post-restart reconciliation.  Empty when unavailable.
  std::string broker_order_id;
  // ADR-032: structured broker reject code (0 = none).  `reason` keeps the
  // human-readable text; OMS and alerts classify on the code.
  std::int32_t reject_code{0};
  // Fill fee in Price units (M11-6): set by the broker on each fill report,
  // recorded in the ledger like any other fill field (old ledgers replay as
  // zero). Enables net-PnL and transaction-cost analysis off the ledger.
  Price fee{0};
};

}  // namespace qts
