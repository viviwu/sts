// clang-format off
/**
  ******************************************************************************
  * @file           : order_manager.cpp
  * @author         : vivi wu
  * @brief          : OMS state machine, idempotent reports, ledger recording and reconcile.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/oms/order_manager.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include "qts/persistence/ledger.hpp"

namespace qts {
namespace {

const char* status_name(OrderStatus status) {
  switch (status) {
    case OrderStatus::pending_new:
      return "pending_new";
    case OrderStatus::live:
      return "live";
    case OrderStatus::partially_filled:
      return "partially_filled";
    case OrderStatus::filled:
      return "filled";
    case OrderStatus::pending_cancel:
      return "pending_cancel";
    case OrderStatus::cancelled:
      return "cancelled";
    case OrderStatus::rejected:
      return "rejected";
  }
  return "unknown";
}

}  // namespace

OrderManager::OrderManager(TradeApi& gateway, RiskEngine& risk, EventBus& events, EventLedger* ledger)
    : gateway_{gateway},
      risk_{risk},
      events_{events},
      ledger_{ledger} {
  gateway_.set_execution_handler([this](const ExecutionReport& report) {
    if (on_execution_report(report)) events_.publish(report);
  });
}

OrderManager::~OrderManager() { gateway_.set_execution_handler({}); }

bool OrderManager::submit(const OrderIntent& intent) {
  // One strategy per order, enforced at the entry point: an empty
  // strategy_id means a manual pass-through order and is normalized before
  // risk, routing, recording and submission all see it.
  OrderIntent normalized = intent;
  if (normalized.strategy_id.empty()) normalized.strategy_id = kPassthroughStrategy;
  const OrderIntent& request = normalized;
  if (orders_.find(request.client_order_id) != orders_.end()) {
    last_reject_reason_ = "duplicate client order id";
    return false;
  }

  if (metrics_ != nullptr) metrics_->inc("oms_orders_submitted_total");
  const auto decision = risk_.check(request);
  if (!decision.allowed) {
    last_reject_reason_ = decision.reason;
    if (metrics_ != nullptr) metrics_->inc("oms_orders_risk_rejected_total");
    const ExecutionReport rejected{"", request.client_order_id, OrderStatus::rejected, 0, 0, decision.reason, "", 0, 0, "", 0};
    orders_.emplace(request.client_order_id, OrderRecord{request, rejected, {}, next_seq_++});
    record_event(LedgerEventType::risk_rejected, request, rejected);
    events_.publish(rejected);
    risk_.note_submit_rejected(request.strategy_id);  // M13 safety-net feed (counters only, never state)
    note_open_orders();
    return false;
  }

  const ExecutionReport pending{"", request.client_order_id, OrderStatus::pending_new, 0, 0, "", "", 0, 0, "", 0};
  orders_.emplace(request.client_order_id, OrderRecord{request, pending, {}, next_seq_++});
  record_event(LedgerEventType::order_submitted, request, pending);
  try {
    gateway_.submit(request);
  } catch (...) {
    auto& order = orders_.at(request.client_order_id).snapshot;
    order.status = OrderStatus::rejected;
    order.reason = "broker submission failed";
    last_reject_reason_ = order.reason;
    ++order.version;
    record_event(LedgerEventType::risk_rejected, request, order);
    events_.publish(order);
    if (metrics_ != nullptr) metrics_->inc("oms_orders_risk_rejected_total");
    risk_.note_submit_rejected(request.strategy_id);  // M13 safety-net feed (counters only, never state)
    note_open_orders();
    return false;
  }
  // T+1 reservation refresh: this live sell draws from the same sellable as
  // later same-tick submits (the per-tick sync re-asserts the absolute map,
  // so fills/cancels release with no leak). Skipped unless enforcement is
  // on and this is a sell — otherwise it is pure hot-path overhead.
  risk_.note_submit_accepted(request.strategy_id);  // M13 safety-net feed (clears the reject streak)
  last_reject_reason_.clear();
  if (risk_.sell_reservation_active() && request.side == Side::sell) risk_.set_sell_reserved(live_sell_exposure());
  // Buying-power reservation refresh, symmetric to sells.
  if (risk_.cash_reservation_active() && request.side == Side::buy) risk_.set_buy_reserved(live_buy_exposure());
  note_open_orders();
  return true;
}

bool OrderManager::cancel(const ClientOrderId& client_order_id) {
  const auto iter = orders_.find(client_order_id);
  if (iter == orders_.end() || (iter->second.snapshot.status != OrderStatus::live && iter->second.snapshot.status != OrderStatus::partially_filled)) {
    return false;
  }

  auto& record = iter->second;
  auto& snapshot = record.snapshot;
  snapshot.status = OrderStatus::pending_cancel;
  ++snapshot.version;
  record_event(LedgerEventType::order_cancel_requested, record.intent, snapshot);
  events_.publish(snapshot);
  if (metrics_ != nullptr) metrics_->inc("oms_cancels_requested_total");
  try {
    gateway_.cancel(client_order_id);
  } catch (...) {
    // A transport exception is ambiguous: keep PendingCancel and let the
    // recovery/reconciliation path determine the broker's actual state.
    note_open_orders();
    return false;
  }
  note_open_orders();
  return true;
}

bool OrderManager::on_execution_report(const ExecutionReport& report) {
  auto iter = orders_.find(report.client_order_id);
  if (iter == orders_.end()) return false;

  auto& record = iter->second;
  auto& current = record.snapshot;
  if (!report.execution_id.empty() && record.seen_execution_ids.find(report.execution_id) != record.seen_execution_ids.end()) {
    return true;
  }
  if (!current.order_id.empty() && !report.order_id.empty() && current.order_id != report.order_id) return false;
  if (report.last_quantity < 0 || report.last_price < 0) return false;

  const Quantity cumulative = report.cumulative_quantity > 0 ? report.cumulative_quantity : current.cumulative_quantity + report.last_quantity;
  if (cumulative < current.cumulative_quantity || cumulative > record.intent.quantity) return false;

  // Repeated terminal acknowledgements without an execution id are harmless.
  if (is_terminal(current.status) && report.status == current.status && cumulative == current.cumulative_quantity && report.last_quantity == 0) {
    return true;
  }
  if (!is_transition_allowed(current.status, report.status)) return false;

  ExecutionReport next = report;
  next.order_id = report.order_id.empty() ? current.order_id : report.order_id;
  next.cumulative_quantity = cumulative;
  next.version = current.version + 1;
  const bool was_rejected = current.status == OrderStatus::rejected;
  current = std::move(next);
  if (!was_rejected && current.status == OrderStatus::rejected) {
    risk_.note_broker_reject(record.intent.strategy_id);  // M13: first broker rejection only, duplicates never recount
  }
  if (!report.execution_id.empty()) record.seen_execution_ids.insert(report.execution_id);
  record_event(LedgerEventType::execution_applied, record.intent, current);
  if (metrics_ != nullptr) metrics_->inc("oms_executions_applied_total");
  note_open_orders();
  return true;
}

const ExecutionReport* OrderManager::find(const ClientOrderId& client_order_id) const {
  const auto iter = orders_.find(client_order_id);
  return iter == orders_.end() ? nullptr : &iter->second.snapshot;
}

const OrderIntent* OrderManager::find_intent(const ClientOrderId& client_order_id) const {
  const auto iter = orders_.find(client_order_id);
  return iter == orders_.end() ? nullptr : &iter->second.intent;
}

void OrderManager::set_ledger_versions(std::string config_version, std::string rules_version) {
  ledger_config_version_ = std::move(config_version);
  ledger_rules_version_ = std::move(rules_version);
}

void OrderManager::set_trade_date(std::string trade_date) { ledger_trade_date_ = std::move(trade_date); }

void OrderManager::set_metrics(MetricsRegistry* metrics) { metrics_ = metrics; }

void OrderManager::set_ledger(EventLedger* ledger) { ledger_ = ledger; }

void OrderManager::note_open_orders() const {
  if (metrics_ == nullptr) return;
  std::int64_t open = 0;
  for (const auto& entry : orders_) {
    if (!is_terminal(entry.second.snapshot.status)) ++open;
  }
  metrics_->set_gauge("oms_open_orders", open);
}

std::vector<ExecutionReport> OrderManager::open_orders() const {
  std::vector<ExecutionReport> out;
  for (const auto& entry : orders_) {
    if (!is_terminal(entry.second.snapshot.status)) out.push_back(entry.second.snapshot);
  }
  return out;
}

std::vector<OrderView> OrderManager::all_orders() const {
  std::vector<OrderView> out;
  out.reserve(orders_.size());
  for (const auto& entry : orders_) out.push_back({entry.second.intent, entry.second.snapshot, entry.second.seq});
  std::sort(out.begin(), out.end(), [](const OrderView& left, const OrderView& right) { return left.seq < right.seq; });
  return out;
}

RiskEngine::SellMap OrderManager::live_sell_exposure() const {
  RiskEngine::SellMap out;
  for (const auto& entry : orders_) {
    if (is_terminal(entry.second.snapshot.status)) continue;
    if (entry.second.intent.side != Side::sell) continue;
    const Quantity remainder = entry.second.intent.quantity - entry.second.snapshot.cumulative_quantity;
    if (remainder > 0) out[{entry.second.intent.strategy_id, entry.second.intent.instrument_id}] += remainder;
  }
  return out;
}

Price OrderManager::live_buy_exposure() const {
  Price out = 0;
  for (const auto& entry : orders_) {
    if (is_terminal(entry.second.snapshot.status)) continue;
    if (entry.second.intent.side != Side::buy) continue;
    if (entry.second.intent.type != OrderType::limit || entry.second.intent.limit_price <= 0) continue;
    const Quantity remainder = entry.second.intent.quantity - entry.second.snapshot.cumulative_quantity;
    if (remainder <= 0) continue;
    // Overflow-safe: absurd remainders saturate the reservation (fail closed
    // at the gate) instead of wrapping around it.
    if (remainder > std::numeric_limits<Price>::max() / entry.second.intent.limit_price) {
      out = std::numeric_limits<Price>::max();
      break;
    }
    out += remainder * entry.second.intent.limit_price;
  }
  return out;
}

std::vector<ClientOrderId> OrderManager::open_parent_ids() const {
  std::vector<ClientOrderId> out;
  for (const auto& entry : orders_) {
    if (is_terminal(entry.second.snapshot.status)) continue;
    const ClientOrderId& parent = entry.second.intent.parent_client_order_id;
    if (parent.empty()) continue;
    bool known = false;
    for (const auto& seen : out) {
      if (seen == parent) {
        known = true;
        break;
      }
    }
    if (!known) out.push_back(parent);
  }
  return out;
}

bool OrderManager::restore_order(const OrderIntent& intent, const ExecutionReport& snapshot) {
  if (orders_.find(intent.client_order_id) != orders_.end()) return false;
  if (snapshot.client_order_id != intent.client_order_id) return false;
  orders_.emplace(intent.client_order_id, OrderRecord{intent, snapshot, {}, next_seq_++});
  return true;
}

void OrderManager::settle_day() {
  for (auto& entry : orders_) entry.second.seen_execution_ids.clear();
}

std::vector<ReconcileDiff> OrderManager::reconcile_open_orders(const std::vector<ExecutionReport>& broker_open) const {
  std::vector<ReconcileDiff> diffs;
  for (const auto& broker : broker_open) {
    const auto iter = orders_.find(broker.client_order_id);
    if (iter == orders_.end()) {
      diffs.push_back({broker.client_order_id, "unknown to OMS"});
      continue;
    }
    const auto& local = iter->second.snapshot;
    if (!broker.order_id.empty() && !local.order_id.empty() && broker.order_id != local.order_id) {
      diffs.push_back({broker.client_order_id, "order id conflict"});
    } else if (broker.status != local.status) {
      diffs.push_back({broker.client_order_id, std::string("status mismatch (local=") + status_name(local.status) + " broker=" + status_name(broker.status) + ")"});
    } else if (broker.cumulative_quantity != local.cumulative_quantity) {
      diffs.push_back({broker.client_order_id, "cumulative quantity mismatch"});
    }
  }
  for (const auto& entry : orders_) {
    if (is_terminal(entry.second.snapshot.status)) continue;
    bool present = false;
    for (const auto& broker : broker_open) {
      if (broker.client_order_id == entry.first) {
        present = true;
        break;
      }
    }
    if (!present) diffs.push_back({entry.first, "missing at broker"});
  }
  return diffs;
}

void OrderManager::record_event(LedgerEventType type, const OrderIntent& intent, const ExecutionReport& snapshot) {
  if (ledger_ == nullptr) return;
  LedgerEvent event{};
  event.type = type;
  event.intent = intent;
  event.snapshot = snapshot;
  event.envelope.trace_id = intent.trace_id;
  event.envelope.config_version = ledger_config_version_;
  event.envelope.rules_version = ledger_rules_version_;
  event.envelope.trade_date = ledger_trade_date_;
  ledger_->append(std::move(event));
}

bool OrderManager::is_terminal(OrderStatus status) noexcept { return status == OrderStatus::filled || status == OrderStatus::cancelled || status == OrderStatus::rejected; }

bool OrderManager::is_transition_allowed(OrderStatus from, OrderStatus target) noexcept {
  if (from == target) return true;
  switch (from) {
    case OrderStatus::pending_new:
      return target == OrderStatus::live || target == OrderStatus::partially_filled || target == OrderStatus::filled || target == OrderStatus::rejected || target == OrderStatus::cancelled;
    case OrderStatus::live:
      return target == OrderStatus::partially_filled || target == OrderStatus::filled || target == OrderStatus::pending_cancel || target == OrderStatus::cancelled;
    case OrderStatus::partially_filled:
      return target == OrderStatus::partially_filled || target == OrderStatus::filled || target == OrderStatus::pending_cancel || target == OrderStatus::cancelled;
    case OrderStatus::pending_cancel:
      return target == OrderStatus::partially_filled || target == OrderStatus::filled || target == OrderStatus::cancelled;
    case OrderStatus::filled:
    case OrderStatus::cancelled:
    case OrderStatus::rejected:
      return false;
  }
  return false;
}

}  // namespace qts
