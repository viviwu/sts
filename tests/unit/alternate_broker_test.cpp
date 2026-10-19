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

#include "qts/broker/alternate_paper_broker.hpp"
#include "qts/broker/simulated_broker.hpp"
#include "qts/oms/order_manager.hpp"
#include "qts/strategy/grid_strategy.hpp"

namespace {

std::filesystem::path fresh_temp(const std::string& tag) {
  static std::atomic<unsigned> counter{0};
  const unsigned n = counter.fetch_add(1);
  const auto path = std::filesystem::temp_directory_path() / ("qts-" + tag + "-" + std::to_string(n) + ".seq");
  std::error_code ec;
  std::filesystem::remove(path, ec);
  return path;
}

qts::OrderIntent make_intent(const std::string& id, qts::Quantity qty = 200, qts::Price price = 1'000) {
  qts::OrderIntent intent{id, "paper", "test", "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, qty, price};
  intent.trace_id = "trace-" + id;
  return intent;
}

void channel_differs_from_simulated() {
  qts::EventBus events;
  qts::AlternatePaperBroker broker{events, fresh_temp("channel").string()};
  assert(broker.channel() == "alternate");
  assert(std::string{qts::SimulatedBroker{events}.channel()} != std::string{broker.channel()});
}

void tick_and_lot_are_enforced() {
  qts::EventBus events;
  qts::AlternatePaperBroker broker{events, fresh_temp("admission").string()};
  std::vector<qts::ExecutionReport> delivered;
  broker.set_execution_handler([&](const qts::ExecutionReport& report) { delivered.push_back(report); });

  broker.submit(make_intent("bad-tick", 200, 951));   // 951 % 50 != 0
  broker.submit(make_intent("bad-lot", 100, 1'000));  // 100 % 200 != 0
  broker.drain();
  assert(delivered.size() == 2);
  assert(delivered[0].status == qts::OrderStatus::rejected);
  assert(delivered[0].reason == "price violates venue tick");
  assert(delivered[1].status == qts::OrderStatus::rejected);
  assert(delivered[1].reason == "quantity violates venue lot");
}

void session_gate_throws() {
  qts::EventBus events;
  qts::AlternatePaperBroker broker{events, fresh_temp("session").string()};
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
  qts::EventBus events;
  qts::AlternatePaperBroker broker{events, fresh_temp("serial").string()};
  broker.set_reference_price("TEST", 1'000);
  int calls = 0;
  int max_depth = 0;
  int depth = 0;
  std::vector<qts::OrderStatus> order;
  broker.set_execution_handler([&](const qts::ExecutionReport& report) {
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
  assert(order[0] == qts::OrderStatus::live && order[1] == qts::OrderStatus::filled);
  assert(order[2] == qts::OrderStatus::live && order[3] == qts::OrderStatus::filled);
  assert(broker.pending_reports() == 0);
}

void sequence_survives_restart() {
  const auto path = fresh_temp("seq");
  {
    qts::EventBus events;
    qts::AlternatePaperBroker broker{events, path.string()};
    broker.submit(make_intent("q1"));
    broker.submit(make_intent("q2"));
    broker.drain();
    assert(broker.pending_reports() == 0);
  }
  qts::EventBus events;
  qts::AlternatePaperBroker broker{events, path.string()};
  std::vector<qts::ExecutionReport> delivered;
  broker.set_execution_handler([&](const qts::ExecutionReport& report) { delivered.push_back(report); });
  broker.submit(make_intent("q3"));
  broker.drain();
  assert(delivered[0].order_id == "ALT-3");  // sequences 1-2 consumed before restart
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

qts::GridConfig compliant_grid() {
  qts::GridConfig config{};
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
  qts::Price cash{0};
  qts::Quantity position{0};
};

DrillResult drill_on_simulated() {
  qts::EventBus events;
  qts::SimulatedBroker broker{events};
  broker.set_starting_cash(1'000'000);
  qts::RiskEngine risk{{10'000, 0, true}};
  qts::OrderManager oms{broker, risk, events};
  qts::GridStrategy strategy{compliant_grid(), 1'000};
  const std::vector<qts::Price> path{1'000, 950, 900, 950, 1'000, 1'050, 1'100};
  DrillResult result{};
  std::uint64_t sequence = 0;
  for (const qts::Price price : path) {
    broker.set_reference_price("TEST", price);
    qts::QuoteEvent quote{};
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
  qts::EventBus events;
  qts::AlternatePaperBroker broker{events, seq_path};
  broker.set_starting_cash(1'000'000);
  qts::RiskEngine risk{{10'000, 0, true}};
  qts::OrderManager oms{broker, risk, events};
  qts::GridStrategy strategy{compliant_grid(), 1'000};
  const std::vector<qts::Price> path{1'000, 950, 900, 950, 1'000, 1'050, 1'100};
  DrillResult result{};
  std::uint64_t sequence = 0;
  for (const qts::Price price : path) {
    broker.set_reference_price("TEST", price);
    qts::QuoteEvent quote{};
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
