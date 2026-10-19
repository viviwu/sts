// clang-format off
/**
  ******************************************************************************
  * @file           : channel_factory.hpp
  * @author         : vivi wu
  * @brief          : Yaml-driven channel construction plus Router wiring for the server.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "qts/broker/order_book.hpp"
#include "qts/broker/routing.hpp"
#include "qts/broker/trade_api.hpp"
#include "qts/config/config.hpp"
#include "qts/core/event_bus.hpp"
#include "qts/portfolio/portfolio.hpp"

namespace qts {

// Gateway channel selection (accounts[].broker + routing.channels).
//
// The gateway used to hardcode one SimulatedBroker and ignore the channel
// configuration entirely. This factory builds one TradeApi per distinct
// broker name in config.accounts and wires OMS to either that single
// channel (no routing configured) or a Router over routing.channels.
// Switching simulated / alternate / routed paper setups is a yaml-only
// change (unknown broker names fail closed with the known-name list).
//
// Paper-only market-data feeding (reference prices, strategy book) is not
// part of the TradeApi port, so each channel optionally carries feed hooks.
// Real (non-paper) channels leave them empty; the gateway fails closed when
// no paper feed exists because the grid strategy needs a book.
struct Channel {
  std::string name;
  std::unique_ptr<TradeApi> api;
  // Empty when the channel offers no paper feed (future vendor channels).
  std::function<const Portfolio&()> book;
  std::function<void(InstrumentId, Price)> set_reference_price;
  std::function<void()> drain;  // no-op except for queued brokers
  // Day-boundary settlement (books forward to their Portfolio); empty for
  // channels without day-scoped state.
  std::function<void()> settle_day;
  // Settled-inventory funding for dynamic strategy starts (position
  // allocation into the channel book basket); set by paper channels,
  // empty where the channel owns no fundable book (vendor channels).
  std::function<void(const StrategyId&, InstrumentId, Quantity, Price)> fund;
  // Standing depth for M6 book simulation; empty where the channel only
  // supports reference-price matching (e.g. alternate).
  std::function<void(InstrumentId, OrderBook)> set_order_book;
  // Lagged-report pump for latency-modeled channels; empty when delivery
  // is inline.
  std::function<void()> deliver_due;
  // Connection switch for fault drills and reconnect recovery; empty where
  // the channel owns its session exclusively (future vendor channels).
  std::function<void(bool)> set_connected;
};

struct ChannelSet {
  std::vector<Channel> channels;   // one per distinct broker name, config order
  std::unique_ptr<Router> router;  // set only when routing is configured

  // The TradeApi OMS must hold: the single channel, or the router.
  [[nodiscard]] TradeApi& trade_for_oms();
  // First channel carrying paper feed hooks, or nullptr.
  [[nodiscard]] Channel* paper();
};

// Vendor trade channels (e.g. quantapi) plug in through this hook.
// It lives outside qts_core so the core never sees a vendor header
// (ADR-033): the gateway passes it only in vendor-gated builds; default
// builds keep the empty hook and stay paper-only.
using VendorChannelHook = std::function<std::unique_ptr<Channel>(const std::string& name, EventBus& events, const AppConfig& config)>;

// Throws std::runtime_error on empty accounts, unknown broker names,
// unknown routing channel names, multi-broker setups without routing, or
// setups with no trade-capable channel (a hook-built quote-only channel
// alone cannot trade). Non-paper names need no approval file (ADR-051):
// the broker password is the single authorization.
// Paper fidelity comes from config.paper_sim (cash for both paper brokers;
// latency/slippage/fees on the simulated channel, which needs a live clock
// for lagged delivery). Alternate's session is opened; its client-seq
// sidecar lives next to the ledger file (or the temp dir when no ledger
// is configured).
[[nodiscard]] ChannelSet build_channels(const AppConfig& config, EventBus& events, const Clock* clock = nullptr, VendorChannelHook vendor = {}, const std::string& trade_date = "");

}  // namespace qts
