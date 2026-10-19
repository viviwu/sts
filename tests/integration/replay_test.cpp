// clang-format off
/**
  ******************************************************************************
  * @file           : replay_test.cpp
  * @author         : vivi wu
  * @brief          : Replay determinism test: identical streams converge twice.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include <cassert>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "sts/broker/simulated_broker.hpp"
#include "sts/execution/execution_api.hpp"
#include "sts/oms/order_manager.hpp"
#include "sts/persistence/ledger.hpp"
#include "sts/research/replayer.hpp"
#include "sts/strategy/grid_strategy.hpp"

namespace {

const std::vector<sts::Price> kPath{1'000, 900, 950, 850, 900, 950, 1'000, 1'050};

sts::GridConfig grid_config() {
  sts::GridConfig config{};
  config.strategy_id = "grid-1";
  config.account_id = "paper";
  config.instrument_id = "TEST";
  config.lower = 900;
  config.upper = 1'100;
  config.grids = 4;
  config.quantity_per_order = 100;
  return config;
}

struct RunSummary {
  int submitted{0};
  std::vector<std::string> order_ids;
  std::vector<sts::ExecutionReport> finals;
  std::vector<sts::LedgerEvent> ledger;
  sts::Price cash{0};
  sts::Quantity position{0};
  sts::Timestamp end_time{};
};

RunSummary run_once() {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  broker.set_starting_cash(1'000'000);
  sts::RiskEngine risk{{10'000, 0, true}};
  sts::InMemoryEventLedger ledger{};
  sts::OrderManager oms{broker, risk, events, &ledger};
  sts::ManualClock clock{};
  sts::GridStrategy strategy{grid_config(), 1'000};
  sts::ImmediateAlgorithm execution{};
  sts::Replayer replayer{strategy, execution, oms, broker, clock, broker.book(), events};

  RunSummary summary{};
  std::uint64_t sequence = 0;
  for (const sts::Price price : kPath) {
    sts::QuoteEvent quote{};
    quote.instrument_id = "TEST";
    quote.last_price = price;
    quote.sequence = ++sequence;
    quote.quality = sts::QuoteQuality::valid;
    const int before = summary.submitted;
    summary.submitted += replayer.run({quote}).submitted;
    for (int i = before; i < summary.submitted; ++i) {
      summary.order_ids.push_back("grid-" + std::to_string(i + 1) + "#0");
    }
  }
  for (const auto& id : summary.order_ids) summary.finals.push_back(*oms.find(id));
  summary.ledger = ledger.replay(1);
  broker.set_account_snapshot_handler([&](const sts::TradeAccountSnapshot& snapshot) {
    summary.cash = snapshot.available_cash;
    for (const auto& position : snapshot.positions) {
      if (position.instrument_id == "TEST") summary.position = position.total_quantity;
    }
  });
  broker.request_account_snapshot("paper");
  summary.end_time = clock.now();
  return summary;
}

void replay_is_deterministic() {
  const RunSummary first = run_once();
  const RunSummary second = run_once();

  // Golden values for this path: 3 grid buys + 4 take-profit sells.
  assert(first.submitted == 7);
  assert(first.ledger.size() == 21);
  assert(first.cash == 1'130'000);
  assert(first.position == -100);
  assert(first.end_time == sts::Timestamp{std::chrono::seconds{8}});

  // Second run over identical bars reproduces everything field by field.
  assert(second.submitted == first.submitted);
  assert(second.order_ids == first.order_ids);
  assert(second.finals.size() == first.finals.size());
  for (std::size_t i = 0; i < first.finals.size(); ++i) {
    assert(second.finals[i].status == first.finals[i].status);
    assert(second.finals[i].cumulative_quantity == first.finals[i].cumulative_quantity);
    assert(second.finals[i].version == first.finals[i].version);
    assert(second.finals[i].order_id == first.finals[i].order_id);
  }
  assert(second.ledger.size() == first.ledger.size());
  for (std::size_t i = 0; i < first.ledger.size(); ++i) {
    assert(second.ledger[i].type == first.ledger[i].type);
    assert(second.ledger[i].snapshot.status == first.ledger[i].snapshot.status);
    assert(second.ledger[i].snapshot.cumulative_quantity == first.ledger[i].snapshot.cumulative_quantity);
    assert(second.ledger[i].envelope.event_id == first.ledger[i].envelope.event_id);
  }
  assert(second.cash == first.cash);
  assert(second.position == first.position);
  assert(second.end_time == first.end_time);
}

// Single-shot parent for the iceberg replay drill: emits one 250-unit
// parent on the first quote, then stays silent. Peaks only refresh when
// fill reports are forwarded to the algorithm.
class SingleParentStrategy final : public sts::Strategy {
 public:
  std::string_view name() const noexcept override { return "single-parent"; }
  std::vector<sts::OrderIntent> on_quote(const sts::QuoteEvent& quote, const sts::Portfolio& /*book*/) override {
    if (emitted_) return {};
    emitted_ = true;
    sts::OrderIntent intent{};
    intent.client_order_id = "ice-1";
    intent.account_id = "paper";
    intent.strategy_id = "ice";
    intent.instrument_id = quote.instrument_id;
    intent.side = sts::Side::buy;
    intent.type = sts::OrderType::limit;
    intent.quantity = 250;
    intent.limit_price = 1'000;
    intent.trace_id = "trace-ice-1";
    return {intent};
  }

 private:
  bool emitted_{false};
};

int run_iceberg_replay(std::vector<std::string>& order_ids) {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  broker.set_starting_cash(1'000'000);
  sts::RiskEngine risk{{10'000, 0, true}};
  sts::InMemoryEventLedger ledger{};
  sts::OrderManager oms{broker, risk, events, &ledger};
  sts::ManualClock clock{};
  SingleParentStrategy strategy{};
  sts::IcebergConfig iceberg{};
  iceberg.display_quantity = 100;
  sts::IcebergAlgorithm execution{iceberg};
  sts::Replayer replayer{strategy, execution, oms, broker, clock, broker.book(), events};

  int submitted = 0;
  std::uint64_t sequence = 0;
  for (int bar = 0; bar < 4; ++bar) {
    sts::QuoteEvent quote{};
    quote.instrument_id = "TEST";
    quote.last_price = 1'000;
    quote.sequence = ++sequence;
    quote.quality = sts::QuoteQuality::valid;
    submitted += replayer.run({quote}).submitted;
  }
  for (const auto& id : std::vector<std::string>{"ice-1#0", "ice-1#1", "ice-1#2"}) {
    if (oms.find(id) != nullptr) order_ids.push_back(id);
  }
  return submitted;
}

void replay_forwards_reports_to_execution() {
  // 250 with display 100 must surface as 100/100/50 across bars; without
  // report forwarding only the first peak would ever emit.
  std::vector<std::string> first_ids;
  const int first = run_iceberg_replay(first_ids);
  assert(first == 3);
  assert((first_ids == std::vector<std::string>{"ice-1#0", "ice-1#1", "ice-1#2"}));

  // Same bars reproduce the same children: replay stays deterministic.
  std::vector<std::string> second_ids;
  const int second = run_iceberg_replay(second_ids);
  assert(second == first);
  assert(second_ids == first_ids);
}

}  // namespace

int main() {
  replay_is_deterministic();
  replay_forwards_reports_to_execution();
  return 0;
}
