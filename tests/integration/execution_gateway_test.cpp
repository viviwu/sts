// clang-format off
/**
  ******************************************************************************
  * @file           : execution_gateway_test.cpp
  * @author         : vivi wu
  * @brief          : Execution algorithms through the real OMS/risk/broker loop.
  * @version        : 0.1.0
  * @date           : 03/10/26
  ******************************************************************************
  */
// clang-format on
#include <cassert>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "sts/broker/simulated_broker.hpp"
#include "sts/execution/execution_api.hpp"
#include "sts/oms/order_manager.hpp"
#include "sts/persistence/ledger.hpp"

namespace {

sts::Timestamp at_second(int seconds) { return sts::Timestamp{std::chrono::seconds{seconds}}; }

sts::OrderIntent make_parent(const std::string& id, sts::Quantity quantity, sts::Price limit) {
  sts::OrderIntent intent{id, "paper", "test", "TEST", sts::Side::buy, sts::OrderType::limit, sts::TimeInForce::day, quantity, limit};
  intent.trace_id = id;
  return intent;
}

sts::QuoteEvent volume_quote(std::uint64_t sequence, sts::Quantity cumulative) {
  sts::QuoteEvent quote{};
  quote.instrument_id = "TEST";
  quote.last_price = 1'000;
  quote.cumulative_volume = cumulative;
  quote.sequence = sequence;
  quote.quality = sts::QuoteQuality::valid;
  quote.market_phase = sts::MarketPhase::continuous;
  return quote;
}

struct GatewayHarness {
  sts::EventBus events;
  sts::SimulatedBroker broker{events};
  sts::RiskEngine risk{{1'000'000, 0, true}};
  sts::InMemoryEventLedger ledger{};
  sts::OrderManager oms{broker, risk, events, &ledger};
  std::unique_ptr<sts::ExecutionAlgorithm> execution;
  sts::Timestamp now{at_second(1'000'000)};
  int submitted{0};

  explicit GatewayHarness(std::unique_ptr<sts::ExecutionAlgorithm> algo)
      : execution{std::move(algo)} {
    broker.set_starting_cash(10'000'000);
    broker.set_reference_price("TEST", 1'000);
    // Gateway production wiring: every OMS report reaches the algorithm
    // (iceberg refresh, live-child pruning).
    events.subscribe_execution([this](const sts::ExecutionReport& report) { execution->on_execution_report(report); });
  }

  void poll_and_submit() {
    for (const auto& child : execution->poll(now)) {
      assert(oms.submit(child));
      ++submitted;
    }
  }

  // Gateway shutdown path: parents fan out through the algorithm first
  // (unknown parents tolerated), stragglers fall to the direct sweep.
  void shutdown_fanout() {
    for (const auto& parent : oms.open_parent_ids()) {
      std::vector<sts::ClientOrderId> children;
      try {
        children = execution->cancel_parent(parent);
      } catch (const std::exception&) {
        continue;
      }
      for (const auto& child : children) oms.cancel(child);
    }
    for (const auto& order : oms.open_orders()) oms.cancel(order.client_order_id);
    assert(oms.open_orders().empty());
    assert(oms.reconcile_open_orders({}).empty());
    assert(!ledger.replay(1).empty());
  }
};

void immediate_fills_and_cancels_through_oms() {
  GatewayHarness harness{std::make_unique<sts::ImmediateAlgorithm>()};
  // Resting child goes live (800 < 1000 reference): cancel fans out to it.
  harness.execution->add_parent(make_parent("imm-rest", 100, 800));
  harness.poll_and_submit();
  assert(harness.submitted == 1);
  const auto children = harness.execution->cancel_parent("imm-rest");
  assert(children.size() == 1);
  assert(harness.oms.cancel(children[0]));
  harness.poll_and_submit();
  assert(harness.submitted == 1);  // nothing more for a dead parent
  // Crossing child fills at once.
  harness.execution->add_parent(make_parent("imm-fill", 100, 2'000));
  harness.poll_and_submit();
  assert(harness.submitted == 2);
  harness.shutdown_fanout();
}

void twap_slices_cancel_and_replace() {
  GatewayHarness harness{std::make_unique<sts::TwapAlgorithm>(sts::TwapConfig{2, std::chrono::seconds{1}})};
  harness.execution->add_parent(make_parent("twap-1", 200, 800));
  harness.poll_and_submit();  // first slice at anchor
  assert(harness.submitted == 1);
  harness.poll_and_submit();  // interval not elapsed: nothing due
  assert(harness.submitted == 1);
  harness.now = at_second(1'000'001);
  harness.poll_and_submit();  // second slice
  assert(harness.submitted == 2);
  // Both slices rest live: fan out cancels both, nothing re-emits.
  const auto children = harness.execution->cancel_parent("twap-1");
  assert(children.size() == 2);
  for (const auto& child : children) assert(harness.oms.cancel(child));
  harness.now = at_second(1'000'005);
  harness.poll_and_submit();
  assert(harness.submitted == 2);
  // Replace re-plans the next generation under a new suffix.
  harness.execution->add_parent(make_parent("twap-2", 200, 800));
  harness.poll_and_submit();
  assert(harness.submitted == 3);
  auto updated = make_parent("twap-2", 400, 800);
  const auto replanned = harness.execution->replace_parent(updated);
  assert(!replanned.empty());
  for (const auto& child : replanned) {
    if (harness.oms.find(child) != nullptr) harness.oms.cancel(child);
  }
  harness.shutdown_fanout();
}

void pov_follows_volume_then_cancels() {
  GatewayHarness harness{std::make_unique<sts::PovAlgorithm>(sts::PovConfig{10'000})};
  harness.execution->add_parent(make_parent("pov-1", 300, 800));
  harness.execution->on_quote(volume_quote(1, 0));
  harness.execution->on_quote(volume_quote(2, 1'000));
  harness.execution->on_quote(volume_quote(3, 2'000));
  harness.poll_and_submit();
  assert(harness.submitted >= 1);  // budget accrued from printed volume
  const auto children = harness.execution->cancel_parent("pov-1");
  for (const auto& child : children) harness.oms.cancel(child);
  harness.poll_and_submit();
  // Crossing parent converts fresh budget into fills (baseline + delta).
  harness.execution->add_parent(make_parent("pov-2", 100, 2'000));
  harness.execution->on_quote(volume_quote(4, 5'000));
  harness.execution->on_quote(volume_quote(5, 6'000));
  harness.poll_and_submit();
  assert(harness.submitted >= 2);
  harness.shutdown_fanout();
}

void vwap_curve_slices_through_oms() {
  sts::VwapConfig config{};
  config.weights_bps = {5000, 5000};
  config.interval = std::chrono::seconds{1};
  GatewayHarness harness{std::make_unique<sts::VwapAlgorithm>(config)};
  harness.execution->add_parent(make_parent("vwap-1", 200, 800));
  harness.poll_and_submit();
  assert(harness.submitted == 1);
  harness.now = at_second(1'000'001);
  harness.poll_and_submit();
  assert(harness.submitted == 2);
  const auto children = harness.execution->cancel_parent("vwap-1");
  assert(children.size() == 2);
  for (const auto& child : children) assert(harness.oms.cancel(child));
  harness.shutdown_fanout();
}

void iceberg_peaks_refresh_on_fills() {
  GatewayHarness harness{std::make_unique<sts::IcebergAlgorithm>(sts::IcebergConfig{100})};
  // Crossing parent: each 100-share peak fills at once, reports refresh the
  // next peak through the subscribed forwarding above.
  harness.execution->add_parent(make_parent("ice-1", 250, 2'000));
  harness.poll_and_submit();
  assert(harness.submitted == 1);
  harness.poll_and_submit();
  assert(harness.submitted == 2);
  harness.poll_and_submit();
  assert(harness.submitted == 3);  // 100 + 100 + 50 tail
  harness.poll_and_submit();
  assert(harness.submitted == 3);  // fully consumed: nothing more
  harness.shutdown_fanout();
}

}  // namespace

int main() {
  immediate_fills_and_cancels_through_oms();
  twap_slices_cancel_and_replace();
  pov_follows_volume_then_cancels();
  vwap_curve_slices_through_oms();
  iceberg_peaks_refresh_on_fills();
  return 0;
}
