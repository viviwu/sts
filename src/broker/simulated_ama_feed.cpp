// clang-format off
/**
  ******************************************************************************
  * @file           : simulated_ama_feed.cpp
  * @author         : vivi wu
  * @brief          : Deterministic AMA mock feed implementation with gap faults.
  * @version        : 0.1.0
  * @date           : 03/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/broker/simulated_ama_feed.hpp"

#include <chrono>
#include <stdexcept>

namespace sts {
namespace {

// Pack (market, channel) into one map key.
std::int64_t tick_key(int market_type, std::int32_t channel_no) { return (static_cast<std::int64_t>(market_type) << 32) | static_cast<std::uint32_t>(channel_no); }

// Add delta_ms to a YYYYMMDDHHMMSSsss stamp. Returns false on corrupt input.
bool add_millis(std::int64_t stamp, std::int64_t delta_ms, std::int64_t& out) {
  if (stamp <= 0 || delta_ms < 0) return false;
  std::int64_t rest = stamp;
  const std::int64_t millis = rest % 1000;
  rest /= 1000;
  const std::int64_t second = rest % 100;
  rest /= 100;
  const std::int64_t minute = rest % 100;
  rest /= 100;
  const std::int64_t hour = rest % 100;
  rest /= 100;
  const std::int64_t day = rest % 100;
  rest /= 100;
  const std::int64_t month = rest % 100;
  const std::int64_t year = rest / 100;
  if (year < 2000 || year > 2100 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59 || millis > 999) {
    return false;
  }
  std::int64_t total_ms = (((((static_cast<std::int64_t>(hour) * 60) + minute) * 60) + second) * 1000) + millis + delta_ms;
  const std::int64_t add_days = total_ms / 86'400'000;
  total_ms -= add_days * 86'400'000;
  const std::int64_t next_hour = total_ms / 3'600'000;
  total_ms -= next_hour * 3'600'000;
  const std::int64_t next_minute = total_ms / 60'000;
  total_ms -= next_minute * 60'000;
  const std::int64_t next_second = total_ms / 1000;
  const std::int64_t next_millis = total_ms - (next_second * 1000);
  // Day rollover is out of scope for intraday demos: clamp at 23:59:59.999.
  std::int64_t next_day = day + add_days;
  if (next_day > 31) return false;
  std::int64_t rebuilt = year;
  rebuilt = (rebuilt * 100) + month;
  rebuilt = (rebuilt * 100) + next_day;
  rebuilt = (rebuilt * 100) + next_hour;
  rebuilt = (rebuilt * 100) + next_minute;
  rebuilt = (rebuilt * 100) + next_second;
  out = (rebuilt * 1000) + next_millis;
  return true;
}

}  // namespace

SimulatedAmaFeed::SimulatedAmaFeed(std::vector<SimulatedAmaSymbol> symbols, SimulatedAmaConfig config)
    : config_{config},
      rng_{config.seed} {
  if (symbols.empty()) throw std::invalid_argument("simulated AMA feed needs at least one symbol");
  for (auto& spec : symbols) {
    if (spec.security_code.empty()) throw std::invalid_argument("simulated AMA symbol needs a security code");
    if (spec.market_type != 101 && spec.market_type != 102 && spec.market_type != 2) {
      throw std::invalid_argument("simulated AMA symbol needs market 101/102/2");
    }
    SymbolState state{};
    state.spec = spec;
    state.current = spec.start;
    state.orig_time = config_.orig_time_base;
    symbols_.push_back(std::move(state));
  }
}

std::string_view SimulatedAmaFeed::channel() const noexcept { return "simulated-ama"; }

bool SimulatedAmaFeed::connected() const noexcept { return connected_; }

void SimulatedAmaFeed::subscribe(const std::vector<InstrumentId>& instruments) {
  for (const auto& instrument : instruments) subscriptions_.insert(instrument);
}

void SimulatedAmaFeed::unsubscribe(const std::vector<InstrumentId>& instruments) {
  for (const auto& instrument : instruments) subscriptions_.erase(instrument);
}

void SimulatedAmaFeed::set_quote_handler(QuoteHandler handler) { handler_ = std::move(handler); }

void SimulatedAmaFeed::disconnect() { connected_ = false; }

void SimulatedAmaFeed::reconnect() { connected_ = true; }

void SimulatedAmaFeed::settle_day() {
  tracker_.clear();
  next_seq_.clear();
  tick_emissions_.clear();
}

SimulatedAmaFeed::SymbolState* SimulatedAmaFeed::find(const std::string& security_code) {
  for (auto& state : symbols_) {
    if (state.spec.security_code == security_code) return &state;
  }
  return nullptr;
}

std::int32_t SimulatedAmaFeed::channel_for(int market_type) const {
  if (market_type == 102) return config_.szse_channel;
  return 0;
}

std::string SimulatedAmaFeed::phase_for(const SymbolState& state) const {
  // state.index is already 1-based here (incremented on entry to emit_one).
  const std::uint64_t tick = state.index;
  if (config_.auction_ticks > 0 && tick <= config_.auction_ticks) {
    if (state.spec.market_type == 101) return "C";
    if (state.spec.market_type == 102) return "O";
    return "C";
  }
  if (config_.halt_for > 0 && config_.halt_after > 0 && tick >= config_.halt_after && tick < config_.halt_after + config_.halt_for) {
    if (state.spec.market_type == 101) return "P";
    return "H";
  }
  if (state.spec.market_type == 101) return "T";
  return "T";
}

AmaSnapshotFixture SimulatedAmaFeed::build_fixture(SymbolState& state) {
  const int move = draw_(rng_);
  Price candidate = state.current + (static_cast<Price>(move) * state.spec.step);
  if (candidate > state.spec.high) candidate = state.spec.high - state.spec.step;
  if (candidate < state.spec.low) candidate = state.spec.low + state.spec.step;
  state.current = candidate;

  const auto shares = static_cast<Quantity>(100 + ((state.index * 37) % 5) * 50);
  state.cumulative_volume += shares;
  state.cumulative_turnover += shares * candidate;

  std::int64_t next_time = state.orig_time;
  if (state.index > 0) {
    std::int64_t advanced = state.orig_time;
    if (add_millis(state.orig_time, config_.orig_time_step_ms, advanced)) next_time = advanced;
  }
  state.orig_time = next_time;

  AmaSnapshotFixture snap{};
  snap.market_type = state.spec.market_type;
  snap.security_code = state.spec.security_code;
  snap.orig_time = next_time;
  snap.trading_phase_code = phase_for(state);
  snap.channel_no = channel_for(state.spec.market_type);
  constexpr std::int64_t kPrice6PerPrice = 100;  // AmaSnapshotNormalizer::kAmaPriceScale / kPriceScale
  for (std::size_t level = 0; level < AmaSnapshotFixture::kLevels; ++level) {
    const Price distance = static_cast<Price>(level + 1) * state.spec.step;
    Price bid = candidate - distance;
    Price ask = candidate + distance;
    if (bid <= 0) bid = state.spec.step;
    if (ask <= 0) ask = state.spec.step * 2;
    snap.bid_price.at(level) = bid * kPrice6PerPrice;
    snap.bid_volume.at(level) = static_cast<std::int64_t>(100 + (level * 50)) * 100;
    snap.offer_price.at(level) = ask * kPrice6PerPrice;
    snap.offer_volume.at(level) = static_cast<std::int64_t>(100 + (level * 50)) * 100;
  }
  snap.last_price = candidate * kPrice6PerPrice;
  snap.total_volume_trade = state.cumulative_volume * 100;
  snap.total_value_trade = state.cumulative_turnover * 10;
  snap.high_limited = state.spec.limit_up * kPrice6PerPrice;
  snap.low_limited = state.spec.limit_down * kPrice6PerPrice;
  return snap;
}

std::optional<QuoteEvent> SimulatedAmaFeed::emit_one(SymbolState& state) {
  ++state.index;
  ++snapshots_;
  AmaSnapshotFixture snap = build_fixture(state);

  // Corrupt fault: fail-closed fixture, nothing emitted, no seq advance.
  if (config_.corrupt_every > 0 && (snapshots_ % config_.corrupt_every) == 0) {
    snap.bid_price[0] = -1;
    const auto now = std::chrono::system_clock::now();
    const auto bad = normalizer_.normalize(snap, now, now);
    if (bad.has_value()) {
      // Normalizer must refuse corrupt input; count it as dropped either way.
    }
    ++dropped_;
    return std::nullopt;
  }

  // Tick sequence with deterministic gap jumps, counted per (market, channel)
  // so every channel gaps independently regardless of symbol interleave.
  const std::int32_t channel_no = channel_for(state.spec.market_type);
  const std::int64_t key = tick_key(state.spec.market_type, channel_no);
  const std::uint64_t tick_emit = ++tick_emissions_[key];
  std::int64_t seq = 1;
  const auto found = next_seq_.find(key);
  if (found != next_seq_.end()) seq = found->second;
  if (config_.gap_every > 0 && config_.gap_size > 0 && (tick_emit % config_.gap_every) == 0) {
    seq += static_cast<std::int64_t>(config_.gap_size);
  }
  const AmaTickResult tick = tracker_.observe(state.spec.market_type, channel_no, seq);
  next_seq_[key] = seq + 1;
  if (tick.accept == AmaTickAccept::gap && tick.backfill.has_value()) gaps_.push_back(*tick.backfill);

  const auto now = std::chrono::system_clock::now();
  Timestamp receive = now;
  if (config_.stale_every > 0 && (snapshots_ % config_.stale_every) == 0) {
    receive = now - std::chrono::seconds{10};
  }
  auto event = normalizer_.normalize(snap, receive, now);
  if (!event.has_value()) {
    ++dropped_;
    return std::nullopt;
  }
  // Crossed fault: rewrite the book after normalization (same shape as the
  // reference-price crossed fault, deterministic by emission count).
  if (config_.crossed_every > 0 && (snapshots_ % config_.crossed_every) == 0) {
    event->bid_price = event->ask_price + state.spec.step;
    if (!event->bid_levels.empty()) event->bid_levels[0].price = event->bid_price;
    event->quality = QuoteQuality::crossed;
  }
  if (!subscriptions_.empty()) {
    if (subscriptions_.find(event->instrument_id) == subscriptions_.end()) {
      ++dropped_;
      return std::nullopt;
    }
  }
  ++emitted_;
  if (handler_) handler_(*event);
  return event;
}

std::vector<QuoteEvent> SimulatedAmaFeed::step() {
  std::vector<QuoteEvent> out;
  if (!connected_) return out;
  for (auto& state : symbols_) {
    auto event = emit_one(state);
    if (event.has_value()) out.push_back(*event);
  }
  return out;
}

std::optional<QuoteEvent> SimulatedAmaFeed::next_for(const std::string& security_code) {
  if (!connected_) return std::nullopt;
  SymbolState* state = find(security_code);
  if (state == nullptr) return std::nullopt;
  return emit_one(*state);
}

}  // namespace sts
