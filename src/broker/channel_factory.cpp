// clang-format off
/**
  ******************************************************************************
  * @file           : channel_factory.cpp
  * @author         : vivi wu
  * @brief          : Channel factory implementation: named builds, feed hooks, Router assembly.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/broker/channel_factory.hpp"

#include <chrono>
#include <filesystem>
#include <stdexcept>

#include "qts/broker/alternate_paper_broker.hpp"
#include "qts/broker/simulated_broker.hpp"
#include "qts/ops/control.hpp"

namespace qts {
namespace {

std::string seq_dir_for(const AppConfig& config) {
  if (!config.ledger_file.empty()) {
    const std::size_t slash = config.ledger_file.find_last_of('/');
    if (slash != std::string::npos) return config.ledger_file.substr(0, slash);
    return ".";
  }
  return std::filesystem::temp_directory_path().string();
}

// Fee source precedence, shared by both paper channels: dedicated fee file
// first, inline fees: section next, legacy fee_bp last.
void apply_simulated_fees(SimulatedBroker& broker, const PaperSimConfig& sim) {
  if (sim.use_fee_table) {
    broker.set_fee_schedule(sim.fee_table.fallback);
    for (const auto& entry : sim.fee_table.overrides) broker.set_fee_override(entry.first, entry.second);
  } else if (sim.use_fees) {
    broker.set_fee_schedule(sim.fees);
  } else {
    broker.set_fee_rate_bp(sim.fee_rate_bp);
  }
}

void apply_alternate_fees(AlternatePaperBroker& broker, const PaperSimConfig& sim) {
  if (sim.use_fee_table) {
    broker.set_fee_schedule(sim.fee_table.fallback);
    for (const auto& entry : sim.fee_table.overrides) broker.set_fee_override(entry.first, entry.second);
  } else if (sim.use_fees) {
    broker.set_fee_schedule(sim.fees);
  }
}

Channel make_channel(const std::string& name, EventBus& events, const std::string& seq_dir, const PaperSimConfig& sim, const Clock* clock) {
  if (name == "simulated") {
    auto broker = std::make_unique<SimulatedBroker>(events);
    broker->set_starting_cash(sim.starting_cash);
    apply_simulated_fees(*broker, sim);
    for (const auto& position : sim.starting_positions) {
      // Empty strategy sorts into passthrough inside fund_settled_position,
      // matching ledger replay normalization.
      broker->fund_settled_position(position.strategy, position.instrument, position.quantity, position.avg_cost);
    }
    broker->set_slippage_per_unit(sim.slippage_per_unit);
    if (sim.latency_ms > 0 && clock != nullptr) {
      broker->set_clock(clock);
      broker->set_latency(std::chrono::milliseconds{sim.latency_ms});
    }
    SimulatedBroker* raw = broker.get();
    Channel channel{};
    channel.name = name;
    channel.api = std::move(broker);
    channel.book = [raw]() -> const Portfolio& { return raw->book(); };
    channel.fund = [raw](const StrategyId& strategy, InstrumentId instrument, Quantity quantity, Price avg_cost) { raw->fund_settled_position(strategy, std::move(instrument), quantity, avg_cost); };
    channel.set_reference_price = [raw](InstrumentId instrument, Price price) { raw->set_reference_price(std::move(instrument), price); };
    channel.drain = [] {};
    channel.settle_day = [raw] { raw->settle_day(); };
    channel.set_order_book = [raw](InstrumentId instrument, OrderBook book) { raw->set_order_book(std::move(instrument), std::move(book)); };
    channel.deliver_due = [raw] { raw->deliver_due(); };
    channel.set_connected = [raw](bool connected) {
      if (connected) {
        raw->reconnect();
      } else {
        raw->disconnect();
      }
    };
    return channel;
  }
  if (name == "alternate") {
    auto broker = std::make_unique<AlternatePaperBroker>(events, seq_dir + "/alt-" + name + ".seq");
    broker->set_starting_cash(sim.starting_cash);
    apply_alternate_fees(*broker, sim);
    for (const auto& position : sim.starting_positions) {
      broker->fund_settled_position(position.strategy, position.instrument, position.quantity, position.avg_cost);
    }
    broker->set_session_open(true);
    AlternatePaperBroker* raw = broker.get();
    Channel channel{};
    channel.name = name;
    channel.api = std::move(broker);
    channel.book = [raw]() -> const Portfolio& { return raw->book(); };
    channel.fund = [raw](const StrategyId& strategy, InstrumentId instrument, Quantity quantity, Price avg_cost) { raw->fund_settled_position(strategy, std::move(instrument), quantity, avg_cost); };
    channel.set_reference_price = [raw](InstrumentId instrument, Price price) { raw->set_reference_price(std::move(instrument), price); };
    channel.drain = [raw] { raw->drain(); };
    channel.settle_day = [raw] { raw->settle_day(); };
    channel.set_connected = [raw](bool connected) {
      if (connected) {
        raw->reconnect();
      } else {
        raw->disconnect();
      }
    };
    return channel;
  }
  throw std::runtime_error("unknown broker '" + name + "' (known paper channels: simulated, alternate; vendor channels need a vendor-gated build)");
}

Channel* find_channel(ChannelSet& set, const std::string& name) {
  for (auto& channel : set.channels) {
    if (channel.name == name) return &channel;
  }
  return nullptr;
}

// One distinct channel per broker name; paper names build inline, anything
// else goes through the vendor hook (null/empty hook keeps the fail-closed
// unknown-broker error). Non-paper names are live counters: no static
// approval file stands in front of them (ADR-051) — the broker password
// (env, checked again at gateway login) is the single authorization, so
// whoever holds it can run.
void add_account_channel(
    ChannelSet& set, const std::string& broker, EventBus& events, const AppConfig& config, const std::string& seq_dir, const Clock* clock, VendorChannelHook& vendor, const std::string& trade_date) {
  (void)trade_date;  // kept for signature stability (ADR-039 callers/tests)
  if (find_channel(set, broker) != nullptr) return;
  if (broker == "simulated" || broker == "alternate") {
    set.channels.push_back(make_channel(broker, events, seq_dir, config.paper_sim, clock));
    return;
  }
  if (vendor) {
    auto built = vendor(broker, events, config);
    if (built != nullptr) {
      set.channels.push_back(std::move(*built));
      return;
    }
  }
  throw std::runtime_error("unknown broker '" + broker + "' (known paper channels: simulated, alternate; vendor channels need a vendor-gated build)");
}

std::size_t count_trade_capable(const ChannelSet& set) {
  std::size_t capable = 0;
  for (const auto& channel : set.channels) {
    if (channel.api != nullptr) ++capable;
  }
  return capable;
}

// Router over routing.channels (caller guarantees non-empty): every entry
// must name a built trade-capable channel.
std::unique_ptr<Router> build_router(ChannelSet& set, const AppConfig& config, const Clock* clock) {
  std::vector<RoutingPolicy::Candidate> candidates;
  for (const auto& stats : config.routing.channels) {
    Channel* found = find_channel(set, stats.channel);
    if (found == nullptr) throw std::runtime_error("routing channel '" + stats.channel + "' has no matching accounts[].broker");
    if (found->api == nullptr) throw std::runtime_error("routing channel '" + stats.channel + "' is quote-only and cannot trade");
    RoutingPolicy::Candidate candidate{};
    candidate.channel = stats.channel;
    candidate.api = found->api.get();
    candidate.stats.channel = stats.channel;
    candidate.stats.fee_bps = stats.fee_bps;
    candidate.stats.fill_rate_bps = stats.fill_rate_bps;
    candidate.stats.latency_us = stats.latency_us;
    candidate.stats.split_weight = stats.split_weight;
    candidates.push_back(std::move(candidate));
  }
  RoutingWeights weights{};
  weights.weight_fee = config.routing.weight_fee;
  weights.weight_latency = config.routing.weight_latency;
  weights.weight_fill = config.routing.weight_fill;
  return std::make_unique<Router>(std::move(candidates), RoutingPolicy{weights}, clock, config.routing.ewma_alpha_bps);
}

}  // namespace

TradeApi& ChannelSet::trade_for_oms() {
  if (router != nullptr) return *router;
  return *channels.front().api;
}

Channel* ChannelSet::paper() {
  for (auto& channel : channels) {
    if (static_cast<bool>(channel.book)) return &channel;
  }
  return nullptr;
}

ChannelSet build_channels(const AppConfig& config, EventBus& events, const Clock* clock, VendorChannelHook vendor, const std::string& trade_date) {
  if (config.accounts.empty()) throw std::runtime_error("at least one account is required to build channels");
  const std::string seq_dir = seq_dir_for(config);

  ChannelSet set;
  for (const auto& account : config.accounts) add_account_channel(set, account.broker, events, config, seq_dir, clock, vendor, trade_date);
  if (count_trade_capable(set) == 0) throw std::runtime_error("no trade-capable channel (quote-only channels cannot trade)");

  if (config.routing.channels.empty()) {
    if (set.channels.size() != 1) {
      throw std::runtime_error("accounts use " + std::to_string(set.channels.size()) + " brokers but no routing.channels is configured");
    }
    return set;
  }
  set.router = build_router(set, config, clock);
  return set;
}

}  // namespace qts
