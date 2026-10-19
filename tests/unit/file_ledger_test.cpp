// clang-format off
/**
  ******************************************************************************
  * @file           : file_ledger_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for JSONL persistence, resume and legacy replay.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/persistence/file_ledger.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"
#include "qts/broker/simulated_broker.hpp"
#include "qts/oms/order_manager.hpp"
#include "qts/strategy/grid_strategy.hpp"

namespace {

std::filesystem::path fresh_temp(const std::string& tag) {
  static std::atomic<unsigned> counter{0};
  const unsigned n = counter.fetch_add(1);
  const auto path = std::filesystem::temp_directory_path() / ("qts-" + tag + "-" + std::to_string(n) + ".jsonl");
  std::error_code ec;
  std::filesystem::remove(path, ec);
  return path;
}

qts::LedgerEvent make_rich_event() {
  qts::LedgerEvent event{};
  event.type = qts::LedgerEventType::execution_applied;
  event.envelope.trace_id = "trace-rich";
  event.envelope.source_time = qts::Timestamp{std::chrono::nanoseconds{1'700'000'000'123'456'789LL}};
  event.envelope.receive_time = qts::Timestamp{std::chrono::nanoseconds{1'700'000'001'000'000'000LL}};
  event.envelope.config_version = "cfg-7";
  event.envelope.rules_version = "rules-9";
  event.envelope.source_message_id = "raw-42";
  event.envelope.trade_date = "2026-10-02";
  event.intent.client_order_id = "k-rich";
  event.intent.account_id = "paper";
  event.intent.strategy_id = "grid-1";
  event.intent.instrument_id = "600000";
  event.intent.side = qts::Side::sell;
  event.intent.type = qts::OrderType::limit;
  event.intent.time_in_force = qts::TimeInForce::day;
  event.intent.quantity = 200;
  event.intent.limit_price = 12345;
  event.intent.trace_id = "trace-rich";
  event.intent.exchange = qts::Exchange::sse;
  event.intent.board = qts::AshareBoard::main;
  event.intent.security_account_id = "A123";
  event.intent.trading_business = qts::TradingBusiness::cash;
  event.intent.market_order_style = qts::MarketOrderStyle::none;
  event.intent.protection_price = 12000;
  event.intent.parent_client_order_id = "parent-1";
  event.snapshot = {"SIM-k-rich", "k-rich", qts::OrderStatus::partially_filled, 100, 12300, "", "fill-1", 100, 3};
  event.snapshot.broker_order_id = "900012345678";
  event.snapshot.reject_code = 0;
  event.snapshot.fee = 250;
  return event;
}

void assert_same_event(const qts::LedgerEvent& got, const qts::LedgerEvent& want) {
  assert(got.envelope.event_id == want.envelope.event_id);
  assert(got.type == want.type);
  assert(got.envelope.trace_id == want.envelope.trace_id);
  assert(got.envelope.source_time == want.envelope.source_time);
  assert(got.envelope.receive_time == want.envelope.receive_time);
  assert(got.envelope.config_version == want.envelope.config_version);
  assert(got.envelope.rules_version == want.envelope.rules_version);
  assert(got.envelope.source_message_id == want.envelope.source_message_id);
  assert(got.envelope.trade_date == want.envelope.trade_date);
  assert(got.intent.client_order_id == want.intent.client_order_id);
  assert(got.intent.account_id == want.intent.account_id);
  assert(got.intent.strategy_id == want.intent.strategy_id);
  assert(got.intent.instrument_id == want.intent.instrument_id);
  assert(got.intent.side == want.intent.side);
  assert(got.intent.type == want.intent.type);
  assert(got.intent.time_in_force == want.intent.time_in_force);
  assert(got.intent.quantity == want.intent.quantity);
  assert(got.intent.limit_price == want.intent.limit_price);
  assert(got.intent.trace_id == want.intent.trace_id);
  assert(got.intent.exchange == want.intent.exchange);
  assert(got.intent.board == want.intent.board);
  assert(got.intent.security_account_id == want.intent.security_account_id);
  assert(got.intent.trading_business == want.intent.trading_business);
  assert(got.intent.market_order_style == want.intent.market_order_style);
  assert(got.intent.protection_price == want.intent.protection_price);
  assert(got.intent.parent_client_order_id == want.intent.parent_client_order_id);
  assert(got.snapshot.order_id == want.snapshot.order_id);
  assert(got.snapshot.client_order_id == want.snapshot.client_order_id);
  assert(got.snapshot.status == want.snapshot.status);
  assert(got.snapshot.last_quantity == want.snapshot.last_quantity);
  assert(got.snapshot.last_price == want.snapshot.last_price);
  assert(got.snapshot.reason == want.snapshot.reason);
  assert(got.snapshot.execution_id == want.snapshot.execution_id);
  assert(got.snapshot.cumulative_quantity == want.snapshot.cumulative_quantity);
  assert(got.snapshot.version == want.snapshot.version);
  assert(got.snapshot.broker_order_id == want.snapshot.broker_order_id);
  assert(got.snapshot.reject_code == want.snapshot.reject_code);
  assert(got.snapshot.fee == want.snapshot.fee);
  assert(got.funding.strategy == want.funding.strategy);
  assert(got.funding.instrument == want.funding.instrument);
  assert(got.funding.quantity == want.funding.quantity);
  assert(got.funding.average_cost == want.funding.average_cost);
  assert(got.funding.cash == want.funding.cash);
  assert(got.attestation.operator_id == want.attestation.operator_id);
  assert(got.attestation.broker_snapshot_time == want.attestation.broker_snapshot_time);
  assert(got.attestation.ledger_watermark == want.attestation.ledger_watermark);
  assert(got.attestation.cash_dirty == want.attestation.cash_dirty);
  assert(got.attestation.position_dirty == want.attestation.position_dirty);
  assert(got.attestation.disclosed_fees == want.attestation.disclosed_fees);
  assert(got.attestation.note == want.attestation.note);
  assert(got.control.command == want.control.command);
  assert(got.control.target == want.control.target);
  assert(got.control.blocked_after == want.control.blocked_after);
  assert(got.control.source == want.control.source);
}

void roundtrip_preserves_every_field() {
  const auto path = fresh_temp("roundtrip");
  const qts::LedgerEvent want = make_rich_event();
  std::uint64_t id = 0;
  {
    qts::FileEventLedger ledger{path.string()};
    assert(ledger.latest_event_id() == 0);
    id = ledger.append(want);
    assert(id == 1);
  }
  qts::FileEventLedger reopened{path.string()};
  assert(reopened.latest_event_id() == id);
  const auto events = reopened.replay(1);
  assert(events.size() == 1);
  qts::LedgerEvent expected = want;
  expected.envelope.event_id = id;
  assert_same_event(events[0], expected);
  // Appending after reopen resumes ids monotonically.
  qts::LedgerEvent second = want;
  second.intent.client_order_id = "k-rich-2";
  assert(reopened.append(second) == id + 1);
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

// ADR-032: ledger lines written before the fields existed must still replay
// (runbook §4.1 restores from backup files).
void legacy_snapshot_without_adr032_fields_replays() {
  const auto path = fresh_temp("legacy");
  {
    qts::FileEventLedger ledger{path.string()};
    assert(ledger.append(make_rich_event()) == 1);
  }
  nlohmann::json line;
  {
    std::ifstream in{path.string()};
    std::string text;
    assert(static_cast<bool>(std::getline(in, text)));
    line = nlohmann::json::parse(text);
  }
  line.at("snapshot").erase("broker_order_id");
  line.at("snapshot").erase("reject_code");
  line.at("snapshot").erase("fee");
  line.erase("trade_date");  // pre-trade_date ledgers replay with unknown date
  {
    std::ofstream out{path.string(), std::ios::trunc};
    out << line.dump() << '\n';
  }
  qts::FileEventLedger reopened{path.string()};
  const auto events = reopened.replay(1);
  assert(events.size() == 1);
  const auto& snap = events[0].snapshot;
  assert(snap.broker_order_id.empty());
  assert(snap.reject_code == 0);
  assert(snap.fee == 0);  // pre-fee ledgers replay as zero-fee
  // Legacy fields still parse.
  assert(snap.order_id == "SIM-k-rich");
  assert(snap.cumulative_quantity == 100);
  assert(snap.version == 3);
  assert(events[0].envelope.trade_date.empty());
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

// M11-7: funding legs round-trip with every field; cash and position legs
// share the shape, only the populated half differs.
// M14/M15: attestation and switch records round-trip with every field.
void attestation_and_control_roundtrip() {
  const auto path = fresh_temp("attest-control");
  qts::LedgerEvent attested{};
  attested.type = qts::LedgerEventType::reconcile_attested;
  attested.envelope.trace_id = "settle-confirm";
  attested.attestation.operator_id = "closer";
  attested.attestation.broker_snapshot_time = "2026-10-11T00:00:00Z";
  attested.attestation.ledger_watermark = 99;
  attested.attestation.position_dirty = 1;
  attested.attestation.disclosed_fees = 250;
  attested.attestation.note = "daily close";
  qts::LedgerEvent switched{};
  switched.type = qts::LedgerEventType::control_applied;
  switched.envelope.trace_id = "control-halt";
  switched.control.command = "halt";
  switched.control.blocked_after = true;
  switched.control.source = "inbox";
  std::uint64_t first = 0;
  {
    qts::FileEventLedger ledger{path.string()};
    first = ledger.append(attested);
    assert(ledger.append(switched) == first + 1);
  }
  qts::FileEventLedger reopened{path.string()};
  const auto events = reopened.replay(1);
  assert(events.size() == 2);
  qts::LedgerEvent want_attested = attested;
  want_attested.envelope.event_id = first;
  qts::LedgerEvent want_switched = switched;
  want_switched.envelope.event_id = first + 1;
  assert_same_event(events[0], want_attested);
  assert_same_event(events[1], want_switched);
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

void funding_roundtrips_with_cash_and_position() {
  const auto path = fresh_temp("funding");
  qts::LedgerEvent cash{};
  cash.type = qts::LedgerEventType::funding_deposited;
  cash.envelope.trace_id = "funding";
  cash.envelope.config_version = "1";
  cash.envelope.rules_version = "1";
  cash.funding.cash = 10'000'000;
  qts::LedgerEvent deposit{};
  deposit.type = qts::LedgerEventType::funding_deposited;
  deposit.envelope.trace_id = "funding";
  deposit.envelope.config_version = "1";
  deposit.envelope.rules_version = "1";
  deposit.funding.instrument = "TEST";
  deposit.funding.quantity = 10'000;
  deposit.funding.average_cost = 1'000;
  std::uint64_t cash_id = 0;
  std::uint64_t deposit_id = 0;
  {
    qts::FileEventLedger ledger{path.string()};
    cash_id = ledger.append(cash);
    deposit_id = ledger.append(deposit);
    assert(deposit_id == cash_id + 1);
  }
  qts::FileEventLedger reopened{path.string()};
  const auto events = reopened.replay(1);
  assert(events.size() == 2);
  assert(events[0].type == qts::LedgerEventType::funding_deposited);
  assert(events[0].funding.cash == 10'000'000 && events[0].funding.quantity == 0);
  assert(events[1].funding.instrument == "TEST");
  assert(events[1].funding.quantity == 10'000 && events[1].funding.average_cost == 1'000);
  assert(events[1].funding.cash == 0);
  qts::LedgerEvent expected_cash = cash;
  expected_cash.envelope.event_id = cash_id;
  qts::LedgerEvent expected_deposit = deposit;
  expected_deposit.envelope.event_id = deposit_id;
  assert_same_event(events[0], expected_cash);
  assert_same_event(events[1], expected_deposit);
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

// Pre-M11-7 ledger lines carry no funding node and must replay with an
// empty deposit (same pattern as the ADR-032 keys above).
void pre_funding_ledger_replays_empty_funding() {
  const auto path = fresh_temp("pre-funding");
  {
    qts::FileEventLedger ledger{path.string()};
    assert(ledger.append(make_rich_event()) == 1);
  }
  nlohmann::json line;
  {
    std::ifstream in{path.string()};
    std::string text;
    assert(static_cast<bool>(std::getline(in, text)));
    line = nlohmann::json::parse(text);
  }
  line.erase("funding");
  {
    std::ofstream out{path.string(), std::ios::trunc};
    out << line.dump() << '\n';
  }
  qts::FileEventLedger reopened{path.string()};
  const auto events = reopened.replay(1);
  assert(events.size() == 1);
  assert(events[0].funding.strategy.empty() && events[0].funding.instrument.empty());
  assert(events[0].funding.quantity == 0 && events[0].funding.average_cost == 0 && events[0].funding.cash == 0);
  assert(events[0].snapshot.order_id == "SIM-k-rich");  // order fields still parse
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

void restart_resumes_ids_and_replay() {
  const auto path = fresh_temp("resume");
  {
    qts::FileEventLedger ledger{path.string()};
    qts::LedgerEvent event{};
    event.intent.client_order_id = "a";
    assert(ledger.append(event) == 1);
    assert(ledger.append(event) == 2);
  }
  {
    qts::FileEventLedger ledger{path.string()};
    assert(ledger.latest_event_id() == 2);
    assert(ledger.replay(1).size() == 2);
    assert(ledger.replay(2).size() == 1);
    assert(ledger.replay(3).empty());
    qts::LedgerEvent event{};
    event.intent.client_order_id = "b";
    assert(ledger.append(event) == 3);
    assert(ledger.latest_event_id() == 3);
  }
  qts::FileEventLedger ledger{path.string()};
  assert(ledger.latest_event_id() == 3);
  assert(ledger.replay(1).size() == 3);
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

qts::OrderIntent make_intent(const std::string& id) {
  qts::OrderIntent intent{id, "paper", "test", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, 10, 100};
  intent.trace_id = "trace-" + id;
  return intent;
}

void file_backed_oms_restart() {
  const auto path = fresh_temp("oms-restart");
  qts::LedgerSnapshot snapshot{};
  std::vector<qts::ExecutionReport> original_open;
  {
    qts::EventBus events1;
    qts::SimulatedBroker broker1{events1};  // no reference price: order stays live
    qts::RiskEngine risk1{{100, 10'000, true}};
    qts::FileEventLedger ledger{path.string()};
    qts::OrderManager oms1{broker1, risk1, events1, &ledger};
    assert(oms1.submit(make_intent("kf1")));
    snapshot.last_event_id = ledger.latest_event_id();
    snapshot.open_orders = oms1.open_orders();
    assert(snapshot.open_orders.size() == 1);
    assert(oms1.on_execution_report({"SIM-kf1", "kf1", qts::OrderStatus::partially_filled, 4, 100, "", "fill-1"}));
    original_open = oms1.open_orders();
  }
  // New process: same file, restore from snapshot, replay newer events.
  qts::EventBus events2;
  qts::SimulatedBroker broker2{events2};
  qts::RiskEngine risk2{{100, 10'000, true}};
  qts::FileEventLedger ledger2{path.string()};
  qts::OrderManager oms2{broker2, risk2, events2};
  assert(oms2.restore_order(make_intent("kf1"), snapshot.open_orders[0]));
  for (const auto& e : ledger2.replay(snapshot.last_event_id + 1)) {
    if (e.type == qts::LedgerEventType::execution_applied) assert(oms2.on_execution_report(e.snapshot));
  }
  const qts::ExecutionReport* rebuilt = oms2.find("kf1");
  assert(rebuilt != nullptr);
  assert(rebuilt->status == qts::OrderStatus::partially_filled);
  assert(rebuilt->cumulative_quantity == 4);
  assert(rebuilt->version == original_open[0].version);
  assert(rebuilt->order_id == original_open[0].order_id);
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

void grid_drill_persists_and_reopens() {
  const auto path = fresh_temp("e2e");
  {
    qts::EventBus events;
    qts::SimulatedBroker broker{events};
    broker.set_starting_cash(1'000'000);
    qts::RiskEngine risk{{10'000, 0, true}};
    qts::FileEventLedger ledger{path.string()};
    qts::OrderManager oms{broker, risk, events, &ledger};
    qts::GridConfig config{};
    config.strategy_id = "grid-1";
    config.account_id = "paper";
    config.instrument_id = "TEST";
    config.lower = 900;
    config.upper = 1'100;
    config.grids = 4;
    config.quantity_per_order = 100;
    qts::GridStrategy strategy{config, 1'000};
    const std::vector<qts::Price> prices{1'000, 950, 900, 950, 1'000, 1'050, 1'100};
    std::uint64_t sequence = 0;
    int submitted = 0;
    for (const qts::Price price : prices) {
      broker.set_reference_price("TEST", price);
      qts::QuoteEvent quote{};
      quote.instrument_id = "TEST";
      quote.last_price = price;
      quote.sequence = ++sequence;
      for (auto intent : strategy.on_quote(quote, broker.book())) {
        assert(oms.submit(intent));
        ++submitted;
      }
    }
    assert(submitted == 6);
    assert(ledger.latest_event_id() == 18);
    assert(oms.open_orders().empty());
  }
  qts::FileEventLedger reopened{path.string()};
  assert(reopened.latest_event_id() == 18);
  assert(reopened.replay(1).size() == 18);
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

void corrupt_line_is_rejected() {
  const auto path = fresh_temp("corrupt");
  {
    std::ofstream out{path};
    out << "{not json\n";
  }
  bool threw = false;
  try {
    qts::FileEventLedger ledger{path.string()};
  } catch (const std::runtime_error& e) {
    threw = std::string{e.what()}.find(path.string()) != std::string::npos;
  }
  assert(threw);
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

void open_snapshot_roundtrips() {
  const auto path = fresh_temp("snapshot");
  const qts::LedgerEvent rich = make_rich_event();
  qts::OpenSnapshot snapshot{};
  snapshot.ledger_bytes = 12345;
  snapshot.last_event_id = 7;
  snapshot.config_version = "cfg-7";
  snapshot.rules_version = "rules-9";
  snapshot.opens.emplace_back(rich.intent, rich.snapshot);
  qts::write_open_snapshot(path.string(), snapshot);
  const qts::OpenSnapshot back = qts::read_open_snapshot(path.string());
  assert(back.ledger_bytes == 12345 && back.last_event_id == 7);
  assert(back.config_version == "cfg-7" && back.rules_version == "rules-9");
  assert(back.opens.size() == 1);
  assert(back.opens[0].first.client_order_id == "k-rich");
  assert(back.opens[0].first.strategy_id == "grid-1");
  assert(back.opens[0].second.status == qts::OrderStatus::partially_filled);
  assert(back.opens[0].second.cumulative_quantity == 100);
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

void open_snapshot_missing_or_corrupt_throws() {
  bool threw = false;
  try {
    (void)qts::read_open_snapshot("/nonexistent/qts-snapshot.json");
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
  const auto path = fresh_temp("snapshot-bad");
  {
    std::ofstream out{path};
    out << "{not json\n";
  }
  threw = false;
  try {
    (void)qts::read_open_snapshot(path.string());
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

}  // namespace

int main() {
  roundtrip_preserves_every_field();
  legacy_snapshot_without_adr032_fields_replays();
  funding_roundtrips_with_cash_and_position();
  attestation_and_control_roundtrip();
  pre_funding_ledger_replays_empty_funding();
  restart_resumes_ids_and_replay();
  file_backed_oms_restart();
  grid_drill_persists_and_reopens();
  corrupt_line_is_rejected();
  open_snapshot_roundtrips();
  open_snapshot_missing_or_corrupt_throws();
  return 0;
}
