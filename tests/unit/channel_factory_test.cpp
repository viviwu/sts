// clang-format off
/**
  ******************************************************************************
  * @file           : channel_factory_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for yaml-driven channel builds and routing assembly.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/broker/channel_factory.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "sts/core/event_bus.hpp"
#include "sts/ops/control.hpp"

namespace {

sts::AppConfig paper_config() {
  sts::AppConfig config = sts::AppConfig::paper_defaults();
  config.ledger_file.clear();
  return config;
}

class FakeTradeApi final : public sts::TradeApi {
 public:
  explicit FakeTradeApi(std::string name)
      : name_(std::move(name)) {}
  [[nodiscard]] std::string_view channel() const noexcept override { return name_; }
  [[nodiscard]] bool connected() const noexcept override { return true; }
  void submit(const sts::OrderIntent&) override {}
  void cancel(const sts::ClientOrderId&) override {}
  void set_execution_handler(ExecutionHandler) override {}
  void request_account_snapshot(const sts::AccountId&) override {}
  void request_open_orders(const sts::AccountId&) override {}
  void set_account_snapshot_handler(AccountSnapshotHandler) override {}
  void set_recovery_complete_handler(RecoveryCompleteHandler) override {}
  void request_security_info(const sts::AccountId&, const sts::InstrumentId&) override {}
  void set_security_info_handler(SecurityInfoHandler) override {}
  void request_asset_snapshot(const sts::AccountId&) override {}
  void set_asset_snapshot_handler(AssetSnapshotHandler) override {}

 private:
  std::string name_;
};

sts::VendorChannelHook fake_hook(const std::string& name, bool quote_only) {
  return [name, quote_only](const std::string& wanted, sts::EventBus&, const sts::AppConfig&) -> std::unique_ptr<sts::Channel> {
    if (wanted != name) return nullptr;
    auto channel = std::make_unique<sts::Channel>();
    channel->name = wanted;
    if (!quote_only) channel->api = std::make_unique<FakeTradeApi>(wanted);
    return channel;
  };
}

// Vendor-path fixture (ADR-051): no approval files; the broker password is
// the single authorization, so the hook builds directly.
sts::AppConfig live_config(const std::string& broker) {
  sts::AppConfig config = paper_config();
  config.accounts.clear();
  config.accounts.push_back({"live", broker, ""});
  return config;
}

void vendor_hook_builds_named_trade_channel() {
  sts::AppConfig config = live_config("fakebroker");
  sts::EventBus events;
  sts::ChannelSet set = sts::build_channels(config, events, nullptr, fake_hook("fakebroker", false), "2026-10-10");
  assert(set.channels.size() == 1 && set.channels[0].name == "fakebroker");
  assert(set.router == nullptr && set.paper() == nullptr);
  assert(std::string{set.trade_for_oms().channel()} == "fakebroker");
}

void vendor_hook_null_stays_unknown() {
  // The hook builds nothing: still unknown (fail closed).
  sts::AppConfig config = live_config("fakebroker");
  sts::EventBus events;
  bool threw = false;
  try {
    (void)sts::build_channels(config, events, nullptr, fake_hook("other", false), "2026-10-10");
  } catch (const std::runtime_error& e) {
    threw = true;
    assert(std::string{e.what()}.find("fakebroker") != std::string::npos);
  }
  assert(threw);
}

void quote_only_channel_cannot_trade_alone() {
  sts::AppConfig config = live_config("fakequote");
  sts::EventBus events;
  bool threw = false;
  try {
    (void)sts::build_channels(config, events, nullptr, fake_hook("fakequote", true), "2026-10-10");
  } catch (const std::runtime_error& e) {
    threw = true;
    assert(std::string{e.what()}.find("trade-capable") != std::string::npos);
  }
  assert(threw);
}

void routing_rejects_quote_only_channel() {
  sts::AppConfig config = live_config("fakequote");
  config.accounts.push_back({"paper", "simulated", ""});
  config.routing.channels.push_back({"fakequote", 25, 9950, 500, 0});
  config.routing.channels.push_back({"simulated", 25, 9950, 500, 0});
  sts::EventBus events;
  bool threw = false;
  try {
    (void)sts::build_channels(config, events, nullptr, fake_hook("fakequote", true), "2026-10-10");
  } catch (const std::runtime_error& e) {
    threw = true;
    assert(std::string{e.what()}.find("quote-only") != std::string::npos);
  }
  assert(threw);
}

void builds_simulated_by_name() {
  sts::EventBus events;
  sts::ChannelSet set = sts::build_channels(paper_config(), events);
  assert(set.channels.size() == 1 && set.channels[0].name == "simulated");
  assert(set.router == nullptr);
  assert(set.paper() != nullptr);
  assert(std::string{set.trade_for_oms().channel()} == "simulated");
}

void builds_alternate_with_open_session() {
  sts::AppConfig config = paper_config();
  config.accounts.clear();
  config.accounts.push_back({"paper-alt", "alternate", ""});
  sts::EventBus events;
  sts::ChannelSet set = sts::build_channels(config, events);
  assert(set.channels.size() == 1 && set.channels[0].name == "alternate");
  assert(set.paper() != nullptr);
  // Factory opens the session: submit must not throw (closed session throws).
  // Quantity/price satisfy the lot/tick rules, so the order queues an ack.
  sts::OrderIntent intent{"alt-1", "paper-alt", "test", "TEST", sts::Side::buy, sts::OrderType::limit, sts::TimeInForce::day, 200, 1000};
  intent.trace_id = "t-alt-1";
  set.trade_for_oms().submit(intent);
  assert(static_cast<bool>(set.channels[0].drain));
  set.channels[0].drain();
}

void unknown_broker_fails_closed() {
  sts::AppConfig config = paper_config();
  config.accounts.clear();
  config.accounts.push_back({"paper-x", "quantapi", ""});
  sts::EventBus events;
  bool threw = false;
  try {
    (void)sts::build_channels(config, events);
  } catch (const std::runtime_error& e) {
    threw = true;
    assert(std::string{e.what()}.find("quantapi") != std::string::npos);
  }
  assert(threw);
}

void multi_broker_without_routing_throws() {
  sts::AppConfig config = paper_config();
  config.accounts.push_back({"paper-alt", "alternate", ""});
  sts::EventBus events;
  bool threw = false;
  try {
    (void)sts::build_channels(config, events);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
}

void routing_wraps_named_channels() {
  sts::AppConfig config = paper_config();
  config.accounts.push_back({"paper-alt", "alternate", ""});
  sts::ChannelStatsConfig sim{"simulated", 25, 9950, 500, 0};
  sts::ChannelStatsConfig alt{"alternate", 50, 10000, 800, 0};
  config.routing.channels.push_back(sim);
  config.routing.channels.push_back(alt);
  sts::EventBus events;
  sts::ChannelSet set = sts::build_channels(config, events);
  assert(set.router != nullptr);
  assert(std::string{set.trade_for_oms().channel()} == "router");
  assert(set.paper() != nullptr);  // paper feed survives routing
}

void routing_unknown_channel_throws() {
  sts::AppConfig config = paper_config();
  config.routing.channels.push_back({"ghost", 25, 9950, 500, 0});
  sts::EventBus events;
  bool threw = false;
  try {
    (void)sts::build_channels(config, events);
  } catch (const std::runtime_error& e) {
    threw = true;
    assert(std::string{e.what()}.find("ghost") != std::string::npos);
  }
  assert(threw);
}

void paper_fidelity_reaches_the_broker_book() {
  sts::AppConfig config = paper_config();
  config.paper_sim.starting_cash = 2'000'000;
  config.paper_sim.slippage_per_unit = 2;
  config.paper_sim.fee_rate_bp = 25;
  sts::EventBus events;
  sts::ChannelSet set = sts::build_channels(config, events);
  assert(!set.channels.empty());
  sts::Channel& channel = set.channels.front();
  channel.set_reference_price("TEST", 1000);
  sts::OrderIntent intent{"fee-1", "paper", "test", "TEST", sts::Side::buy, sts::OrderType::limit, sts::TimeInForce::day, 100, 1000};
  intent.trace_id = "t-fee-1";
  channel.api->submit(intent);
  channel.drain();
  // Slippage penalizes the taker (100 @ 1002, not 1000); the 25bp fee lands
  // in the broker book (invisible to the ledger by design).
  const sts::PositionView position = channel.book().position("TEST");
  assert(position.total == 100 && position.average_cost == 1002);
  assert(channel.book().total_fees() > 0);
  assert(channel.book().cash() == 2'000'000 - 100 * 1002 - channel.book().total_fees());
}

void starting_positions_fund_settled_inventory() {
  sts::AppConfig config = paper_config();
  sts::StartingPosition inventory{};
  inventory.instrument = "TEST";
  inventory.quantity = 1000;
  inventory.avg_cost = 950;
  config.paper_sim.starting_positions.push_back(inventory);
  sts::EventBus events;
  sts::ChannelSet set = sts::build_channels(config, events);
  assert(!set.channels.empty());
  const sts::PositionView position = set.channels.front().book().position("TEST");
  // Settled funding: total present, fully sellable, cash untouched.
  assert(position.total == 1000 && position.sellable == 1000);
  assert(position.average_cost == 950);
  assert(set.channels.front().book().cash() == config.paper_sim.starting_cash);
}

void starting_positions_name_their_owner_strategy() {
  sts::AppConfig config = paper_config();
  sts::StartingPosition inventory{};
  inventory.instrument = "TEST";
  inventory.quantity = 1000;
  inventory.avg_cost = 950;
  inventory.strategy = "bal-1";
  config.paper_sim.starting_positions.push_back(inventory);
  sts::EventBus events;
  sts::ChannelSet set = sts::build_channels(config, events);
  assert(!set.channels.empty());
  const sts::PositionView owned = set.channels.front().book().position_for("bal-1", "TEST");
  assert(owned.total == 1000 && owned.sellable == 1000 && owned.average_cost == 950);
  // Nothing leaks into the passthrough basket.
  const sts::PositionView shared = set.channels.front().book().position_for("passthrough", "TEST");
  assert(shared.total == 0);
}

void fee_schedule_reaches_the_broker_book() {
  sts::AppConfig config = paper_config();
  config.paper_sim.use_fees = true;
  config.paper_sim.fees.commission_ppm = 2500;
  config.paper_sim.fees.commission_minimum = 50000;
  config.paper_sim.fees.stamp_ppm = 500;
  config.paper_sim.fees.transfer_ppm = 100;
  sts::EventBus events;
  sts::ChannelSet set = sts::build_channels(config, events);
  assert(!set.channels.empty());
  sts::Channel& channel = set.channels.front();
  channel.set_reference_price("TEST", 1000);
  sts::OrderIntent intent{"schedf-1", "paper", "test", "TEST", sts::Side::buy, sts::OrderType::limit, sts::TimeInForce::day, 100, 1000};
  intent.trace_id = "t-schedf-1";
  channel.api->submit(intent);
  channel.drain();
  // Notional 100,000: commission 250 floored to 50,000 + transfer 10.
  assert(channel.book().total_fees() == 50010);
}

void alternate_seq_lives_next_to_ledger() {
  const auto dir = std::filesystem::temp_directory_path() / "sts-test-chanseq";
  std::filesystem::create_directories(dir);
  sts::AppConfig config = paper_config();
  config.accounts.clear();
  config.accounts.push_back({"paper-alt", "alternate", ""});
  config.ledger_file = (dir / "ledger.jsonl").string();
  sts::EventBus events;
  sts::ChannelSet set = sts::build_channels(config, events);
  assert(std::filesystem::exists(dir / "alt-alternate.seq") == false);  // created lazily on first submit
  sts::OrderIntent intent{"seq-1", "paper-alt", "test", "TEST", sts::Side::buy, sts::OrderType::limit, sts::TimeInForce::day, 200, 1000};
  intent.trace_id = "t-seq-1";
  set.trade_for_oms().submit(intent);
  assert(std::filesystem::exists(dir / "alt-alternate.seq"));
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

}  // namespace

int main() {
  builds_simulated_by_name();
  builds_alternate_with_open_session();
  unknown_broker_fails_closed();
  vendor_hook_builds_named_trade_channel();
  vendor_hook_null_stays_unknown();
  quote_only_channel_cannot_trade_alone();
  routing_rejects_quote_only_channel();
  multi_broker_without_routing_throws();
  routing_wraps_named_channels();
  routing_unknown_channel_throws();
  alternate_seq_lives_next_to_ledger();
  paper_fidelity_reaches_the_broker_book();
  starting_positions_fund_settled_inventory();
  starting_positions_name_their_owner_strategy();
  fee_schedule_reaches_the_broker_book();
  return 0;
}
