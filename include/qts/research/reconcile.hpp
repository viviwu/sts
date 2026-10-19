// clang-format off
/**
  ******************************************************************************
  * @file           : reconcile.hpp
  * @author         : vivi wu
  * @brief          : Settlement reconciliation: broker snapshot versus book diff.
  * @version        : 0.1.0
  * @date           : 11/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include "qts/broker/trade_api.hpp"
#include "qts/portfolio/portfolio.hpp"

namespace qts::reconcile {

// Settlement diff (M14, option A): QTS book versus one broker account
// snapshot. Only comparable fields judge clean/dirty; broker-only fields
// (total/withdrawable/frozen cash, frozen inventory) are disclosed, never
// judged. Fee stance (ADR-046): QTS discloses its own ledger-carried fee
// total; the broker publishes no fee total to compare against, so fee gaps
// surface inside the cash difference by construction.
//
// Positions compare at account level: broker snapshots carry no strategy
// attribution, so per-basket books aggregate first. Row order is sorted by
// instrument for deterministic reports.
struct CashRow {
  std::string field;
  Price qts_value{0};
  Price broker_value{0};
  bool comparable{true};
};

struct PositionRow {
  InstrumentId instrument;
  Quantity qts_total{0};
  Quantity broker_total{0};
  Quantity qts_sellable{0};
  Quantity broker_sellable{0};
  Price qts_avg{0};
  Price broker_avg{0};
  Quantity broker_frozen{0};  // informational only, never judged
};

struct BooksDiff {
  std::vector<CashRow> cash;
  std::vector<PositionRow> positions;
  Price qts_fees_disclosed{0};

  [[nodiscard]] int cash_dirty() const {
    int dirty = 0;
    for (const auto& row : cash) {
      if (row.comparable && row.qts_value != row.broker_value) ++dirty;
    }
    return dirty;
  }

  [[nodiscard]] int position_dirty() const {
    int dirty = 0;
    for (const auto& row : positions) {
      if (row.qts_total != row.broker_total || row.qts_sellable != row.broker_sellable || row.qts_avg != row.broker_avg) ++dirty;
    }
    return dirty;
  }

  [[nodiscard]] bool clean() const { return cash_dirty() == 0 && position_dirty() == 0; }
};

inline BooksDiff diff_books(const Portfolio& book, const TradeAccountSnapshot& broker) {
  BooksDiff diff{};
  CashRow available{};
  available.field = "available_cash";
  available.qts_value = book.cash();
  available.broker_value = broker.available_cash;
  available.comparable = true;
  diff.cash.push_back(available);
  for (const auto& info : {std::pair<const char*, Price>{"total_asset", broker.total_asset}, {"withdrawable_cash", broker.withdrawable_cash}, {"frozen_cash", broker.frozen_cash}}) {
    CashRow row{};
    row.field = info.first;
    row.qts_value = 0;
    row.broker_value = info.second;
    row.comparable = false;
    diff.cash.push_back(row);
  }
  const auto holdings = book.all_positions();
  std::unordered_set<InstrumentId> seen;
  std::vector<InstrumentId> universe;
  for (const auto& entry : holdings) {
    if (seen.insert(entry.first).second) universe.push_back(entry.first);
  }
  for (const auto& position : broker.positions) {
    if (seen.insert(position.instrument_id).second) universe.push_back(position.instrument_id);
  }
  std::sort(universe.begin(), universe.end());
  for (const auto& instrument : universe) {
    PositionRow row{};
    row.instrument = instrument;
    const auto held = holdings.find(instrument);
    if (held != holdings.end()) {
      row.qts_total = held->second.total;
      row.qts_sellable = held->second.sellable;
      row.qts_avg = held->second.average_cost;
    }
    for (const auto& position : broker.positions) {
      if (position.instrument_id != instrument) continue;
      row.broker_total = position.total_quantity;
      row.broker_sellable = position.sellable_quantity;
      row.broker_avg = position.average_cost;
      row.broker_frozen = position.frozen_quantity;
      break;
    }
    diff.positions.push_back(row);
  }
  diff.qts_fees_disclosed = book.total_fees();
  return diff;
}

}  // namespace qts::reconcile
