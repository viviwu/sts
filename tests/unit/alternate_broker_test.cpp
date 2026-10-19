// clang-format off
/**
  ******************************************************************************
  * @file           : alternate_broker_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for the alternate paper channel (admission, drain, resume).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include <atomic>
#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

#include "sts/broker/alternate_paper_broker.hpp"
#include "sts/broker/simulated_broker.hpp"
#include "sts/oms/order_manager.hpp"
#include "sts/strategy/grid_strategy.hpp"

namespace {

std::filesystem::path fresh_temp(const std::string& tag) {
  static std::atomic<unsigned> counter{0};
  const unsigned n = counter.fetch_add(1);
  const auto path = std::filesystem::temp_directory_path() / ("sts-" + tag + "-" + std::to_string(n) + ".seq");
  std::error_code ec;
  std::filesystem::remove(path, ec);
  return path;
}

sts::OrderIntent make_intent(const std::string& id, sts::Quantity qty = 200, sts::Price price = 1'000) {
  sts::OrderIntent intent{id, "paper", "test", "TEST", sts::Side::buy, sts::OrderType::limit, sts::TimeInForce::day, qty, price};
  intent.trace_id = "trace-" + id;
  return intent;
}

void channel_differs_from_simulated() {
  sts::EventBus events;
  sts::AlternatePaperBroker broker{events, fresh_temp("channel").string()};
  assert(broker.channel() == "alternate");
  assert(std::string{sts::SimulatedBroker{events}.channel()} != std::string{broker.channel()});
}

void tick_and_lot_are_enforced() {
  sts::EventBus events;
  sts::AlternatePaperBroker broker{events, fresh_temp("admission").string()};
  std::vector<sts::ExecutionReport> delivered;
  broker.set_execution_handler([&](const sts::ExecutionReport& report) { delivered.push_back(report); });

  broker.submit(make_intent("bad-tick", 200, 951));   // 951 % 50 != 0
  broker.submit(make_intent("bad-lot", 100, 1'000));  // 100 % 200 != 0
  broker.drain();
  assert(delivered.size() == 2);
  assert(delivered[0].status == sts::OrderStatus::rejected);
  assert(delivered[0].reason == "price violates venue tick");
  assert(delivered[1].status == sts::OrderStatus::rejected);
  assert(delivered[1].reason == "quantity violates venue lot");
}

void session_gate_throws() {
  sts::EventBus events;
  sts::AlternatePaperBroker broker{events, fresh_temp("session").string()};
  broker.set_session_open(false);
  bool threw = false;
  try {
    broker.submit(make_intent("s1"));
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    broker.cancel("s1");
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
  broker.set_session_open(true);
  broker.submit(make_intent("s1"));
  broker.drain();  // live ack only: no reference price set
}

void delivery_is_serial_never_reentrant() {
  sts::EventBus events;
  sts::AlternatePaperBroker broker{events, fresh_temp("serial").string()};
  broker.set_reference_price("TEST", 1'000);
  int calls = 0;
  int max_depth = 0;
  int depth = 0;
  std::vector<sts::OrderStatus> order;
  broker.set_execution_handler([&](const sts::ExecutionReport& report) {
    ++depth;
    max_depth = depth > max_depth ? depth : max_depth;
    ++calls;
    order.push_back(report.status);
    if (calls == 1) broker.submit(make_intent("nested"));  // must not recurse
    --depth;
  });
  broker.submit(make_intent("first"));
  assert(calls == 0);  // nothing delivered inline
  assert(broker.pending_reports() == 2);
  broker.drain();
  assert(max_depth == 1);
  assert(order.size() == 4);  // live+fill for first, then live+fill for nested
  assert(order[0] == sts::OrderStatus::live && order[1] == sts::OrderStatus::filled);
  assert(order[2] == sts::OrderStatus::live && order[3] == sts::OrderStatus::filled);
  assert(broker.pending_reports() == 0);
}

void sequence_survives_restart() {
  const auto path = fresh_temp("seq");
  {
    sts::EventBus events;
    sts::AlternatePaperBroker broker{events, path.string()};
    broker.submit(make_intent("q1"));
    broker.submit(make_intent("q2"));
    broker.drain();
    assert(broker.pending_reports() == 0);
  }
  sts::EventBus events;
  sts::AlternatePaperBroker broker{events, path.string()};
  std::vector<sts::ExecutionReport> delivered;
  broker.set_execution_handler([&](const sts::ExecutionReport& report) { delivered.push_back(report); });
  broker.submit(make_intent("q3"));
  broker.drain();
  assert(delivered[0].order_id == "ALT-3");  // sequences 1-2 consumed before restart
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

sts::GridConfig compliant_grid() {
  sts::GridConfig config{};
  config.strategy_id = "grid-1";
  config.account_id = "paper";
  config.instrument_id = "TEST";
  config.lower = 900;  // levels 900..1100 step 50: all satisfy the 50-tick
  config.upper = 1'100;
  config.grids = 4;
  config.quantity_per_order = 200;  // satisfies the 200 lot on both channels
  return config;
}

struct DrillResult {
  int submitted{0};
  sts::Price cash{0};
  sts::Quantity position{0};
};

DrillResult drill_on_simulated() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  broker.set_starting_cash(1'000'000);
  sts::RiskEngine risk{{10'000, 0, true}};
  sts::OrderManager oms{broker, risk, events};
  sts::GridStrategy strategy{compliant_grid(), 1'000};
  const std::vector<sts::Price> path{1'000, 950, 900, 950, 1'000, 1'050, 1'100};
  DrillResult result{};
  std::uint64_t sequence = 0;
  for (const sts::Price price : path) {
    broker.set_reference_price("TEST", price);
    sts::QuoteEvent quote{};
    quote.instrument_id = "TEST";
    quote.last_price = price;
    quote.sequence = ++sequence;
    for (auto intent : strategy.on_quote(quote, broker.book())) {
      assert(oms.submit(intent));
      ++result.submitted;
    }
  }
  assert(oms.open_orders().empty());
  result.cash = broker.book().cash();
  for (const auto& kv : broker.book().all_positions()) result.position += kv.second.total;
  return result;
}

DrillResult drill_on_alternate(const std::string& seq_path) {
  sts::EventBus events;
  sts::AlternatePaperBroker broker{events, seq_path};
  broker.set_starting_cash(1'000'000);
  sts::RiskEngine risk{{10'000, 0, true}};
  sts::OrderManager oms{broker, risk, events};
  sts::GridStrategy strategy{compliant_grid(), 1'000};
  const std::vector<sts::Price> path{1'000, 950, 900, 950, 1'000, 1'050, 1'100};
  DrillResult result{};
  std::uint64_t sequence = 0;
  for (const sts::Price price : path) {
    broker.set_reference_price("TEST", price);
    sts::QuoteEvent quote{};
    quote.instrument_id = "TEST";
    quote.last_price = price;
    quote.sequence = ++sequence;
    for (auto intent : strategy.on_quote(quote, broker.book())) {
      assert(oms.submit(intent));
      ++result.submitted;
    }
    broker.drain();  // serial delivery: OMS learns fills here, not in submit
  }
  broker.drain();
  assert(oms.open_orders().empty());
  result.cash = broker.book().cash();
  for (const auto& kv : broker.book().all_positions()) result.position += kv.second.total;
  std::error_code ec;
  std::filesystem::remove(seq_path, ec);
  return result;
}

void same_strategy_same_outcome_on_both_channels() {
  const DrillResult sim = drill_on_simulated();
  const DrillResult alt = drill_on_alternate(fresh_temp("dual").string());
  assert(sim.submitted == alt.submitted);
  assert(sim.submitted == 6);
  assert(sim.cash == alt.cash);
  assert(sim.position == alt.position);
}

}  // namespace

int main() {
  channel_differs_from_simulated();
  tick_and_lot_are_enforced();
  session_gate_throws();
  delivery_is_serial_never_reentrant();
  sequence_survives_restart();
  same_strategy_same_outcome_on_both_channels();
  return 0;
}
