// clang-format off
/**
  ******************************************************************************
  * @file           : ama_tick_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for AMA tick sequencing and gap reporting.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/broker/ama_tick.hpp"

#include <cassert>

namespace {

void order_side_table() {
  using sts::Side;
  // SSE spot.
  assert(sts::map_tick_order_side(101, 'B', 'A') == Side::buy);
  assert(sts::map_tick_order_side(101, 'S', 'D') == Side::sell);
  // SSE status reuse: side carries TickBSFlag, never a direction.
  assert(!sts::map_tick_order_side(101, 'B', 'S').has_value());
  assert(!sts::map_tick_order_side(101, 'T', 'S').has_value());
  assert(!sts::map_tick_order_side(101, 'X', 'A').has_value());
  // SZSE spot + lending fold.
  assert(sts::map_tick_order_side(102, '1', '2') == Side::buy);
  assert(sts::map_tick_order_side(102, '2', '2') == Side::sell);
  assert(sts::map_tick_order_side(102, 'G', '2') == Side::buy);
  assert(sts::map_tick_order_side(102, 'F', '2') == Side::sell);
  assert(!sts::map_tick_order_side(102, 'X', '2').has_value());
  // Out-of-scope markets refuse.
  assert(!sts::map_tick_order_side(103, 'B', 'A').has_value());
  assert(!sts::map_tick_order_side(2, 'B', 'A').has_value());
}

void order_kind_table() {
  using sts::AmaTickOrderKind;
  assert(sts::map_tick_order_kind(101, 'A') == AmaTickOrderKind::add);
  assert(sts::map_tick_order_kind(101, 'D') == AmaTickOrderKind::remove);
  assert(sts::map_tick_order_kind(101, 'S') == AmaTickOrderKind::status);
  assert(sts::map_tick_order_kind(101, 'X') == AmaTickOrderKind::unknown);
  assert(sts::map_tick_order_kind(102, '1') == AmaTickOrderKind::quote);
  assert(sts::map_tick_order_kind(102, '2') == AmaTickOrderKind::quote);
  assert(sts::map_tick_order_kind(102, 'U') == AmaTickOrderKind::quote);
  assert(sts::map_tick_order_kind(102, 'A') == AmaTickOrderKind::unknown);
  assert(sts::map_tick_order_kind(103, 'A') == AmaTickOrderKind::unknown);
}

void execution_classification() {
  // SZSE: cancel vs trade, no aggressor side.
  const auto sz_trade = sts::classify_tick_execution(102, ' ', 'F');
  assert(!sz_trade.is_cancel && !sz_trade.aggressor.has_value());
  const auto sz_cancel = sts::classify_tick_execution(102, ' ', '4');
  assert(sz_cancel.is_cancel && !sz_cancel.aggressor.has_value());
  // SSE: aggressor side only; 'N' unknown stays empty.
  const auto sh_buy = sts::classify_tick_execution(101, 'B', 'F');
  assert(!sh_buy.is_cancel && sh_buy.aggressor == sts::Side::buy);
  const auto sh_sell = sts::classify_tick_execution(101, 'S', 'F');
  assert(!sh_sell.is_cancel && sh_sell.aggressor == sts::Side::sell);
  const auto sh_unknown = sts::classify_tick_execution(101, 'N', 'F');
  assert(!sh_unknown.is_cancel && !sh_unknown.aggressor.has_value());
  const auto other = sts::classify_tick_execution(103, 'B', 'F');
  assert(!other.is_cancel && !other.aggressor.has_value());
}

void tracker_continuous_and_baseline() {
  sts::AmaTickSequenceTracker tracker;
  assert(tracker.tracked_channels() == 0);
  auto first = tracker.observe(102, 5, 1000);
  assert(first.accept == sts::AmaTickAccept::accepted && !first.backfill.has_value());
  assert(tracker.tracked_channels() == 1);
  auto next = tracker.observe(102, 5, 1001);
  assert(next.accept == sts::AmaTickAccept::accepted && !next.backfill.has_value());
}

void tracker_gap_reports_backfill_range() {
  sts::AmaTickSequenceTracker tracker;
  tracker.observe(102, 5, 1000);
  tracker.observe(102, 5, 1001);
  const auto gap = tracker.observe(102, 5, 1005);
  assert(gap.accept == sts::AmaTickAccept::gap);
  assert(gap.backfill.has_value());
  assert(gap.backfill->market_type == 102 && gap.backfill->channel_no == 5);
  assert(gap.backfill->begin_seq == 1002 && gap.backfill->end_seq == 1004);
  // Edge advances: live consumption continues past the hole.
  const auto after = tracker.observe(102, 5, 1006);
  assert(after.accept == sts::AmaTickAccept::accepted && !after.backfill.has_value());
}

void tracker_duplicates_do_not_move_edge() {
  sts::AmaTickSequenceTracker tracker;
  tracker.observe(102, 5, 1000);
  tracker.observe(102, 5, 1001);
  const auto same = tracker.observe(102, 5, 1001);
  assert(same.accept == sts::AmaTickAccept::duplicate && !same.backfill.has_value());
  const auto older = tracker.observe(102, 5, 999);
  assert(older.accept == sts::AmaTickAccept::duplicate && !older.backfill.has_value());
  const auto corrupt = tracker.observe(102, 5, 0);
  assert(corrupt.accept == sts::AmaTickAccept::duplicate && !corrupt.backfill.has_value());
  // Edge unmoved: next expected seq still reports the same hole.
  const auto gap = tracker.observe(102, 5, 1005);
  assert(gap.accept == sts::AmaTickAccept::gap);
  assert(gap.backfill->begin_seq == 1002 && gap.backfill->end_seq == 1004);
}

void tracker_channels_are_independent() {
  sts::AmaTickSequenceTracker tracker;
  tracker.observe(102, 5, 1000);
  // Another channel and another market start their own baselines.
  const auto other_channel = tracker.observe(102, 6, 1);
  assert(other_channel.accept == sts::AmaTickAccept::accepted);
  const auto other_market = tracker.observe(101, 5, 1);
  assert(other_market.accept == sts::AmaTickAccept::accepted);
  assert(tracker.tracked_channels() == 3);
  // Gaps are reported against the right edge.
  const auto gap = tracker.observe(102, 6, 4);
  assert(gap.accept == sts::AmaTickAccept::gap);
  assert(gap.backfill->begin_seq == 2 && gap.backfill->end_seq == 3);
}

void tracker_day_boundary_clears() {
  sts::AmaTickSequenceTracker tracker;
  tracker.observe(102, 5, 1000);
  tracker.clear();  // seq numbers reset daily, same beat as settle_day
  assert(tracker.tracked_channels() == 0);
  // Restarted numbering is a fresh baseline, not a gap.
  const auto restart = tracker.observe(102, 5, 1);
  assert(restart.accept == sts::AmaTickAccept::accepted && !restart.backfill.has_value());
}

}  // namespace

int main() {
  order_side_table();
  order_kind_table();
  execution_classification();
  tracker_continuous_and_baseline();
  tracker_gap_reports_backfill_range();
  tracker_duplicates_do_not_move_edge();
  tracker_channels_are_independent();
  tracker_day_boundary_clears();
  return 0;
}
