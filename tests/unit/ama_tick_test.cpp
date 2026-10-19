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
#include "qts/broker/ama_tick.hpp"

#include <cassert>

namespace {

void order_side_table() {
  using qts::Side;
  // SSE spot.
  assert(qts::map_tick_order_side(101, 'B', 'A') == Side::buy);
  assert(qts::map_tick_order_side(101, 'S', 'D') == Side::sell);
  // SSE status reuse: side carries TickBSFlag, never a direction.
  assert(!qts::map_tick_order_side(101, 'B', 'S').has_value());
  assert(!qts::map_tick_order_side(101, 'T', 'S').has_value());
  assert(!qts::map_tick_order_side(101, 'X', 'A').has_value());
  // SZSE spot + lending fold.
  assert(qts::map_tick_order_side(102, '1', '2') == Side::buy);
  assert(qts::map_tick_order_side(102, '2', '2') == Side::sell);
  assert(qts::map_tick_order_side(102, 'G', '2') == Side::buy);
  assert(qts::map_tick_order_side(102, 'F', '2') == Side::sell);
  assert(!qts::map_tick_order_side(102, 'X', '2').has_value());
  // Out-of-scope markets refuse.
  assert(!qts::map_tick_order_side(103, 'B', 'A').has_value());
  assert(!qts::map_tick_order_side(2, 'B', 'A').has_value());
}

void order_kind_table() {
  using qts::AmaTickOrderKind;
  assert(qts::map_tick_order_kind(101, 'A') == AmaTickOrderKind::add);
  assert(qts::map_tick_order_kind(101, 'D') == AmaTickOrderKind::remove);
  assert(qts::map_tick_order_kind(101, 'S') == AmaTickOrderKind::status);
  assert(qts::map_tick_order_kind(101, 'X') == AmaTickOrderKind::unknown);
  assert(qts::map_tick_order_kind(102, '1') == AmaTickOrderKind::quote);
  assert(qts::map_tick_order_kind(102, '2') == AmaTickOrderKind::quote);
  assert(qts::map_tick_order_kind(102, 'U') == AmaTickOrderKind::quote);
  assert(qts::map_tick_order_kind(102, 'A') == AmaTickOrderKind::unknown);
  assert(qts::map_tick_order_kind(103, 'A') == AmaTickOrderKind::unknown);
}

void execution_classification() {
  // SZSE: cancel vs trade, no aggressor side.
  const auto sz_trade = qts::classify_tick_execution(102, ' ', 'F');
  assert(!sz_trade.is_cancel && !sz_trade.aggressor.has_value());
  const auto sz_cancel = qts::classify_tick_execution(102, ' ', '4');
  assert(sz_cancel.is_cancel && !sz_cancel.aggressor.has_value());
  // SSE: aggressor side only; 'N' unknown stays empty.
  const auto sh_buy = qts::classify_tick_execution(101, 'B', 'F');
  assert(!sh_buy.is_cancel && sh_buy.aggressor == qts::Side::buy);
  const auto sh_sell = qts::classify_tick_execution(101, 'S', 'F');
  assert(!sh_sell.is_cancel && sh_sell.aggressor == qts::Side::sell);
  const auto sh_unknown = qts::classify_tick_execution(101, 'N', 'F');
  assert(!sh_unknown.is_cancel && !sh_unknown.aggressor.has_value());
  const auto other = qts::classify_tick_execution(103, 'B', 'F');
  assert(!other.is_cancel && !other.aggressor.has_value());
}

void tracker_continuous_and_baseline() {
  qts::AmaTickSequenceTracker tracker;
  assert(tracker.tracked_channels() == 0);
  auto first = tracker.observe(102, 5, 1000);
  assert(first.accept == qts::AmaTickAccept::accepted && !first.backfill.has_value());
  assert(tracker.tracked_channels() == 1);
  auto next = tracker.observe(102, 5, 1001);
  assert(next.accept == qts::AmaTickAccept::accepted && !next.backfill.has_value());
}

void tracker_gap_reports_backfill_range() {
  qts::AmaTickSequenceTracker tracker;
  tracker.observe(102, 5, 1000);
  tracker.observe(102, 5, 1001);
  const auto gap = tracker.observe(102, 5, 1005);
  assert(gap.accept == qts::AmaTickAccept::gap);
  assert(gap.backfill.has_value());
  assert(gap.backfill->market_type == 102 && gap.backfill->channel_no == 5);
  assert(gap.backfill->begin_seq == 1002 && gap.backfill->end_seq == 1004);
  // Edge advances: live consumption continues past the hole.
  const auto after = tracker.observe(102, 5, 1006);
  assert(after.accept == qts::AmaTickAccept::accepted && !after.backfill.has_value());
}

void tracker_duplicates_do_not_move_edge() {
  qts::AmaTickSequenceTracker tracker;
  tracker.observe(102, 5, 1000);
  tracker.observe(102, 5, 1001);
  const auto same = tracker.observe(102, 5, 1001);
  assert(same.accept == qts::AmaTickAccept::duplicate && !same.backfill.has_value());
  const auto older = tracker.observe(102, 5, 999);
  assert(older.accept == qts::AmaTickAccept::duplicate && !older.backfill.has_value());
  const auto corrupt = tracker.observe(102, 5, 0);
  assert(corrupt.accept == qts::AmaTickAccept::duplicate && !corrupt.backfill.has_value());
  // Edge unmoved: next expected seq still reports the same hole.
  const auto gap = tracker.observe(102, 5, 1005);
  assert(gap.accept == qts::AmaTickAccept::gap);
  assert(gap.backfill->begin_seq == 1002 && gap.backfill->end_seq == 1004);
}

void tracker_channels_are_independent() {
  qts::AmaTickSequenceTracker tracker;
  tracker.observe(102, 5, 1000);
  // Another channel and another market start their own baselines.
  const auto other_channel = tracker.observe(102, 6, 1);
  assert(other_channel.accept == qts::AmaTickAccept::accepted);
  const auto other_market = tracker.observe(101, 5, 1);
  assert(other_market.accept == qts::AmaTickAccept::accepted);
  assert(tracker.tracked_channels() == 3);
  // Gaps are reported against the right edge.
  const auto gap = tracker.observe(102, 6, 4);
  assert(gap.accept == qts::AmaTickAccept::gap);
  assert(gap.backfill->begin_seq == 2 && gap.backfill->end_seq == 3);
}

void tracker_day_boundary_clears() {
  qts::AmaTickSequenceTracker tracker;
  tracker.observe(102, 5, 1000);
  tracker.clear();  // seq numbers reset daily, same beat as settle_day
  assert(tracker.tracked_channels() == 0);
  // Restarted numbering is a fresh baseline, not a gap.
  const auto restart = tracker.observe(102, 5, 1);
  assert(restart.accept == qts::AmaTickAccept::accepted && !restart.backfill.has_value());
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
