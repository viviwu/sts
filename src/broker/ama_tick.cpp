// clang-format off
/**
  ******************************************************************************
  * @file           : ama_tick.cpp
  * @author         : vivi wu
  * @brief          : AMA tick sequence tracking implementation.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/broker/ama_tick.hpp"

namespace qts {

std::optional<Side> map_tick_order_side(int market_type, char side, char order_type) {
  if (market_type == 101) {
    // order_type 'S' reuses side for TickBSFlag (O/C/T/P/U/E/Q): never a
    // tradable direction (ama_struct.h MDTickOrder comment).
    if (order_type == 'S') return std::nullopt;
    if (side == 'B') return Side::buy;
    if (side == 'S') return Side::sell;
    return std::nullopt;
  }
  if (market_type == 102) {
    if (side == '1' || side == 'G') return Side::buy;
    if (side == '2' || side == 'F') return Side::sell;
    return std::nullopt;
  }
  return std::nullopt;
}

AmaTickOrderKind map_tick_order_kind(int market_type, char order_type) {
  if (market_type == 101) {
    switch (order_type) {
      case 'A':
        return AmaTickOrderKind::add;
      case 'D':
        return AmaTickOrderKind::remove;
      case 'S':
        return AmaTickOrderKind::status;
      default:
        return AmaTickOrderKind::unknown;
    }
  }
  if (market_type == 102) {
    switch (order_type) {
      case '1':
      case '2':
      case 'U':
        return AmaTickOrderKind::quote;
      default:
        return AmaTickOrderKind::unknown;
    }
  }
  return AmaTickOrderKind::unknown;
}

AmaTickExecutionClass classify_tick_execution(int market_type, char side, char exec_type) {
  AmaTickExecutionClass out{};
  if (market_type == 102) {
    out.is_cancel = (exec_type == '4');
    return out;  // SZSE carries no aggressor side on executions
  }
  if (market_type == 101) {
    if (side == 'B') out.aggressor = Side::buy;
    if (side == 'S') out.aggressor = Side::sell;  // 'N'/other stays nullopt
    return out;
  }
  return out;
}

AmaTickResult AmaTickSequenceTracker::observe(int market_type, std::int32_t channel_no, std::int64_t appl_seq_num) {
  AmaTickResult result{};
  if (appl_seq_num <= 0) {
    result.accept = AmaTickAccept::duplicate;  // corrupt seq: drop, no backfill
    return result;
  }
  const Key key{market_type, channel_no};
  const auto iter = last_.find(key);
  if (iter == last_.end()) {
    last_.emplace(key, appl_seq_num);  // baseline for a new channel
    return result;
  }
  if (appl_seq_num <= iter->second) {
    result.accept = AmaTickAccept::duplicate;
    return result;
  }
  if (appl_seq_num > iter->second + 1) {
    result.accept = AmaTickAccept::gap;
    result.backfill = AmaTickGap{market_type, channel_no, iter->second + 1, appl_seq_num - 1};
  }
  iter->second = appl_seq_num;
  return result;
}

void AmaTickSequenceTracker::clear() { last_.clear(); }

}  // namespace qts
