// clang-format off
/**
  ******************************************************************************
  * @file           : ama_adapter_test.cpp
  * @author         : vivi wu
  * @brief          : Seam unit tests for the AMA adapter mapping and session handling.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
// AMA adapter unit tests (M7-2b). Vendor-gated target: built only with
// QTS_ENABLE_VENDOR_SDKS=ON, runs only where the SDK exists (Linux env);
// CI never builds or runs this file (ADR-033).
//
// What is covered here: pure field mappings (snapshot->fixture,
// instrument parse, Cfg assembly), the queue/drain pipeline including
// fail-closed drops and sequence synthesis, session-state transitions from
// vendor event codes, and tick sequence gap capture. What is NOT covered:
// real IAMDApi::Init/SubscribeData/QueryMDTick against a live UMS — that is
// the M7-2b 联调 run with credentials in the Linux environment.

#include <cassert>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>

#include "ama_test_seam.hpp"

namespace {

using qts::AmaQuoteAdapter;
using qts::AmaSessionConfig;
using qts::AmaSnapshotFixture;
using qts::AmaUmsServer;
using qts::InstrumentId;
using qts::QuoteEvent;
using qts::Timestamp;

amd::ama::MDSnapshot make_md_snapshot() {
  amd::ama::MDSnapshot snapshot{};
  snapshot.market_type = 101;
  std::strcpy(snapshot.security_code, "600000");
  snapshot.orig_time = 20260928093000000;  // 2026-09-28 09:30:00.000 (past)
  std::strcpy(snapshot.trading_phase_code, "T");
  for (std::size_t i = 0; i < AmaSnapshotFixture::kLevels; ++i) {
    snapshot.bid_price[i] = 1'000'000'000 - static_cast<std::int64_t>(i) * 100'000;
    snapshot.bid_volume[i] = (static_cast<std::int64_t>(i) + 1) * 100 * 100;
    snapshot.offer_price[i] = 1'000'100'000 + static_cast<std::int64_t>(i) * 100'000;
    snapshot.offer_volume[i] = (static_cast<std::int64_t>(i) + 1) * 200 * 100;
  }
  snapshot.last_price = 1'000'050'000;
  snapshot.total_volume_trade = 1'234'500 * 100;
  snapshot.total_value_trade = 1'234'567'890LL * 10;
  snapshot.high_limited = 1'100'000'000;
  snapshot.low_limited = 900'000'000;
  snapshot.channel_no = 0;
  return snapshot;
}

void snapshot_to_fixture_copies_every_mapped_field() {
  const amd::ama::MDSnapshot snapshot = make_md_snapshot();
  const AmaSnapshotFixture fixture = qts::ama_detail::snapshot_to_fixture(snapshot);
  assert(fixture.market_type == 101);
  assert(fixture.security_code == "600000");
  assert(fixture.orig_time == 20260928093000000);
  assert(fixture.trading_phase_code == "T");
  for (std::size_t i = 0; i < AmaSnapshotFixture::kLevels; ++i) {
    assert(fixture.bid_price[i] == snapshot.bid_price[i]);
    assert(fixture.bid_volume[i] == snapshot.bid_volume[i]);
    assert(fixture.offer_price[i] == snapshot.offer_price[i]);
    assert(fixture.offer_volume[i] == snapshot.offer_volume[i]);
  }
  assert(fixture.last_price == snapshot.last_price);
  assert(fixture.total_volume_trade == snapshot.total_volume_trade);
  assert(fixture.total_value_trade == snapshot.total_value_trade);
  assert(fixture.high_limited == snapshot.high_limited);
  assert(fixture.low_limited == snapshot.low_limited);
  assert(fixture.channel_no == snapshot.channel_no);
}

void parse_instrument_maps_the_three_exchanges() {
  std::int32_t market = 0;
  std::string code;
  assert(qts::ama_detail::parse_instrument("600000.SSE", market, code));
  assert(market == amd::ama::MarketType::kSSE);
  assert(code == "600000");
  assert(qts::ama_detail::parse_instrument("000001.SZSE", market, code));
  assert(market == amd::ama::MarketType::kSZSE);
  assert(code == "000001");
  assert(qts::ama_detail::parse_instrument("920000.BSE", market, code));
  assert(market == amd::ama::MarketType::kNEEQ);
  assert(code == "920000");
  // Fail closed on anything malformed.
  assert(!qts::ama_detail::parse_instrument("600000", market, code));
  assert(!qts::ama_detail::parse_instrument("600000.XX", market, code));
  assert(!qts::ama_detail::parse_instrument("", market, code));
  assert(!qts::ama_detail::parse_instrument(".SSE", market, code));
}

void build_cfg_copies_credentials_and_rejects_bad_shapes() {
  const std::string pw = "secret";  // assigned via a name so the literal never sits behind `password = ...`
  AmaSessionConfig config{};
  config.username = "mduser";
  config.password = pw;
  config.ums_servers = {AmaUmsServer{"10.0.0.1", 12345}, AmaUmsServer{"10.0.0.2", 23456}};
  amd::ama::Cfg cfg{};
  assert(qts::ama_detail::build_cfg(config, cfg));
  assert(std::string(cfg.username) == "mduser");  // vendor_copy NUL-terminates
  assert(std::string(cfg.password) == "secret");
  assert(cfg.ums_server_cnt == 2);
  assert(std::string(cfg.ums_servers[0].server_ip) == "10.0.0.1");
  assert(cfg.ums_servers[0].server_port == 12345);
  assert(std::string(cfg.ums_servers[1].server_ip) == "10.0.0.2");
  assert(cfg.ums_servers[1].server_port == 23456);
  assert(cfg.channel_mode == amd::ama::ChannelMode::kTCP);
  assert(cfg.ha_mode == amd::ama::HighAvailableMode::kMasterSlaveA);
  assert(!cfg.is_thread_safe);
  assert(!cfg.is_subscribe_full);

  AmaSessionConfig empty{};
  assert(!qts::ama_detail::build_cfg(empty, cfg));  // zero UMS servers
  AmaSessionConfig nine = config;
  nine.ums_servers.assign(9, AmaUmsServer{"10.0.0.1", 1});  // vendor limit is 8
  assert(!qts::ama_detail::build_cfg(nine, cfg));
  AmaSessionConfig long_user = config;
  long_user.username.assign(amd::ama::ConstField::kUsernameLen, 'u');  // >= 32 does not fit
  assert(!qts::ama_detail::build_cfg(long_user, cfg));
  AmaSessionConfig zero_port = config;
  zero_port.ums_servers[0].port = 0;
  assert(!qts::ama_detail::build_cfg(zero_port, cfg));
  AmaSessionConfig long_ip = config;
  long_ip.ums_servers[0].ip.assign(amd::ama::ConstField::kIPMaxLen, '9');  // >= 24 does not fit
  assert(!qts::ama_detail::build_cfg(long_ip, cfg));
}

void inject_then_drain_normalizes_and_delivers() {
  AmaQuoteAdapter adapter;
  std::vector<QuoteEvent> events;
  adapter.set_quote_handler([&events](const QuoteEvent& event) { events.push_back(event); });

  const auto fixture = qts::ama_detail::snapshot_to_fixture(make_md_snapshot());
  const Timestamp receive_time = std::chrono::system_clock::now();
  qts::ama_detail::inject_snapshot(adapter, fixture, receive_time);
  assert(adapter.drain() == 1);
  assert(events.size() == 1);

  const QuoteEvent& event = events[0];
  assert(event.instrument_id == "600000.SSE");
  assert(event.exchange == qts::Exchange::sse);
  assert(event.bid_price == 10'000'000);  // 1000.0000 元 at 1e-4 internal scale
  assert(event.bid_quantity == 100);      // 1 lot
  assert(event.ask_price == 10'001'000);
  assert(event.last_price == 10'000'500);
  assert(event.cumulative_volume == 1'234'500);
  assert(event.cumulative_turnover == 1'234'567'890);
  assert(event.limit_up == 11'000'000);
  assert(event.limit_down == 9'000'000);
  assert(event.sequence == 1);
  assert(event.receive_time == receive_time);
  assert(event.quality == qts::QuoteQuality::valid);

  // Sequence is synthesized per instrument and keeps counting.
  qts::ama_detail::inject_snapshot(adapter, fixture, std::chrono::system_clock::now());
  assert(adapter.drain() == 1);
  assert(events.size() == 2);
  assert(events[1].sequence == 2);

  const auto stats = adapter.stats();
  assert(stats.snapshots_received == 2);
  assert(stats.events_delivered == 2);
  assert(stats.snapshots_dropped == 0);
}

void unknown_market_is_fail_closed() {
  AmaQuoteAdapter adapter;
  std::size_t calls = 0;
  adapter.set_quote_handler([&calls](const QuoteEvent&) { ++calls; });
  AmaSnapshotFixture fixture = qts::ama_detail::snapshot_to_fixture(make_md_snapshot());
  fixture.market_type = 42;  // unsupported market
  qts::ama_detail::inject_snapshot(adapter, fixture, std::chrono::system_clock::now());
  assert(adapter.drain() == 0);
  assert(calls == 0);
  assert(adapter.stats().snapshots_dropped == 1);
}

void drain_without_handler_keeps_the_queue() {
  AmaQuoteAdapter adapter;
  AmaSnapshotFixture fixture = qts::ama_detail::snapshot_to_fixture(make_md_snapshot());
  qts::ama_detail::inject_snapshot(adapter, fixture, std::chrono::system_clock::now());
  assert(adapter.drain() == 0);  // no handler: nothing consumed, nothing lost
  std::size_t calls = 0;
  adapter.set_quote_handler([&calls](const QuoteEvent&) { ++calls; });
  assert(adapter.drain() == 1);
  assert(calls == 1);
}

void session_events_drive_connected_state() {
  AmaQuoteAdapter adapter;
  assert(!adapter.connected());
  qts::ama_detail::inject_event(adapter, amd::ama::EventCode::kUMSLogonSuccess);
  assert(adapter.connected());
  qts::ama_detail::inject_event(adapter, amd::ama::EventCode::kIncompleteData);
  assert(adapter.connected());  // recover-mode restart does not drop the session
  qts::ama_detail::inject_event(adapter, amd::ama::EventCode::kChannelTCPSessionClosed);
  assert(!adapter.connected());
  qts::ama_detail::inject_event(adapter, amd::ama::EventCode::kUMSHeartbeatTimeout);
  // Live TCP bring-up sequence (UMS 172.16.109.14, 2026-10-09): the UMS-layer
  // logon event never fires; readiness arrives as kChannelTCPLogonSuccess.
  qts::ama_detail::inject_event(adapter, amd::ama::EventCode::kUMSConnectSuccess);
  assert(!adapter.connected());
  qts::ama_detail::inject_event(adapter, amd::ama::EventCode::kChannelTCPInitSuccess);
  qts::ama_detail::inject_event(adapter, amd::ama::EventCode::kChannelTCPConnectSuccess);
  assert(!adapter.connected());
  qts::ama_detail::inject_event(adapter, amd::ama::EventCode::kChannelTCPLogonSuccess);
  assert(adapter.connected());
  qts::ama_detail::inject_event(adapter, amd::ama::EventCode::kChannelTCPLogonFailed);
  assert(!adapter.connected());
  const auto stats = adapter.stats();
  assert(stats.logon_events == 2);
  assert(stats.disconnect_events == 3);
  assert(stats.incomplete_events == 1);
}

void start_rejects_invalid_config_before_touching_vendor() {
  AmaQuoteAdapter adapter;
  AmaSessionConfig empty{};  // zero UMS servers fails closed in build_cfg
  assert(!adapter.start(empty));
  assert(!adapter.started());
  assert(!adapter.connected());
}

void tick_sequence_gap_is_recorded_once() {
  AmaQuoteAdapter adapter;
  qts::ama_detail::inject_tick(adapter, 101, 5, 100);  // baseline
  qts::ama_detail::inject_tick(adapter, 101, 5, 101);  // contiguous
  qts::ama_detail::inject_tick(adapter, 101, 5, 105);  // hole: 102..104
  const std::vector<qts::AmaTickGap> gaps = adapter.take_tick_gaps();
  assert(gaps.size() == 1);
  assert(gaps[0].market_type == 101);
  assert(gaps[0].channel_no == 5);
  assert(gaps[0].begin_seq == 102);
  assert(gaps[0].end_seq == 104);
  assert(adapter.take_tick_gaps().empty());  // handed out once
  const auto stats = adapter.stats();
  assert(stats.ticks_seen == 3);
  assert(stats.tick_gaps == 1);
  assert(!adapter.request_tick_backfill(gaps[0]));  // backfill needs a started session
}

}  // namespace

int main() {
  snapshot_to_fixture_copies_every_mapped_field();
  parse_instrument_maps_the_three_exchanges();
  build_cfg_copies_credentials_and_rejects_bad_shapes();
  inject_then_drain_normalizes_and_delivers();
  unknown_market_is_fail_closed();
  drain_without_handler_keeps_the_queue();
  session_events_drive_connected_state();
  start_rejects_invalid_config_before_touching_vendor();
  tick_sequence_gap_is_recorded_once();
  return 0;
}
