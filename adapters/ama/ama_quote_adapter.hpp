// clang-format off
/**
  ******************************************************************************
  * @file           : ama_quote_adapter.hpp
  * @author         : vivi wu
  * @brief          : AMA market-data QuoteApi adapter interface (pimpl isolates vendor headers).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "qts/broker/ama_snapshot.hpp"
#include "qts/broker/ama_tick.hpp"
#include "qts/broker/quote_api.hpp"

namespace qts {

class AmaQuoteAdapter;

namespace ama_detail {
// Vendor-gated test seams (adapters/tests, see ama_test_seam.hpp). They feed the
// adapter's queue/session state without a live vendor session; production
// code never calls them.
void inject_snapshot(AmaQuoteAdapter& adapter, const AmaSnapshotFixture& fixture, Timestamp receive_time);
void inject_tick(AmaQuoteAdapter& adapter, std::int32_t market, std::int32_t channel, std::int64_t appl_seq);
void inject_event(AmaQuoteAdapter& adapter, std::uint32_t event_code);
}  // namespace ama_detail

// AMA V4.5.8 read-only QuoteApi adapter (M7-2b, ADR-029).
//
// The vendor SDK lives behind pimpl: no vendor header is referenced here,
// only in the .cpp (roadmap M7 rule: core never includes vendor headers;
// this header itself stays vendor-free so tests/tools without the SDK can
// still parse it).
//
// Threading model (archforce-ama.md §3.4, repo drain discipline):
// vendor callbacks (AMA threads) convert to value fixtures, stamp
// receive_time and enqueue; a single pump thread calls drain() to normalize
// and deliver. No vendor pointer ever crosses threads: batches are converted
// and FreeMemory'd inside the callback. Nothing else touches the queues.
struct AmaUmsServer {
  std::string ip;
  std::uint16_t port{0};
};

struct AmaSessionConfig {
  std::string username;
  std::string password;
  std::vector<AmaUmsServer> ums_servers;                            // 1..8 (vendor Cfg limit)
  std::chrono::nanoseconds max_staleness{std::chrono::seconds{3}};  // quality gate; tuned during联调 (archforce-ama.md §3.2)
};

struct AmaAdapterStats {
  std::uint64_t snapshots_received{0};     // vendor batches observed
  std::uint64_t events_delivered{0};       // normalized QuoteEvents from drain()
  std::uint64_t snapshots_dropped{0};      // fail-closed nullopt (unknown market etc.)
  std::uint64_t ticks_seen{0};             // order + execution ticks observed
  std::uint64_t tick_gaps{0};              // sequence gaps recorded (ADR-028)
  std::uint64_t backfill_ticks{0};         // ticks replayed via QueryMDTick
  std::uint64_t backfill_errors{0};        // QueryMDTick OnError replies
  std::uint64_t incomplete_events{0};      // vendor kIncompleteData events
  std::uint64_t logon_events{0};           // successful UMS logons
  std::uint64_t disconnect_events{0};      // logon failed / session lost / heartbeat timeout
  std::uint64_t subscriptions_applied{0};  // instruments pushed with kSet/kAdd
  std::uint64_t subscriptions_invalid{0};  // unparseable InstrumentIds skipped
};

class AmaQuoteAdapter final : public QuoteApi {
 public:
  AmaQuoteAdapter();
  ~AmaQuoteAdapter() override;
  AmaQuoteAdapter(const AmaQuoteAdapter&) = delete;
  AmaQuoteAdapter& operator=(const AmaQuoteAdapter&) = delete;

  // ---- QuoteApi -----------------------------------------------------------
  [[nodiscard]] std::string_view channel() const noexcept override { return "ama"; }
  [[nodiscard]] bool connected() const noexcept override;
  // Records the desired set. Once started (and connected) the adapter pushes
  // it to the vendor with kSet/kAdd/kDel; before that it is buffered.
  void subscribe(const std::vector<InstrumentId>& instruments) override;
  void unsubscribe(const std::vector<InstrumentId>& instruments) override;
  void set_quote_handler(QuoteHandler handler) override;

  // ---- M7-2b lifecycle ----------------------------------------------------
  // IAMDApi::Init only validates cfg; the UMS logon completes asynchronously
  // (OnEvent). Returns false on invalid cfg (too many UMS servers, oversized
  // credentials) or when already started.
  bool start(const AmaSessionConfig& config);
  // IAMDApi::Join + Release; idempotent.
  void stop();
  [[nodiscard]] bool started() const noexcept;

  // Single-thread pump (cf. AlternatePaperBroker::drain): normalizes every
  // queued snapshot and invokes the quote handler. Returns events delivered.
  std::size_t drain();

  // Tick sequence gaps observed since the last call (ADR-028): intervals are
  // moved out for the caller to replay via request_tick_backfill.
  std::vector<AmaTickGap> take_tick_gaps();

  // IAMDApi::QueryMDTick replay for one gap (closed interval, begin<=end).
  // Replayed ticks re-enter through the query SPI and only bump stats: tick
  // data never reaches QuoteEvent (ADR-028), local book rebuild is M7-2.
  bool request_tick_backfill(const AmaTickGap& gap);

  [[nodiscard]] AmaAdapterStats stats() const;
  [[nodiscard]] std::string vendor_version() const;  // IAMDApi::GetVersion()
  [[nodiscard]] const AmaSessionConfig& config() const;

 private:
  friend void ama_detail::inject_snapshot(AmaQuoteAdapter&, const AmaSnapshotFixture&, Timestamp);
  friend void ama_detail::inject_tick(AmaQuoteAdapter&, std::int32_t, std::int32_t, std::int64_t);
  friend void ama_detail::inject_event(AmaQuoteAdapter&, std::uint32_t);

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace qts
