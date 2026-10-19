// clang-format off
/**
  ******************************************************************************
  * @file           : replayer.hpp
  * @author         : vivi wu
  * @brief          : Deterministic quote-driven replayer over strategy, OMS and paper matching.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <chrono>
#include <cstdint>
#include <vector>

#include "sts/broker/quote_api.hpp"
#include "sts/broker/simulated_broker.hpp"
#include "sts/core/clock.hpp"
#include "sts/core/event_bus.hpp"
#include "sts/execution/execution_api.hpp"
#include "sts/oms/order_manager.hpp"
#include "sts/strategy/strategy_api.hpp"

namespace sts {

// Deterministic quote-driven replay (M2-1). Each bar advances the manual
// clock by a fixed step, stamps the quote, aligns the paper reference
// price, then routes strategy intents through risk/OMS/matching. Two runs
// over the same bars with equally-constructed components (fresh ledger
// starting at the same event id, epoch manual clock) must produce
// identical OMS states and ledger streams.
struct ReplayConfig {
  std::chrono::nanoseconds time_step{std::chrono::seconds{1}};
};

class Replayer {
 public:
  Replayer(Strategy& strategy, ExecutionAlgorithm& execution, OrderManager& oms, SimulatedBroker& broker, ManualClock& clock, const Portfolio& book, EventBus& events, ReplayConfig config = {})
      : strategy_{strategy},
        execution_{execution},
        oms_{oms},
        broker_{broker},
        clock_{clock},
        book_{book},
        events_{events},
        config_{config} {
    // Live-path parity (M4-4): execution sees every OMS report for
    // fill-driven algorithms (iceberg refresh, live-child pruning).
    events_.subscribe_execution([this](const ExecutionReport& report) { execution_.on_execution_report(report); });
  }

  struct Result {
    int quotes{0};
    int submitted{0};
  };

  Result run(const std::vector<QuoteEvent>& quotes) {
    Result result{};
    for (auto quote : quotes) {
      clock_.advance(config_.time_step);
      quote.receive_time = clock_.now();
      if (quote.last_price > 0) broker_.set_reference_price(quote.instrument_id, quote.last_price);
      execution_.on_quote(quote);  // no-op except for volume-tracking algorithms
      for (auto intent : strategy_.on_quote(quote, book_)) {
        execution_.add_parent(intent);
      }
      for (auto child : execution_.poll(clock_.now())) {
        if (oms_.submit(child)) ++result.submitted;
      }
      broker_.deliver_due();  // no-op without a latency model; keeps lagged drills coherent
      ++result.quotes;
    }
    return result;
  }

 private:
  Strategy& strategy_;
  ExecutionAlgorithm& execution_;
  OrderManager& oms_;
  SimulatedBroker& broker_;
  ManualClock& clock_;
  const Portfolio& book_;
  EventBus& events_;
  ReplayConfig config_;
};

}  // namespace sts
