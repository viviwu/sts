// clang-format off
/**
  ******************************************************************************
  * @file           : ama_tick.hpp
  * @author         : vivi wu
  * @brief          : AMA tick-by-tick sequence tracker and gap backfill ranges.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>

#include "sts/core/types.hpp"

namespace sts {

// Tick-by-tick linkage (M7-1, ADR-028).
//
// Port decision: ticks never enter QuoteEvent (snapshot semantics stay
// pure). M7-1 locks the two prerequisites for any future tick consumption:
// sequence continuity per (market, channel) with backfill ranges shaped
// exactly like TickQueryItem, and the direction/kind code tables. Full local
// book reconstruction (orig_order_no tracking, level aggregation, '4'-cancel
// association) is deferred until a live feed can validate it (M7-2).
// Field shapes mirror MDTickOrder/MDTickExecution; no vendor header is
// referenced (cf. archforce-ama.md §2.4).

// SSE 'B'/SZSE '1' buy side; SSE 'S'/SZSE '2' sell side. SZSE 'G'/'F'
// (lending in/out) fold to buy/sell by declaration direction; the business
// type distinction belongs to M7-2. Returns nullopt for status reuse
// (SSE order_type 'S', where side carries TickBSFlag) and unknown codes.
std::optional<Side> map_tick_order_side(int market_type, char side, char order_type);

enum class AmaTickOrderKind : std::uint8_t { add, remove, status, quote, unknown };

// SSE 'A' add / 'D' remove / 'S' status; SZSE '1'/'2'/'U' are plain orders
// with no lifecycle meaning (kind quote). SZSE cancels arrive as executions
// with exec_type '4', never as orders.
AmaTickOrderKind map_tick_order_kind(int market_type, char order_type);

struct AmaTickExecutionClass {
  bool is_cancel{false};  // SZSE exec_type '4'; SSE only knows 'F'
  // Aggressor side; only SSE provides it ('B'/'S', 'N' unknown). SZSE
  // executions carry no usable side field.
  std::optional<Side> aggressor;
};

AmaTickExecutionClass classify_tick_execution(int market_type, char side, char exec_type);

struct AmaTickGap {
  int market_type{0};
  std::int32_t channel_no{0};
  std::int64_t begin_seq{0};  // inclusive, = last seen + 1
  std::int64_t end_seq{0};    // inclusive, = received - 1
};

enum class AmaTickAccept : std::uint8_t { accepted, duplicate, gap };

struct AmaTickResult {
  AmaTickAccept accept{AmaTickAccept::accepted};
  std::optional<AmaTickGap> backfill;
};

// Per-(market, channel) appl_seq_num continuity. First observation of a
// channel is the baseline (accepted, no backfill possible). A gap advances
// the edge to the received seq after reporting the backfill range, so live
// consumption continues while QueryMDTick repairs history. Sequence numbers
// reset daily: call clear() at the day boundary (same beat as settle_day).
class AmaTickSequenceTracker {
 public:
  AmaTickResult observe(int market_type, std::int32_t channel_no, std::int64_t appl_seq_num);
  void clear();
  [[nodiscard]] std::size_t tracked_channels() const { return last_.size(); }

 private:
  struct Key {
    int market_type{0};
    std::int32_t channel_no{0};
    bool operator==(const Key& other) const { return market_type == other.market_type && channel_no == other.channel_no; }
  };
  struct KeyHash {
    std::size_t operator()(const Key& key) const noexcept { return std::hash<int>{}(key.market_type) * 31U + std::hash<std::int32_t>{}(key.channel_no); }
  };

  std::unordered_map<Key, std::int64_t, KeyHash> last_;
};

}  // namespace sts
