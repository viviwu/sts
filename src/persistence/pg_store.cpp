// clang-format off
/**
  ******************************************************************************
  * @file           : pg_store.cpp
  * @author         : vivi wu
  * @brief          : PostgreSQL backend: libpq implementation behind QTS_ENABLE_PG, fail-fast stub otherwise.
  * @version        : 0.1.0
  * @date           : 10/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/persistence/pg_store.hpp"

#ifdef QTS_HAS_LIBPQ
  #include <libpq-fe.h>
#endif

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"
#include "qts/portfolio/portfolio.hpp"

namespace qts {

#ifdef QTS_HAS_LIBPQ
namespace {

using nlohmann::json;

// libpq errors never echo the conninfo (the DSN may carry a password).
std::string last_error(PGconn* conn) {
  const char* raw = PQerrorMessage(conn);
  std::string out = raw != nullptr ? raw : "unknown libpq error";
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
  return out;
}

void check_result(PGconn* conn, PGresult* result, ExecStatusType ok, const char* what) {
  const bool good = result != nullptr && PQresultStatus(result) == ok;
  const std::string detail = good ? "" : last_error(conn);
  PQclear(result);
  if (!good) throw std::runtime_error(std::string("postgres ") + what + ": " + detail);
}

void exec_sql(PGconn* conn, const char* sql, const char* what) { check_result(conn, PQexec(conn, sql), PGRES_COMMAND_OK, what); }

// All binds go as text ($1..$n, null-terminated): every column type here
// has a text input cast, so no binary-format bookkeeping. Returns the
// command-rows count ("1" first sight, "0" re-apply converging).
bool run_params(PGconn* conn, const char* sql, const std::vector<std::string>& params, const char* what) {
  std::vector<const char*> values;
  values.reserve(params.size());
  for (const auto& entry : params) values.push_back(entry.c_str());
  PGresult* result = PQexecParams(conn, sql, static_cast<int>(params.size()), nullptr, values.empty() ? nullptr : values.data(), nullptr, nullptr, 0);
  const bool good = result != nullptr && PQresultStatus(result) == PGRES_COMMAND_OK;
  const bool changed = good && std::string(PQcmdTuples(result)) == "1";
  const std::string detail = good ? "" : last_error(conn);
  PQclear(result);
  if (!good) throw std::runtime_error(std::string("postgres ") + what + ": " + detail);
  return changed;
}

std::uint64_t query_u64(PGconn* conn, const char* sql, const char* what) {
  PGresult* result = PQexec(conn, sql);
  const bool good = result != nullptr && PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1;
  std::uint64_t value = 0;
  if (good) value = static_cast<std::uint64_t>(std::stoull(PQgetvalue(result, 0, 0)));
  const std::string detail = good ? "" : last_error(conn);
  PQclear(result);
  if (!good) throw std::runtime_error(std::string("postgres ") + what + ": " + detail);
  return value;
}

// Same tables/columns as the SQLite backend (BIGINT for INTEGER primary
// keys and counters; payload stays TEXT so both backends store identical
// bytes). Fresh databases self-migrate on open — no separate migration
// job; there are no pre-column PG databases (the stub never wrote any).
constexpr const char* kSchemaEvents =
    "CREATE TABLE IF NOT EXISTS events(event_id BIGINT PRIMARY KEY, type TEXT NOT NULL, trace_id TEXT NOT NULL, payload TEXT NOT NULL, trade_date TEXT NOT NULL DEFAULT '');";
constexpr const char* kSchemaOrders =
    "CREATE TABLE IF NOT EXISTS orders(client_order_id TEXT PRIMARY KEY, status INTEGER NOT NULL, cumulative_quantity BIGINT NOT NULL, last_event_id BIGINT NOT NULL, "
    "strategy_id TEXT NOT NULL DEFAULT '', trade_date TEXT NOT NULL DEFAULT '');";
constexpr const char* kSchemaExecutions =
    "CREATE TABLE IF NOT EXISTS executions(event_id BIGINT PRIMARY KEY, client_order_id TEXT NOT NULL, status INTEGER NOT NULL, last_quantity BIGINT NOT NULL, last_price BIGINT NOT NULL, "
    "fee BIGINT NOT NULL DEFAULT 0, strategy_id TEXT NOT NULL DEFAULT '', trade_date TEXT NOT NULL DEFAULT '');";
constexpr const char* kSchemaPositions =
    "CREATE TABLE IF NOT EXISTS positions(strategy_id TEXT NOT NULL, instrument_id TEXT NOT NULL, quantity BIGINT NOT NULL, average_cost BIGINT NOT NULL, realized_pnl BIGINT NOT NULL, "
    "last_event_id BIGINT NOT NULL, PRIMARY KEY(strategy_id, instrument_id));";

void migrate(PGconn* conn) {
  exec_sql(conn, kSchemaEvents, "migrate events");
  exec_sql(conn, kSchemaOrders, "migrate orders");
  exec_sql(conn, kSchemaExecutions, "migrate executions");
  exec_sql(conn, kSchemaPositions, "migrate positions");
}

// Strategy backfill from the stored JSON payload (mirrors the SQLite
// backend; a no-op on fresh databases, kept for backend symmetry).
void backfill_strategy_ids(PGconn* conn) {
  exec_sql(conn,
           "UPDATE orders SET strategy_id = COALESCE((SELECT e.payload::jsonb -> 'intent' ->> 'strategy_id' FROM events e "
           "WHERE e.payload::jsonb -> 'snapshot' ->> 'client_order_id' = orders.client_order_id ORDER BY e.event_id DESC LIMIT 1), '') "
           "WHERE strategy_id = '';",
           "backfill orders");
  exec_sql(conn,
           "UPDATE executions SET strategy_id = COALESCE((SELECT e.payload::jsonb -> 'intent' ->> 'strategy_id' FROM events e "
           "WHERE e.event_id = executions.event_id), '') WHERE strategy_id = '';",
           "backfill executions");
}

using Books = std::map<std::string, Portfolio>;

void upsert_position_row(PGconn* conn, const std::string& basket, const std::string& instrument, const PositionView& view, Price realized, std::uint64_t event_id) {
  run_params(conn,
             "INSERT INTO positions(strategy_id,instrument_id,quantity,average_cost,realized_pnl,last_event_id) VALUES($1,$2,$3,$4,$5,$6) "
             "ON CONFLICT(strategy_id,instrument_id) DO UPDATE SET quantity=excluded.quantity, average_cost=excluded.average_cost, "
             "realized_pnl=excluded.realized_pnl, last_event_id=excluded.last_event_id WHERE excluded.last_event_id > positions.last_event_id;",
             {basket, instrument, std::to_string(view.total), std::to_string(view.average_cost), std::to_string(realized), std::to_string(event_id)}, "upsert positions");
}

// One fill feeds exactly one strategy basket and upserts its positions row
// (guarded by last_event_id, so replays converge). Shared shape with the
// SQLite backend; only the row writer differs (backend-owned SQL).
void project_fill(PGconn* conn, Books& books, const std::string& strategy, const std::string& instrument, Side side, Quantity quantity, Price price, std::uint64_t event_id) {
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
  upsert_position_row(conn, basket, instrument, view, iter->second.realized_for(basket), event_id);
}

// Startup funding lands as settled inventory: immediately sellable, cash
// untouched — the same semantics as broker-side fund_settled. Cash legs
// (quantity == 0) carry no position and only persist in the events payload.
void project_funding(PGconn* conn, Books& books, const std::string& strategy, const std::string& instrument, Quantity quantity, Price average_cost, std::uint64_t event_id) {
  if (quantity <= 0 || average_cost < 0 || instrument.empty()) return;
  const std::string basket = strategy.empty() ? kPassthroughStrategy : strategy;
  auto iter = books.find(basket);
  if (iter == books.end()) iter = books.emplace(basket, Portfolio{0}).first;
  iter->second.fund_settled(basket, instrument, quantity, average_cost);
  const PositionView view = iter->second.position_for(basket, instrument);
  upsert_position_row(conn, basket, instrument, view, iter->second.realized_for(basket), event_id);
}

// Rebuilds the positions projection from the stored event payloads (open
// time): heals crash gaps between ledger append and projection push.
void rebuild_positions(PGconn* conn, Books& books) {
  PGresult* scan = PQexec(conn, "SELECT event_id, payload FROM events ORDER BY event_id;");
  const bool good = scan != nullptr && PQresultStatus(scan) == PGRES_TUPLES_OK;
  if (!good) {
    const std::string detail = last_error(conn);
    PQclear(scan);
    throw std::runtime_error(std::string("postgres rebuild scan: ") + detail);
  }
  const int rows = PQntuples(scan);
  for (int i = 0; i < rows; ++i) {
    const auto event_id = static_cast<std::uint64_t>(std::stoull(PQgetvalue(scan, i, 0)));
    json node;
    try {
      node = json::parse(PQgetvalue(scan, i, 1));
    } catch (const json::exception& e) {
      PQclear(scan);
      throw std::runtime_error(std::string("corrupt event payload at event ") + std::to_string(event_id) + ": " + e.what());
    }
    if (node.value("type", std::string{}) != "execution_applied" && node.value("type", std::string{}) != "funding_deposited") continue;
    if (node.value("type", std::string{}) == "funding_deposited") {
      project_funding(conn, books, node.value("/funding/strategy_id"_json_pointer, std::string{}), node.value("/funding/instrument_id"_json_pointer, std::string{}),
                      node.value("/funding/quantity"_json_pointer, Quantity{0}), node.value("/funding/average_cost"_json_pointer, Price{0}), event_id);
      continue;
    }
    const Quantity quantity = node.value("/snapshot/last_quantity"_json_pointer, Quantity{0});
    if (quantity <= 0) continue;
    const int side_raw = node.value("/intent/side"_json_pointer, -1);
    if (side_raw != 0 && side_raw != 1) continue;
    project_fill(conn, books, node.value("/intent/strategy_id"_json_pointer, std::string{}), node.value("/intent/instrument_id"_json_pointer, std::string{}), static_cast<Side>(side_raw), quantity,
                 node.value("/snapshot/last_price"_json_pointer, Price{0}), event_id);
  }
  PQclear(scan);
}

}  // namespace

struct PgEventStore::Impl {
  PGconn* conn{nullptr};
  // Per-strategy basket books backing the positions projection. Fed exactly
  // once per event_id (guarded by the events INSERT row count), rebuilt
  // from the events table on open.
  std::map<std::string, Portfolio> books;
};

// Session resilience: the projection worker owns one connection; a dropped
// session re-establishes here and the watermark resume above it converges
// (no prepared statements, no open transactions, books survive in Impl).
void ensure_live(PGconn* conn) {
  if (PQstatus(conn) == CONNECTION_OK) return;
  PQreset(conn);
  if (PQstatus(conn) != CONNECTION_OK) throw std::runtime_error("postgres connection lost: " + last_error(conn));
}

PgEventStore::PgEventStore(std::string conninfo)
    : conninfo_(std::move(conninfo)),
      impl_{std::make_unique<Impl>()} {
  if (conninfo_.empty()) throw std::invalid_argument("postgres backend requires a non-empty conninfo");
  impl_->conn = PQconnectdb(conninfo_.c_str());
  if (PQstatus(impl_->conn) != CONNECTION_OK) {
    const std::string detail = last_error(impl_->conn);
    PQfinish(impl_->conn);
    impl_->conn = nullptr;
    throw std::runtime_error("postgres projection backend unavailable: " + detail);
  }
  migrate(impl_->conn);
  backfill_strategy_ids(impl_->conn);
  rebuild_positions(impl_->conn, impl_->books);
}

PgEventStore::~PgEventStore() {
  if (impl_ != nullptr && impl_->conn != nullptr) PQfinish(impl_->conn);
}

std::uint64_t PgEventStore::latest_event_id() {
  std::scoped_lock lock{mutex_};
  ensure_live(impl_->conn);
  return query_u64(impl_->conn, "SELECT COALESCE(MAX(event_id),0) FROM events;", "watermark");
}

void PgEventStore::apply(const LedgerEvent& event) {
  std::scoped_lock lock{mutex_};
  ensure_live(impl_->conn);
  PGconn* conn = impl_->conn;
  const std::string payload = serialize_event_payload(event);
  const std::string type = event_type_name(event.type);
  const auto event_id = event.envelope.event_id;

  const bool first_sight = run_params(conn, "INSERT INTO events(event_id,type,trace_id,payload,trade_date) VALUES($1,$2,$3,$4,$5) ON CONFLICT DO NOTHING;",
                                      {std::to_string(event_id), type, event.envelope.trace_id, payload, event.envelope.trade_date}, "insert events");
  // Exactly-once book feeding per event_id: re-applied streams (rebuilds,
  // watermark resumes) converge through the SQL upserts above, while the
  // stateful basket books only move on first sight.
  if (event.type == LedgerEventType::execution_applied || event.type == LedgerEventType::order_submitted) {
    run_params(conn,
               "INSERT INTO orders(client_order_id,status,cumulative_quantity,last_event_id,strategy_id,trade_date) VALUES($1,$2,$3,$4,$5,$6) "
               "ON CONFLICT(client_order_id) DO UPDATE SET status=excluded.status, cumulative_quantity=excluded.cumulative_quantity, "
               "last_event_id=excluded.last_event_id, strategy_id=excluded.strategy_id, trade_date=excluded.trade_date "
               "WHERE excluded.last_event_id > orders.last_event_id;",
               {event.snapshot.client_order_id, std::to_string(static_cast<int>(event.snapshot.status)), std::to_string(event.snapshot.cumulative_quantity), std::to_string(event_id),
                event.intent.strategy_id, event.envelope.trade_date},
               "upsert orders");
  }

  if (event.type == LedgerEventType::execution_applied && event.snapshot.last_quantity > 0) {
    run_params(conn, "INSERT INTO executions(event_id,client_order_id,status,last_quantity,last_price,fee,strategy_id,trade_date) VALUES($1,$2,$3,$4,$5,$6,$7,$8) ON CONFLICT DO NOTHING;",
               {std::to_string(event_id), event.snapshot.client_order_id, std::to_string(static_cast<int>(event.snapshot.status)), std::to_string(event.snapshot.last_quantity),
                std::to_string(event.snapshot.last_price), std::to_string(event.snapshot.fee), event.intent.strategy_id, event.envelope.trade_date},
               "insert executions");
    if (first_sight) {
      project_fill(conn, impl_->books, event.intent.strategy_id, event.intent.instrument_id, event.intent.side, event.snapshot.last_quantity, event.snapshot.last_price, event_id);
    }
  }

  if (event.type == LedgerEventType::funding_deposited && first_sight) {
    project_funding(conn, impl_->books, event.funding.strategy, event.funding.instrument, event.funding.quantity, event.funding.average_cost, event_id);
  }
}

EventStore::Counts PgEventStore::counts() {
  std::scoped_lock lock{mutex_};
  ensure_live(impl_->conn);
  Counts out{};
  out.events = query_u64(impl_->conn, "SELECT COUNT(*) FROM events;", "count events");
  out.orders = query_u64(impl_->conn, "SELECT COUNT(*) FROM orders;", "count orders");
  out.executions = query_u64(impl_->conn, "SELECT COUNT(*) FROM executions;", "count executions");
  return out;
}

std::vector<std::pair<std::string, std::uint64_t>> PgEventStore::strategy_order_counts() {
  std::scoped_lock lock{mutex_};
  ensure_live(impl_->conn);
  PGresult* result = PQexec(impl_->conn, "SELECT strategy_id, COUNT(*) FROM orders GROUP BY strategy_id ORDER BY strategy_id;");
  const bool good = result != nullptr && PQresultStatus(result) == PGRES_TUPLES_OK;
  if (!good) {
    const std::string detail = last_error(impl_->conn);
    PQclear(result);
    throw std::runtime_error(std::string("postgres strategy counts: ") + detail);
  }
  std::vector<std::pair<std::string, std::uint64_t>> out;
  const int rows = PQntuples(result);
  for (int i = 0; i < rows; ++i) out.emplace_back(PQgetvalue(result, i, 0), static_cast<std::uint64_t>(std::stoull(PQgetvalue(result, i, 1))));
  PQclear(result);
  return out;
}

std::vector<EventStore::PositionRow> PgEventStore::positions() {
  std::scoped_lock lock{mutex_};
  ensure_live(impl_->conn);
  PGresult* result = PQexec(impl_->conn, "SELECT strategy_id, instrument_id, quantity, average_cost, realized_pnl FROM positions ORDER BY strategy_id, instrument_id;");
  const bool good = result != nullptr && PQresultStatus(result) == PGRES_TUPLES_OK;
  if (!good) {
    const std::string detail = last_error(impl_->conn);
    PQclear(result);
    throw std::runtime_error(std::string("postgres positions: ") + detail);
  }
  std::vector<PositionRow> out;
  const int rows = PQntuples(result);
  for (int i = 0; i < rows; ++i) {
    PositionRow row{};
    row.strategy_id = PQgetvalue(result, i, 0);
    row.instrument_id = PQgetvalue(result, i, 1);
    row.quantity = static_cast<std::int64_t>(std::stoll(PQgetvalue(result, i, 2)));
    row.average_cost = static_cast<std::int64_t>(std::stoll(PQgetvalue(result, i, 3)));
    row.realized_pnl = static_cast<std::int64_t>(std::stoll(PQgetvalue(result, i, 4)));
    out.push_back(row);
  }
  PQclear(result);
  return out;
}

#else

namespace {

[[noreturn]] void unavailable() {
  throw std::runtime_error(
      "postgres projection backend is not wired in this build (formal phase): rebuild with -DQTS_ENABLE_PG=ON and system libpq "
      "(see ADR-038), or point the deployment at SQLite (sqlite_file) until then");
}

}  // namespace

struct PgEventStore::Impl {};

PgEventStore::PgEventStore(std::string conninfo)
    : conninfo_(std::move(conninfo)),
      impl_{std::make_unique<Impl>()} {
  if (conninfo_.empty()) throw std::invalid_argument("postgres backend requires a non-empty conninfo");
  unavailable();
}

PgEventStore::~PgEventStore() = default;

std::uint64_t PgEventStore::latest_event_id() { unavailable(); }

void PgEventStore::apply(const LedgerEvent& /*event*/) { unavailable(); }

EventStore::Counts PgEventStore::counts() { unavailable(); }

std::vector<std::pair<std::string, std::uint64_t>> PgEventStore::strategy_order_counts() { unavailable(); }

std::vector<EventStore::PositionRow> PgEventStore::positions() { unavailable(); }

#endif

}  // namespace qts
