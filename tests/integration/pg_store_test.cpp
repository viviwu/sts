// clang-format off
/**
  ******************************************************************************
  * @file           : pg_store_test.cpp
  * @author         : vivi wu
  * @brief          : PostgreSQL projection conformance: same contract as SQLite, live server required.
  * @version        : 0.1.0
  * @date           : 10/10/26
  ******************************************************************************
  */
// clang-format on
// PG-gated test (QTS_ENABLE_PG): needs a live server with an empty
// database. The conninfo comes from QTS_PG_TEST_CONNINFO only (never
// committed); unset means skip with exit 0 so untargeted builds stay
// green. Each case resets the four tables for isolation.
#include "qts/persistence/pg_store.hpp"

#include <libpq-fe.h>

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "qts/broker/simulated_broker.hpp"
#include "qts/market/quote_feed.hpp"
#include "qts/oms/order_manager.hpp"
#include "qts/persistence/event_store.hpp"
#include "qts/persistence/sqlite_store.hpp"

namespace {

std::string test_conninfo() {
  const char* raw = std::getenv("QTS_PG_TEST_CONNINFO");
  return raw != nullptr ? raw : "";
}

void reset_tables(const std::string& conninfo) {
  PGconn* conn = PQconnectdb(conninfo.c_str());
  assert(PQstatus(conn) == CONNECTION_OK);
  PGresult* result = PQexec(conn, "DROP TABLE IF EXISTS positions; DROP TABLE IF EXISTS executions; DROP TABLE IF EXISTS orders; DROP TABLE IF EXISTS events;");
  assert(PQresultStatus(result) == PGRES_COMMAND_OK);
  PQclear(result);
  PQfinish(conn);
}

std::int64_t query_sum_fee(const std::string& conninfo) {
  PGconn* conn = PQconnectdb(conninfo.c_str());
  assert(PQstatus(conn) == CONNECTION_OK);
  PGresult* result = PQexec(conn, "SELECT COALESCE(SUM(fee),0) FROM executions;");
  assert(PQresultStatus(result) == PGRES_TUPLES_OK);
  const std::int64_t total = static_cast<std::int64_t>(std::stoll(PQgetvalue(result, 0, 0)));
  PQclear(result);
  PQfinish(conn);
  return total;
}

// One filled order through the real OMS path, recorded in-memory.
std::vector<qts::LedgerEvent> one_fill_stream() {
  static unsigned tag = 0;
  qts::EventBus events;
  qts::SimulatedBroker broker{events};
  broker.set_reference_price("TEST", 1'000);
  qts::RiskEngine risk{{10'000, 0, true}};
  qts::InMemoryEventLedger ledger{};
  qts::OrderManager oms{broker, risk, events, &ledger};
  qts::OrderIntent intent{"pg-1-" + std::to_string(tag++), "paper", "test", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, 100, 1'000};
  intent.trace_id = intent.client_order_id;
  assert(oms.submit(intent));
  const auto stream = ledger.replay(1);
  assert(!stream.empty());
  return stream;
}

void pg_applies_idempotently(const std::string& conninfo) {
  reset_tables(conninfo);
  const auto stream = one_fill_stream();
  {
    qts::PgEventStore store{conninfo};
    assert(store.latest_event_id() == 0);
    for (const auto& event : stream) store.apply(event);
    const auto counts = store.counts();
    assert(counts.events == stream.size());
    assert(counts.orders == 1);
    assert(store.latest_event_id() == stream.back().envelope.event_id);
    for (const auto& event : stream) store.apply(event);
    const auto replayed = store.counts();
    assert(replayed.events == counts.events);
    assert(replayed.orders == counts.orders);
    assert(replayed.executions == counts.executions);
    assert(store.latest_event_id() == stream.back().envelope.event_id);
  }
  {
    qts::PgEventStore store{conninfo};
    assert(store.latest_event_id() == stream.back().envelope.event_id);
    assert(store.counts().events == stream.size());
  }
}

void pg_watermark_resume_catches_up(const std::string& conninfo) {
  reset_tables(conninfo);
  const auto stream = one_fill_stream();
  assert(stream.size() >= 2);
  const std::size_t split = stream.size() / 2;
  {
    qts::PgEventStore store{conninfo};
    for (std::size_t i = 0; i < split; ++i) store.apply(stream[i]);
    assert(store.latest_event_id() == stream[split - 1].envelope.event_id);
  }
  {
    qts::PgEventStore store{conninfo};
    const auto water = store.latest_event_id();
    std::size_t applied = 0;
    for (const auto& event : stream) {
      if (event.envelope.event_id > water) {
        store.apply(event);
        ++applied;
      }
    }
    assert(applied == stream.size() - split);
    assert(store.counts().events == stream.size());
    assert(store.latest_event_id() == stream.back().envelope.event_id);
  }
}

void pg_factory_port_and_async_sink(const std::string& conninfo) {
  reset_tables(conninfo);
  const auto stream = one_fill_stream();
  {
    std::unique_ptr<qts::EventStore> port = qts::open_event_store("", conninfo);
    assert(port != nullptr && port->latest_event_id() == 0);
    qts::AsyncEventSink sink{std::move(port)};
    for (const auto& event : stream) sink.push(event);
    sink.wait_empty();
    sink.shutdown();
  }
  qts::PgEventStore store{conninfo};
  assert(store.counts().events == stream.size());
  assert(store.counts().orders == 1);
  assert(store.strategy_order_counts().size() == 1);
  assert(store.strategy_order_counts()[0].first == "test");
  assert(store.positions().size() == 1);
}

void pg_positions_split_by_strategy(const std::string& conninfo) {
  reset_tables(conninfo);
  qts::EventBus events;
  qts::SimulatedBroker broker{events};
  qts::RiskEngine risk{{10'000, 0, true}};
  qts::InMemoryEventLedger ledger{};
  qts::OrderManager oms{broker, risk, events, &ledger};
  oms.set_trade_date("2026-10-10");
  broker.set_reference_price("TEST", 1000);
  auto buy_s1 = qts::OrderIntent{"pg-pos-1", "paper", "s1", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, 100, 1000};
  buy_s1.trace_id = "pg-pos-1";
  auto buy_s2 = qts::OrderIntent{"pg-pos-2", "paper", "s2", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, 100, 2000};
  buy_s2.trace_id = "pg-pos-2";
  auto sell_s1 = qts::OrderIntent{"pg-pos-3", "paper", "s1", "TEST", qts::Side::sell, qts::OrderType::limit, qts::TimeInForce::day, 100, 1500};
  sell_s1.trace_id = "pg-pos-3";
  assert(oms.submit(buy_s1));
  broker.set_reference_price("TEST", 2000);
  assert(oms.submit(buy_s2));
  broker.set_reference_price("TEST", 1500);
  assert(oms.submit(sell_s1));
  {
    qts::PgEventStore store{conninfo};
    for (auto event : ledger.replay(1)) store.apply(event);
    const auto rows = store.positions();
    assert(rows.size() == 2);
    assert(rows[0].strategy_id == "s1" && rows[0].quantity == 0 && rows[0].realized_pnl == 50'000);
    assert(rows[1].strategy_id == "s2" && rows[1].quantity == 100 && rows[1].average_cost == 2000 && rows[1].realized_pnl == 0);
    for (auto event : ledger.replay(1)) store.apply(event);
    const auto replayed = store.positions();
    assert(replayed.size() == 2 && replayed[0].realized_pnl == 50'000 && replayed[1].quantity == 100);
    const auto counts = store.strategy_order_counts();
    assert(counts.size() == 2);
  }
  // Reopen rebuilds the books from the events payloads (crash-gap heal).
  {
    qts::PgEventStore store{conninfo};
    const auto rows = store.positions();
    assert(rows.size() == 2 && rows[0].realized_pnl == 50'000 && rows[1].quantity == 100);
  }
}

void pg_fee_and_funding(const std::string& conninfo) {
  reset_tables(conninfo);
  auto stream = one_fill_stream();
  for (auto& event : stream) {
    if (event.snapshot.last_quantity > 0) event.snapshot.fee = 1234;
  }
  std::int64_t fills = 0;
  for (const auto& event : stream) {
    if (event.snapshot.last_quantity > 0) ++fills;
  }
  {
    qts::PgEventStore store{conninfo};
    for (const auto& event : stream) store.apply(event);
  }
  assert(query_sum_fee(conninfo) == fills * 1234);

  qts::LedgerEvent cash{};
  cash.type = qts::LedgerEventType::funding_deposited;
  cash.envelope.event_id = 1'000'001;
  cash.envelope.trace_id = "funding";
  cash.funding.cash = 10'000'000;
  qts::LedgerEvent deposit{};
  deposit.type = qts::LedgerEventType::funding_deposited;
  deposit.envelope.event_id = 1'000'002;
  deposit.envelope.trace_id = "funding";
  deposit.funding.instrument = "TEST";
  deposit.funding.quantity = 10'000;
  deposit.funding.average_cost = 1'000;
  {
    qts::PgEventStore store{conninfo};
    store.apply(cash);
    store.apply(deposit);
    const auto rows = store.positions();
    bool found = false;
    for (const auto& row : rows) {
      if (row.strategy_id == "passthrough" && row.instrument_id == "TEST") {
        assert(row.quantity == 10'000 && row.average_cost == 1'000 && row.realized_pnl == 0);
        found = true;
      }
    }
    assert(found);
    store.apply(cash);
    store.apply(deposit);
  }
}

// Cross-backend conformance: the same stream projects identically in
// SQLite and PostgreSQL (counts, strategy split, positions).
void pg_matches_sqlite(const std::string& conninfo) {
  reset_tables(conninfo);
  qts::EventBus events;
  qts::SimulatedBroker broker{events};
  qts::RiskEngine risk{{10'000, 0, true}};
  qts::InMemoryEventLedger ledger{};
  qts::OrderManager oms{broker, risk, events, &ledger};
  broker.set_reference_price("TEST", 1000);
  auto buy = qts::OrderIntent{"pg-x-1", "paper", "s1", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, 100, 1000};
  buy.trace_id = "pg-x-1";
  auto sell = qts::OrderIntent{"pg-x-2", "paper", "s1", "TEST", qts::Side::sell, qts::OrderType::limit, qts::TimeInForce::day, 50, 1500};
  sell.trace_id = "pg-x-2";
  assert(oms.submit(buy));
  broker.set_reference_price("TEST", 1500);
  assert(oms.submit(sell));
  const auto stream = ledger.replay(1);

  const auto path = std::filesystem::temp_directory_path() / "qts-pg-x.db";
  std::error_code ec;
  std::filesystem::remove(path, ec);
  qts::SqliteEventStore lite{path.string()};
  qts::PgEventStore pg{conninfo};
  for (const auto& event : stream) {
    lite.apply(event);
    pg.apply(event);
  }
  const auto lite_counts = lite.counts();
  const auto pg_counts = pg.counts();
  assert(lite_counts.events == pg_counts.events && lite_counts.orders == pg_counts.orders && lite_counts.executions == pg_counts.executions);
  assert(lite.latest_event_id() == pg.latest_event_id());
  const auto lite_strategies = lite.strategy_order_counts();
  const auto pg_strategies = pg.strategy_order_counts();
  assert(lite_strategies.size() == pg_strategies.size());
  for (std::size_t i = 0; i < lite_strategies.size(); ++i) assert(lite_strategies[i] == pg_strategies[i]);
  const auto lite_rows = lite.positions();
  const auto pg_rows = pg.positions();
  assert(lite_rows.size() == pg_rows.size());
  for (std::size_t i = 0; i < lite_rows.size(); ++i) {
    assert(lite_rows[i].strategy_id == pg_rows[i].strategy_id && lite_rows[i].instrument_id == pg_rows[i].instrument_id && lite_rows[i].quantity == pg_rows[i].quantity &&
           lite_rows[i].average_cost == pg_rows[i].average_cost && lite_rows[i].realized_pnl == pg_rows[i].realized_pnl);
  }
  std::filesystem::remove(path, ec);
}

}  // namespace

int main() {
  const std::string conninfo = test_conninfo();
  if (conninfo.empty()) {
    std::cout << "qts_test_pg skipped: QTS_PG_TEST_CONNINFO is not set (needs an empty database).\n";
    return 0;
  }
  pg_applies_idempotently(conninfo);
  pg_watermark_resume_catches_up(conninfo);
  pg_factory_port_and_async_sink(conninfo);
  pg_positions_split_by_strategy(conninfo);
  pg_fee_and_funding(conninfo);
  pg_matches_sqlite(conninfo);
  std::cout << "qts_test_pg: all conformance cases passed.\n";
  return 0;
}
