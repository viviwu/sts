// clang-format off
/**
  ******************************************************************************
  * @file           : event_bus.cpp
  * @author         : vivi wu
  * @brief          : Event-bus fan-out implementation (synchronous, ordered).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/core/event_bus.hpp"

namespace sts {

EventBus::Subscription EventBus::subscribe_execution(ExecutionHandler handler) {
  std::scoped_lock lock{mutex_};
  const Subscription token = next_token_++;
  execution_handlers_.emplace_back(token, std::move(handler));
  return token;
}

void EventBus::unsubscribe_execution(Subscription token) {
  std::scoped_lock lock{mutex_};
  for (auto iter = execution_handlers_.begin(); iter != execution_handlers_.end(); ++iter) {
    if (iter->first == token) {
      execution_handlers_.erase(iter);
      return;
    }
  }
}

void EventBus::publish(const ExecutionReport& report) const {
  std::vector<ExecutionHandler> handlers;
  {
    std::scoped_lock lock{mutex_};
    handlers.reserve(execution_handlers_.size());
    for (const auto& entry : execution_handlers_) handlers.push_back(entry.second);
  }
  for (const auto& handler : handlers) {
    handler(report);
  }
}

}  // namespace sts
