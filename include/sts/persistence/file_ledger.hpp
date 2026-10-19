// clang-format off
/**
  ******************************************************************************
  * @file           : file_ledger.hpp
  * @author         : vivi wu
  * @brief          : Append-only JSONL event ledger with restart resume and strict validation.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "sts/persistence/ledger.hpp"

namespace sts {

// Append-only JSONL event log (M3-1). Each line is one self-contained
// LedgerEvent; construction replays the whole file to rebuild state and
// resumes event ids monotonically after the max stored id.
//
// Durability: every append flushes the stream before returning, so a
// restarted process sees all acknowledged events. OS-level fsync is left
// to the deployment (tmpfs vs disk tradeoff), noted for M3-4 runbook.
class FileEventLedger final : public EventLedger {
 public:
  explicit FileEventLedger(std::string path);
  ~FileEventLedger() override = default;

  FileEventLedger(const FileEventLedger&) = delete;
  FileEventLedger& operator=(const FileEventLedger&) = delete;
  FileEventLedger(FileEventLedger&&) = delete;
  FileEventLedger& operator=(FileEventLedger&&) = delete;

  std::uint64_t append(LedgerEvent event) override;
  std::vector<LedgerEvent> replay(std::uint64_t from_event_id) const override;
  std::uint64_t latest_event_id() const override;
  [[nodiscard]] const std::string& path() const noexcept { return path_; }

 private:
  std::string path_;
  mutable std::mutex mutex_;
  std::vector<LedgerEvent> events_;
  std::uint64_t next_id_{1};
};

// Graceful-shutdown snapshot for ledger rotation and crash recovery.
// open_orders carries (intent, latest snapshot) pairs per working order so
// a rotated-away ledger never strands state: intents travel with the
// snapshot instead of being looked up in a possibly archived file.
// ledger_bytes is the epoch guard: a snapshot only applies when the live
// ledger file is at least that large (append-only growth); a replaced
// (rotated/wiped) ledger makes it stale and it must be ignored.
struct OpenSnapshot {
  std::uint64_t ledger_bytes{0};
  std::uint64_t last_event_id{0};
  std::string config_version;
  std::string rules_version;
  std::vector<std::pair<OrderIntent, ExecutionReport>> opens;
};

// Overwrites path with the snapshot (throws on I/O or serialization
// failure: a half-written snapshot must never pass as valid).
void write_open_snapshot(const std::string& path, const OpenSnapshot& snapshot);
// Throws on missing file or corrupt content (fail closed, like the ledger).
[[nodiscard]] OpenSnapshot read_open_snapshot(const std::string& path);

}  // namespace sts
