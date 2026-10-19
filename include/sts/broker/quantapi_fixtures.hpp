// clang-format off
/**
  ******************************************************************************
  * @file           : quantapi_fixtures.hpp
  * @author         : vivi wu
  * @brief          : QuantAPI field-mapping fixtures and conversion tables (vendor-free).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
// SPDX-License-Identifier: MIT
// QuantAPI value fixtures (M7-2c, ADR-032/ADR-033).
//
// This header is vendor-free by contract: it is the exact field-for-field
// value copy the ATPQuantHandler callbacks in quantapi_trade_adapter.cpp
// perform before enqueueing, so unit tests exercise attribution, status
// mapping, conversion and drain without the SDK and without a live session.
// Field semantics trace to atp_quant_msg.h / atp_quant_constant.h
// (QuantAPI V2.6.3.5) and resources/broker-mappings/archforce.md §2/§3.
#pragma once

#include <cmath>
#include <cstdint>
#include <string>

#include "sts/core/types.hpp"

namespace sts {

// Mirror of atp::quant_api::ATPOrdStatusConst (numeric so tests and the
// adapter agree without including the vendor header here).
namespace quantapi_fixtures {
inline constexpr std::int32_t kOrdNew = 0;                            // 已申报
inline constexpr std::int32_t kOrdPartiallyFilled = 1;                // 部分成交
inline constexpr std::int32_t kOrdFilled = 2;                         // 全部成交
inline constexpr std::int32_t kOrdPartiallyFilledCancelled = 3;       // 部成部分撤
inline constexpr std::int32_t kOrdCancelled = 4;                      // 已撤销
inline constexpr std::int32_t kOrdPartiallyCancelled = 5;             // 部分撤销
inline constexpr std::int32_t kOrdReject = 8;                         // 已拒绝
inline constexpr std::int32_t kOrdUnSend = 9;                         // 未申报
inline constexpr std::int32_t kOrdSended = 10;                        // 正报
inline constexpr std::int32_t kOrdWaitCancelled = 11;                 // 待撤销
inline constexpr std::int32_t kOrdPartiallyFilledWaitCancelled = 12;  // 部成待撤
inline constexpr std::int32_t kOrdInternalReject = 90;                // 内部拒单
inline constexpr std::int32_t kOrdInternalCancelled = 91;             // 内部撤单
inline constexpr std::int32_t kOrdCancelSuccess = 92;                 // 撤单成功

// 交易所市场代码 (ATPMarketIDConst).
inline constexpr std::int32_t kMarketSse = 101;
inline constexpr std::int32_t kMarketSzse = 102;
inline constexpr std::int32_t kMarketBse = 104;

// 委托拒绝原因取值边界：vendor 错误码/拒绝码占用整数空间，
// 适配器本地拒单固定 reject_code=0 且 reason 以 "adapter: " 前缀标识。
inline constexpr std::int32_t kNoRejectCode = 0;
}  // namespace quantapi_fixtures

// QuantAPI price is a double in yuan (N13(4)); STS Price is an integer in
// 1/10,000 yuan. Quantity is a double in shares for stocks (N15(2)); STS
// Quantity is an integer share count.
inline double quantapi_price_to_vendor(Price price) { return static_cast<double>(price) / 10000.0; }

inline Price quantapi_price_from_vendor(double price) { return static_cast<Price>(std::llround(price * 10000.0)); }

inline double quantapi_quantity_to_vendor(Quantity quantity) { return static_cast<double>(quantity); }

inline Quantity quantapi_quantity_from_vendor(double quantity) { return static_cast<Quantity>(std::llround(quantity)); }

// ---- Value events (copied inside vendor callbacks, delivered via drain) ----

// OnLogin: account triple discovered from the customer info repeat group,
// plus the customer id itself (present in both cust-mode and fund-mode
// logins, so either identity can be mapped to the other, ADR-053).
// session_password is seam-only: production vendor callbacks never carry a
// password (it arrives via the login() caller); tests use it to exercise
// the session-verify path without a counter.
struct QuantapiLoginFixture {
  std::string cust_id;
  std::string session_password;
  std::string fund_account_id;
  std::string branch_id;
  std::string account_id_sse;
  std::string account_id_szse;
  std::string account_id_bse;
};

// OnRspCashAuctionOrder: submission acknowledgement or gateway rejection.
// request_id is the client sequence allocated by submit() (archforce §3.2);
// cl_ord_no is broker-generated and 0 when the gateway rejected before
// assigning one.
struct QuantapiAckFixture {
  std::int64_t request_id{0};
  std::int64_t cl_ord_no{0};
  std::int32_t error_id{0};
  std::string error_msg;
  std::string security_id;
  std::int32_t market_id{0};
  char side{0};
  char order_type{0};
  double price{0.0};
  double order_qty{0.0};
};

// OnRtnCashAuctionOrder: the single order/fill push channel (QuantAPI has no
// separate trade push, archforce §4).
struct QuantapiRtnFixture {
  std::int64_t cl_ord_no{0};
  std::int64_t orig_cl_ord_no{0};
  std::int32_t ord_status{0};
  std::string security_id;
  std::int32_t market_id{0};
  char side{0};
  double price{0.0};
  double order_qty{0.0};
  double leaves_qty{0.0};
  double cum_qty{0.0};
  double last_px{0.0};
  double last_qty{0.0};
  std::string exec_id;  // trade reports only; empty elsewhere
  std::int32_t reject_reason_code{0};
  std::string ord_rej_reason;
  std::int64_t transact_time{0};
};

// OnRspCashCancelOrder: cancel acknowledgement/rejection. orig_cl_ord_no is
// the order being cancelled.
struct QuantapiCancelRspFixture {
  std::int64_t request_id{0};
  std::int64_t cl_ord_no{0};
  std::int64_t orig_cl_ord_no{0};
  std::int32_t error_id{0};
  std::string error_msg;
};

// OnRspCashOrderQueryResult: one paged row (archforce §4 pagination).
// has_row=false marks an empty/terminal callback that only carries is_last.
struct QuantapiOrderQueryFixture {
  std::int64_t request_id{0};
  bool is_last{false};
  std::int32_t error_id{0};
  std::string error_msg;
  bool has_row{false};
  std::int64_t cl_ord_no{0};
  std::int64_t orig_cl_ord_no{0};
  std::int32_t ord_status{0};
  std::string security_id;
  std::int32_t market_id{0};
  char side{0};
  double order_price{0.0};
  double order_qty{0.0};
  double leaves_qty{0.0};
  double cum_qty{0.0};
  std::string order_status_info;
};

// OnRspCashAssetQueryResult: account asset summary (total-asset view).
// has_row=false marks the empty is_last marker and must never overwrite.
struct QuantapiAssetFixture {
  std::int64_t request_id{0};
  bool is_last{false};
  std::int32_t error_id{0};
  bool has_row{false};
  double leaves_value{0.0};
  double init_leaves_value{0.0};
  double frozen_all{0.0};
  double available_t0{0.0};
  double available_t1{0.0};
  double total_asset{0.0};
  double market_value{0.0};
  double fund_buy{0.0};
  double fund_sale{0.0};
};

// OnRspCashExtQueryResultSecurityInfo: one master row (ADR-054). has_row
// follows the same empty-marker convention as fund/share rows.
struct QuantapiSecInfoFixture {
  std::int64_t request_id{0};
  bool is_last{false};
  std::int32_t error_id{0};
  bool has_row{false};
  std::string security_id;  // naked 6-digit code; market carries the suffix
  std::int32_t market_id{0};
  std::string symbol;
  std::uint16_t security_type{0};
  double prev_close{0.0};
  double price_upper_limit{0.0};
  double price_lower_limit{0.0};
  double price_tick{0.0};
  double buy_qty_lower{0.0};
  double sell_qty_lower{0.0};
  bool day_trading{false};
  std::uint64_t security_status{0};
  double buy_qty_unit{0.0};
  double sell_qty_unit{0.0};
};

// OnRspCashFundQueryResult: fund balances (single logical result).
// has_row=false marks the empty is_last marker (identity field empty in the
// vendor message) — it must never overwrite already-cached balances.
struct QuantapiFundFixture {
  std::int64_t request_id{0};
  bool is_last{false};
  std::int32_t error_id{0};
  bool has_row{false};
  double leaves_value{0.0};
  double init_leaves_value{0.0};
  double frozen_all{0.0};
  double available_t0{0.0};  // 当日可取
  double available_t1{0.0};  // 当日可用
};

// OnRspCashShareQueryResult: one position row.
struct QuantapiShareFixture {
  std::int64_t request_id{0};
  bool is_last{false};
  std::int32_t error_id{0};
  bool has_row{false};
  std::string security_id;
  std::int32_t market_id{0};
  double init_qty{0.0};
  double leaves_qty{0.0};
  double available_qty{0.0};
  double cost_price{0.0};
  double market_value{0.0};
  double stock_buy{0.0};
  double stock_sale{0.0};
};

// Observable adapter behaviour (seam-visible; tests assert on these).
struct QuantapiAdapterStats {
  std::uint64_t init_calls{0};
  std::uint64_t login_calls{0};
  std::uint64_t logout_events{0};
  std::uint64_t recovering_events{0};
  std::uint64_t submit_calls{0};
  std::uint64_t cancel_calls{0};
  std::uint64_t acks_received{0};
  std::uint64_t rtns_received{0};
  std::uint64_t cancel_resps_received{0};
  std::uint64_t query_rows{0};
  std::uint64_t reports_enqueued{0};
  std::uint64_t reports_delivered{0};
  std::uint64_t local_rejects{0};
  std::uint64_t gateway_rejects{0};
  std::uint64_t status_unknown_drops{0};
  std::uint64_t unattributed_rtns{0};
  std::uint64_t unmatched_acks{0};
  std::uint64_t snapshots_delivered{0};
  std::uint64_t recovery_completes{0};
  std::uint64_t secinfo_rows{0};
  std::uint64_t secinfo_batches_delivered{0};
  std::uint64_t assets_delivered{0};
  std::uint64_t drains{0};
  std::int32_t init_rc{0};  // last vendor ATPQuantAPI::Init return code (append-only)
};

}  // namespace sts
