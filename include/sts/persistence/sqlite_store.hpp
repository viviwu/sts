// clang-format off
/**
  ******************************************************************************
  * @file           : sqlite_store.hpp
  * @author         : vivi wu
  * @brief          : Async SQLite projection (events/orders/executions) with watermark resume.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "sts/broker/trade_api.hpp"
#include "sts/persistence/event_store.hpp"

namespace sts {

// Restartable SQLite read projection (M9-3, ADR-034). The JSONL ledger
// stays the recovery authority; this store is an asynchronous query
// projection rebuilt from event_id watermarks.
//
// Tables (portable standard SQL, PG swappable later):
//   events(event_id PK, type, trace_id, payload JSON, trade_date)
//   orders(client_order_id PK, status, cumulative_quantity, last_event_id, strategy_id, trade_date)
//   executions(event_id PK, client_order_id, status, last_quantity, last_price, strategy_id, trade_date)
//   positions(strategy_id, instrument_id PK, quantity, average_cost, realized_pnl, last_event_id)
// strategy_id comes from the order intent (empty normalizes to the
// pass-through basket); trade_date is the driver-stamped session label.
// positions accumulates per-strategy baskets from fills (T+1 sellable and
// marks are not projected); pre-column databases gain columns via ADD
// COLUMN, strategy rows backfill from JSON, trade_date has no historic
// source and stays '' (unknown), and positions rebuilds from events on open.
// All writes are idempotent: re-applying the same event stream (full
// rebuild or watermark resume) converges to the same rows.
// Restartable SQLite read projection (M9-3, ADR-034): the current
// EventStore backend. The JSONL ledger stays the recovery authority; this
// store is an asynchronous query projection rebuilt from event_id
// watermarks. See EventStore (event_store.hpp) for the backend contract
// the formal-phase PostgreSQL backend will share.
class SqliteEventStore final : public EventStore {
 public:
  explicit SqliteEventStore(std::string path);
  ~SqliteEventStore() override;

  SqliteEventStore(const SqliteEventStore&) = delete;
  SqliteEventStore& operator=(const SqliteEventStore&) = delete;
  SqliteEventStore(SqliteEventStore&&) = delete;
  SqliteEventStore& operator=(SqliteEventStore&&) = delete;

  // Highest projected event_id, 0 when the database is empty.
  [[nodiscard]] std::uint64_t latest_event_id() override;
  void apply(const LedgerEvent& event) override;

  [[nodiscard]] Counts counts() override;
  // Per-strategy order counts ordered by strategy_id (empty id means the
  // row predates strategy tracking and missed the JSON backfill).
  [[nodiscard]] std::vector<std::pair<std::string, std::uint64_t>> strategy_order_counts() override;
  // Current per-strategy positions ordered by (strategy_id, instrument_id).
  [[nodiscard]] std::vector<PositionRow> positions() override;
  [[nodiscard]] const std::string& path() const noexcept { return path_; }
  // Counter securities master cache (ADR-054): reference data, rebuildable
  // from a sweep; never the recovery authority. Single-writer discipline:
  // the server tick thread upserts drain batches; readers see batch
  // granularity (updated_at per batch).
  void upsert_instruments(const std::vector<SecurityInfoRow>& rows, const std::string& updated_at);
  [[nodiscard]] std::vector<SecurityInfoRow> cached_instruments();
  [[nodiscard]] std::uint64_t cached_instrument_count();

 private:
  struct Impl;
  std::string path_;
  std::unique_ptr<Impl> impl_;
  std::mutex mutex_;
};

}  // namespace sts
