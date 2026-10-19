// clang-format off
/**
  ******************************************************************************
  * @file           : sqlite_store.cpp
  * @author         : vivi wu
  * @brief          : SQLite store implementation: idempotent apply, watermark, worker queue.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/persistence/sqlite_store.hpp"

#include <sqlite3.h>

#include <map>
#include <stdexcept>

#include "nlohmann/json.hpp"
#include "qts/portfolio/portfolio.hpp"

namespace qts {
namespace {

using nlohmann::json;

void check(int result, sqlite3* database, const char* what) {
  if (result != SQLITE_OK && result != SQLITE_DONE && result != SQLITE_ROW) {
    throw std::runtime_error(std::string(what) + ": " + sqlite3_errmsg(database));
  }
}

void exec_all(sqlite3* database, const char* sql, const char* what) {
  char* message = nullptr;
  const int result = sqlite3_exec(database, sql, nullptr, nullptr, &message);
  const std::string error = message != nullptr ? message : "";
  sqlite3_free(message);
  if (result != SQLITE_OK) throw std::runtime_error(std::string(what) + ": " + error);
}

std::uint64_t query_single_u64(sqlite3* database, const char* sql) {
  sqlite3_stmt* stmt = nullptr;
  check(sqlite3_prepare_v2(database, sql, -1, &stmt, nullptr), database, "prepare");
  const int step = sqlite3_step(stmt);
  std::uint64_t value = 0;
  if (step == SQLITE_ROW) value = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0));
  check(sqlite3_finalize(stmt), database, "finalize");
  return value;
}

constexpr const char* kSchema =
    "CREATE TABLE IF NOT EXISTS events("
    "event_id INTEGER PRIMARY KEY, type TEXT NOT NULL, trace_id TEXT NOT NULL, payload TEXT NOT NULL, trade_date TEXT NOT NULL DEFAULT '');"
    "CREATE TABLE IF NOT EXISTS orders("
    "client_order_id TEXT PRIMARY KEY, status INTEGER NOT NULL, cumulative_quantity INTEGER NOT NULL, last_event_id INTEGER NOT NULL, "
    "strategy_id TEXT NOT NULL DEFAULT '', trade_date TEXT NOT NULL DEFAULT '');"
    "CREATE TABLE IF NOT EXISTS executions("
    "event_id INTEGER PRIMARY KEY, client_order_id TEXT NOT NULL, status INTEGER NOT NULL, last_quantity INTEGER NOT NULL, last_price INTEGER NOT NULL, fee INTEGER NOT NULL DEFAULT 0, "
    "strategy_id TEXT NOT NULL DEFAULT '', trade_date TEXT NOT NULL DEFAULT '');"
    "CREATE TABLE IF NOT EXISTS positions("
    "strategy_id TEXT NOT NULL, instrument_id TEXT NOT NULL, quantity INTEGER NOT NULL, average_cost INTEGER NOT NULL, realized_pnl INTEGER NOT NULL, "
    "last_event_id INTEGER NOT NULL, PRIMARY KEY(strategy_id, instrument_id));"
    "CREATE TABLE IF NOT EXISTS instruments_cache("
    "market TEXT NOT NULL, security_id TEXT NOT NULL, symbol TEXT NOT NULL DEFAULT '', security_type INTEGER NOT NULL DEFAULT 0, "
    "price_tick INTEGER NOT NULL DEFAULT 0, buy_unit INTEGER NOT NULL DEFAULT 0, sell_unit INTEGER NOT NULL DEFAULT 0, "
    "buy_lower INTEGER NOT NULL DEFAULT 0, sell_lower INTEGER NOT NULL DEFAULT 0, limit_up INTEGER NOT NULL DEFAULT 0, "
    "limit_down INTEGER NOT NULL DEFAULT 0, prev_close INTEGER NOT NULL DEFAULT 0, day_trading INTEGER NOT NULL DEFAULT 0, "
    "status INTEGER NOT NULL DEFAULT 0, updated_at TEXT NOT NULL DEFAULT '', PRIMARY KEY(market, security_id));";

// Pre-column databases keep working: the column arrives via ADD COLUMN.
// strategy_id rows additionally backfill from the stored JSON payload
// (which always carried the intent); trade_date has no historic source and
// stays '' (unknown) until the stream is re-projected.
void ensure_column(sqlite3* database, const char* table, const char* column) {
  sqlite3_stmt* stmt = nullptr;
  const std::string probe = "SELECT COUNT(*) FROM pragma_table_info('" + std::string(table) + "') WHERE name='" + column + "';";
  check(sqlite3_prepare_v2(database, probe.c_str(), -1, &stmt, nullptr), database, "prepare probe");
  const int step = sqlite3_step(stmt);
  std::uint64_t found = 0;
  if (step == SQLITE_ROW) found = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0));
  check(sqlite3_finalize(stmt), database, "finalize probe");
  if (found != 0) return;
  exec_all(database, ("ALTER TABLE " + std::string(table) + " ADD COLUMN " + column + " TEXT NOT NULL DEFAULT '';").c_str(), "add column");
}

// Integer variant for pre-fee databases: executions.fee arrives the same way.
void ensure_int_column(sqlite3* database, const char* table, const char* column) {
  sqlite3_stmt* stmt = nullptr;
  const std::string probe = "SELECT COUNT(*) FROM pragma_table_info('" + std::string(table) + "') WHERE name='" + column + "';";
  check(sqlite3_prepare_v2(database, probe.c_str(), -1, &stmt, nullptr), database, "prepare int probe");
  const int step = sqlite3_step(stmt);
  std::uint64_t found = 0;
  if (step == SQLITE_ROW) found = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0));
  check(sqlite3_finalize(stmt), database, "finalize int probe");
  if (found != 0) return;
  exec_all(database, ("ALTER TABLE " + std::string(table) + " ADD COLUMN " + column + " INTEGER NOT NULL DEFAULT 0;").c_str(), "add int column");
}

using Books = std::map<std::string, Portfolio>;

void upsert_position_row(sqlite3* database, const std::string& basket, const std::string& instrument, const PositionView& view, Price realized, std::uint64_t event_id) {
  sqlite3_stmt* upsert = nullptr;
  check(sqlite3_prepare_v2(database,
                           "INSERT INTO positions(strategy_id,instrument_id,quantity,average_cost,realized_pnl,last_event_id) VALUES(?,?,?,?,?,?) "
                           "ON CONFLICT(strategy_id,instrument_id) DO UPDATE SET quantity=excluded.quantity, average_cost=excluded.average_cost, "
                           "realized_pnl=excluded.realized_pnl, last_event_id=excluded.last_event_id WHERE excluded.last_event_id > positions.last_event_id;",
                           -1, &upsert, nullptr),
        database, "prepare positions");
  check(sqlite3_bind_text(upsert, 1, basket.c_str(), -1, SQLITE_TRANSIENT), database, "bind positions.strategy");
  check(sqlite3_bind_text(upsert, 2, instrument.c_str(), -1, SQLITE_TRANSIENT), database, "bind positions.instrument");
  check(sqlite3_bind_int64(upsert, 3, static_cast<sqlite3_int64>(view.total)), database, "bind positions.qty");
  check(sqlite3_bind_int64(upsert, 4, static_cast<sqlite3_int64>(view.average_cost)), database, "bind positions.avg");
  check(sqlite3_bind_int64(upsert, 5, static_cast<sqlite3_int64>(realized)), database, "bind positions.realized");
  check(sqlite3_bind_int64(upsert, 6, static_cast<sqlite3_int64>(event_id)), database, "bind positions.event");
  check(sqlite3_step(upsert), database, "step positions");
  check(sqlite3_finalize(upsert), database, "finalize positions");
}

// One fill feeds exactly one strategy basket and upserts its positions row
// (guarded by last_event_id, so replays converge). Shared by live apply()
// and open-time rebuild; both paths see fills in ledger order.
void project_fill(sqlite3* database, Books& books, const std::string& strategy, const std::string& instrument, Side side, Quantity quantity, Price price, std::uint64_t event_id) {
  if (quantity <= 0 || price < 0) return;
  const std::string basket = strategy.empty() ? kPassthroughStrategy : strategy;
  auto iter = books.find(basket);
  if (iter == books.end()) iter = books.emplace(basket, Portfolio{0}).first;
  OrderIntent intent{};
  intent.strategy_id = basket;
  intent.instrument_id = instrument;
  intent.side = side;
  iter->second.apply_fill(intent, quantity, price);
  const PositionView view = iter->second.position_for(basket, instrument);
  upsert_position_row(database, basket, instrument, view, iter->second.realized_for(basket), event_id);
}

// Startup funding (M11-7) lands as settled inventory: immediately sellable,
// cash untouched — the same semantics as broker-side fund_settled. Cash legs
// (quantity == 0) carry no position and only persist in the events payload.
void project_funding(sqlite3* database, Books& books, const std::string& strategy, const std::string& instrument, Quantity quantity, Price average_cost, std::uint64_t event_id) {
  if (quantity <= 0 || average_cost < 0 || instrument.empty()) return;
  const std::string basket = strategy.empty() ? kPassthroughStrategy : strategy;
  auto iter = books.find(basket);
  if (iter == books.end()) iter = books.emplace(basket, Portfolio{0}).first;
  iter->second.fund_settled(basket, instrument, quantity, average_cost);
  const PositionView view = iter->second.position_for(basket, instrument);
  upsert_position_row(database, basket, instrument, view, iter->second.realized_for(basket), event_id);
}

// Rebuilds the positions projection from the stored event payloads (open
// time): heals crash gaps between ledger append and sqlite push, and
// backfills the table for databases created before it existed.
void rebuild_positions(sqlite3* database, Books& books) {
  sqlite3_stmt* scan = nullptr;
  check(sqlite3_prepare_v2(database, "SELECT event_id, payload FROM events ORDER BY event_id;", -1, &scan, nullptr), database, "prepare rebuild scan");
  for (;;) {
    const int step = sqlite3_step(scan);
    if (step == SQLITE_DONE) break;
    check(step, database, "step rebuild scan");
    const auto event_id = static_cast<std::uint64_t>(sqlite3_column_int64(scan, 0));
    const auto* raw = sqlite3_column_text(scan, 1);
    json node;
    try {
      node = json::parse(static_cast<const char*>(static_cast<const void*>(raw)));
    } catch (const json::exception& e) {
      check(sqlite3_finalize(scan), database, "finalize rebuild scan");
      throw std::runtime_error(std::string("corrupt event payload at event ") + std::to_string(event_id) + ": " + e.what());
    }
    if (node.value("type", std::string{}) != "execution_applied" && node.value("type", std::string{}) != "funding_deposited") continue;
    if (node.value("type", std::string{}) == "funding_deposited") {
      project_funding(database, books, node.value("/funding/strategy_id"_json_pointer, std::string{}), node.value("/funding/instrument_id"_json_pointer, std::string{}),
                      node.value("/funding/quantity"_json_pointer, Quantity{0}), node.value("/funding/average_cost"_json_pointer, Price{0}), event_id);
      continue;
    }
    const Quantity quantity = node.value("/snapshot/last_quantity"_json_pointer, Quantity{0});
    if (quantity <= 0) continue;
    const int side_raw = node.value("/intent/side"_json_pointer, -1);
    if (side_raw != 0 && side_raw != 1) continue;
    project_fill(database, books, node.value("/intent/strategy_id"_json_pointer, std::string{}), node.value("/intent/instrument_id"_json_pointer, std::string{}), static_cast<Side>(side_raw), quantity,
                 node.value("/snapshot/last_price"_json_pointer, Price{0}), event_id);
  }
  check(sqlite3_finalize(scan), database, "finalize rebuild scan");
}

void backfill_strategy_ids(sqlite3* database) {
  exec_all(database,
           "UPDATE orders SET strategy_id = COALESCE((SELECT json_extract(e.payload, '$.intent.strategy_id') FROM events e "
           "WHERE json_extract(e.payload, '$.snapshot.client_order_id') = orders.client_order_id ORDER BY e.event_id DESC LIMIT 1), '') "
           "WHERE strategy_id = '';",
           "backfill orders");
  exec_all(database,
           "UPDATE executions SET strategy_id = COALESCE((SELECT json_extract(e.payload, '$.intent.strategy_id') FROM events e "
           "WHERE e.event_id = executions.event_id), '') WHERE strategy_id = '';",
           "backfill executions");
}

}  // namespace

struct SqliteEventStore::Impl {
  sqlite3* db{nullptr};
  // Per-strategy basket books backing the positions projection. Fed exactly
  // once per event_id (guarded by INSERT OR IGNORE row changes), rebuilt
  // from the events table on open.
  std::map<std::string, Portfolio> books;
};

SqliteEventStore::SqliteEventStore(std::string path)
    : path_{std::move(path)},
      impl_{std::make_unique<Impl>()} {
  check(sqlite3_open(path_.c_str(), &impl_->db), impl_->db, "open sqlite db");
  exec_all(impl_->db, "PRAGMA journal_mode=WAL;", "wal");
  exec_all(impl_->db, "PRAGMA synchronous=NORMAL;", "synchronous");
  exec_all(impl_->db, kSchema, "migrate");
  ensure_column(impl_->db, "orders", "strategy_id");
  ensure_column(impl_->db, "executions", "strategy_id");
  backfill_strategy_ids(impl_->db);
  ensure_column(impl_->db, "events", "trade_date");
  ensure_column(impl_->db, "orders", "trade_date");
  ensure_column(impl_->db, "executions", "trade_date");
  ensure_int_column(impl_->db, "executions", "fee");
  rebuild_positions(impl_->db, impl_->books);
}

SqliteEventStore::~SqliteEventStore() {
  if (impl_ != nullptr && impl_->db != nullptr) sqlite3_close(impl_->db);
}

std::uint64_t SqliteEventStore::latest_event_id() {
  std::scoped_lock lock{mutex_};
  return query_single_u64(impl_->db, "SELECT COALESCE(MAX(event_id),0) FROM events;");
}

void SqliteEventStore::apply(const LedgerEvent& event) {
  std::scoped_lock lock{mutex_};
  sqlite3* database = impl_->db;
  const std::string payload = serialize_event_payload(event);
  const std::string type = event_type_name(event.type);
  const auto event_id = static_cast<sqlite3_int64>(event.envelope.event_id);

  sqlite3_stmt* insert_event = nullptr;
  check(sqlite3_prepare_v2(database, "INSERT OR IGNORE INTO events(event_id,type,trace_id,payload,trade_date) VALUES(?,?,?,?,?);", -1, &insert_event, nullptr), database, "prepare events");
  check(sqlite3_bind_int64(insert_event, 1, event_id), database, "bind events.id");
  check(sqlite3_bind_text(insert_event, 2, type.c_str(), -1, SQLITE_TRANSIENT), database, "bind events.type");
  check(sqlite3_bind_text(insert_event, 3, event.envelope.trace_id.c_str(), -1, SQLITE_TRANSIENT), database, "bind events.trace");
  check(sqlite3_bind_text(insert_event, 4, payload.c_str(), -1, SQLITE_TRANSIENT), database, "bind events.payload");
  check(sqlite3_bind_text(insert_event, 5, event.envelope.trade_date.c_str(), -1, SQLITE_TRANSIENT), database, "bind events.date");
  check(sqlite3_step(insert_event), database, "step events");
  check(sqlite3_finalize(insert_event), database, "finalize events");
  // Exactly-once book feeding per event_id: re-applied streams (rebuilds,
  // watermark resumes) converge through the SQL upserts above, while the
  // stateful basket books only move on first sight.
  const bool first_sight = sqlite3_changes(database) == 1;

  if (event.type == LedgerEventType::execution_applied || event.type == LedgerEventType::order_submitted) {
    sqlite3_stmt* upsert_order = nullptr;
    check(sqlite3_prepare_v2(database,
                             "INSERT INTO orders(client_order_id,status,cumulative_quantity,last_event_id,strategy_id,trade_date) VALUES(?,?,?,?,?,?) "
                             "ON CONFLICT(client_order_id) DO UPDATE SET status=excluded.status, cumulative_quantity=excluded.cumulative_quantity, "
                             "last_event_id=excluded.last_event_id, strategy_id=excluded.strategy_id, trade_date=excluded.trade_date "
                             "WHERE excluded.last_event_id > orders.last_event_id;",
                             -1, &upsert_order, nullptr),
          database, "prepare orders");
    check(sqlite3_bind_text(upsert_order, 1, event.snapshot.client_order_id.c_str(), -1, SQLITE_TRANSIENT), database, "bind orders.id");
    check(sqlite3_bind_int(upsert_order, 2, static_cast<int>(event.snapshot.status)), database, "bind orders.status");
    check(sqlite3_bind_int64(upsert_order, 3, static_cast<sqlite3_int64>(event.snapshot.cumulative_quantity)), database, "bind orders.cum");
    check(sqlite3_bind_int64(upsert_order, 4, event_id), database, "bind orders.event");
    check(sqlite3_bind_text(upsert_order, 5, event.intent.strategy_id.c_str(), -1, SQLITE_TRANSIENT), database, "bind orders.strategy");
    check(sqlite3_bind_text(upsert_order, 6, event.envelope.trade_date.c_str(), -1, SQLITE_TRANSIENT), database, "bind orders.date");
    check(sqlite3_step(upsert_order), database, "step orders");
    check(sqlite3_finalize(upsert_order), database, "finalize orders");
  }

  if (event.type == LedgerEventType::execution_applied && event.snapshot.last_quantity > 0) {
    sqlite3_stmt* insert_exec = nullptr;
    check(sqlite3_prepare_v2(database, "INSERT OR IGNORE INTO executions(event_id,client_order_id,status,last_quantity,last_price,fee,strategy_id,trade_date) VALUES(?,?,?,?,?,?,?,?);", -1,
                             &insert_exec, nullptr),
          database, "prepare executions");
    check(sqlite3_bind_int64(insert_exec, 1, event_id), database, "bind exec.id");
    check(sqlite3_bind_text(insert_exec, 2, event.snapshot.client_order_id.c_str(), -1, SQLITE_TRANSIENT), database, "bind exec.order");
    check(sqlite3_bind_int(insert_exec, 3, static_cast<int>(event.snapshot.status)), database, "bind exec.status");
    check(sqlite3_bind_int64(insert_exec, 4, static_cast<sqlite3_int64>(event.snapshot.last_quantity)), database, "bind exec.qty");
    check(sqlite3_bind_int64(insert_exec, 5, static_cast<sqlite3_int64>(event.snapshot.last_price)), database, "bind exec.price");
    check(sqlite3_bind_int64(insert_exec, 6, static_cast<sqlite3_int64>(event.snapshot.fee)), database, "bind exec.fee");
    check(sqlite3_bind_text(insert_exec, 7, event.intent.strategy_id.c_str(), -1, SQLITE_TRANSIENT), database, "bind exec.strategy");
    check(sqlite3_bind_text(insert_exec, 8, event.envelope.trade_date.c_str(), -1, SQLITE_TRANSIENT), database, "bind exec.date");
    check(sqlite3_step(insert_exec), database, "step executions");
    check(sqlite3_finalize(insert_exec), database, "finalize executions");
    if (first_sight) {
      project_fill(database, impl_->books, event.intent.strategy_id, event.intent.instrument_id, event.intent.side, event.snapshot.last_quantity, event.snapshot.last_price,
                   static_cast<std::uint64_t>(event_id));
    }
  }

  if (event.type == LedgerEventType::funding_deposited && first_sight) {
    project_funding(database, impl_->books, event.funding.strategy, event.funding.instrument, event.funding.quantity, event.funding.average_cost, static_cast<std::uint64_t>(event_id));
  }
}

SqliteEventStore::Counts SqliteEventStore::counts() {
  std::scoped_lock lock{mutex_};
  Counts out{};
  out.events = query_single_u64(impl_->db, "SELECT COUNT(*) FROM events;");
  out.orders = query_single_u64(impl_->db, "SELECT COUNT(*) FROM orders;");
  out.executions = query_single_u64(impl_->db, "SELECT COUNT(*) FROM executions;");
  return out;
}

std::vector<std::pair<std::string, std::uint64_t>> SqliteEventStore::strategy_order_counts() {
  std::scoped_lock lock{mutex_};
  sqlite3* database = impl_->db;
  sqlite3_stmt* stmt = nullptr;
  check(sqlite3_prepare_v2(database, "SELECT strategy_id, COUNT(*) FROM orders GROUP BY strategy_id ORDER BY strategy_id;", -1, &stmt, nullptr), database, "prepare strategy counts");
  std::vector<std::pair<std::string, std::uint64_t>> out;
  for (;;) {
    const int step = sqlite3_step(stmt);
    if (step == SQLITE_DONE) break;
    check(step, database, "step strategy counts");
    const auto* text = sqlite3_column_text(stmt, 0);
    out.emplace_back(text != nullptr ? static_cast<const char*>(static_cast<const void*>(text)) : "", static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 1)));
  }
  check(sqlite3_finalize(stmt), database, "finalize strategy counts");
  return out;
}

std::vector<SqliteEventStore::PositionRow> SqliteEventStore::positions() {
  std::scoped_lock lock{mutex_};
  sqlite3* database = impl_->db;
  sqlite3_stmt* stmt = nullptr;
  check(sqlite3_prepare_v2(database, "SELECT strategy_id, instrument_id, quantity, average_cost, realized_pnl FROM positions ORDER BY strategy_id, instrument_id;", -1, &stmt, nullptr), database,
        "prepare positions");
  std::vector<PositionRow> out;
  for (;;) {
    const int step = sqlite3_step(stmt);
    if (step == SQLITE_DONE) break;
    check(step, database, "step positions");
    const auto* strategy = sqlite3_column_text(stmt, 0);
    const auto* instrument = sqlite3_column_text(stmt, 1);
    PositionRow row{};
    row.strategy_id = strategy != nullptr ? static_cast<const char*>(static_cast<const void*>(strategy)) : "";
    row.instrument_id = instrument != nullptr ? static_cast<const char*>(static_cast<const void*>(instrument)) : "";
    row.quantity = sqlite3_column_int64(stmt, 2);
    row.average_cost = sqlite3_column_int64(stmt, 3);
    row.realized_pnl = sqlite3_column_int64(stmt, 4);
    out.push_back(row);
  }
  check(sqlite3_finalize(stmt), database, "finalize positions");
  return out;
}

// Reference-data cache (ADR-054): counter securities master. Projection,
// rebuildable from a sweep; never the recovery authority.
void SqliteEventStore::upsert_instruments(const std::vector<SecurityInfoRow>& rows, const std::string& updated_at) {
  std::scoped_lock lock{mutex_};
  sqlite3* database = impl_->db;
  exec_all(database, "BEGIN IMMEDIATE;", "begin instruments");
  try {
    sqlite3_stmt* upsert = nullptr;
    check(sqlite3_prepare_v2(database,
                             "INSERT INTO instruments_cache(market,security_id,symbol,security_type,price_tick,buy_unit,sell_unit,buy_lower,sell_lower,"
                             "limit_up,limit_down,prev_close,day_trading,status,updated_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
                             "ON CONFLICT(market,security_id) DO UPDATE SET symbol=excluded.symbol, security_type=excluded.security_type, price_tick=excluded.price_tick, "
                             "buy_unit=excluded.buy_unit, sell_unit=excluded.sell_unit, buy_lower=excluded.buy_lower, sell_lower=excluded.sell_lower, "
                             "limit_up=excluded.limit_up, limit_down=excluded.limit_down, prev_close=excluded.prev_close, day_trading=excluded.day_trading, "
                             "status=excluded.status, updated_at=excluded.updated_at;",
                             -1, &upsert, nullptr),
          database, "prepare instruments");
    for (const SecurityInfoRow& row : rows) {
      std::string market;
      std::string code = row.instrument_id;
      const std::size_t dot = code.rfind('.');
      if (dot != std::string::npos) {
        market = code.substr(dot + 1);
        code = code.substr(0, dot);
      }
      check(sqlite3_bind_text(upsert, 1, market.c_str(), -1, SQLITE_TRANSIENT), database, "bind instruments.market");
      check(sqlite3_bind_text(upsert, 2, code.c_str(), -1, SQLITE_TRANSIENT), database, "bind instruments.code");
      check(sqlite3_bind_text(upsert, 3, row.symbol.c_str(), -1, SQLITE_TRANSIENT), database, "bind instruments.symbol");
      check(sqlite3_bind_int64(upsert, 4, static_cast<sqlite3_int64>(row.security_type)), database, "bind instruments.type");
      check(sqlite3_bind_int64(upsert, 5, static_cast<sqlite3_int64>(row.price_tick)), database, "bind instruments.tick");
      check(sqlite3_bind_int64(upsert, 6, static_cast<sqlite3_int64>(row.buy_unit)), database, "bind instruments.buy_unit");
      check(sqlite3_bind_int64(upsert, 7, static_cast<sqlite3_int64>(row.sell_unit)), database, "bind instruments.sell_unit");
      check(sqlite3_bind_int64(upsert, 8, static_cast<sqlite3_int64>(row.buy_lower)), database, "bind instruments.buy_lower");
      check(sqlite3_bind_int64(upsert, 9, static_cast<sqlite3_int64>(row.sell_lower)), database, "bind instruments.sell_lower");
      check(sqlite3_bind_int64(upsert, 10, static_cast<sqlite3_int64>(row.limit_up)), database, "bind instruments.up");
      check(sqlite3_bind_int64(upsert, 11, static_cast<sqlite3_int64>(row.limit_down)), database, "bind instruments.down");
      check(sqlite3_bind_int64(upsert, 12, static_cast<sqlite3_int64>(row.prev_close)), database, "bind instruments.prev");
      check(sqlite3_bind_int64(upsert, 13, row.day_trading ? 1 : 0), database, "bind instruments.t0");
      check(sqlite3_bind_int64(upsert, 14, static_cast<sqlite3_int64>(row.status)), database, "bind instruments.status");
      check(sqlite3_bind_text(upsert, 15, updated_at.c_str(), -1, SQLITE_TRANSIENT), database, "bind instruments.updated");
      check(sqlite3_step(upsert), database, "step instruments");
      check(sqlite3_reset(upsert), database, "reset instruments");
    }
    check(sqlite3_finalize(upsert), database, "finalize instruments");
    exec_all(database, "COMMIT;", "commit instruments");
  } catch (...) {
    exec_all(database, "ROLLBACK;", "rollback instruments");
    throw;
  }
}

std::vector<SecurityInfoRow> SqliteEventStore::cached_instruments() {
  std::scoped_lock lock{mutex_};
  sqlite3* database = impl_->db;
  sqlite3_stmt* stmt = nullptr;
  check(sqlite3_prepare_v2(database,
                           "SELECT market,security_id,symbol,security_type,price_tick,buy_unit,sell_unit,buy_lower,sell_lower,"
                           "limit_up,limit_down,prev_close,day_trading,status FROM instruments_cache ORDER BY market,security_id;",
                           -1, &stmt, nullptr),
        database, "prepare instruments read");
  std::vector<SecurityInfoRow> out;
  for (;;) {
    const int step = sqlite3_step(stmt);
    if (step == SQLITE_DONE) break;
    check(step, database, "step instruments read");
    const auto* market = sqlite3_column_text(stmt, 0);
    const auto* code = sqlite3_column_text(stmt, 1);
    const auto* symbol = sqlite3_column_text(stmt, 2);
    const std::string market_text = market != nullptr ? static_cast<const char*>(static_cast<const void*>(market)) : "";
    const std::string code_text = code != nullptr ? static_cast<const char*>(static_cast<const void*>(code)) : "";
    SecurityInfoRow row{};
    row.instrument_id = market_text.empty() ? code_text : code_text + "." + market_text;
    row.symbol = symbol != nullptr ? static_cast<const char*>(static_cast<const void*>(symbol)) : "";
    row.security_type = static_cast<std::uint16_t>(sqlite3_column_int64(stmt, 3));
    row.price_tick = sqlite3_column_int64(stmt, 4);
    row.buy_unit = sqlite3_column_int64(stmt, 5);
    row.sell_unit = sqlite3_column_int64(stmt, 6);
    row.buy_lower = sqlite3_column_int64(stmt, 7);
    row.sell_lower = sqlite3_column_int64(stmt, 8);
    row.limit_up = sqlite3_column_int64(stmt, 9);
    row.limit_down = sqlite3_column_int64(stmt, 10);
    row.prev_close = sqlite3_column_int64(stmt, 11);
    row.day_trading = sqlite3_column_int64(stmt, 12) != 0;
    row.status = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 13));
    out.push_back(std::move(row));
  }
  check(sqlite3_finalize(stmt), database, "finalize instruments read");
  return out;
}

std::uint64_t SqliteEventStore::cached_instrument_count() {
  std::scoped_lock lock{mutex_};
  return query_single_u64(impl_->db, "SELECT COUNT(*) FROM instruments_cache;");
}

}  // namespace qts
