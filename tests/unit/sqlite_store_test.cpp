// clang-format off
/**
  ******************************************************************************
  * @file           : sqlite_store_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for SQLite projection, idempotency and watermark resume.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/persistence/sqlite_store.hpp"

#include <sqlite3.h>

#include <atomic>
#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

#include "qts/broker/simulated_broker.hpp"
#include "qts/market/quote_feed.hpp"
#include "qts/oms/order_manager.hpp"
#include "qts/ops/admin_view.hpp"
#include "qts/persistence/event_store.hpp"

namespace {

std::filesystem::path fresh_temp(const std::string& tag) {
  static std::atomic<unsigned> counter{0};
  const unsigned n = counter.fetch_add(1);
  const auto path = std::filesystem::temp_directory_path() / ("qts-sqlite-" + tag + "-" + std::to_string(n) + ".db");
  std::error_code ec;
  std::filesystem::remove(path, ec);
  return path;
}

void remove_file(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

void feed_is_deterministic_and_bounded() {
  qts::SimulatedQuoteFeed first{7, 1'000, 50, 800, 1'200};
  qts::SimulatedQuoteFeed second{7, 1'000, 50, 800, 1'200};
  std::vector<qts::Price> path_a;
  std::vector<qts::Price> path_b;
  for (std::uint64_t i = 1; i <= 200; ++i) {
    const auto quote_a = first.next("TEST", i);
    const auto quote_b = second.next("TEST", i);
    assert(quote_a.last_price == quote_b.last_price);
    assert(quote_a.last_price >= 800 && quote_a.last_price <= 1'200);
    assert(quote_a.quality == qts::QuoteQuality::valid);
    path_a.push_back(quote_a.last_price);
    path_b.push_back(quote_b.last_price);
  }
  qts::SimulatedQuoteFeed other{8, 1'000, 50, 800, 1'200};
  bool differs = false;
  for (std::uint64_t i = 1; i <= 200; ++i) {
    if (other.next("TEST", i).last_price != path_a[i - 1]) differs = true;
  }
  assert(differs);
  assert(path_b == path_a);
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
  qts::OrderIntent intent{"sqlite-1-" + std::to_string(tag++), "paper", "test", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, 100, 1'000};
  intent.trace_id = intent.client_order_id;
  assert(oms.submit(intent));
  const auto stream = ledger.replay(1);
  assert(!stream.empty());
  return stream;
}

void store_applies_idempotently() {
  const auto path = fresh_temp("idem");
  const auto stream = one_fill_stream();
  {
    qts::SqliteEventStore store{path.string()};
    assert(store.latest_event_id() == 0);
    for (const auto& event : stream) store.apply(event);
    const auto counts = store.counts();
    assert(counts.events == stream.size());
    assert(counts.orders == 1);
    assert(store.latest_event_id() == stream.back().envelope.event_id);
    // Re-applying the same stream converges (full rebuild is idempotent).
    for (const auto& event : stream) store.apply(event);
    const auto replayed = store.counts();
    assert(replayed.events == counts.events);
    assert(replayed.orders == counts.orders);
    assert(replayed.executions == counts.executions);
    assert(store.latest_event_id() == stream.back().envelope.event_id);
  }
  // Watermark survives reopen: resume above it yields nothing new.
  {
    qts::SqliteEventStore store{path.string()};
    assert(store.latest_event_id() == stream.back().envelope.event_id);
    assert(store.counts().events == stream.size());
  }
  remove_file(path);
}

void watermark_resume_catches_up() {
  const auto path = fresh_temp("resume");
  const auto stream = one_fill_stream();
  assert(stream.size() >= 2);
  const std::size_t split = stream.size() / 2;
  {
    qts::SqliteEventStore store{path.string()};
    for (std::size_t i = 0; i < split; ++i) store.apply(stream[i]);
    assert(store.latest_event_id() == stream[split - 1].envelope.event_id);
  }
  {
    qts::SqliteEventStore store{path.string()};
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
  remove_file(path);
}

void async_sink_drains_before_shutdown() {
  const auto path = fresh_temp("async");
  const auto stream = one_fill_stream();
  {
    auto store = qts::open_event_store(path.string(), "");
    assert(store != nullptr);
    qts::AsyncEventSink sink{std::move(store)};
    assert(sink.watermark() == 0);
    for (const auto& event : stream) sink.push(event);
    sink.wait_empty();
    sink.shutdown();
    sink.shutdown();  // idempotent
  }
  qts::SqliteEventStore store{path.string()};
  assert(store.counts().events == stream.size());
  assert(store.counts().orders == 1);
  remove_file(path);
}

// Backend selection (fail-closed): empty/empty disables, both set throws,
// unknown backends never silently fall back. The SQLite backend behind the
// port behaves identically to the concrete class (conformance).
void store_factory_selects_backends() {
  assert(qts::open_event_store("", "") == nullptr);
  bool threw = false;
  try {
    (void)qts::open_event_store("/tmp/qts-both.db", "host=db user=qts");
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
  // SQLite behind the port: full read surface works polymorphically.
  const auto path = fresh_temp("port");
  const auto stream = one_fill_stream();
  std::unique_ptr<qts::EventStore> port = qts::open_event_store(path.string(), "");
  assert(port != nullptr && port->latest_event_id() == 0);
  for (const auto& event : stream) port->apply(event);
  const qts::EventStore::Counts counts = port->counts();
  assert(counts.events == stream.size() && counts.orders == 1);
  assert(port->strategy_order_counts().size() == 1);
  assert(port->positions().size() == 1);
  // PostgreSQL stub fails fast with an actionable message, never silently.
  bool pg_threw = false;
  try {
    (void)qts::open_event_store("", "host=db user=qts dbname=qts");
  } catch (const std::runtime_error& e) {
    pg_threw = std::string{e.what()}.find("postgres") != std::string::npos;
  }
  assert(pg_threw);
  remove_file(path);
}

void admin_views_match_oms() {
  const auto stream = one_fill_stream();
  const auto views = qts::admin::summarize_orders(stream);
  assert(views.size() == 1);
  assert(views[0].status == qts::OrderStatus::filled);
  assert(views[0].cumulative_quantity == 100);
  assert(qts::admin::open_orders(stream).empty());

  qts::Portfolio book = qts::admin::rebuild_positions(stream, 1'000'000);
  const auto position = book.position("TEST");
  assert(position.total == 100);
  assert(book.cash() == 1'000'000 - 100 * 1'000);

  // A resting (unfilled) order shows up as open.
  qts::EventBus events;
  qts::SimulatedBroker broker{events};  // no reference price: stays live
  qts::RiskEngine risk{{10'000, 0, true}};
  qts::InMemoryEventLedger ledger{};
  qts::OrderManager oms{broker, risk, events, &ledger};
  qts::OrderIntent intent{"sqlite-open-1", "paper", "test", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, 100, 900};
  intent.trace_id = intent.client_order_id;
  assert(oms.submit(intent));
  const auto resting = ledger.replay(1);
  assert(qts::admin::open_orders(resting).size() == 1);
  assert(qts::admin::summarize_orders(resting)[0].status == qts::OrderStatus::live);
}

void strategy_id_is_recorded() {
  const auto path = fresh_temp("strategy");
  const auto stream = one_fill_stream();  // strategy_id "test" on every intent
  {
    qts::SqliteEventStore store{path.string()};
    for (const auto& event : stream) store.apply(event);
    const auto counts = store.strategy_order_counts();
    assert(counts.size() == 1);
    assert(counts[0].first == "test" && counts[0].second == 1);
  }
  remove_file(path);
}

void legacy_database_migrates_and_backfills() {
  // Simulate a pre-strategy_id database: old schema rows without the column.
  // The backfill reads strategy_id out of the stored JSON payload.
  const auto path = fresh_temp("legacy");
  {
    sqlite3* raw = nullptr;
    assert(sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK);
    const char* setup =
        "CREATE TABLE events(event_id INTEGER PRIMARY KEY, type TEXT NOT NULL, trace_id TEXT NOT NULL, payload TEXT NOT NULL);"
        "CREATE TABLE orders(client_order_id TEXT PRIMARY KEY, status INTEGER NOT NULL, cumulative_quantity INTEGER NOT NULL, last_event_id INTEGER NOT NULL);"
        "CREATE TABLE executions(event_id INTEGER PRIMARY KEY, client_order_id TEXT NOT NULL, status INTEGER NOT NULL, last_quantity INTEGER NOT NULL, "
        "last_price INTEGER NOT NULL);"
        "INSERT INTO events(event_id,type,trace_id,payload) VALUES(1,'execution_applied','t-leg-1','{\"event_id\":1,\"type\":\"execution_applied\","
        "\"intent\":{\"strategy_id\":\"legacy-strat\",\"instrument_id\":\"LEG\",\"side\":0},\"snapshot\":{\"client_order_id\":\"leg-1\",\"last_quantity\":100,"
        "\"last_price\":1000}}');"
        "INSERT INTO orders(client_order_id,status,cumulative_quantity,last_event_id) VALUES('leg-1',3,100,1);"
        "INSERT INTO executions(event_id,client_order_id,status,last_quantity,last_price) VALUES(1,'leg-1',3,100,1000);";
    char* message = nullptr;
    const int rc = sqlite3_exec(raw, setup, nullptr, nullptr, &message);
    assert(message == nullptr);
    assert(rc == SQLITE_OK);
    assert(sqlite3_close(raw) == SQLITE_OK);
  }
  {
    qts::SqliteEventStore store{path.string()};
    const auto counts = store.strategy_order_counts();
    assert(counts.size() == 1);
    assert(counts[0].first == "legacy-strat" && counts[0].second == 1);
    // Positions rebuild from the stored payloads on open (pre-table database).
    const auto rows = store.positions();
    assert(rows.size() == 1);
    assert(rows[0].strategy_id == "legacy-strat" && rows[0].instrument_id == "LEG");
    assert(rows[0].quantity == 100 && rows[0].average_cost == 1000 && rows[0].realized_pnl == 0);
    sqlite3* raw = nullptr;
    assert(sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK);
    sqlite3_stmt* stmt = nullptr;
    assert(sqlite3_prepare_v2(raw, "SELECT strategy_id FROM executions WHERE event_id=1;", -1, &stmt, nullptr) == SQLITE_OK);
    assert(sqlite3_step(stmt) == SQLITE_ROW);
    const auto* text = sqlite3_column_text(stmt, 0);
    assert(text != nullptr && std::string(reinterpret_cast<const char*>(text)) == "legacy-strat");
    assert(sqlite3_finalize(stmt) == SQLITE_OK);
    // Pre-fee databases migrate with zero fees.
    assert(sqlite3_prepare_v2(raw, "SELECT fee FROM executions WHERE event_id=1;", -1, &stmt, nullptr) == SQLITE_OK);
    assert(sqlite3_step(stmt) == SQLITE_ROW);
    assert(sqlite3_column_int64(stmt, 0) == 0);
    assert(sqlite3_finalize(stmt) == SQLITE_OK);
    assert(sqlite3_close(raw) == SQLITE_OK);
  }
  remove_file(path);
  remove_file(std::filesystem::path{path.string() + "-wal"});
  remove_file(std::filesystem::path{path.string() + "-shm"});
}

void execution_fee_is_recorded() {
  const auto path = fresh_temp("execfee");
  auto stream = one_fill_stream();
  for (auto& event : stream) {
    if (event.snapshot.last_quantity > 0) event.snapshot.fee = 1234;
  }
  {
    qts::SqliteEventStore store{path.string()};
    for (const auto& event : stream) store.apply(event);
    sqlite3* raw = nullptr;
    assert(sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK);
    sqlite3_stmt* stmt = nullptr;
    assert(sqlite3_prepare_v2(raw, "SELECT SUM(fee) FROM executions;", -1, &stmt, nullptr) == SQLITE_OK);
    assert(sqlite3_step(stmt) == SQLITE_ROW);
    const auto total = sqlite3_column_int64(stmt, 0);
    assert(sqlite3_finalize(stmt) == SQLITE_OK);
    assert(sqlite3_close(raw) == SQLITE_OK);
    std::int64_t fills = 0;
    for (const auto& event : stream) {
      if (event.snapshot.last_quantity > 0) ++fills;
    }
    assert(total == fills * 1234);
  }
  remove_file(path);
}

void positions_split_by_strategy_and_carry_trade_date() {
  qts::EventBus events;
  qts::SimulatedBroker broker{events};
  qts::RiskEngine risk{{10'000, 0, true}};
  qts::InMemoryEventLedger ledger{};
  qts::OrderManager oms{broker, risk, events, &ledger};
  oms.set_trade_date("2026-10-02");
  // Limit fills need crossing references: buys fill at/under limit, sells at/over.
  broker.set_reference_price("TEST", 1000);
  auto buy_s1 = qts::OrderIntent{"pos-s1", "paper", "s1", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, 100, 1000};
  buy_s1.trace_id = "pos-s1";
  auto buy_s2 = qts::OrderIntent{"pos-s2", "paper", "s2", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, 100, 2000};
  buy_s2.trace_id = "pos-s2";
  auto sell_s1 = qts::OrderIntent{"pos-s3", "paper", "s1", "TEST", qts::Side::sell, qts::OrderType::limit, qts::TimeInForce::day, 100, 1500};
  sell_s1.trace_id = "pos-s3";
  assert(oms.submit(buy_s1));
  broker.set_reference_price("TEST", 2000);
  assert(oms.submit(buy_s2));
  broker.set_reference_price("TEST", 1500);
  assert(oms.submit(sell_s1));

  const auto path = fresh_temp("positions");
  {
    qts::SqliteEventStore store{path.string()};
    for (auto event : ledger.replay(1)) store.apply(event);
    const auto rows = store.positions();
    assert(rows.size() == 2);
    // Ordered by (strategy_id, instrument_id): s1 flat with realized 50k, s2 holding 100 @ 2000.
    assert(rows[0].strategy_id == "s1" && rows[0].quantity == 0 && rows[0].realized_pnl == 50'000);
    assert(rows[1].strategy_id == "s2" && rows[1].quantity == 100 && rows[1].average_cost == 2000 && rows[1].realized_pnl == 0);
    // Re-applying converges (idempotent, no double-counted books).
    for (auto event : ledger.replay(1)) store.apply(event);
    const auto replayed = store.positions();
    assert(replayed.size() == 2 && replayed[0].realized_pnl == 50'000 && replayed[1].quantity == 100);
  }
  // Trade dates recorded on orders and executions.
  {
    sqlite3* raw = nullptr;
    assert(sqlite3_open(path.string().c_str(), &raw) == SQLITE_OK);
    sqlite3_stmt* stmt = nullptr;
    assert(sqlite3_prepare_v2(raw, "SELECT COUNT(*) FROM orders WHERE trade_date='2026-10-02';", -1, &stmt, nullptr) == SQLITE_OK);
    assert(sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int64(stmt, 0) == 3);
    assert(sqlite3_finalize(stmt) == SQLITE_OK);
    assert(sqlite3_prepare_v2(raw, "SELECT COUNT(*) FROM executions WHERE trade_date='2026-10-02' AND strategy_id IN ('s1','s2');", -1, &stmt, nullptr) == SQLITE_OK);
    assert(sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int64(stmt, 0) == 3);
    assert(sqlite3_finalize(stmt) == SQLITE_OK);
    assert(sqlite3_close(raw) == SQLITE_OK);
  }
  remove_file(path);
  remove_file(std::filesystem::path{path.string() + "-wal"});
  remove_file(std::filesystem::path{path.string() + "-shm"});
}

// M11-7: funding legs project settled inventory into the passthrough
// basket (immediately sellable), touch no order/execution rows, and
// converge on re-apply (no double funding).
void funding_projects_settled_positions() {
  qts::LedgerEvent cash{};
  cash.type = qts::LedgerEventType::funding_deposited;
  cash.envelope.event_id = 1;
  cash.envelope.trace_id = "funding";
  cash.funding.cash = 10'000'000;
  qts::LedgerEvent deposit{};
  deposit.type = qts::LedgerEventType::funding_deposited;
  deposit.envelope.event_id = 2;
  deposit.envelope.trace_id = "funding";
  deposit.funding.instrument = "TEST";
  deposit.funding.quantity = 10'000;
  deposit.funding.average_cost = 1'000;
  const auto path = fresh_temp("funding");
  {
    qts::SqliteEventStore store{path.string()};
    store.apply(cash);
    store.apply(deposit);
    const auto counts = store.counts();
    assert(counts.events == 2 && counts.orders == 0 && counts.executions == 0);
    const auto rows = store.positions();
    assert(rows.size() == 1);
    assert(rows[0].strategy_id == "passthrough" && rows[0].instrument_id == "TEST");
    assert(rows[0].quantity == 10'000 && rows[0].average_cost == 1'000 && rows[0].realized_pnl == 0);
    // Re-applying converges: settled funding is fed exactly once.
    store.apply(cash);
    store.apply(deposit);
    const auto replayed = store.positions();
    assert(replayed.size() == 1 && replayed[0].quantity == 10'000);
  }
  // Reopen rebuilds the books from the events payloads (crash-gap heal).
  {
    qts::SqliteEventStore store{path.string()};
    const auto rows = store.positions();
    assert(rows.size() == 1 && rows[0].quantity == 10'000 && rows[0].average_cost == 1'000);
  }
  remove_file(path);
  remove_file(std::filesystem::path{path.string() + "-wal"});
  remove_file(std::filesystem::path{path.string() + "-shm"});
}

// M11-7: admin replay sees the funded book — stream cash wins over the
// fallback argument, settled inventory is immediately sellable.
void admin_replay_sees_funded_book() {
  qts::LedgerEvent cash{};
  cash.type = qts::LedgerEventType::funding_deposited;
  cash.envelope.event_id = 1;
  cash.funding.cash = 10'000'000;
  qts::LedgerEvent deposit{};
  deposit.type = qts::LedgerEventType::funding_deposited;
  deposit.envelope.event_id = 2;
  deposit.funding.instrument = "TEST";
  deposit.funding.quantity = 10'000;
  deposit.funding.average_cost = 1'000;
  qts::LedgerEvent sell{};
  sell.type = qts::LedgerEventType::execution_applied;
  sell.envelope.event_id = 3;
  sell.intent.instrument_id = "TEST";
  sell.intent.side = qts::Side::sell;
  sell.snapshot.last_quantity = 100;
  sell.snapshot.last_price = 1'000;
  const std::vector<qts::LedgerEvent> stream{cash, deposit, sell};
  qts::Portfolio book = qts::admin::rebuild_positions(stream, 1'000'000);  // fallback ignored: stream funds 10M
  assert(book.cash() == 10'000'000 + 100 * 1'000);
  const auto position = book.position("TEST");
  assert(position.total == 9'900 && position.sellable == 9'900);
  assert(position.average_cost == 1'000);
}

}  // namespace

void instruments_cache_roundtrips_and_upserts() {
  const auto path = fresh_temp("instruments");
  qts::SqliteEventStore store{path.string()};
  assert(store.cached_instrument_count() == 0);
  qts::SecurityInfoRow sse{};
  sse.instrument_id = "600000.SSE";
  sse.symbol = "浦发银行";
  sse.security_type = 1;
  sse.price_tick = 100;
  sse.buy_unit = 100;
  sse.sell_unit = 100;
  sse.buy_lower = 100;
  sse.sell_lower = 100;
  sse.limit_up = 106700;
  sse.limit_down = 87300;
  sse.prev_close = 97000;
  qts::SecurityInfoRow szse{};
  szse.instrument_id = "000001.SZSE";
  szse.security_type = 1;
  szse.price_tick = 100;
  szse.buy_unit = 100;
  szse.sell_unit = 100;
  store.upsert_instruments({sse, szse}, "2026-10-16T00:00:00Z");
  assert(store.cached_instrument_count() == 2);
  // Re-upsert converges (daily band refresh is idempotent).
  sse.limit_up = 107000;
  store.upsert_instruments({sse}, "2026-10-17T00:00:00Z");
  assert(store.cached_instrument_count() == 2);
  const auto rows = store.cached_instruments();
  assert(rows.size() == 2);
  assert(rows[0].instrument_id == "600000.SSE");  // (market, code) order
  assert(rows[0].symbol == "浦发银行" && rows[0].limit_up == 107000 && rows[0].prev_close == 97000);
  assert(rows[1].instrument_id == "000001.SZSE");
  remove_file(path);
}

int main() {
  feed_is_deterministic_and_bounded();
  store_applies_idempotently();
  watermark_resume_catches_up();
  async_sink_drains_before_shutdown();
  store_factory_selects_backends();
  admin_views_match_oms();
  strategy_id_is_recorded();
  legacy_database_migrates_and_backfills();
  execution_fee_is_recorded();
  positions_split_by_strategy_and_carry_trade_date();
  funding_projects_settled_positions();
  admin_replay_sees_funded_book();
  instruments_cache_roundtrips_and_upserts();
  return 0;
}
