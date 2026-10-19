// clang-format off
/**
  ******************************************************************************
  * @file           : pg_store.hpp
  * @author         : vivi wu
  * @brief          : PostgreSQL projection backend for the formal database phase.
  * @version        : 0.1.0
  * @date           : 05/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "qts/persistence/event_store.hpp"

namespace qts {

// PostgreSQL projection backend (formal phase, ADR-038). Same EventStore
// contract and same tables/payload bytes as the SQLite backend; schema text
// stays owned once per backend (see pg_store.cpp).
//
// libpq arrives through the QTS_ENABLE_PG gate (default OFF, system libpq):
// gate OFF compiles the fail-fast stub below (throw with an actionable
// message, never a silent fallback), gate ON compiles the real
// implementation. This header never includes libpq-fe.h (pimpl), so no
// translation unit outside pg_store.cpp sees the client library.
class PgEventStore final : public EventStore {
 public:
  explicit PgEventStore(std::string conninfo);
  ~PgEventStore() override;

  PgEventStore(const PgEventStore&) = delete;
  PgEventStore& operator=(const PgEventStore&) = delete;
  PgEventStore(PgEventStore&&) = delete;
  PgEventStore& operator=(PgEventStore&&) = delete;

  [[nodiscard]] std::uint64_t latest_event_id() override;
  void apply(const LedgerEvent& event) override;
  [[nodiscard]] Counts counts() override;
  [[nodiscard]] std::vector<std::pair<std::string, std::uint64_t>> strategy_order_counts() override;
  [[nodiscard]] std::vector<PositionRow> positions() override;

 private:
  struct Impl;
  std::string conninfo_;
  std::unique_ptr<Impl> impl_;
  std::mutex mutex_;
};

}  // namespace qts
