// clang-format off
/**
  ******************************************************************************
  * @file           : event.hpp
  * @author         : vivi wu
  * @brief          : Event envelope, deterministic event-id generator and bus contract.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "qts/core/types.hpp"

namespace qts {

// Metadata attached to every immutable domain event written to the ledger.
// Payloads (order snapshots, execution reports, risk decisions) stay in
// their own structs; the envelope carries correlation and recovery context.
// M1 will persist envelopes; the field set is frozen now so replay and
// production share the same contract.
struct EventEnvelope {
  std::uint64_t event_id{0};      // unique within the producing partition
  TraceId trace_id;               // correlates command -> reports -> audit
  Timestamp source_time;          // broker/exchange timestamp, if reported
  Timestamp receive_time;         // gateway receive time (Clock::now at ingress)
  std::string config_version;     // risk/config snapshot version at decision time
  std::string rules_version;      // instrument-rules version at decision time
  std::string source_message_id;  // raw broker message correlation for reconciliation
  // Trading day (YYYY-MM-DD, session label like config_version) stamped by
  // the driver at record time; "" means unknown (pre-column ledgers).
  // Intraday timestamps stay in source/receive_time; this field is what
  // daily account/strategy statistics group by.
  std::string trade_date;
};

// Monotonic id source, one instance per producing partition. Constructed
// with an explicit start so replay runs reproduce identical event ids.
class EventIdGenerator {
 public:
  explicit EventIdGenerator(std::uint64_t start = 1)
      : next_{start} {}

  std::uint64_t next() { return next_.fetch_add(1, std::memory_order_relaxed); }

 private:
  std::atomic<std::uint64_t> next_;
};

}  // namespace qts
