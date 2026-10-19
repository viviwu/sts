// clang-format off
/**
  ******************************************************************************
  * @file           : event_bus.hpp
  * @author         : vivi wu
  * @brief          : Synchronous in-memory event bus for quotes and execution reports.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <functional>
#include <mutex>
#include <vector>

#include "sts/core/types.hpp"

namespace sts {

// Synchronous in-process bus for M0. Handlers run inline on the publisher's
// thread with a snapshot of the subscriber list: no persistence, no queue,
// no back-pressure. The M1 async ledger will consume the same immutable
// events (see core/event.hpp) without changing publisher call sites.
// Subscriptions are permanent unless released: subscribe_execution returns a
// token, unsubscribe_execution drops it. One-shot collectors (broker-truth
// queries) must release before their captured locals die — a dangling
// handler segfaults the next publish (found by restart e2e, M11-7).
class EventBus {
 public:
  using ExecutionHandler = std::function<void(const ExecutionReport&)>;
  using Subscription = std::uint64_t;

  // Non-nodiscard by design: lifelong subscribers (OMS, log taps) ignore
  // the token; only one-shot collectors keep it for release.
  Subscription subscribe_execution(ExecutionHandler handler);
  void unsubscribe_execution(Subscription token);
  void publish(const ExecutionReport& report) const;

 private:
  mutable std::mutex mutex_;
  std::uint64_t next_token_{1};
  std::vector<std::pair<Subscription, ExecutionHandler>> execution_handlers_;
};

}  // namespace sts
