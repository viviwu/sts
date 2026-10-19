// clang-format off
/**
  ******************************************************************************
  * @file           : event_store.cpp
  * @author         : vivi wu
  * @brief          : Generic async projection sink implementation and backend factory.
  * @version        : 0.1.0
  * @date           : 05/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/persistence/event_store.hpp"

#include <stdexcept>

#include "nlohmann/json.hpp"
#include "qts/persistence/pg_store.hpp"
#include "qts/persistence/sqlite_store.hpp"

namespace qts {

std::string event_type_name(LedgerEventType type) {
  switch (type) {
    case LedgerEventType::order_submitted:
      return "order_submitted";
    case LedgerEventType::execution_applied:
      return "execution_applied";
    case LedgerEventType::order_cancel_requested:
      return "order_cancel_requested";
    case LedgerEventType::risk_rejected:
      return "risk_rejected";
    case LedgerEventType::funding_deposited:
      return "funding_deposited";
    case LedgerEventType::reconcile_attested:
      return "reconcile_attested";
    case LedgerEventType::control_applied:
      return "control_applied";
  }
  return "order_submitted";
}

// Full payload kept for backtracking (events table); orders/executions
// are the indexed projection over the same stream.
std::string serialize_event_payload(const LedgerEvent& event) {
  const nlohmann::json node{{"event_id", event.envelope.event_id},
                            {"type", event_type_name(event.type)},
                            {"trace_id", event.envelope.trace_id},
                            {"config_version", event.envelope.config_version},
                            {"rules_version", event.envelope.rules_version},
                            {"source_message_id", event.envelope.source_message_id},
                            {"trade_date", event.envelope.trade_date},
                            {"intent",
                             {{"client_order_id", event.intent.client_order_id},
                              {"account_id", event.intent.account_id},
                              {"strategy_id", event.intent.strategy_id},
                              {"instrument_id", event.intent.instrument_id},
                              {"side", static_cast<int>(event.intent.side)},
                              {"type", static_cast<int>(event.intent.type)},
                              {"quantity", event.intent.quantity},
                              {"limit_price", event.intent.limit_price},
                              {"trace_id", event.intent.trace_id}}},
                            {"snapshot",
                             {{"order_id", event.snapshot.order_id},
                              {"client_order_id", event.snapshot.client_order_id},
                              {"status", static_cast<int>(event.snapshot.status)},
                              {"last_quantity", event.snapshot.last_quantity},
                              {"last_price", event.snapshot.last_price},
                              {"reason", event.snapshot.reason},
                              {"execution_id", event.snapshot.execution_id},
                              {"cumulative_quantity", event.snapshot.cumulative_quantity},
                              {"version", event.snapshot.version},
                              {"broker_order_id", event.snapshot.broker_order_id},
                              {"reject_code", event.snapshot.reject_code}}},
                            {"funding",
                             {{"strategy_id", event.funding.strategy},
                              {"instrument_id", event.funding.instrument},
                              {"quantity", event.funding.quantity},
                              {"average_cost", event.funding.average_cost},
                              {"cash", event.funding.cash}}}};
  return node.dump();
}

AsyncEventSink::AsyncEventSink(std::unique_ptr<EventStore> store)
    : store_(std::move(store)),
      watermark_(store_ != nullptr ? store_->latest_event_id() : 0),
      worker_{&AsyncEventSink::run, this} {
  if (store_ == nullptr) throw std::invalid_argument("async sink requires a store");
}

AsyncEventSink::~AsyncEventSink() { shutdown(); }

void AsyncEventSink::push(LedgerEvent event) {
  {
    std::scoped_lock lock{queue_mutex_};
    queue_.push(std::move(event));
  }
  pushed_.notify_one();
}

void AsyncEventSink::wait_empty() {
  std::unique_lock<std::mutex> lock{queue_mutex_};
  drained_.wait(lock, [&] { return queue_.empty() && !busy_; });
}

void AsyncEventSink::shutdown() {
  {
    std::scoped_lock lock{queue_mutex_};
    if (joined_) return;
    stop_ = true;
    joined_ = true;
  }
  pushed_.notify_one();
  if (worker_.joinable()) worker_.join();
}

void AsyncEventSink::run() {
  for (;;) {
    LedgerEvent next{};
    {
      std::unique_lock<std::mutex> lock{queue_mutex_};
      pushed_.wait(lock, [&] { return stop_ || !queue_.empty(); });
      if (stop_ && queue_.empty()) return;
      next = std::move(queue_.front());
      queue_.pop();
      busy_ = true;
    }
    store_->apply(next);
    {
      std::scoped_lock lock{queue_mutex_};
      busy_ = false;
    }
    drained_.notify_all();
  }
}

std::unique_ptr<EventStore> open_event_store(const std::string& sqlite_file, const std::string& pg_conninfo) {
  if (!sqlite_file.empty() && !pg_conninfo.empty()) {
    throw std::runtime_error("sqlite_file and postgres conninfo are mutually exclusive: configure exactly one projection backend");
  }
  if (!sqlite_file.empty()) return std::make_unique<SqliteEventStore>(sqlite_file);
  if (!pg_conninfo.empty()) return std::make_unique<PgEventStore>(pg_conninfo);
  return nullptr;
}

}  // namespace qts
