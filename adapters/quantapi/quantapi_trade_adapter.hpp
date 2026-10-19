// clang-format off
/**
  ******************************************************************************
  * @file           : quantapi_trade_adapter.hpp
  * @author         : vivi wu
  * @brief          : QuantAPI TradeApi adapter interface (pimpl isolates vendor headers).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
// SPDX-License-Identifier: MIT
// QuantAPI TradeApi adapter (M7-2c, ADR-029/ADR-032/ADR-033).
//
// pimpl: this header stays vendor-free (core/port rule); atp_quant_api.h is
// included only in quantapi_trade_adapter.cpp, which CI never builds (the
// vendor gate defaults OFF). Tests and tools parse this header without the
// SDK.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "qts/broker/quantapi_fixtures.hpp"
#include "qts/broker/trade_api.hpp"

namespace qts {

// Session configuration. ADR-053: customer identity/password are NEVER
// configured — the trader passes them at gateway login and the counter
// session is built lazily (v1: one counter session per process, surviving
// gateway logout). locations/agw stay yaml; feature code is generated per
// the mandated format unless pinned here (automation/tests).
// The gateway is TCP-only (M7-2c scope decision): "ip:port;ip:port".
struct QuantapiSessionConfig {
  std::string locations;            // gateway address, prop::kLocations, required
  std::string client_feature_code;  // terminal fingerprint override; empty = generate
  std::string order_way{"4"};       // prop::kOrderWay, required by vendor, <=4 chars
  std::string sidecar_path;         // append-only seq/map persistence (archforce §3.2)
  std::string cust_id;              // retained for compat; login identity wins
  std::string fund_account_id;      // retained for compat; login identity wins
  std::string password;             // retained for compat; session password wins
  std::string agw_user;             // optional gateway user; pair with agw_password
  std::string agw_password;
};

class QuantapiTradeAdapter;

namespace quantapi_detail {
// Vendor-gated test seams (adapters/tests, see quantapi_test_seam.hpp). They
// feed the adapter the value fixtures its vendor callbacks would produce,
// without a live session. Production code never calls them.
void inject_login(QuantapiTradeAdapter& adapter, const QuantapiLoginFixture& fixture);
void inject_ack(QuantapiTradeAdapter& adapter, const QuantapiAckFixture& fixture);
void inject_rtn(QuantapiTradeAdapter& adapter, const QuantapiRtnFixture& fixture);
void inject_cancel_rsp(QuantapiTradeAdapter& adapter, const QuantapiCancelRspFixture& fixture);
void inject_order_query(QuantapiTradeAdapter& adapter, const QuantapiOrderQueryFixture& fixture);
void inject_fund(QuantapiTradeAdapter& adapter, const QuantapiFundFixture& fixture);
void inject_share(QuantapiTradeAdapter& adapter, const QuantapiShareFixture& fixture);
void inject_asset(QuantapiTradeAdapter& adapter, const QuantapiAssetFixture& fixture);
void inject_secinfo(QuantapiTradeAdapter& adapter, const QuantapiSecInfoFixture& fixture);
// Seeds the submit bookkeeping a successful channel send creates
// (request_id -> client_order_id), so the ack -> attribution -> deferred
// cancel chain is testable without a live session.
void inject_inflight(QuantapiTradeAdapter& adapter, std::int64_t request_id, const ClientOrderId& client_order_id);

// Pure mappings (archforce.md §2), implemented in the adapter .cpp and
// asserted directly by unit tests.
bool parse_instrument(const InstrumentId& id, std::int32_t& market, std::string& code);
// Terminal fingerprint (#<APP>IIP:<ip>;MAC:<mac>;HD:<hd>, ADR-053):
// missing parts stay empty but the shape is kept; stdlib/POSIX only.
std::string generate_feature_code(const std::string& app);
// Returns false for ord_status values outside ATPOrdStatusConst (fail closed:
// unknown statuses never reach OMS).
bool map_ord_status(std::int32_t ord_status, OrderStatus& out);
// 'a' limit; market orders map via MarketOrderStyle ('b'/'c'/'f'/'g',
// tentative per archforce.md — verify in 联调); returns 0 when unsupported.
char map_order_type(const OrderIntent& intent);
}  // namespace quantapi_detail

class QuantapiTradeAdapter final : public TradeApi {
 public:
  explicit QuantapiTradeAdapter(QuantapiSessionConfig config);
  ~QuantapiTradeAdapter() override;
  QuantapiTradeAdapter(const QuantapiTradeAdapter&) = delete;
  QuantapiTradeAdapter& operator=(const QuantapiTradeAdapter&) = delete;

  // ---- TradeApi ------------------------------------------------------------
  [[nodiscard]] std::string_view channel() const noexcept override { return "quantapi"; }
  [[nodiscard]] bool connected() const noexcept override;
  // Async by contract: local failures are enqueued as rejected reports,
  // never thrown (OrderManager::submit keeps its pending_new record).
  void submit(const OrderIntent& intent) override;
  void cancel(const ClientOrderId& client_order_id) override;
  void set_execution_handler(ExecutionHandler handler) override;
  // Control path (archforce §4): issues ReqCashOrderQuery per known market;
  // each open row is enqueued as a report, then RecoveryCompleteHandler.
  void request_open_orders(const AccountId& account_id) override;
  // Issues ReqCashFundQuery + ReqCashShareQuery per known market; assembles
  // TradeAccountSnapshot (fund + position rows) and delivers it in drain().
  void request_account_snapshot(const AccountId& account_id) override;
  void set_account_snapshot_handler(AccountSnapshotHandler handler) override;
  void set_recovery_complete_handler(RecoveryCompleteHandler handler) override;
  // Reference-data path (ADR-054): ReqCashExtQuerySecurityInfo per known
  // market (empty instrument = full-market sweep); rows accumulate until
  // every is_last lands, then one batch per drain chunk.
  void request_security_info(const AccountId& account_id, const InstrumentId& instrument) override;
  void set_security_info_handler(SecurityInfoHandler handler) override;
  // Total-asset view (ReqCashAssetQuery, SZSE leg like the fund query).
  void request_asset_snapshot(const AccountId& account_id) override;
  void set_asset_snapshot_handler(AssetSnapshotHandler handler) override;

  // ---- M7-2c lifecycle (ADR-053: standby-first) ---------------------------
  // start() only validates non-secret config and opens the sidecar: no
  // vendor interaction (server standby). connect() brings the vendor
  // session to connected-but-not-logged-in (TCP ready, no counter Login).
  // login() logs into the counter with the trader-supplied identity (cust
  // or fund id) and password; later calls verify against the live session
  // without counter traffic. The counter session survives gateway logout
  // (v1: strategies outlive the client); stop() tears everything down.
  bool start();
  bool connect();
  // Counter login (cust mode, then fund mode) or session verify when up.
  // Empty identity/password refuse. Failed counter attempts count at the
  // counter: callers pass through what the trader typed, verbatim.
  bool login(const std::string& identity, const std::string& password);
  // Canonical fund account of the live session (empty when logged out).
  [[nodiscard]] std::string session_fund() const;
  // Logout + Release + Stop + session-password zeroing; idempotent.
  void stop();
  [[nodiscard]] bool started() const noexcept;

  // Single-thread pump (cf. AlternatePaperBroker::drain): delivers every
  // queued report/snapshot/recovery callback FIFO, handlers invoked outside
  // any lock so vendor callbacks can never re-enter OMS. Returns reports
  // delivered (snapshot/recovery deliveries included in stats).
  std::size_t drain();

  [[nodiscard]] QuantapiAdapterStats stats() const;
  [[nodiscard]] const QuantapiSessionConfig& config() const;

 private:
  friend void quantapi_detail::inject_login(QuantapiTradeAdapter&, const QuantapiLoginFixture&);
  friend void quantapi_detail::inject_ack(QuantapiTradeAdapter&, const QuantapiAckFixture&);
  friend void quantapi_detail::inject_rtn(QuantapiTradeAdapter&, const QuantapiRtnFixture&);
  friend void quantapi_detail::inject_cancel_rsp(QuantapiTradeAdapter&, const QuantapiCancelRspFixture&);
  friend void quantapi_detail::inject_order_query(QuantapiTradeAdapter&, const QuantapiOrderQueryFixture&);
  friend void quantapi_detail::inject_fund(QuantapiTradeAdapter&, const QuantapiFundFixture&);
  friend void quantapi_detail::inject_share(QuantapiTradeAdapter&, const QuantapiShareFixture&);
  friend void quantapi_detail::inject_asset(QuantapiTradeAdapter&, const QuantapiAssetFixture&);
  friend void quantapi_detail::inject_secinfo(QuantapiTradeAdapter&, const QuantapiSecInfoFixture&);
  friend void quantapi_detail::inject_inflight(QuantapiTradeAdapter&, std::int64_t, const ClientOrderId&);

  // Value-fixture entry points: the vendor callbacks below copy into these
  // and never pass vendor pointers across threads (archforce §3.4).
  void on_login(const QuantapiLoginFixture& fixture);
  void on_ack(const QuantapiAckFixture& fixture);
  void on_rtn(const QuantapiRtnFixture& fixture);
  void on_cancel_rsp(const QuantapiCancelRspFixture& fixture);
  void on_order_query(const QuantapiOrderQueryFixture& fixture);
  void on_fund(const QuantapiFundFixture& fixture);
  void on_share(const QuantapiShareFixture& fixture);
  void on_asset(const QuantapiAssetFixture& fixture);
  void on_secinfo(const QuantapiSecInfoFixture& fixture);
  void on_logout(const std::string& desc);
  void on_recovering(const std::string& desc);

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace qts
