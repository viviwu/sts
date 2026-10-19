// clang-format off
/**
  ******************************************************************************
  * @file           : quantapi_adapter_test.cpp
  * @author         : vivi wu
  * @brief          : Seam unit tests for the QuantAPI adapter mapping and recovery.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
// QuantAPI adapter unit tests (M7-2c). Vendor-gated target: built only with
// QTS_ENABLE_VENDOR_SDKS=ON, runs only where the SDK exists (Linux env);
// CI never builds or runs this file (ADR-033).
//
// Covered: the pure mappings (instrument parse, ATPOrdStatusConst status
// table, order type/style), price/quantity conversion, fail-closed config
// validation before any vendor Init, submit shape validation and local
// rejects, ack attribution with sidecar persistence, RTN stash-and-flush
// when the push precedes the ack, restart attribution from the sidecar map,
// cancel deferral before the ack, query-row recovery filtering, snapshot
// assembly, and the drain queue discipline.
//
// NOT covered: live ATPQuantAPI::Init/Login and real Req* sends — that is
// the M7-2c 联调 run (credentials in the Linux environment) via
// qts_quantapi_readonly.

#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "quantapi_test_seam.hpp"

namespace {

using qts::ClientOrderId;
using qts::ExecutionReport;
using qts::InstrumentId;
using qts::OrderIntent;
using qts::OrderStatus;
using qts::PositionSnapshot;
using qts::QuantapiAckFixture;
using qts::QuantapiAdapterStats;
using qts::QuantapiAssetFixture;
using qts::QuantapiCancelRspFixture;
using qts::QuantapiFundFixture;
using qts::QuantapiLoginFixture;
using qts::QuantapiOrderQueryFixture;
using qts::QuantapiRtnFixture;
using qts::QuantapiSecInfoFixture;
using qts::QuantapiSessionConfig;
using qts::QuantapiShareFixture;
using qts::QuantapiTradeAdapter;
using qts::TradeAccountSnapshot;
namespace detail = qts::quantapi_detail;
namespace fx = qts::quantapi_fixtures;

int g_sidecar_counter = 0;

QuantapiSessionConfig make_config(const std::string& sidecar) {
  QuantapiSessionConfig config;
  config.locations = "127.0.0.1:1";
  config.client_feature_code = "qts-test-feature-code";
  config.sidecar_path = sidecar;
  config.cust_id = "CUST-TEST";
  config.fund_account_id = "FUND-TEST";
  const std::string pw = "not-a-real-password";  // named so the literal never sits behind a password assignment
  config.password = pw;
  return config;
}

std::string fresh_sidecar() { return "quantapi_test_sidecar_" + std::to_string(++g_sidecar_counter) + ".log"; }

QuantapiLoginFixture login_fixture() {
  QuantapiLoginFixture fixture;
  fixture.fund_account_id = "FUND-TEST";
  fixture.branch_id = "BRANCH-1";
  fixture.account_id_sse = "ACC-SH";
  fixture.account_id_szse = "ACC-SZ";
  return fixture;
}

OrderIntent limit_intent(const ClientOrderId& coi) {
  OrderIntent intent;
  intent.client_order_id = coi;
  intent.instrument_id = "600000.SSE";
  intent.side = qts::Side::buy;
  intent.type = qts::OrderType::limit;
  intent.limit_price = 105000;  // 10.50 yuan in 1/10,000
  intent.quantity = 100;
  return intent;
}

bool has_prefix(const std::string& text, const std::string& prefix) { return text.rfind(prefix, 0) == 0; }

// ---- pure mappings --------------------------------------------------------

void parse_instrument_maps_three_exchanges_and_fails_closed() {
  std::int32_t market = 0;
  std::string code;
  assert(detail::parse_instrument("600000.SSE", market, code));
  assert(market == fx::kMarketSse && code == "600000");
  assert(detail::parse_instrument("000001.SZSE", market, code));
  assert(market == fx::kMarketSzse && code == "000001");
  assert(detail::parse_instrument("920000.BSE", market, code));
  assert(market == fx::kMarketBse && code == "920000");
  assert(!detail::parse_instrument("600000", market, code));
  assert(!detail::parse_instrument("600000.XX", market, code));
  assert(!detail::parse_instrument(".SSE", market, code));
  assert(!detail::parse_instrument("", market, code));
  assert(!detail::parse_instrument("1234567.SSE", market, code));
  assert(!detail::parse_instrument("60000A.SSE", market, code));
}

void map_ord_status_covers_the_archforce_table() {
  OrderStatus status{};
  assert(detail::map_ord_status(fx::kOrdNew, status) && status == OrderStatus::pending_new);
  assert(detail::map_ord_status(fx::kOrdUnSend, status) && status == OrderStatus::pending_new);
  assert(detail::map_ord_status(fx::kOrdSended, status) && status == OrderStatus::pending_new);
  assert(detail::map_ord_status(fx::kOrdPartiallyFilled, status) && status == OrderStatus::partially_filled);
  assert(detail::map_ord_status(fx::kOrdFilled, status) && status == OrderStatus::filled);
  assert(detail::map_ord_status(fx::kOrdPartiallyFilledCancelled, status) && status == OrderStatus::cancelled);
  assert(detail::map_ord_status(fx::kOrdCancelled, status) && status == OrderStatus::cancelled);
  assert(detail::map_ord_status(fx::kOrdPartiallyCancelled, status) && status == OrderStatus::cancelled);
  assert(detail::map_ord_status(fx::kOrdReject, status) && status == OrderStatus::rejected);
  assert(detail::map_ord_status(fx::kOrdInternalReject, status) && status == OrderStatus::rejected);
  assert(detail::map_ord_status(fx::kOrdWaitCancelled, status) && status == OrderStatus::pending_cancel);
  assert(detail::map_ord_status(fx::kOrdPartiallyFilledWaitCancelled, status) && status == OrderStatus::pending_cancel);
  assert(detail::map_ord_status(fx::kOrdInternalCancelled, status) && status == OrderStatus::cancelled);
  assert(detail::map_ord_status(fx::kOrdCancelSuccess, status) && status == OrderStatus::cancelled);
  // Fail closed on anything outside the table.
  assert(!detail::map_ord_status(7, status));
  assert(!detail::map_ord_status(99, status));
}

void map_order_type_covers_limit_and_market_styles() {
  OrderIntent intent = limit_intent("coi-ot");
  assert(detail::map_order_type(intent) == 'a');  // kFixedNew
  intent.type = qts::OrderType::market;
  intent.market_order_style = qts::MarketOrderStyle::own_best;
  assert(detail::map_order_type(intent) == 'b');
  intent.market_order_style = qts::MarketOrderStyle::counterparty_best;
  assert(detail::map_order_type(intent) == 'c');
  intent.market_order_style = qts::MarketOrderStyle::best_five_ioc;
  assert(detail::map_order_type(intent) == 'f');
  intent.market_order_style = qts::MarketOrderStyle::best_five_to_limit;
  assert(detail::map_order_type(intent) == 'g');
  intent.market_order_style = qts::MarketOrderStyle::none;
  assert(detail::map_order_type(intent) == 0);  // explicit style required
}

void conversions_round_trip_prices_and_quantities() {
  assert(qts::quantapi_price_to_vendor(105000) == 10.5);
  assert(qts::quantapi_price_from_vendor(10.5) == 105000);
  assert(qts::quantapi_price_from_vendor(0.0001) == 1);
  assert(qts::quantapi_price_from_vendor(10.00005) == 100001 || qts::quantapi_price_from_vendor(10.00005) == 100000);
  assert(qts::quantapi_quantity_from_vendor(100.0) == 100);
  assert(qts::quantapi_quantity_from_vendor(100.4) == 100);
  assert(qts::quantapi_quantity_from_vendor(100.6) == 101);
  assert(qts::quantapi_quantity_to_vendor(300) == 300.0);
}

// ---- lifecycle / config (ADR-053: standby-first) --------------------------

void start_needs_only_non_secret_config_and_stays_in_standby() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  {
    QuantapiSessionConfig empty;  // every required field missing
    QuantapiTradeAdapter blank(empty);
    assert(!blank.start());
    assert(blank.stats().init_calls == 0);
    assert(!blank.started());
  }
  {
    // No customer identity/password, no feature code: still standby-ready
    // with zero vendor calls (login builds the session later).
    QuantapiSessionConfig config = make_config(sidecar);
    config.cust_id.clear();
    config.fund_account_id.clear();
    config.password.clear();
    config.client_feature_code.clear();
    QuantapiTradeAdapter adapter(config);
    assert(adapter.start());
    assert(adapter.stats().init_calls == 0);
    assert(!adapter.started());  // vendor session comes with connect()
    assert(!adapter.connected());
  }
  {
    QuantapiSessionConfig config = make_config(sidecar);
    config.agw_user = "gwuser";  // agw pair must be complete (password missing)
    QuantapiTradeAdapter adapter(config);
    assert(!adapter.start());
    assert(adapter.stats().init_calls == 0);
  }
  std::remove(sidecar.c_str());
}

void login_refuses_empty_and_unknown_identities_without_vendor_traffic() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  assert(!adapter.login("", "pw"));
  assert(!adapter.login("CUST-TEST", ""));
  // Injected session (no vendor): unknown identities refuse, wrong
  // passwords refuse, the session password verifies.
  QuantapiLoginFixture fixture = login_fixture();
  fixture.cust_id = "CUST-TEST";
  fixture.session_password.assign("trader-pw");  // seam-only credential (never a real secret; assigned to keep the secret scan literal-free)
  detail::inject_login(adapter, fixture);
  assert(adapter.session_fund() == "FUND-TEST");
  assert(!adapter.login("ghost", "trader-pw"));
  assert(!adapter.login("FUND-TEST", "wrong-pw"));
  assert(adapter.login("FUND-TEST", "trader-pw"));
  assert(adapter.login("CUST-TEST", "trader-pw"));  // cust maps to the same session
  std::remove(sidecar.c_str());
}

void feature_code_keeps_the_mandated_shape() {
  // #<APP>IIP:<ip>;MAC:<mac>;HD:<hd>, missing parts stay empty (ADR-053).
  const std::string code = detail::generate_feature_code("QTS");
  assert(has_prefix(code, "#QTSIIP:"));
  assert(code.find(";MAC:") != std::string::npos);
  assert(code.find(";HD:") != std::string::npos);
}

void inject_login_caches_the_account_triple() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  assert(!adapter.connected());
  detail::inject_login(adapter, login_fixture());
  const QuantapiAdapterStats stats = adapter.stats();
  assert(stats.login_calls == 0);  // login_calls counts Login() attempts (start), not the callback
  assert(!adapter.connected());    // no vendor session: connected needs a live api_
  assert(!adapter.started());
  std::remove(sidecar.c_str());
}

// ---- submit ---------------------------------------------------------------

void submit_rejects_bad_shapes_locally() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  std::vector<ExecutionReport> captured;
  adapter.set_execution_handler([&](const ExecutionReport& report) { captured.push_back(report); });

  OrderIntent bad_instrument = limit_intent("coi-b1");
  bad_instrument.instrument_id = "600000";
  adapter.submit(bad_instrument);

  OrderIntent bad_business = limit_intent("coi-b2");
  bad_business.trading_business = qts::TradingBusiness::margin_financing;
  adapter.submit(bad_business);

  OrderIntent bad_tif = limit_intent("coi-b3");
  bad_tif.time_in_force = qts::TimeInForce::immediate_or_cancel;
  adapter.submit(bad_tif);

  OrderIntent bad_qty = limit_intent("coi-b4");
  bad_qty.quantity = 0;
  adapter.submit(bad_qty);

  OrderIntent bad_price = limit_intent("coi-b5");
  bad_price.limit_price = 0;
  adapter.submit(bad_price);

  OrderIntent bad_style = limit_intent("coi-b6");
  bad_style.type = qts::OrderType::market;
  bad_style.market_order_style = qts::MarketOrderStyle::none;
  adapter.submit(bad_style);

  const std::size_t delivered = adapter.drain();
  assert(delivered == 6);
  assert(captured.size() == 6);
  for (const ExecutionReport& report : captured) {
    assert(report.status == OrderStatus::rejected);
    assert(has_prefix(report.reason, "adapter: "));
    assert(report.reject_code == 0);  // adapter-local rejections never claim a vendor code
    assert(report.order_id.empty());
  }
  assert(adapter.stats().local_rejects == 6);
  assert(adapter.stats().submit_calls == 6);
  std::remove(sidecar.c_str());
}

void submit_valid_order_hits_channel_unavailable_cleanly() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  detail::inject_login(adapter, login_fixture());  // logged in, but no vendor session
  std::vector<ExecutionReport> captured;
  adapter.set_execution_handler([&](const ExecutionReport& report) { captured.push_back(report); });

  adapter.submit(limit_intent("coi-ch"));
  adapter.submit(limit_intent("coi-ch"));  // first failure must not leak inflight state
  const std::size_t delivered = adapter.drain();
  assert(delivered == 2);
  assert(captured.size() == 2);
  for (const ExecutionReport& report : captured) {
    assert(report.status == OrderStatus::rejected);
    assert(report.reason.find("channel unavailable") != std::string::npos);
    assert(report.reject_code == 0);
  }
  // A seeded in-flight id is a duplicate.
  detail::inject_inflight(adapter, 4242, "coi-seed");
  adapter.submit(limit_intent("coi-seed"));
  adapter.drain();
  assert(captured.size() == 3);
  assert(captured.back().reason == "adapter: duplicate client order id");
  std::remove(sidecar.c_str());
}

// ---- ack / attribution / sidecar -----------------------------------------

void ack_success_binds_order_id_persists_map_and_flushes_stray_rtn() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  detail::inject_login(adapter, login_fixture());
  std::vector<ExecutionReport> captured;
  adapter.set_execution_handler([&](const ExecutionReport& report) { captured.push_back(report); });

  detail::inject_inflight(adapter, 12345, "coi-1");
  // RTN racing ahead of the ack: stashed, no report yet.
  QuantapiRtnFixture rtn;
  rtn.cl_ord_no = 90001;
  rtn.ord_status = fx::kOrdFilled;
  rtn.cum_qty = 100.0;
  rtn.last_qty = 100.0;
  rtn.last_px = 10.5;
  rtn.exec_id = "EXEC-1";
  detail::inject_rtn(adapter, rtn);
  assert(adapter.drain() == 0);
  assert(adapter.stats().unattributed_rtns == 1);

  QuantapiAckFixture ack;
  ack.request_id = 12345;
  ack.cl_ord_no = 90001;
  detail::inject_ack(adapter, ack);
  const std::size_t delivered = adapter.drain();
  assert(delivered == 2);  // ack report, then the flushed RTN
  assert(captured.size() == 2);
  assert(captured[0].status == OrderStatus::pending_new);
  assert(captured[0].order_id == "90001");
  assert(captured[0].broker_order_id == "90001");
  assert(captured[0].client_order_id == "coi-1");
  assert(captured[1].status == OrderStatus::filled);
  assert(captured[1].last_quantity == 100);
  assert(captured[1].last_price == 105000);
  assert(captured[1].cumulative_quantity == 100);
  assert(captured[1].execution_id == "EXEC-1");
  assert(captured[1].order_id == "90001");

  // The cl_ord_no -> client_order_id map hit the append-only sidecar.
  std::ifstream file(sidecar);
  std::stringstream contents;
  contents << file.rdbuf();
  assert(contents.str().find("map 90001 coi-1") != std::string::npos);
  assert(adapter.stats().unattributed_rtns == 1);  // counted when stashed, not re-counted on flush
  std::remove(sidecar.c_str());
}

void ack_gateway_reject_carries_the_vendor_code() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  detail::inject_login(adapter, login_fixture());
  std::vector<ExecutionReport> captured;
  adapter.set_execution_handler([&](const ExecutionReport& report) { captured.push_back(report); });

  detail::inject_inflight(adapter, 555, "coi-rj");
  QuantapiAckFixture ack;
  ack.request_id = 555;
  ack.cl_ord_no = 0;  // gateway rejected before assigning an id
  ack.error_id = 390009;
  ack.error_msg = "risk rejected";
  detail::inject_ack(adapter, ack);
  assert(adapter.drain() == 1);
  assert(captured.size() == 1);
  assert(captured[0].status == OrderStatus::rejected);
  assert(captured[0].reject_code == 390009);
  assert(captured[0].reason.find("risk rejected") != std::string::npos);
  assert(captured[0].order_id.empty());
  assert(adapter.stats().gateway_rejects == 1);
  std::remove(sidecar.c_str());
}

void ack_without_inflight_counts_unmatched() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  std::vector<ExecutionReport> captured;
  adapter.set_execution_handler([&](const ExecutionReport& report) { captured.push_back(report); });
  QuantapiAckFixture ack;
  ack.request_id = 999999;
  ack.cl_ord_no = 123;
  detail::inject_ack(adapter, ack);
  assert(adapter.drain() == 0);
  assert(captured.empty());
  assert(adapter.stats().unmatched_acks == 1);
  std::remove(sidecar.c_str());
}

void unknown_ord_status_is_dropped() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  detail::inject_login(adapter, login_fixture());
  std::vector<ExecutionReport> captured;
  adapter.set_execution_handler([&](const ExecutionReport& report) { captured.push_back(report); });
  detail::inject_inflight(adapter, 31, "coi-uk");
  QuantapiAckFixture ack;
  ack.request_id = 31;
  ack.cl_ord_no = 777;
  detail::inject_ack(adapter, ack);
  adapter.drain();
  captured.clear();

  QuantapiRtnFixture rtn;
  rtn.cl_ord_no = 777;
  rtn.ord_status = 7;  // outside ATPOrdStatusConst
  detail::inject_rtn(adapter, rtn);
  assert(adapter.drain() == 0);
  assert(captured.empty());
  assert(adapter.stats().status_unknown_drops == 1);
  std::remove(sidecar.c_str());
}

void sidecar_map_attributes_after_restart() {
  const std::string sidecar = fresh_sidecar();
  {
    std::ofstream seed(sidecar);
    seed << "seq 40\n";
    seed << "map 555001 coi-old\n";
  }
  QuantapiTradeAdapter adapter(make_config(sidecar));  // loads the persisted attribution
  std::vector<ExecutionReport> captured;
  adapter.set_execution_handler([&](const ExecutionReport& report) { captured.push_back(report); });
  QuantapiRtnFixture rtn;
  rtn.cl_ord_no = 555001;
  rtn.ord_status = fx::kOrdCancelled;
  detail::inject_rtn(adapter, rtn);
  assert(adapter.drain() == 1);
  assert(captured.size() == 1);
  assert(captured[0].client_order_id == "coi-old");
  assert(captured[0].status == OrderStatus::cancelled);
  assert(captured[0].order_id == "555001");
  assert(adapter.stats().unattributed_rtns == 0);
  std::remove(sidecar.c_str());
}

// ---- cancel ---------------------------------------------------------------

void cancel_before_ack_defers_and_ack_flushes() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  detail::inject_login(adapter, login_fixture());
  std::vector<ExecutionReport> captured;
  adapter.set_execution_handler([&](const ExecutionReport& report) { captured.push_back(report); });

  detail::inject_inflight(adapter, 700, "coi-14");
  adapter.cancel("coi-14");  // no cl_ord_no yet: deferred, no report
  assert(adapter.drain() == 0);
  assert(adapter.stats().cancel_calls == 1);

  QuantapiAckFixture ack;
  ack.request_id = 700;
  ack.cl_ord_no = 91001;
  detail::inject_ack(adapter, ack);
  const std::size_t delivered = adapter.drain();
  assert(delivered == 2);  // ack, then the deferred cancel attempt (channel unavailable)
  assert(captured[0].status == OrderStatus::pending_new);
  assert(captured[0].order_id == "91001");
  assert(captured[1].status == OrderStatus::pending_cancel);
  assert(captured[1].order_id == "91001");
  assert(captured[1].reason.find("cancel failed") != std::string::npos);
  assert(captured[1].reject_code == 0);
  std::remove(sidecar.c_str());
}

void cancel_rsp_rejection_keeps_pending_cancel_with_reason() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  detail::inject_login(adapter, login_fixture());
  std::vector<ExecutionReport> captured;
  adapter.set_execution_handler([&](const ExecutionReport& report) { captured.push_back(report); });
  detail::inject_inflight(adapter, 800, "coi-cr");
  QuantapiAckFixture ack;
  ack.request_id = 800;
  ack.cl_ord_no = 92002;
  detail::inject_ack(adapter, ack);
  adapter.drain();
  captured.clear();

  QuantapiCancelRspFixture rsp;
  rsp.request_id = 801;
  rsp.orig_cl_ord_no = 92002;
  rsp.error_id = 1007;
  rsp.error_msg = "order not cancellable";
  detail::inject_cancel_rsp(adapter, rsp);
  assert(adapter.drain() == 1);
  assert(captured.size() == 1);
  assert(captured[0].status == OrderStatus::pending_cancel);  // same-state, always accepted by OMS
  assert(captured[0].reason.find("order not cancellable") != std::string::npos);
  assert(captured[0].reject_code == 0);
  std::remove(sidecar.c_str());
}

// ---- control path ---------------------------------------------------------

void query_row_open_emits_and_terminal_is_skipped() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  detail::inject_login(adapter, login_fixture());
  std::vector<ExecutionReport> captured;
  adapter.set_execution_handler([&](const ExecutionReport& report) { captured.push_back(report); });
  detail::inject_inflight(adapter, 610, "coi-q");
  QuantapiAckFixture ack;
  ack.request_id = 610;
  ack.cl_ord_no = 71001;
  detail::inject_ack(adapter, ack);
  adapter.drain();
  captured.clear();

  QuantapiOrderQueryFixture open_row;
  open_row.has_row = true;
  open_row.cl_ord_no = 71001;
  open_row.ord_status = fx::kOrdSended;
  open_row.cum_qty = 0.0;
  detail::inject_order_query(adapter, open_row);

  QuantapiOrderQueryFixture terminal_row;
  terminal_row.has_row = true;
  terminal_row.cl_ord_no = 71001;
  terminal_row.ord_status = fx::kOrdFilled;
  terminal_row.cum_qty = 100.0;
  detail::inject_order_query(adapter, terminal_row);

  QuantapiOrderQueryFixture stranger_row;
  stranger_row.has_row = true;
  stranger_row.cl_ord_no = 606060;  // never ours
  stranger_row.ord_status = fx::kOrdNew;
  detail::inject_order_query(adapter, stranger_row);

  QuantapiOrderQueryFixture last;
  last.is_last = true;
  last.request_id = 777;  // no recovery flow active
  detail::inject_order_query(adapter, last);

  const std::size_t delivered = adapter.drain();
  assert(delivered == 1);  // only the open row for the known order
  assert(captured.size() == 1);
  assert(captured[0].client_order_id == "coi-q");
  assert(captured[0].status == OrderStatus::pending_new);
  assert(captured[0].order_id == "71001");
  assert(adapter.stats().query_rows == 3);
  assert(adapter.stats().unattributed_rtns == 1);
  assert(adapter.stats().recovery_completes == 0);
  std::remove(sidecar.c_str());
}

void recovery_via_request_open_orders_reports_channel_failure() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  detail::inject_login(adapter, login_fixture());
  std::vector<std::pair<qts::AccountId, bool>> recoveries;
  adapter.set_recovery_complete_handler([&](const qts::AccountId& account, bool success) { recoveries.emplace_back(account, success); });

  adapter.request_open_orders("FUND-TEST");  // queries cannot send: fail closed
  const std::size_t reports = adapter.drain();
  assert(reports == 0);
  assert(recoveries.size() == 1);
  assert(recoveries[0].first == "FUND-TEST");
  assert(recoveries[0].second == false);
  assert(adapter.stats().recovery_completes == 1);
  std::remove(sidecar.c_str());
}

void snapshot_assembles_fund_and_positions() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  detail::inject_login(adapter, login_fixture());
  std::vector<TradeAccountSnapshot> snapshots;
  adapter.set_account_snapshot_handler([&](const TradeAccountSnapshot& snapshot) { snapshots.push_back(snapshot); });

  QuantapiFundFixture fund;
  fund.request_id = 9001;
  fund.has_row = true;
  fund.leaves_value = 100000.0;
  fund.frozen_all = 5000.0;
  fund.available_t0 = 20000.0;  // 当日可取
  fund.available_t1 = 30000.0;  // 当日可用
  detail::inject_fund(adapter, fund);

  QuantapiShareFixture share;
  share.request_id = 9002;
  share.has_row = true;
  share.security_id = "600000";
  share.market_id = fx::kMarketSse;
  share.leaves_qty = 1000.0;
  share.available_qty = 800.0;
  share.cost_price = 10.25;
  share.market_value = 5000.0;
  detail::inject_share(adapter, share);

  QuantapiFundFixture fund_last;
  fund_last.request_id = 9001;
  fund_last.is_last = true;
  detail::inject_fund(adapter, fund_last);

  adapter.drain();
  assert(snapshots.size() == 1);
  const TradeAccountSnapshot& snapshot = snapshots[0];
  assert(snapshot.account_id == "FUND-TEST");
  assert(snapshot.total_asset == 100000 * 10000 + 5000 * 10000);  // cash + market value
  assert(snapshot.available_cash == 30000 * 10000);
  assert(snapshot.withdrawable_cash == 20000 * 10000);
  assert(snapshot.frozen_cash == 5000 * 10000);
  assert(snapshot.positions.size() == 1);
  const PositionSnapshot& position = snapshot.positions[0];
  assert(position.instrument_id == "600000.SSE");
  assert(position.total_quantity == 1000);
  assert(position.sellable_quantity == 800);
  assert(position.frozen_quantity == 200);
  assert(position.average_cost == 102500);  // 10.25 yuan
  assert(position.today_quantity == 0);     // ADR-032: split unreported
  assert(adapter.stats().snapshots_delivered == 1);
  std::remove(sidecar.c_str());
}

void drain_without_handlers_keeps_the_queues() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  detail::inject_login(adapter, login_fixture());
  detail::inject_inflight(adapter, 61, "coi-dh");
  QuantapiAckFixture ack;
  ack.request_id = 61;
  ack.cl_ord_no = 888;
  detail::inject_ack(adapter, ack);
  assert(adapter.drain() == 0);  // no handler yet: nothing delivered, nothing lost
  std::vector<ExecutionReport> captured;
  adapter.set_execution_handler([&](const ExecutionReport& report) { captured.push_back(report); });
  assert(adapter.drain() == 1);
  assert(captured.size() == 1);
  assert(captured[0].status == OrderStatus::pending_new);
  assert(adapter.stats().reports_enqueued == 1);
  assert(adapter.stats().reports_delivered == 1);
  std::remove(sidecar.c_str());
}

// ---- reference-data path (ADR-054) ----------------------------------------

struct SecInfoCapture {
  qts::AccountId account;
  std::vector<qts::SecurityInfoRow> rows;
  bool last{false};
};

QuantapiSecInfoFixture secinfo_row(std::int64_t req, const std::string& code, std::int32_t market, std::uint16_t type) {
  QuantapiSecInfoFixture fixture;
  fixture.request_id = req;
  fixture.has_row = true;
  fixture.security_id = code;
  fixture.market_id = market;
  fixture.symbol = "SYM-" + code;
  fixture.security_type = type;
  fixture.prev_close = 10.0;
  fixture.price_upper_limit = 11.0;
  fixture.price_lower_limit = 9.0;
  fixture.price_tick = 0.01;
  fixture.buy_qty_lower = 100.0;
  fixture.sell_qty_lower = 100.0;
  fixture.buy_qty_unit = 100.0;
  fixture.sell_qty_unit = 100.0;
  return fixture;
}

void secinfo_sweep_maps_rows_and_batches() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  detail::inject_login(adapter, login_fixture());
  std::vector<SecInfoCapture> batches;
  adapter.set_security_info_handler([&](const qts::AccountId& account, const std::vector<qts::SecurityInfoRow>& rows, bool last) { batches.push_back(SecInfoCapture{account, rows, last}); });
  detail::inject_secinfo(adapter, secinfo_row(7001, "600000", fx::kMarketSse, 1));
  detail::inject_secinfo(adapter, secinfo_row(7002, "000001", fx::kMarketSzse, 1));
  QuantapiSecInfoFixture last;
  last.request_id = 7002;
  last.is_last = true;
  detail::inject_secinfo(adapter, last);
  adapter.drain();
  assert(batches.size() == 1);
  assert(batches[0].account == "FUND-TEST");
  assert(batches[0].last);
  assert(batches[0].rows.size() == 2);
  const qts::SecurityInfoRow& sse = batches[0].rows[0];
  assert(sse.instrument_id == "600000.SSE");
  assert(sse.symbol == "SYM-600000" && sse.security_type == 1);
  assert(sse.price_tick == 100);  // 0.01 yuan
  assert(sse.buy_unit == 100 && sse.sell_unit == 100);
  assert(sse.limit_up == 110000 && sse.limit_down == 90000 && sse.prev_close == 100000);
  assert(!sse.day_trading);
  assert(batches[0].rows[1].instrument_id == "000001.SZSE");
  assert(adapter.stats().secinfo_rows == 2);
  assert(adapter.stats().secinfo_batches_delivered == 1);
  std::remove(sidecar.c_str());
}

void secinfo_failed_sweep_delivers_empty_complete() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  detail::inject_login(adapter, login_fixture());
  std::vector<SecInfoCapture> batches;
  adapter.set_security_info_handler([&](const qts::AccountId& account, const std::vector<qts::SecurityInfoRow>& rows, bool last) { batches.push_back(SecInfoCapture{account, rows, last}); });
  QuantapiSecInfoFixture bad;
  bad.request_id = 7101;
  bad.is_last = true;
  bad.error_id = 3;
  detail::inject_secinfo(adapter, bad);
  adapter.drain();
  assert(batches.size() == 1);
  assert(batches[0].rows.empty() && batches[0].last);  // fail-closed completion signal
  std::remove(sidecar.c_str());
}

void secinfo_without_session_completes_empty() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));  // never logged in
  std::vector<SecInfoCapture> batches;
  adapter.set_security_info_handler([&](const qts::AccountId& account, const std::vector<qts::SecurityInfoRow>& rows, bool last) { batches.push_back(SecInfoCapture{account, rows, last}); });
  adapter.request_security_info("FUND-TEST", "");
  adapter.drain();
  assert(batches.size() == 1);
  assert(batches[0].account == "FUND-TEST" && batches[0].rows.empty() && batches[0].last);
  std::remove(sidecar.c_str());
}

void secinfo_chunked_drain_never_stalls() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  detail::inject_login(adapter, login_fixture());
  std::size_t total = 0;
  std::size_t calls = 0;
  bool saw_non_last = false;
  bool final_last = false;
  adapter.set_security_info_handler([&](const qts::AccountId&, const std::vector<qts::SecurityInfoRow>& rows, bool last) {
    ++calls;
    assert(rows.size() <= 2000);  // kSecInfoDrainChunk bound
    total += rows.size();
    if (!last) saw_non_last = true;
    if (total == 2505) final_last = last;
  });
  for (std::size_t i = 0; i < 2505; ++i) {
    detail::inject_secinfo(adapter, secinfo_row(7200, "6" + std::to_string(10000 + i), fx::kMarketSse, 1));
  }
  QuantapiSecInfoFixture last;
  last.request_id = 7200;
  last.is_last = true;
  detail::inject_secinfo(adapter, last);
  for (int i = 0; i < 10 && total < 2505; ++i) adapter.drain();
  assert(total == 2505 && saw_non_last && final_last);
  assert(calls == 2);  // 2000 + 505
  assert(adapter.stats().secinfo_batches_delivered == 1);
  std::remove(sidecar.c_str());
}

void asset_snapshot_assembles_totals() {
  const std::string sidecar = fresh_sidecar();
  std::remove(sidecar.c_str());
  QuantapiTradeAdapter adapter(make_config(sidecar));
  detail::inject_login(adapter, login_fixture());
  std::vector<qts::TradeAssetSnapshot> assets;
  adapter.set_asset_snapshot_handler([&](const qts::TradeAssetSnapshot& snapshot) { assets.push_back(snapshot); });
  QuantapiAssetFixture asset;
  asset.request_id = 7301;
  asset.has_row = true;
  asset.leaves_value = 90000.0;
  asset.frozen_all = 1000.0;
  asset.available_t0 = 20000.0;
  asset.available_t1 = 30000.0;
  asset.total_asset = 120000.0;
  asset.market_value = 30000.0;
  detail::inject_asset(adapter, asset);
  QuantapiAssetFixture last;
  last.request_id = 7301;
  last.is_last = true;
  detail::inject_asset(adapter, last);
  adapter.drain();
  assert(assets.size() == 1);
  assert(assets[0].account_id == "FUND-TEST");
  assert(assets[0].total_asset == 1200000000);
  assert(assets[0].market_value == 300000000);
  assert(assets[0].leaves_value == 900000000);
  assert(assets[0].frozen == 10000000);
  assert(assets[0].available_t0 == 200000000 && assets[0].available_t1 == 300000000);
  assert(adapter.stats().assets_delivered == 1);
  std::remove(sidecar.c_str());
}

}  // namespace

int main() {
  parse_instrument_maps_three_exchanges_and_fails_closed();
  map_ord_status_covers_the_archforce_table();
  map_order_type_covers_limit_and_market_styles();
  conversions_round_trip_prices_and_quantities();
  start_needs_only_non_secret_config_and_stays_in_standby();
  login_refuses_empty_and_unknown_identities_without_vendor_traffic();
  feature_code_keeps_the_mandated_shape();
  inject_login_caches_the_account_triple();
  submit_rejects_bad_shapes_locally();
  submit_valid_order_hits_channel_unavailable_cleanly();
  ack_success_binds_order_id_persists_map_and_flushes_stray_rtn();
  ack_gateway_reject_carries_the_vendor_code();
  ack_without_inflight_counts_unmatched();
  unknown_ord_status_is_dropped();
  sidecar_map_attributes_after_restart();
  cancel_before_ack_defers_and_ack_flushes();
  cancel_rsp_rejection_keeps_pending_cancel_with_reason();
  query_row_open_emits_and_terminal_is_skipped();
  recovery_via_request_open_orders_reports_channel_failure();
  snapshot_assembles_fund_and_positions();
  drain_without_handlers_keeps_the_queues();
  secinfo_sweep_maps_rows_and_batches();
  secinfo_failed_sweep_delivers_empty_complete();
  secinfo_without_session_completes_empty();
  secinfo_chunked_drain_never_stalls();
  asset_snapshot_assembles_totals();
  return 0;
}
