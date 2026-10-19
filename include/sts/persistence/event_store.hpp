// clang-format off
/**
  ******************************************************************************
  * @file           : event_store.hpp
  * @author         : vivi wu
  * @brief          : Query-projection store port, generic async sink and backend factory.
  * @version        : 0.1.0
  * @date           : 05/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "sts/persistence/ledger.hpp"

namespace sts {

// Query-projection store port (PG migration design, formal-db phase).
// The JSONL ledger stays the recovery authority; stores are restartable
// read models rebuilt from event_id watermarks. SQLite is the current
// backend (sqlite_store.hpp); PostgreSQL is the formal-phase backend
// (pg_store.hpp, stub until infra lands). All backends share one contract:
// idempotent apply (re-applied streams converge), watermark resume, and
// portable standard SQL text owned once per backend.
class EventStore {
 public:
  EventStore() = default;
  virtual ~EventStore() = default;

  EventStore(const EventStore&) = delete;
  EventStore& operator=(const EventStore&) = delete;
  EventStore(EventStore&&) = delete;
  EventStore& operator=(EventStore&&) = delete;

  // Highest projected event_id, 0 when the projection is empty.
  [[nodiscard]] virtual std::uint64_t latest_event_id() = 0;
  virtual void apply(const LedgerEvent& event) = 0;

  struct Counts {
    std::uint64_t events{0};
    std::uint64_t orders{0};
    std::uint64_t executions{0};
  };
  [[nodiscard]] virtual Counts counts() = 0;
  // Per-strategy order counts ordered by strategy_id.
  [[nodiscard]] virtual std::vector<std::pair<std::string, std::uint64_t>> strategy_order_counts() = 0;
  // Current per-strategy positions ordered by (strategy_id, instrument_id).
  struct PositionRow {
    std::string strategy_id;
    std::string instrument_id;
    std::int64_t quantity{0};
    std::int64_t average_cost{0};
    std::int64_t realized_pnl{0};
  };
  [[nodiscard]] virtual std::vector<PositionRow> positions() = 0;
};

// Async sink for the gateway hot path (§2 async data path, ADR-034):
// submit-side code only pushes into a queue, a single worker thread owns
// the backend connection. Backend-agnostic: constructed over any EventStore.
// Startup catch-up is explicit: read watermark() once, replay the ledger
// above it, push everything.
class AsyncEventSink {
 public:
  explicit AsyncEventSink(std::unique_ptr<EventStore> store);
  ~AsyncEventSink();

  AsyncEventSink(const AsyncEventSink&) = delete;
  AsyncEventSink& operator=(const AsyncEventSink&) = delete;
  AsyncEventSink(AsyncEventSink&&) = delete;
  AsyncEventSink& operator=(AsyncEventSink&&) = delete;

  void push(LedgerEvent event);
  // Blocks until every pushed event has been applied (tests, shutdown).
  void wait_empty();
  // Idempotent drain + join; the destructor calls it.
  void shutdown();
  [[nodiscard]] std::uint64_t watermark() const noexcept { return watermark_; }

 private:
  void run();

  std::unique_ptr<EventStore> store_;
  std::uint64_t watermark_{0};
  std::mutex queue_mutex_;
  std::condition_variable pushed_;
  std::condition_variable drained_;
  std::queue<LedgerEvent> queue_;
  bool busy_{false};
  bool stop_{false};
  bool joined_{false};
  std::thread worker_;
};

// Shared event-payload text (events.payload): identical bytes in every
// backend so payloads stay comparable across SQLite/PostgreSQL and the
// positions rebuild parses one format. Owned here, used by all backends.
[[nodiscard]] std::string event_type_name(LedgerEventType type);
[[nodiscard]] std::string serialize_event_payload(const LedgerEvent& event);

// Backend selection (fail-closed): exactly one of sqlite_file /
// pg_conninfo may be set; both empty disables the projection (nullptr),
// both set throws (ambiguous). pg_conninfo is a resolved DSN, never read
// from yaml here — the server resolves it from the configured env name.
[[nodiscard]] std::unique_ptr<EventStore> open_event_store(const std::string& sqlite_file, const std::string& pg_conninfo);

}  // namespace sts
