// clang-format off
/**
  ******************************************************************************
  * @file           : report.hpp
  * @author         : vivi wu
  * @brief          : Pure-function run report: fills, gross, drawdown and version match.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "qts/persistence/ledger.hpp"

namespace qts {

// Run report over a ledger stream. Pure function: same events always give
// the same report, which is what makes experiments reproducible. Gross
// figures come from fill notional (last_quantity * last_price); fees are
// invisible to the ledger and stay in Portfolio/broker books. Drawdown is
// measured on the cumulative gross fill series and therefore excludes open
// inventory marks -- a marked-to-market drawdown needs the M4 market feed.
struct RunReport {
  int submitted{0};
  int filled{0};
  int cancelled{0};
  int rejected{0};
  int fills{0};
  Quantity bought_quantity{0};
  Quantity sold_quantity{0};
  Price gross_buy{0};
  Price gross_sell{0};
  Price gross_pnl{0};     // gross_sell - gross_buy
  Price total_fees{0};    // sum of per-fill fees carried on execution reports
  Price net_pnl{0};       // gross_pnl - total_fees
  Price max_drawdown{0};  // worst peak-to-trough on the gross series
  std::unordered_map<std::string, int> rejection_reasons;
  bool versions_match{false};  // every event carries the expected config/rules versions
};

inline void accumulate_fill(RunReport& report,
                            const LedgerEvent& event,
                            std::unordered_set<ClientOrderId>& filled_ids,
                            std::unordered_set<ClientOrderId>& cancelled_ids,
                            std::unordered_set<ClientOrderId>& rejected_ids,
                            Price& cumulative,
                            Price& peak) {
  if (event.snapshot.last_quantity > 0) {
    ++report.fills;
    const Price notional = event.snapshot.last_quantity * event.snapshot.last_price;
    report.total_fees += event.snapshot.fee;
    if (event.intent.side == Side::buy) {
      report.bought_quantity += event.snapshot.last_quantity;
      report.gross_buy += notional;
      cumulative -= notional;
    } else {
      report.sold_quantity += event.snapshot.last_quantity;
      report.gross_sell += notional;
      cumulative += notional;
    }
    peak = std::max(peak, cumulative);
    report.max_drawdown = std::max(report.max_drawdown, peak - cumulative);
  }
  if (event.snapshot.status == OrderStatus::filled) filled_ids.insert(event.snapshot.client_order_id);
  if (event.snapshot.status == OrderStatus::cancelled) cancelled_ids.insert(event.snapshot.client_order_id);
  if (event.snapshot.status == OrderStatus::rejected) rejected_ids.insert(event.snapshot.client_order_id);
}

[[nodiscard]] inline RunReport summarize_ledger(const std::vector<LedgerEvent>& events, const std::string& config_version, const std::string& rules_version) {
  RunReport report{};
  report.versions_match = true;
  std::unordered_set<ClientOrderId> filled_ids;
  std::unordered_set<ClientOrderId> cancelled_ids;
  std::unordered_set<ClientOrderId> rejected_ids;
  Price cumulative = 0;
  Price peak = 0;
  for (const auto& event : events) {
    if (event.envelope.config_version != config_version || event.envelope.rules_version != rules_version) {
      report.versions_match = false;
    }
    if (event.type == LedgerEventType::order_submitted) ++report.submitted;
    if (event.type == LedgerEventType::risk_rejected) {
      ++report.rejection_reasons[event.snapshot.reason];
      rejected_ids.insert(event.snapshot.client_order_id);
    }
    if (event.type != LedgerEventType::execution_applied) continue;
    accumulate_fill(report, event, filled_ids, cancelled_ids, rejected_ids, cumulative, peak);
  }
  report.filled = static_cast<int>(filled_ids.size());
  report.cancelled = static_cast<int>(cancelled_ids.size());
  report.rejected = static_cast<int>(rejected_ids.size());
  report.gross_pnl = report.gross_sell - report.gross_buy;
  report.net_pnl = report.gross_pnl - report.total_fees;
  return report;
}

}  // namespace qts
