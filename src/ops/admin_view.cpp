// clang-format off
/**
  ******************************************************************************
  * @file           : admin_view.cpp
  * @author         : vivi wu
  * @brief          : Offline projection aggregation implementation.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/ops/admin_view.hpp"

#include <unordered_map>

namespace sts::admin {
namespace {

bool is_terminal(OrderStatus status) noexcept { return status == OrderStatus::filled || status == OrderStatus::cancelled || status == OrderStatus::rejected; }

}  // namespace

std::vector<OrderView> summarize_orders(const std::vector<LedgerEvent>& events) {
  std::vector<OrderView> views;
  std::unordered_map<ClientOrderId, std::size_t> index;
  for (const auto& event : events) {
    if (event.type != LedgerEventType::execution_applied && event.type != LedgerEventType::order_submitted) continue;
    const auto order_id = event.snapshot.client_order_id.empty() ? event.intent.client_order_id : event.snapshot.client_order_id;
    if (order_id.empty()) continue;
    const auto found = index.find(order_id);
    if (found == index.end()) {
      OrderView view{};
      view.client_order_id = order_id;
      view.status = event.snapshot.status;
      view.cumulative_quantity = event.snapshot.cumulative_quantity;
      view.version = event.snapshot.version;
      view.last_event_id = event.envelope.event_id;
      index.emplace(order_id, views.size());
      views.push_back(view);
    } else {
      OrderView& view = views[found->second];
      if (event.envelope.event_id >= view.last_event_id) {
        view.status = event.snapshot.status;
        view.cumulative_quantity = event.snapshot.cumulative_quantity;
        view.version = event.snapshot.version;
        view.last_event_id = event.envelope.event_id;
      }
    }
  }
  return views;
}

std::vector<OrderView> open_orders(const std::vector<LedgerEvent>& events) {
  std::vector<OrderView> out;
  for (const auto& view : summarize_orders(events)) {
    if (!is_terminal(view.status)) out.push_back(view);
  }
  return out;
}

Portfolio rebuild_positions(const std::vector<LedgerEvent>& events, Price starting_cash) {
  // Startup cash rides the stream when the gateway funded it (M11-7): the
  // first non-zero cash leg wins, otherwise the caller's configured value
  // applies (pre-funding ledgers).
  Price cash = starting_cash;
  for (const auto& event : events) {
    if (event.type != LedgerEventType::funding_deposited) continue;
    if (event.funding.cash > 0) {
      cash = event.funding.cash;
      break;
    }
  }
  Portfolio book{cash};
  for (const auto& event : events) {
    if (event.type == LedgerEventType::funding_deposited) {
      if (event.funding.quantity <= 0 || event.funding.instrument.empty()) continue;  // cash leg
      const StrategyId basket = event.funding.strategy.empty() ? kPassthroughStrategy : event.funding.strategy;
      book.fund_settled(basket, event.funding.instrument, event.funding.quantity, event.funding.average_cost);
      continue;
    }
    if (event.type != LedgerEventType::execution_applied) continue;
    if (event.snapshot.last_quantity <= 0) continue;
    book.apply_fill(event.intent, event.snapshot.last_quantity, event.snapshot.last_price);
    book.apply_fee(event.snapshot.fee);  // ledger-carried fill fees (zero on pre-fee ledgers)
  }
  return book;
}

}  // namespace sts::admin
