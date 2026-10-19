// clang-format off
/**
  ******************************************************************************
  * @file           : throughput_bench.cpp
  * @author         : vivi wu
  * @brief          : Hot-path throughput baseline (informational, no hard gate).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>

#include "qts/broker/simulated_broker.hpp"
#include "qts/execution/execution_api.hpp"
#include "qts/oms/order_manager.hpp"

namespace {

constexpr int kParents = 20000;

// Hot-path baseline (M5-2, zero infra): strategy-grade parents flow through
// Immediate execution -> risk -> OMS -> paper matching with no ledger. The
// run asserts full acceptance + terminal fills and prints elapsed time and
// throughput. No hard perf assertion: numbers are a recorded baseline for
// future SLA work (roadmap: optimize only against measured bottlenecks),
// not a CI gate, so the test stays non-flaky across machines.
void hot_path_throughput() {
  qts::EventBus events;
  qts::SimulatedBroker broker{events};
  broker.set_starting_cash(1'000'000'000);
  broker.set_reference_price("TEST", 1'000);
  qts::RiskEngine risk{{1'000'000, 0, true}};
  qts::OrderManager oms{broker, risk, events, nullptr};
  qts::ImmediateAlgorithm execution{};

  for (int i = 0; i < kParents; ++i) {
    qts::OrderIntent intent{};
    intent.client_order_id = "bench-" + std::to_string(i);
    intent.account_id = "paper";
    intent.strategy_id = "bench";
    intent.instrument_id = "TEST";
    intent.side = qts::Side::buy;
    intent.type = qts::OrderType::limit;
    intent.quantity = 100;
    intent.limit_price = 1'000;
    intent.trace_id = "trace-bench-" + std::to_string(i);
    execution.add_parent(intent);
  }

  const auto start = std::chrono::steady_clock::now();
  int submitted = 0;
  for (const auto& child : execution.poll(qts::Timestamp{})) {
    assert(oms.submit(child));
    ++submitted;
  }
  const auto elapsed = std::chrono::steady_clock::now() - start;
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

  assert(submitted == kParents);
  const auto* first = oms.find("bench-0#0");
  const auto* last = oms.find("bench-" + std::to_string(kParents - 1) + "#0");
  assert(first != nullptr && first->status == qts::OrderStatus::filled);
  assert(last != nullptr && last->status == qts::OrderStatus::filled);

  const std::int64_t per_sec = static_cast<std::int64_t>(submitted) * 1000 / (ms > 0 ? ms : 1);
  std::printf("bench hot_path: parents=%d submitted=%d elapsed_ms=%lld orders_per_sec=%lld\n", kParents, submitted, static_cast<long long>(ms), static_cast<long long>(per_sec));
}

}  // namespace

int main() {
  hot_path_throughput();
  return 0;
}
