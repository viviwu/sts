// clang-format off
/**
  ******************************************************************************
  * @file           : ama_quote_adapter.cpp
  * @author         : vivi wu
  * @brief          : AMA market-data QuoteApi adapter implementation (vendor-gated, single-pump drain).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
// AMA V4.5.8 read-only QuoteApi adapter (M7-2b, ADR-029/033).
//
// Threading contract (archforce-ama.md §3.4): vendor callbacks run on AMA
// threads. A callback only converts batch entries into value fixtures,
// stamps receive_time, appends them to the queue and returns vendor memory
// with FreeMemory on that same callback thread — no vendor pointer crosses
// threads and no normalization/business work happens upstream of the queue.
// A single pump thread calls drain() to normalize (AmaSnapshotNormalizer)
// and invoke the quote handler; drain() must not be called concurrently.
// All state shared with callbacks is guarded by mutex_; connected_/started_
// are atomics readable from any thread.

#include "ama_quote_adapter.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <utility>

#include "ama_test_seam.hpp"
#include "query/ama_query_spi.h"

namespace sts {
namespace {

// Bounded NUL-terminated read from a fixed vendor char array.
std::string vendor_string(const char* src, std::size_t cap) {
  if (src == nullptr) return {};
  const std::size_t len = ::strnlen(src, cap);
  return std::string{src, len};
}

// Bounded write into a fixed vendor char array; false when it does not fit
// (caller treats that as an invalid config and fails closed).
bool vendor_copy(char* dst, std::size_t cap, const std::string& src) {
  if (cap == 0 || src.size() >= cap) return false;
  std::memcpy(dst, src.data(), src.size());
  dst[src.size()] = '\0';
  return true;
}

// Subscription flag per market: snapshot + the two tick streams the sequence
// tracker consumes (ADR-028) for SSE/SZSE; snapshot only for BSE (NEEQ has
// no SSE/SZSE-style tick streams in scope).
std::uint64_t subscription_flag(std::int32_t market) {
  using SD = amd::ama::SubscribeDataType;
  if (market == amd::ama::MarketType::kNEEQ) return SD::kNEEQSnapshot;
  return SD::kSnapshot | SD::kTickOrder | SD::kTickExecution;
}

}  // namespace

namespace ama_detail {

AmaSnapshotFixture snapshot_to_fixture(const amd::ama::MDSnapshot& snapshot) {
  static_assert(AmaSnapshotFixture::kLevels == amd::ama::ConstField::kPositionLevelLen, "level count must match the vendor layout");
  AmaSnapshotFixture fixture{};
  fixture.market_type = snapshot.market_type;
  fixture.security_code = vendor_string(snapshot.security_code, sizeof(snapshot.security_code));
  fixture.orig_time = snapshot.orig_time;
  fixture.trading_phase_code = vendor_string(snapshot.trading_phase_code, sizeof(snapshot.trading_phase_code));
  for (std::size_t i = 0; i < AmaSnapshotFixture::kLevels; ++i) {
    fixture.bid_price[i] = snapshot.bid_price[i];
    fixture.bid_volume[i] = snapshot.bid_volume[i];
    fixture.offer_price[i] = snapshot.offer_price[i];
    fixture.offer_volume[i] = snapshot.offer_volume[i];
  }
  fixture.last_price = snapshot.last_price;
  fixture.total_volume_trade = snapshot.total_volume_trade;
  fixture.total_value_trade = snapshot.total_value_trade;
  fixture.high_limited = snapshot.high_limited;
  fixture.low_limited = snapshot.low_limited;
  fixture.channel_no = snapshot.channel_no;
  return fixture;
}

bool build_cfg(const AmaSessionConfig& config, amd::ama::Cfg& out) {
  if (config.ums_servers.empty() || config.ums_servers.size() > amd::ama::ConstField::kUMSItemLen) return false;
  if (config.username.size() >= amd::ama::ConstField::kUsernameLen) return false;
  if (config.password.size() >= amd::ama::ConstField::kPasswordLen) return false;

  amd::ama::Cfg cfg{};  // vendor defaults from its own constructor
  if (!vendor_copy(cfg.username, sizeof(cfg.username), config.username)) return false;
  if (!vendor_copy(cfg.password, sizeof(cfg.password), config.password)) return false;
  for (std::size_t i = 0; i < config.ums_servers.size(); ++i) {
    const AmaUmsServer& server = config.ums_servers[i];
    if (server.ip.empty() || server.port == 0) return false;
    if (!vendor_copy(cfg.ums_servers[i].server_ip, sizeof(cfg.ums_servers[i].server_ip), server.ip)) return false;
    cfg.ums_servers[i].server_port = server.port;
  }
  cfg.ums_server_cnt = static_cast<std::uint32_t>(config.ums_servers.size());
  cfg.channel_mode = amd::ama::ChannelMode::kTCP;            // TCP 接入（demo 单通道必设，否则 Init -1；
                                                             // RDMA/AMI/FPGA 等硬件通道需要场地设施，暂不开放）
  cfg.ha_mode = amd::ama::HighAvailableMode::kMasterSlaveA;  // 单通道时 demo 建议 kMasterSlaveA/B
  cfg.is_thread_safe = false;                                // callbacks are serialized by our queue, not by the vendor
  cfg.is_subscribe_full = false;                             // subscriptions are explicit exact-code kAdd/kSet only (§3.7)
  out = cfg;
  return true;
}

bool parse_instrument(const InstrumentId& id, std::int32_t& market, std::string& code) {
  const std::size_t dot = id.rfind('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= id.size()) return false;
  const std::string suffix = id.substr(dot + 1);
  if (suffix == "SSE") {
    market = amd::ama::MarketType::kSSE;
  } else if (suffix == "SZSE") {
    market = amd::ama::MarketType::kSZSE;
  } else if (suffix == "BSE") {
    market = amd::ama::MarketType::kNEEQ;
  } else {
    return false;
  }
  code = id.substr(0, dot);
  return !code.empty();
}

}  // namespace ama_detail

struct AmaQuoteAdapter::Impl {
  struct Spi final : amd::ama::IAMDSpi {
    explicit Spi(Impl* owner)
        : owner_(owner) {}

    void OnEvent(uint32_t level, uint32_t code, const char* event_msg, uint32_t len) override {
      (void)level;
      (void)event_msg;
      (void)len;
      owner_->handle_event(code);
    }
    void OnMDSnapshot(amd::ama::MDSnapshot* snapshots, uint32_t cnt) override {
      owner_->enqueue_snapshots(snapshots, cnt);
      amd::ama::IAMDApi::FreeMemory(snapshots);
    }
    void OnMDTickOrder(amd::ama::MDTickOrder* ticks, uint32_t cnt) override {
      owner_->enqueue_ticks(ticks, cnt);
      amd::ama::IAMDApi::FreeMemory(ticks);
    }
    void OnMDTickExecution(amd::ama::MDTickExecution* ticks, uint32_t cnt) override {
      owner_->enqueue_ticks(ticks, cnt);
      amd::ama::IAMDApi::FreeMemory(ticks);
    }
    // OnLog/OnIndicator and every other default callback free vendor memory
    // in the base class; nothing else is of interest to the read-only path.

    Impl* owner_;
  };

  struct QuerySpi final : amd::ama::IQuerySpi {
    explicit QuerySpi(Impl* owner)
        : owner_(owner) {}

    void OnMDTickOrder(const amd::ama::TickQueryItem& item, amd::ama::MDTickOrder* ticks, uint32_t cnt, bool is_last) override {
      (void)item;
      (void)is_last;
      count_backfill(cnt);
      amd::ama::IAMDApi::FreeMemory(ticks);
    }
    void OnMDTickExecution(const amd::ama::TickQueryItem& item, amd::ama::MDTickExecution* ticks, uint32_t cnt, bool is_last) override {
      (void)item;
      (void)is_last;
      count_backfill(cnt);
      amd::ama::IAMDApi::FreeMemory(ticks);
    }
    void OnError(const amd::ama::TickQueryItem& item, int32_t error_code) override {
      (void)item;
      (void)error_code;
      std::lock_guard<std::mutex> lock{owner_->mutex_};
      ++owner_->stats_.backfill_errors;
    }

    void count_backfill(uint32_t cnt) {
      std::lock_guard<std::mutex> lock{owner_->mutex_};
      owner_->stats_.backfill_ticks += cnt;
    }

    Impl* owner_;
  };

  void handle_event(uint32_t code) {
    // Live-observed (2026-10-09, UMS 172.16.109.14: TCP + kMasterSlaveA): the
    // session comes up as 1 (UMS connect) → 20/22 (TCP init/connect) → 24
    // (TCP logon); the UMS-layer kUMSLogonSuccess never fires on this path,
    // so readiness must also accept kChannelTCPLogonSuccess.
    switch (code) {
      case amd::ama::EventCode::kUMSLogonSuccess:
      case amd::ama::EventCode::kChannelTCPLogonSuccess: {
        std::lock_guard<std::mutex> lock{mutex_};
        ++stats_.logon_events;
        connected_.store(true);
        subs_dirty_ = true;  // (re)apply the desired set after every logon
        break;
      }
      case amd::ama::EventCode::kUMSConnectFailed:
      case amd::ama::EventCode::kUMSLogonFailed:
      case amd::ama::EventCode::kUMSHeartbeatTimeout:
      case amd::ama::EventCode::kChannelTCPInitFailed:
      case amd::ama::EventCode::kChannelTCPConnectFailed:
      case amd::ama::EventCode::kChannelTCPLogonFailed:
      case amd::ama::EventCode::kChannelTCPSessionClosed:
      case amd::ama::EventCode::kChannelTCPHeartbeatTimeout: {
        std::lock_guard<std::mutex> lock{mutex_};
        ++stats_.disconnect_events;
        connected_.store(false);
        break;
      }
      case amd::ama::EventCode::kIncompleteData: {
        // Server-side book restarted in recover mode: history may have holes
        // (archforce-ama.md §3.5). Counted; tick gaps surface via the tracker.
        std::lock_guard<std::mutex> lock{mutex_};
        ++stats_.incomplete_events;
        break;
      }
      default:
        break;
    }
  }

  void enqueue_snapshots(const amd::ama::MDSnapshot* snapshots, uint32_t cnt) {
    const Timestamp receive_time = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> lock{mutex_};
    stats_.snapshots_received += cnt;
    for (uint32_t i = 0; i < cnt; ++i) {
      pending_snapshots_.emplace_back(ama_detail::snapshot_to_fixture(snapshots[i]), receive_time);
    }
  }

  template <typename Tick>
  void enqueue_ticks(const Tick* ticks, uint32_t cnt) {
    std::lock_guard<std::mutex> lock{mutex_};
    stats_.ticks_seen += cnt;
    for (uint32_t i = 0; i < cnt; ++i) {
      const AmaTickResult result = tick_tracker_.observe(ticks[i].market_type, ticks[i].channel_no, ticks[i].appl_seq_num);
      if (result.backfill.has_value()) {
        pending_gaps_.push_back(*result.backfill);
        ++stats_.tick_gaps;
      }
    }
  }

  // Applies desired_ to the vendor with kSet (replace-all keeps the session
  // and our set in sync, including after reconnects). Called from the pump
  // thread (drain) and user threads (subscribe/unsubscribe/start), never
  // from a vendor callback.
  void flush_subscriptions() {
    std::vector<InstrumentId> desired;
    bool started = false;
    bool connected = false;
    {
      std::lock_guard<std::mutex> lock{mutex_};
      started = started_.load();
      connected = connected_.load();
      if (!subs_dirty_ || !started || !connected || desired_.empty()) return;
      subs_dirty_ = false;
      desired = desired_;
    }

    std::vector<amd::ama::SubscribeItem> items;
    items.reserve(desired.size());
    std::size_t invalid = 0;
    std::size_t applied = 0;
    for (const InstrumentId& id : desired) {
      std::int32_t market = 0;
      std::string code;
      amd::ama::SubscribeItem item{};
      if (!ama_detail::parse_instrument(id, market, code) || code.size() >= amd::ama::ConstField::kFutureSecurityCodeLen) {
        ++invalid;
        continue;
      }
      item.market = market;
      item.flag = subscription_flag(market);
      std::memcpy(item.security_code, code.data(), code.size());
      item.security_code[code.size()] = '\0';
      items.push_back(item);
    }
    if (!items.empty()) {
      const int rc = amd::ama::IAMDApi::SubscribeData(amd::ama::SubscribeType::kSet, items.data(), static_cast<uint32_t>(items.size()));
      if (rc == amd::ama::ErrorCode::kSuccess) applied = items.size();
    }
    std::lock_guard<std::mutex> lock{mutex_};
    stats_.subscriptions_invalid += invalid;
    stats_.subscriptions_applied += applied;
  }

  Impl()
      : spi(std::make_unique<Spi>(this)),
        query_spi(std::make_unique<QuerySpi>(this)) {}

  std::unique_ptr<Spi> spi;
  std::unique_ptr<QuerySpi> query_spi;

  mutable std::mutex mutex_;
  std::deque<std::pair<AmaSnapshotFixture, Timestamp>> pending_snapshots_;
  std::vector<AmaTickGap> pending_gaps_;
  std::vector<InstrumentId> desired_;
  QuoteHandler handler_;
  AmaSessionConfig config_;
  AmaSnapshotNormalizer normalizer_;
  AmaTickSequenceTracker tick_tracker_;
  AmaAdapterStats stats_;
  bool subs_dirty_{false};
  std::atomic<bool> connected_{false};
  std::atomic<bool> started_{false};
};

AmaQuoteAdapter::AmaQuoteAdapter()
    : impl_(std::make_unique<Impl>()) {}

AmaQuoteAdapter::~AmaQuoteAdapter() { stop(); }

bool AmaQuoteAdapter::connected() const noexcept { return impl_->connected_.load(); }

bool AmaQuoteAdapter::started() const noexcept { return impl_->started_.load(); }

void AmaQuoteAdapter::set_quote_handler(QuoteHandler handler) {
  std::lock_guard<std::mutex> lock{impl_->mutex_};
  impl_->handler_ = std::move(handler);
}

void AmaQuoteAdapter::subscribe(const std::vector<InstrumentId>& instruments) {
  {
    std::lock_guard<std::mutex> lock{impl_->mutex_};
    for (const InstrumentId& id : instruments) {
      if (std::find(impl_->desired_.begin(), impl_->desired_.end(), id) == impl_->desired_.end()) impl_->desired_.push_back(id);
    }
    impl_->subs_dirty_ = true;
  }
  impl_->flush_subscriptions();
}

void AmaQuoteAdapter::unsubscribe(const std::vector<InstrumentId>& instruments) {
  {
    std::lock_guard<std::mutex> lock{impl_->mutex_};
    for (const InstrumentId& id : instruments) {
      impl_->desired_.erase(std::remove(impl_->desired_.begin(), impl_->desired_.end(), id), impl_->desired_.end());
    }
    impl_->subs_dirty_ = true;
  }
  impl_->flush_subscriptions();
}

bool AmaQuoteAdapter::start(const AmaSessionConfig& config) {
  amd::ama::Cfg cfg{};
  if (!ama_detail::build_cfg(config, cfg)) return false;
  bool expected = false;
  if (!impl_->started_.compare_exchange_strong(expected, true)) return false;
  {
    std::lock_guard<std::mutex> lock{impl_->mutex_};
    impl_->config_ = config;
    impl_->subs_dirty_ = true;
  }
  if (amd::ama::IAMDApi::Init(impl_->spi.get(), cfg) != amd::ama::ErrorCode::kSuccess) {
    impl_->started_.store(false);
    return false;
  }
  return true;
}

void AmaQuoteAdapter::stop() {
  if (!impl_->started_.exchange(false)) return;
  impl_->connected_.store(false);
  // Demo shutdown path: Release() alone joins and frees internally (Join()
  // is the separate "join without freeing" primitive, not part of teardown;
  // see ama_demo_tcp.cpp main flow).
  amd::ama::IAMDApi::Release();
}

std::size_t AmaQuoteAdapter::drain() {
  impl_->flush_subscriptions();
  std::deque<std::pair<AmaSnapshotFixture, Timestamp>> local;
  QuoteHandler handler;
  AmaSessionConfig config;
  {
    std::lock_guard<std::mutex> lock{impl_->mutex_};
    handler = impl_->handler_;
    if (!handler) return 0;  // no consumer yet: keep events queued, do not count them
    local.swap(impl_->pending_snapshots_);
    config = impl_->config_;
  }
  const Timestamp now = std::chrono::system_clock::now();
  std::size_t delivered = 0;
  std::size_t dropped = 0;
  for (auto& [fixture, receive_time] : local) {
    std::optional<QuoteEvent> event = impl_->normalizer_.normalize(fixture, receive_time, now, config.max_staleness);
    if (event.has_value()) {
      handler(*event);
      ++delivered;
    } else {
      ++dropped;
    }
  }
  if (delivered != 0 || dropped != 0) {
    std::lock_guard<std::mutex> lock{impl_->mutex_};
    impl_->stats_.events_delivered += delivered;
    impl_->stats_.snapshots_dropped += dropped;
  }
  return delivered;
}

std::vector<AmaTickGap> AmaQuoteAdapter::take_tick_gaps() {
  std::lock_guard<std::mutex> lock{impl_->mutex_};
  std::vector<AmaTickGap> gaps;
  gaps.swap(impl_->pending_gaps_);
  return gaps;
}

bool AmaQuoteAdapter::request_tick_backfill(const AmaTickGap& gap) {
  if (!impl_->started_.load()) return false;
  if (gap.begin_seq <= 0 || gap.end_seq < gap.begin_seq) return false;
  amd::ama::TickQueryItem item{};
  item.market = gap.market_type;
  item.channel_no = gap.channel_no;
  item.begin_appl_seq_num = gap.begin_seq;
  item.end_appl_seq_num = gap.end_seq;
  item.user_ctx = 0;
  return amd::ama::IAMDApi::QueryMDTick(impl_->query_spi.get(), item) == amd::ama::ErrorCode::kSuccess;
}

AmaAdapterStats AmaQuoteAdapter::stats() const {
  std::lock_guard<std::mutex> lock{impl_->mutex_};
  return impl_->stats_;
}

std::string AmaQuoteAdapter::vendor_version() const {
  const char* version = amd::ama::IAMDApi::GetVersion();
  return version == nullptr ? std::string{} : std::string{version};
}

const AmaSessionConfig& AmaQuoteAdapter::config() const { return impl_->config_; }

namespace ama_detail {

void inject_snapshot(AmaQuoteAdapter& adapter, const AmaSnapshotFixture& fixture, Timestamp receive_time) {
  std::lock_guard<std::mutex> lock{adapter.impl_->mutex_};
  ++adapter.impl_->stats_.snapshots_received;
  adapter.impl_->pending_snapshots_.emplace_back(fixture, receive_time);
}

void inject_tick(AmaQuoteAdapter& adapter, std::int32_t market, std::int32_t channel, std::int64_t appl_seq) {
  std::lock_guard<std::mutex> lock{adapter.impl_->mutex_};
  ++adapter.impl_->stats_.ticks_seen;
  const AmaTickResult result = adapter.impl_->tick_tracker_.observe(market, channel, appl_seq);
  if (result.backfill.has_value()) {
    adapter.impl_->pending_gaps_.push_back(*result.backfill);
    ++adapter.impl_->stats_.tick_gaps;
  }
}

void inject_event(AmaQuoteAdapter& adapter, std::uint32_t event_code) { adapter.impl_->handle_event(event_code); }

}  // namespace ama_detail
}  // namespace sts
