// clang-format off
/**
  ******************************************************************************
  * @file           : gateway_service_test.cpp
  * @author         : vivi wu
  * @brief          : GatewayService tests: sessions, scopes, orders, strategies, queries, subscriptions.
  * @version        : 0.1.0
  * @date           : 10/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/gateway/gateway_service.hpp"

#include <cassert>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "qts/broker/simulated_broker.hpp"
#include "qts/execution/execution_api.hpp"
#include "qts/persistence/ledger.hpp"
#include "qts/research/ledger_verify.hpp"
#include "qts/strategy/grid_strategy.hpp"

namespace {

qts::OrderIntent make_intent(const std::string& id, const std::string& strategy, qts::Quantity quantity, qts::Price limit) {
  qts::OrderIntent intent{id, "paper", strategy, "TEST", qts::Side::buy, qts::OrderType::limit, qts::TimeInForce::day, quantity, limit};
  intent.trace_id = id;
  return intent;
}

std::unique_ptr<qts::Strategy> make_test_grid(const qts::StrategyConfig& configured, const std::string& id_prefix) {
  if (configured.type != "grid") throw std::runtime_error("unknown strategy type: " + configured.type);
  qts::GridConfig grid{};
  grid.strategy_id = configured.id;
  grid.account_id = configured.account;
  grid.instrument_id = configured.instrument;
  grid.lower = configured.lower;
  grid.upper = configured.upper;
  grid.grids = configured.grids;
  grid.quantity_per_order = configured.quantity;
  grid.id_prefix = id_prefix;
  return std::make_unique<qts::GridStrategy>(grid, configured.start);
}

qts::StrategyConfig grid_ticket(const std::string& id);

struct Harness {
  qts::EventBus events;
  qts::SimulatedBroker broker{events};
  qts::RiskEngine risk{{1'000'000, 0, true}};
  qts::InMemoryEventLedger ledger{};
  qts::OrderManager oms{broker, risk, events, &ledger};
  qts::ImmediateAlgorithm execution{};
  qts::Portfolio book{10'000'000};
  std::vector<std::unique_ptr<qts::Strategy>> live;
  qts::control::StrategyRegistry registry{live, make_test_grid, "gateway-seed"};
  qts::control::ControlContext context{oms, execution, risk, &registry};
  qts::gateway::GatewayService front{context, book};

  Harness() { broker.set_starting_cash(10'000'000); }
};

void submit_ticket_validation() {
  Harness harness{};
  // Broken tickets fail closed with a reason, not a crash.
  assert(!harness.front.submit_order(make_intent("", "test", 100, 900)).ok);
  assert(!harness.front.submit_order(make_intent("g-1", "test", 0, 900)).ok);
  // Risk refusals stay loud and leave a rejected snapshot in OMS.
  harness.risk.set_instrument_blocked("TEST", true);
  assert(!harness.front.submit_order(make_intent("g-1", "test", 100, 900)).ok);
  assert(harness.oms.find("g-1")->status == qts::OrderStatus::rejected);
}

void submit_cancel_roundtrip() {
  Harness harness{};
  // Resting buy (limit under the unset reference) is accepted live.
  const auto accepted = harness.front.submit_order(make_intent("f-1", "test", 100, 900));
  assert(accepted.ok);
  std::vector<qts::ExecutionReport> opens;
  assert(harness.front.open_orders(opens).ok && opens.size() == 1);
  qts::ExecutionReport found{};
  assert(harness.front.find_order("f-1", found).ok && found.client_order_id == "f-1");
  assert(!harness.front.find_order("ghost", found).ok);
  // Duplicate ids refuse.
  assert(!harness.front.submit_order(make_intent("f-1", "test", 100, 900)).ok);
  assert(harness.front.cancel_order("f-1").ok);
  assert(harness.front.open_orders(opens).ok && opens.empty());
  // True ghosts stay loud through the shared cancel path.
  assert(!harness.front.cancel_order("ghost-1").ok);
}

void strategy_lifecycle() {
  Harness harness{};
  qts::StrategyConfig configured{};
  configured.id = "t-grid";
  configured.enabled = true;
  configured.type = "grid";
  configured.account = "paper";
  configured.instrument = "TEST";
  configured.lower = 900;
  configured.upper = 1100;
  configured.grids = 4;
  configured.quantity = 100;
  configured.start = 1000;
  assert(harness.front.start_strategy(configured).ok);
  assert(!harness.front.start_strategy(configured).ok);
  const auto listed = harness.front.list_strategies();
  assert(listed.ok && listed.message.find("t-grid grid paper TEST") != std::string::npos);
  assert(!harness.front.stop_strategy("nope").ok);
  assert(harness.front.stop_strategy("t-grid").ok);

  // Without a registry the verbs refuse loudly instead of crashing.
  qts::control::ControlContext bare{harness.oms, harness.execution, harness.risk};
  qts::gateway::GatewayService bare_front{bare, harness.book};
  assert(!bare_front.start_strategy(configured).ok);
}

void queries_read_live_books() {
  Harness harness{};
  qts::Price cash = 0;
  assert(harness.front.account_cash(cash).ok && cash == 10'000'000);
  harness.book.fund_settled("test", "TEST", 500, 950);
  std::unordered_map<qts::InstrumentId, qts::PositionView> positions;
  assert(harness.front.positions_for("test", positions).ok);
  assert(positions["TEST"].total == 500 && positions["TEST"].sellable == 500);
}

void account_scoped_queries_fills_and_progress() {
  Harness harness{};
  // Known accounts come from the server config; empty keeps legacy callers working.
  qts::gateway::GatewayService auth{harness.context, harness.book, {"paper"}};
  qts::Price cash = 0;
  assert(auth.account_cash(cash).ok);
  assert(auth.account_cash(cash, "paper").ok && cash == 10'000'000);
  assert(!auth.account_cash(cash, "ghost").ok);
  std::vector<qts::ExecutionReport> opens;
  assert(!auth.open_orders(opens, "ghost").ok);
  std::unordered_map<qts::InstrumentId, qts::PositionView> positions;
  assert(!auth.positions_for("test", positions, "ghost").ok);

  // f-live rests live, f-fill and f-partial take fills (strategy test = sub-account).
  assert(auth.submit_order(make_intent("f-live", "test", 100, 900)).ok);
  assert(auth.submit_order(make_intent("f-fill", "test", 100, 900)).ok);
  qts::ExecutionReport filled{"SIM-f-fill", "f-fill", qts::OrderStatus::filled, 100, 900, "", "e-fill", 100, 0};
  assert(harness.oms.on_execution_report(filled));
  assert(auth.open_orders(opens, "paper").ok && opens.size() == 1 && opens[0].client_order_id == "f-live");
  assert(auth.open_orders(opens).ok && opens.size() == 1);  // legacy unfiltered path unchanged

  std::vector<qts::gateway::FillView> rows;
  bool truncated = true;
  assert(auth.fills("paper", "test", 0, rows, truncated).ok);
  assert(!truncated && rows.size() == 1 && rows[0].report.client_order_id == "f-fill");
  assert(rows[0].intent.account_id == "paper" && rows[0].intent.strategy_id == "test" && rows[0].seq == 2);
  assert(!auth.fills("ghost", "test", 0, rows, truncated).ok);
  assert(auth.fills("paper", "other", 0, rows, truncated).ok && rows.empty());
  // Second fill overflows limit 1: newest first plus truncated.
  assert(auth.submit_order(make_intent("f-fill2", "test", 100, 900)).ok);
  qts::ExecutionReport filled2{"SIM-f-fill2", "f-fill2", qts::OrderStatus::filled, 100, 900, "", "e-fill2", 100, 0};
  assert(harness.oms.on_execution_report(filled2));
  assert(auth.fills("paper", "test", 1, rows, truncated).ok);
  assert(truncated && rows.size() == 1 && rows[0].report.client_order_id == "f-fill2");

  qts::gateway::StrategyProgress progress{};
  assert(!auth.strategy_progress("ghost", progress).ok);
  assert(!auth.strategy_progress("", progress).ok);
  assert(auth.strategy_progress("test", progress).ok);
  assert(progress.submitted == 3 && progress.open == 1 && progress.filled == 2 && !progress.running);
  // A live registry instance flips running without changing counters.
  assert(auth.start_strategy(grid_ticket("t-grid")).ok);
  assert(auth.strategy_progress("t-grid", progress).ok);
  assert(progress.running && progress.submitted == 0 && progress.open == 0);
  assert(auth.stop_strategy("t-grid").ok);
  // Stopped with zero retained orders: unknown again (fail closed).
  assert(!auth.strategy_progress("t-grid", progress).ok);
}

void subscriptions_fan_out() {
  Harness harness{};
  std::string error;
  // Null handlers refuse loudly.
  assert(harness.front.subscribe_quotes(nullptr, error) == 0 && !error.empty());
  int quotes = 0;
  int reports = 0;
  const auto quote_id = harness.front.subscribe_quotes(
      [&](const qts::QuoteEvent& quote) {
        ++quotes;
        assert(quote.instrument_id == "TEST");
      },
      error);
  const auto report_id = harness.front.subscribe_reports(
      [&](const qts::ExecutionReport& report) {
        ++reports;
        assert(report.client_order_id == "f-9");
      },
      error);
  assert(quote_id != 0 && report_id != 0 && quote_id != report_id);
  qts::QuoteEvent quote{};
  quote.instrument_id = "TEST";
  harness.front.on_quote(quote);
  qts::ExecutionReport report{};
  report.client_order_id = "f-9";
  harness.front.on_execution_report(report);
  assert(quotes == 1 && reports == 1);
  assert(harness.front.unsubscribe(quote_id));
  harness.front.on_quote(quote);
  harness.front.on_execution_report(report);
  assert(quotes == 1 && reports == 2);
  assert(!harness.front.unsubscribe(quote_id));
}

qts::StrategyConfig grid_ticket(const std::string& id) {
  qts::StrategyConfig configured{};
  configured.id = id;
  configured.enabled = true;
  configured.type = "grid";
  configured.account = "paper";
  configured.instrument = "TEST";
  configured.lower = 900;
  configured.upper = 1100;
  configured.grids = 4;
  configured.quantity = 100;
  configured.start = 1000;
  return configured;
}

void per_account_books_serve_truth() {
  Harness harness{};
  qts::Portfolio book_b{5'000'000};
  harness.book.fund_settled("s1", "TEST", 500, 950);
  book_b.fund_settled("s1", "TEST", 100, 900);
  qts::gateway::GatewayService split{harness.context, harness.book, {"acc-a", "acc-b"}, {{"acc-a", &harness.book}, {"acc-b", &book_b}}};
  qts::Price cash = 0;
  assert(split.account_cash(cash, "acc-a").ok && cash == 10'000'000);
  assert(!split.book_shared("acc-a"));
  assert(split.account_cash(cash, "acc-b").ok && cash == 5'000'000);
  assert(!split.book_shared("acc-b"));
  assert(split.account_cash(cash).ok && cash == 10'000'000);
  assert(split.book_shared({}));  // unfiltered global view mixes
  assert(!split.account_cash(cash, "ghost").ok);
  std::unordered_map<qts::InstrumentId, qts::PositionView> positions;
  assert(split.positions_for("s1", positions, "acc-a").ok && positions["TEST"].total == 500);
  assert(split.positions_for("s1", positions, "acc-b").ok && positions["TEST"].total == 100);
  // Known account with no wired book refuses honestly (never another
  // account's numbers); legacy wiring without books stays shared.
  qts::gateway::GatewayService partial{harness.context, harness.book, {"acc-a", "acc-c"}, {{"acc-a", &harness.book}}};
  assert(!partial.account_cash(cash, "acc-c").ok);
  assert(harness.front.book_shared("paper"));  // legacy: topology unknown
}

void strategy_start_funds_stock() {
  Harness harness{};
  qts::StrategyConfig stocked = grid_ticket("s-flat");
  stocked.stock = 800;
  stocked.stock_avg = 44200;
  // No funder wired: allocation refuses instead of starting flat.
  assert(!harness.front.start_strategy(stocked).ok);
  // Negative allocations refuse.
  qts::StrategyConfig negative = grid_ticket("s-neg");
  negative.stock = -1;
  assert(!harness.front.start_strategy(negative).ok);
  // Wired funder: the basket is funded at start (sellable from tick one).
  harness.front.set_stock_funder(
      [&](const qts::StrategyId& strategy, const qts::InstrumentId& instrument, qts::Quantity quantity, qts::Price avg) { harness.book.fund_settled(strategy, instrument, quantity, avg); });
  assert(harness.front.start_strategy(stocked).ok);
  std::unordered_map<qts::InstrumentId, qts::PositionView> positions;
  assert(harness.front.positions_for("s-flat", positions).ok);
  assert(positions["TEST"].total == 800 && positions["TEST"].sellable == 800 && positions["TEST"].average_cost == 44200);
}

void strategy_events_push() {
  Harness harness{};
  std::string error;
  int events = 0;
  qts::gateway::StrategyEvent last{};
  const auto sub = harness.front.subscribe_strategies(
      [&](const qts::gateway::StrategyEvent& event) {
        ++events;
        last = event;
      },
      error);
  assert(sub != 0);
  assert(harness.front.start_strategy(grid_ticket("t-grid")).ok);
  assert(events == 1 && last.strategy_id == "t-grid" && last.kind == qts::gateway::StrategyEventKind::started);
  assert(!harness.front.start_strategy(grid_ticket("t-grid")).ok);  // duplicate: no event
  assert(events == 1);
  assert(harness.front.stop_strategy("t-grid").ok);
  assert(events == 2 && last.kind == qts::gateway::StrategyEventKind::stopped);
  assert(harness.front.unsubscribe(sub));
  assert(harness.front.start_strategy(grid_ticket("t-grid")).ok);
  assert(events == 2);
}

}  // namespace

qts::gateway::SnapshotBatch canned_batch(qts::Price cash) {
  qts::TradeAccountSnapshot snapshot{};
  snapshot.account_id = "paper";
  snapshot.total_asset = cash;
  snapshot.available_cash = cash;
  snapshot.withdrawable_cash = cash;
  qts::PositionSnapshot position{};
  position.instrument_id = "TEST";
  position.total_quantity = 100;
  position.sellable_quantity = 100;
  position.average_cost = 900;
  snapshot.positions.push_back(position);
  return qts::gateway::SnapshotBatch{{snapshot}, "2026-10-11T00:00:00Z"};
}

void settle_report_and_confirm() {
  Harness harness{};
  qts::gateway::SettleReport report{};
  // Unwired server: refused loudly, never invented.
  assert(!harness.front.settle_report(report).ok);
  qts::ReconcileAttestation claimed{};
  assert(!harness.front.settle_confirm(claimed).ok);
  // Wire a matching snapshot plus the attestation ledger.
  harness.book.fund_settled("test", "TEST", 100, 900);
  qts::InMemoryEventLedger ledger{};
  harness.front.set_snapshot_provider([&] { return canned_batch(harness.book.cash()); });
  harness.front.set_attestation_ledger(&ledger);
  assert(harness.front.settle_report(report).ok);
  assert(report.accounts.size() == 1 && report.accounts[0].account == "paper");
  assert(report.accounts[0].diff.clean() && report.ledger_watermark == 0);
  // Claims need a sane shape; the operator is always the default admin.
  qts::ReconcileAttestation good{};
  good.broker_snapshot_time = report.captured_at;
  good.ledger_watermark = report.ledger_watermark;
  good.note = "daily close";
  const auto confirmed = harness.front.settle_confirm(good);
  assert(confirmed.ok);
  assert(ledger.latest_event_id() == 1);
  const auto stored = ledger.replay(1);
  assert(stored.size() == 1 && stored[0].type == qts::LedgerEventType::reconcile_attested);
  assert(stored[0].attestation.operator_id == "admin" && stored[0].attestation.note == "daily close");
  // Future watermarks, empty times and negative dirties refuse.
  qts::ReconcileAttestation future = good;
  future.ledger_watermark = 99;
  assert(!harness.front.settle_confirm(future).ok);
  qts::ReconcileAttestation timeless = good;
  timeless.broker_snapshot_time.clear();
  assert(!harness.front.settle_confirm(timeless).ok);
  qts::ReconcileAttestation negative = good;
  negative.cash_dirty = -1;
  assert(!harness.front.settle_confirm(negative).ok);
  // Dirty books report dirty but still attest (the claim is the audit).
  qts::InMemoryEventLedger ledger2{};
  harness.front.set_attestation_ledger(&ledger2);
  harness.front.set_snapshot_provider([] { return canned_batch(1); });
  qts::gateway::SettleReport dirty{};
  assert(harness.front.settle_report(dirty).ok);
  assert(!dirty.accounts[0].diff.clean() && dirty.accounts[0].diff.cash_dirty() == 1);
  qts::ReconcileAttestation owned{};
  owned.broker_snapshot_time = dirty.captured_at;
  owned.cash_dirty = 1;
  assert(harness.front.settle_confirm(owned).ok);
  // Attestations replay cleanly (audit-only, books untouched).
  assert(qts::replay::verify_ledger_replay(ledger.replay(1)).ok);
}

void live_cash_serves_counter_snapshot() {
  Harness harness{};
  auto cache = std::make_shared<qts::gateway::GatewayService::CounterSnapshotCache>();
  // paper wired to the shared book; live is a known bookless account.
  qts::gateway::GatewayService front{harness.context, harness.book, {"paper", "live"}, {{"paper", &harness.book}}};
  front.set_counter_snapshot_cache(cache);
  qts::Price cash = 0;
  assert(!front.account_cash(cash, "live").ok);  // no snapshot yet: refuses
  qts::gateway::GatewayService::CounterSnapshot snapshot;
  snapshot.account.available_cash = 5'000'000;
  snapshot.captured_at = "2026-10-16T00:00:00Z";
  snapshot.has_account = true;
  cache->update("live", snapshot);
  assert(front.account_cash(cash, "live").ok && cash == 5'000'000);
  assert(!front.book_shared("live"));  // dedicated counter view, not shared
  assert(front.counter_snapshot_time("live") == "2026-10-16T00:00:00Z");
  assert(!front.account_cash(cash, "ghost").ok);  // unknown still refuses
}

int main() {
  submit_ticket_validation();
  submit_cancel_roundtrip();
  strategy_lifecycle();
  queries_read_live_books();
  account_scoped_queries_fills_and_progress();
  per_account_books_serve_truth();
  strategy_start_funds_stock();
  subscriptions_fan_out();
  strategy_events_push();
  settle_report_and_confirm();
  live_cash_serves_counter_snapshot();
  return 0;
}
