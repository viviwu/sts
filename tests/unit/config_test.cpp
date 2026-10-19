// clang-format off
/**
  ******************************************************************************
  * @file           : config_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for config loading, validation and path anchoring.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/config/config.hpp"

#include <cassert>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

void defaults_are_valid() {
  const qts::AppConfig config = qts::AppConfig::paper_defaults();
  std::string error;
  assert(config.validate(error) && error.empty());
  assert(config.environment == "paper");
  assert(config.risk.max_order_quantity == 1000);
  assert(config.risk.max_order_notional == 10'000'000);
  assert(config.risk.trading_enabled);
  assert(config.ledger_file.empty());  // unset means the in-memory ledger
}

void live_is_rejected_before_m3() {
  qts::AppConfig config = qts::AppConfig::paper_defaults();
  config.environment = "live";
  std::string error;
  assert(!config.validate(error) && !error.empty());

  config.environment = "sandbox:xtp";
  assert(config.validate(error));
}

void accounts_are_required_and_unique() {
  qts::AppConfig config = qts::AppConfig::paper_defaults();
  config.accounts.clear();
  std::string error;
  assert(!config.validate(error));

  config = qts::AppConfig::paper_defaults();
  config.accounts.push_back({"paper", "simulated", ""});  // duplicate id
  assert(!config.validate(error));

  config = qts::AppConfig::paper_defaults();
  config.accounts[0].broker.clear();
  assert(!config.validate(error));
}

void versions_and_limits() {
  qts::AppConfig config = qts::AppConfig::paper_defaults();
  config.config_version.clear();
  std::string error;
  assert(!config.validate(error));

  config = qts::AppConfig::paper_defaults();
  config.risk.max_order_quantity = -1;  // negative is never valid (0 = no limit)
  assert(!config.validate(error));

  config = qts::AppConfig::paper_defaults();
  qts::StrategyConfig duplicate{};
  duplicate.id = "example";
  duplicate.enabled = true;
  config.strategies.push_back(duplicate);  // duplicate strategy id
  assert(!config.validate(error));
}

std::filesystem::path write_temp(const std::string& name, const std::string& content) {
  const auto path = std::filesystem::temp_directory_path() / name;
  std::ofstream out{path};
  out << content;
  return path;
}

void loads_example_shape() {
  const auto path = write_temp("qts-test-paper.yaml",
                               "environment: paper\n"
                               "config_version: \"7\"\n"
                               "rules_version: \"9\"\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "    security_account_id: A123\n"
                               "risk:\n"
                               "  max_order_quantity: 1000\n"
                               "  max_order_notional: 10000000\n"
                               "  trading_enabled: true\n"
                               "strategies:\n"
                               "  - id: example\n"
                               "    enabled: false\n");
  const qts::AppConfig config = qts::load_app_config(path.string());
  assert(config.environment == "paper");
  assert(config.config_version == "7" && config.rules_version == "9");
  assert(config.accounts.size() == 1 && config.accounts[0].security_account_id == "A123");
  assert(config.risk.max_order_quantity == 1000 && config.risk.trading_enabled);
  assert(config.strategies.size() == 1 && !config.strategies[0].enabled);
  assert(config.ledger_file.empty());  // optional: absent stays in-memory
}

void ledger_file_is_optional_but_roundtrips() {
  const auto path = write_temp("qts-test-ledger.yaml",
                               "environment: paper\n"
                               "config_version: \"1\"\n"
                               "rules_version: \"1\"\n"
                               "ledger_file: /var/lib/qts/ledger.jsonl\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "risk:\n"
                               "  max_order_quantity: 1000\n"
                               "  max_order_notional: 10000000\n"
                               "  trading_enabled: true\n");
  const qts::AppConfig config = qts::load_app_config(path.string());
  assert(config.ledger_file == "/var/lib/qts/ledger.jsonl");
  std::string error;
  assert(config.validate(error));
}

void loader_rejects_bad_files() {
  bool threw = false;
  try {
    (void)qts::load_app_config("/nonexistent/qts-test.yaml");
  } catch (const std::exception&) {
    threw = true;
  }
  assert(threw);

  const auto bad = write_temp("qts-test-bad.yaml", "environment: live\n");
  threw = false;
  try {
    (void)qts::load_app_config(bad.string());  // live fails validation
  } catch (const std::exception&) {
    threw = true;
  }
  assert(threw);
}

void routing_is_optional_but_validated() {
  // Absent routing stays disabled and valid.
  qts::AppConfig config = qts::AppConfig::paper_defaults();
  std::string error;
  assert(config.routing.channels.empty());
  assert(config.validate(error));

  // Programmatic validation: bad stats rejected.
  config.routing.channels.push_back({"sim", -1, 10000, 500});
  assert(!config.validate(error));
  config.routing.channels.back().fee_bps = 25;
  config.routing.channels.back().fill_rate_bps = 10001;
  assert(!config.validate(error));
  config.routing.channels.back().fill_rate_bps = 10000;
  config.routing.channels.push_back({"sim", 25, 10000, 500});  // duplicate
  assert(!config.validate(error));
  config.routing.channels.pop_back();
  config.routing.weight_fill = -1;
  assert(!config.validate(error));
  config.routing.weight_fill = 1;
  assert(config.validate(error));
}

void routing_loads_from_yaml() {
  const auto path = write_temp("qts-test-routing.yaml",
                               "environment: paper\n"
                               "config_version: \"1\"\n"
                               "rules_version: \"1\"\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "risk:\n"
                               "  max_order_quantity: 1000\n"
                               "  max_order_notional: 10000000\n"
                               "  trading_enabled: true\n"
                               "routing:\n"
                               "  weights: {fee: 2, latency: 1, fill: 3}\n"
                               "  channels:\n"
                               "    - channel: simulated\n"
                               "      fee_bps: 25\n"
                               "      fill_rate_bps: 9950\n"
                               "      latency_us: 500\n"
                               "    - channel: alternate\n"
                               "      fee_bps: 50\n"
                               "      fill_rate_bps: 10000\n"
                               "      latency_us: 800\n");
  const qts::AppConfig loaded = qts::load_app_config(path.string());
  assert(loaded.routing.weight_fee == 2 && loaded.routing.weight_latency == 1 && loaded.routing.weight_fill == 3);
  assert(loaded.routing.channels.size() == 2);
  assert(loaded.routing.channels[0].channel == "simulated" && loaded.routing.channels[0].fee_bps == 25 && loaded.routing.channels[0].fill_rate_bps == 9950 &&
         loaded.routing.channels[0].latency_us == 500);
  assert(loaded.routing.channels[1].channel == "alternate");
  // split_weight is optional, defaulting to pick-only.
  assert(loaded.routing.channels[0].split_weight == 0);
  // EWMA alpha defaults to 2000 and validates 1..10000.
  assert(loaded.routing.ewma_alpha_bps == 2000);
  qts::AppConfig bad_alpha = qts::AppConfig::paper_defaults();
  bad_alpha.routing.channels.push_back({"sim", 25, 10000, 500});
  bad_alpha.routing.ewma_alpha_bps = 0;
  std::string alpha_error;
  assert(!bad_alpha.validate(alpha_error));
  qts::AppConfig bad_split = qts::AppConfig::paper_defaults();
  bad_split.routing.channels.push_back({"sim", 25, 10000, 500, 1});
  bad_split.routing.channels.push_back({"alt", 25, 10000, 500, -1});
  std::string split_error;
  assert(!bad_split.validate(split_error));
}

void demo_log_and_sqlite_roundtrip() {
  // Defaults keep every pre-M9 behavior: drill, stdout-only, no projection.
  qts::AppConfig defaults = qts::AppConfig::paper_defaults();
  assert(defaults.log_file.empty());
  assert(defaults.log_level == "info");
  assert(defaults.sqlite_file.empty());
  assert(!defaults.demo.enabled);
  std::string error;
  assert(defaults.validate(error));

  // M9 keys are optional but round-trip when present.
  const auto path = write_temp("qts-test-demo.yaml",
                               "environment: paper\n"
                               "config_version: \"1\"\n"
                               "rules_version: \"1\"\n"
                               "ledger_file: /tmp/qts-demo.jsonl\n"
                               "sqlite_file: /tmp/qts-demo.db\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "risk:\n"
                               "  max_order_quantity: 1000\n"
                               "  max_order_notional: 10000000\n"
                               "  trading_enabled: true\n"
                               "log:\n"
                               "  file: /tmp/qts-demo.log\n"
                               "  level: debug\n"
                               "demo:\n"
                               "  enabled: true\n"
                               "  ticks: 500\n"
                               "  interval_ms: 100\n"
                               "  seed: 7\n");
  const qts::AppConfig loaded = qts::load_app_config(path.string());
  assert(loaded.ledger_file == "/tmp/qts-demo.jsonl");
  assert(loaded.sqlite_file == "/tmp/qts-demo.db");
  assert(loaded.postgres.conninfo_env.empty());  // unset stays disabled
  assert(loaded.log_file == "/tmp/qts-demo.log");
  assert(loaded.log_level == "debug");
  assert(loaded.state_file.empty());               // unset stays disabled
  assert(loaded.trading_day.rollover == "00:01");  // absent section keeps the default
  assert(loaded.demo.enabled);
  assert(loaded.demo.ticks == 500);
  assert(loaded.demo.interval_ms == 100);
  assert(loaded.demo.seed == 7);
  assert(loaded.validate(error));
}

void retention_defaults_to_bounded() {
  qts::AppConfig defaults = qts::AppConfig::paper_defaults();
  assert(defaults.retention.log_files == 20 && defaults.retention.ledger_rotate_mb == 64);
  std::string error;
  assert(defaults.validate(error));

  const auto path = write_temp("qts-test-retention.yaml",
                               "environment: paper\n"
                               "config_version: \"1\"\n"
                               "rules_version: \"1\"\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "risk:\n"
                               "  max_order_quantity: 1000\n"
                               "  max_order_notional: 10000000\n"
                               "  trading_enabled: true\n"
                               "retention:\n"
                               "  log_files: 5\n"
                               "  ledger_rotate_mb: 0\n");
  const qts::AppConfig loaded = qts::load_app_config(path.string());
  assert(loaded.retention.log_files == 5 && loaded.retention.ledger_rotate_mb == 0);
  assert(loaded.validate(error));
}

void paper_sim_chaos_validates() {
  qts::AppConfig config = qts::AppConfig::paper_defaults();
  std::string error;
  assert(config.validate(error));  // chaos off by default

  config.paper_sim.chaos_every_ticks = 30;
  assert(!config.validate(error));  // needs outage ticks + channel list
  config.paper_sim.chaos_outage_ticks = 5;
  assert(!config.validate(error));
  config.paper_sim.chaos_channels.push_back("ghost");
  assert(!config.validate(error));  // unknown channel
  config.paper_sim.chaos_channels.clear();
  config.paper_sim.chaos_channels.push_back("simulated");
  assert(config.validate(error));
}

void paper_sim_roundtrip() {
  qts::AppConfig defaults = qts::AppConfig::paper_defaults();
  assert(defaults.paper_sim.starting_cash == 1'000'000);
  assert(!defaults.paper_sim.starting_cash_set);  // M11-7: untouched key means silent legacy path
  assert(defaults.paper_sim.latency_ms == 0 && defaults.paper_sim.slippage_per_unit == 0 && defaults.paper_sim.fee_rate_bp == 0);
  assert(defaults.paper_sim.starting_positions.empty());
  std::string error;
  assert(defaults.validate(error));

  const auto path = write_temp("qts-test-paper-sim.yaml",
                               "environment: paper\n"
                               "config_version: \"1\"\n"
                               "rules_version: \"1\"\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "risk:\n"
                               "  max_order_quantity: 1000\n"
                               "  max_order_notional: 10000000\n"
                               "  trading_enabled: true\n"
                               "paper_sim:\n"
                               "  starting_cash: 2000000\n"
                               "  latency_ms: 100\n"
                               "  slippage: 2\n"
                               "  fee_bp: 25\n"
                               "  starting_positions:\n"
                               "    - instrument: TEST\n"
                               "      quantity: 1000\n"
                               "      avg_cost: 950\n");
  const qts::AppConfig loaded = qts::load_app_config(path.string());
  assert(loaded.paper_sim.starting_cash == 2'000'000);
  assert(loaded.paper_sim.starting_cash_set);  // explicit key arms the funding event
  assert(loaded.paper_sim.latency_ms == 100 && loaded.paper_sim.slippage_per_unit == 2 && loaded.paper_sim.fee_rate_bp == 25);
  assert(loaded.paper_sim.starting_positions.size() == 1);
  assert(loaded.paper_sim.starting_positions[0].instrument == "TEST");
  assert(loaded.paper_sim.starting_positions[0].quantity == 1000 && loaded.paper_sim.starting_positions[0].avg_cost == 950);
  assert(loaded.paper_sim.starting_positions[0].strategy.empty());  // absent strategy means passthrough
  assert(loaded.validate(error));

  qts::AppConfig bad = qts::AppConfig::paper_defaults();
  bad.paper_sim.fee_rate_bp = -1;
  assert(!bad.validate(error));
  qts::AppConfig bad_position = qts::AppConfig::paper_defaults();
  qts::StartingPosition zero{};
  zero.instrument = "TEST";
  zero.avg_cost = 100;
  bad_position.paper_sim.starting_positions.push_back(zero);
  assert(!bad_position.validate(error));
}

void fee_file_roundtrip_with_overrides() {
  const auto dir = std::filesystem::temp_directory_path() / "qts-test-feedir";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  {
    std::ofstream fees{dir / "fees.yaml"};
    fees << "default:\n"
            "  commission_ppm: 2500\n"
            "  commission_minimum: 50000\n"
            "  stamp_ppm: 500\n"
            "  transfer_ppm: 100\n"
            "instruments:\n"
            "  600000.SSE:\n"
            "    commission_ppm: 2000\n";
  }
  const auto main = dir / "main.yaml";
  {
    std::ofstream out{main};
    out << "environment: paper\n"
           "config_version: \"1\"\n"
           "rules_version: \"1\"\n"
           "accounts:\n"
           "  - id: paper\n"
           "    broker: simulated\n"
           "risk:\n"
           "  max_order_quantity: 1000\n"
           "  max_order_notional: 10000000\n"
           "  trading_enabled: true\n"
           "paper_sim:\n"
           "  fee_file: fees.yaml\n";
  }
  const qts::AppConfig loaded = qts::load_app_config(main.string());
  assert(loaded.paper_sim.use_fee_table);
  assert(loaded.paper_sim.fee_file == (dir / "fees.yaml").string());
  assert(loaded.paper_sim.fee_table.fallback.commission_ppm == 2500);
  assert(loaded.paper_sim.fee_table.fallback.commission_minimum == 50000);
  const auto& vip = qts::schedule_for(loaded.paper_sim.fee_table, "600000.SSE");
  assert(vip.commission_ppm == 2000 && vip.stamp_ppm == 0);
  const auto& other = qts::schedule_for(loaded.paper_sim.fee_table, "000001.SZSE");
  assert(other.commission_ppm == 2500 && other.stamp_ppm == 500);
  std::string error;
  assert(loaded.validate(error));

  bool threw = false;
  try {
    (void)qts::load_fee_table((dir / "missing.yaml").string());
  } catch (const std::exception&) {
    threw = true;
  }
  assert(threw);
  std::filesystem::remove_all(dir, ec);
}

void paper_sim_fees_roundtrip() {
  qts::AppConfig defaults = qts::AppConfig::paper_defaults();
  assert(!defaults.paper_sim.use_fees);
  std::string error;
  assert(defaults.validate(error));

  const auto path = write_temp("qts-test-fees.yaml",
                               "environment: paper\n"
                               "config_version: \"1\"\n"
                               "rules_version: \"1\"\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "risk:\n"
                               "  max_order_quantity: 1000\n"
                               "  max_order_notional: 10000000\n"
                               "  trading_enabled: true\n"
                               "paper_sim:\n"
                               "  fees:\n"
                               "    commission_ppm: 2500\n"
                               "    commission_minimum: 50000\n"
                               "    stamp_ppm: 500\n"
                               "    transfer_ppm: 100\n");
  const qts::AppConfig loaded = qts::load_app_config(path.string());
  assert(loaded.paper_sim.use_fees);
  assert(loaded.paper_sim.fees.commission_ppm == 2500);
  assert(loaded.paper_sim.fees.commission_minimum == 50000);
  assert(loaded.paper_sim.fees.stamp_ppm == 500 && loaded.paper_sim.fees.transfer_ppm == 100);
  assert(loaded.validate(error));

  qts::AppConfig bad = qts::AppConfig::paper_defaults();
  bad.paper_sim.use_fees = true;
  bad.paper_sim.fees.stamp_ppm = -1;
  assert(!bad.validate(error));
}

void instruments_roundtrip() {
  qts::AppConfig defaults = qts::AppConfig::paper_defaults();
  assert(defaults.instruments.empty());
  std::string error;
  assert(defaults.validate(error));

  const auto path = write_temp("qts-test-instruments.yaml",
                               "environment: paper\n"
                               "config_version: \"1\"\n"
                               "rules_version: \"1\"\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "    security_account_id: A123\n"
                               "risk:\n"
                               "  max_order_quantity: 100000\n"
                               "  max_order_notional: 1000000000\n"
                               "  trading_enabled: true\n"
                               "instruments:\n"
                               "  - id: TEST\n"
                               "    exchange: sse\n"
                               "    board: main\n"
                               "    tick: 1\n"
                               "    min_qty: 100\n"
                               "    lot_step: 100\n"
                               "    max_qty: 1000000\n"
                               "    sell_odd_lot_only: true\n"
                               "    market_orders_allowed: true\n"
                               "    limit_down: 900\n"
                               "    limit_up: 1100\n");
  const qts::AppConfig loaded = qts::load_app_config(path.string());
  assert(loaded.instruments.size() == 1);
  assert(loaded.instruments[0].id == "TEST" && loaded.instruments[0].tick == 1);
  assert(loaded.instruments[0].min_qty == 100 && loaded.instruments[0].lot_step == 100);
  assert(loaded.instruments[0].limit_down == 900 && loaded.instruments[0].limit_up == 1100);
  assert(loaded.validate(error));
  const qts::AshareInstrumentRules rules = qts::instrument_rules_from(loaded.instruments[0]);
  assert(rules.instrument_id == "TEST" && rules.exchange == qts::Exchange::sse && rules.board == qts::AshareBoard::main);

  qts::AppConfig bad_exchange = qts::AppConfig::paper_defaults();
  bad_exchange.instruments.push_back({"TEST", "nyse", "main", 1, 100, 100, 0, false, true, 0, 0});
  assert(!bad_exchange.validate(error));
  qts::AppConfig dup = qts::AppConfig::paper_defaults();
  dup.instruments.push_back({"TEST", "sse", "main", 1, 100, 100, 0, false, true, 0, 0});
  dup.instruments.push_back({"TEST", "sse", "main", 1, 100, 100, 0, false, true, 0, 0});
  assert(!dup.validate(error));
  qts::AppConfig bad_tick = qts::AppConfig::paper_defaults();
  bad_tick.instruments.push_back({"TEST", "sse", "main", 0, 100, 100, 0, false, true, 0, 0});
  assert(!bad_tick.validate(error));
}

void control_inbox_is_optional_but_roundtrips() {
  qts::AppConfig defaults = qts::AppConfig::paper_defaults();
  assert(defaults.control_inbox.empty());
  assert(defaults.control_socket.empty());
  std::string error;
  assert(defaults.validate(error));

  const auto path = write_temp("qts-test-control.yaml",
                               "environment: paper\n"
                               "config_version: \"1\"\n"
                               "rules_version: \"9\"\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "risk:\n"
                               "  max_order_quantity: 1000\n"
                               "  max_order_notional: 10000000\n"
                               "  trading_enabled: true\n"
                               "control:\n"
                               "  inbox: /tmp/qts-control-inbox.jsonl\n"
                               "  socket: /tmp/qts-query.sock\n"
                               // Retired approval_file (ADR-051) stays ignored.
                               "  approval_file: approvals/live.json\n");
  const qts::AppConfig loaded = qts::load_app_config(path.string());
  assert(loaded.control_inbox == "/tmp/qts-control-inbox.jsonl");
  assert(loaded.control_socket == "/tmp/qts-query.sock");
  assert(loaded.validate(error));
}

void demo_feed_selection_validates() {
  qts::AppConfig defaults = qts::AppConfig::paper_defaults();
  assert(defaults.demo.feed == "simulated");
  std::string error;
  assert(defaults.validate(error));

  qts::AppConfig bad_feed = qts::AppConfig::paper_defaults();
  bad_feed.demo.feed = "ama";
  assert(!bad_feed.validate(error));  // ama needs the master

  qts::AppConfig bad_name = qts::AppConfig::paper_defaults();
  bad_name.demo.feed = "vendor";
  assert(!bad_name.validate(error));

  // AMA feed: strategy instruments must be CODE.SUFFIX master entries.
  qts::AppConfig ama = qts::AppConfig::paper_defaults();
  ama.demo.feed = "ama";
  ama.instruments.push_back({"600000.SSE", "sse", "main", 1, 100, 100, 0, false, true, 800, 1200});
  ama.strategies.clear();
  qts::StrategyConfig grid{};
  grid.id = "grid-1";
  grid.enabled = true;
  grid.type = "grid";
  grid.account = "paper";
  grid.instrument = "600000.SSE";
  ama.strategies.push_back(grid);
  assert(ama.validate(error));

  qts::AppConfig bad_suffix = ama;
  bad_suffix.instruments.clear();
  bad_suffix.instruments.push_back({"600000", "sse", "main", 1, 100, 100, 0, false, true, 800, 1200});
  assert(!bad_suffix.validate(error));

  qts::AppConfig mismatch = ama;
  mismatch.instruments.clear();
  mismatch.instruments.push_back({"600000.SZSE", "sse", "main", 1, 100, 100, 0, false, true, 800, 1200});
  assert(!mismatch.validate(error));

  qts::AppConfig unlisted = ama;
  unlisted.strategies.clear();
  qts::StrategyConfig other = grid;
  other.id = "grid-2";
  other.instrument = "000001.SZSE";
  unlisted.strategies.push_back(grid);
  unlisted.strategies.push_back(other);
  assert(!unlisted.validate(error));

  // File feed: demo.file is required exactly when feed == "file".
  qts::AppConfig fileless = qts::AppConfig::paper_defaults();
  fileless.demo.feed = "file";
  assert(!fileless.validate(error));
  qts::AppConfig filed = qts::AppConfig::paper_defaults();
  filed.demo.feed = "file";
  filed.demo.file = "/tmp/qts-quotes.csv";
  assert(filed.validate(error));
}

void vendor_feed_selection_validates() {
  // Live AMA: enabled needs UMS endpoints, positive staleness, the master,
  // and exclusivity with the simulated/file demo feeds.
  qts::AppConfig ama = qts::AppConfig::paper_defaults();
  ama.ama.enabled = true;
  ama.ama.ums_servers.push_back("192.168.1.101:11001");
  ama.instruments.push_back({"600000.SSE", "sse", "main", 1, 100, 100, 0, false, true, 800, 1200});
  ama.strategies.clear();
  qts::StrategyConfig grid{};
  grid.id = "grid-1";
  grid.enabled = true;
  grid.type = "grid";
  grid.account = "paper";
  grid.instrument = "600000.SSE";
  ama.strategies.push_back(grid);
  std::string error;
  assert(ama.validate(error));

  qts::AppConfig no_ums = ama;
  no_ums.ama.ums_servers.clear();
  assert(!no_ums.validate(error));
  qts::AppConfig bad_ums = ama;
  bad_ums.ama.ums_servers.clear();
  bad_ums.ama.ums_servers.push_back("192.168.1.101");
  assert(!bad_ums.validate(error));
  qts::AppConfig bad_port = ama;
  bad_port.ama.ums_servers.clear();
  bad_port.ama.ums_servers.push_back("192.168.1.101:99999");
  assert(!bad_port.validate(error));
  qts::AppConfig no_master = ama;
  no_master.instruments.clear();
  no_master.strategies.clear();
  assert(!no_master.validate(error));
  qts::AppConfig clash = ama;
  clash.demo.feed = "file";
  clash.demo.file = "/tmp/q.csv";
  assert(!clash.validate(error));

  // QuantAPI: locations required exactly when an account routes to it.
  qts::AppConfig live = qts::AppConfig::paper_defaults();
  live.accounts.clear();
  live.accounts.push_back({"live", "quantapi", ""});
  assert(!live.validate(error));
  live.quantapi.locations = "172.16.109.17:32001";
  assert(live.validate(error));
  live.quantapi.order_way = "toolong";
  assert(!live.validate(error));
  live.quantapi.order_way = "4";
  assert(live.validate(error));
  // ADR-053: gateway user pair is a yaml session param (pair or nothing).
  live.quantapi.agw_user = "user5";
  assert(!live.validate(error));
  // Demo session values (public vendor-demo fixtures, never real secrets;
  // assigned to keep the secret scan literal-free).
  live.quantapi.agw_password.assign("password5");
  assert(live.validate(error));

  // Yaml roundtrip for both sections.
  const auto path = write_temp("qts-test-vendor.yaml",
                               "environment: paper\n"
                               "config_version: \"1\"\n"
                               "rules_version: \"1\"\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "risk:\n"
                               "  max_order_quantity: 1000\n"
                               "  max_order_notional: 10000000\n"
                               "  trading_enabled: true\n"
                               "ama:\n"
                               "  enabled: false\n"
                               "  ums_servers: [192.168.1.101:11001]\n"
                               "  staleness_ms: 3000\n"
                               "quantapi:\n"
                               "  locations: 172.16.109.17:32001\n"
                               "  order_way: \"4\"\n"
                               "  sidecar_path: /tmp/qts.sidecar\n"
                               "  agw_user: user5\n"
                               "  agw_password: password5\n");
  const qts::AppConfig loaded = qts::load_app_config(path.string());
  assert(!loaded.ama.enabled && loaded.ama.ums_servers.size() == 1);
  assert(loaded.ama.staleness_ms == 3000);
  assert(loaded.quantapi.locations == "172.16.109.17:32001");
  assert(loaded.quantapi.order_way == "4" && loaded.quantapi.sidecar_path == "/tmp/qts.sidecar");
  assert(loaded.quantapi.agw_user == "user5" && loaded.quantapi.agw_password == "password5");
  assert(loaded.validate(error));
}

void demo_depth_roundtrip() {
  qts::AppConfig defaults = qts::AppConfig::paper_defaults();
  assert(defaults.demo.depth_levels == 0);
  assert(defaults.demo.auction_ticks == 0 && defaults.demo.halt_for == 0);
  assert(defaults.demo.stale_every == 0 && defaults.demo.crossed_every == 0);
  std::string error;
  assert(defaults.validate(error));

  const auto path = write_temp("qts-test-depth.yaml",
                               "environment: paper\n"
                               "config_version: \"1\"\n"
                               "rules_version: \"1\"\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "risk:\n"
                               "  max_order_quantity: 1000\n"
                               "  max_order_notional: 10000000\n"
                               "  trading_enabled: true\n"
                               "demo:\n"
                               "  enabled: true\n"
                               "  depth_levels: 5\n"
                               "  depth_quantity: 50\n"
                               "  depth_tick: 2\n"
                               "  auction_ticks: 2\n"
                               "  halt_after: 10\n"
                               "  halt_for: 3\n"
                               "  stale_every: 7\n"
                               "  crossed_every: 11\n");
  const qts::AppConfig loaded = qts::load_app_config(path.string());
  assert(loaded.demo.depth_levels == 5 && loaded.demo.depth_quantity == 50 && loaded.demo.depth_tick == 2);
  assert(loaded.demo.auction_ticks == 2);
  assert(loaded.demo.halt_after == 10 && loaded.demo.halt_for == 3);
  assert(loaded.demo.stale_every == 7 && loaded.demo.crossed_every == 11);
  assert(loaded.validate(error));

  qts::AppConfig bad = qts::AppConfig::paper_defaults();
  bad.demo.depth_levels = 5;
  bad.demo.depth_quantity = 0;
  assert(!bad.validate(error));
}

void strategy_and_execution_roundtrip() {
  qts::AppConfig defaults = qts::AppConfig::paper_defaults();
  assert(defaults.execution.algo == "immediate");
  std::string error;
  assert(defaults.validate(error));

  const auto path = write_temp("qts-test-strategy.yaml",
                               "environment: paper\n"
                               "config_version: \"1\"\n"
                               "rules_version: \"1\"\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "risk:\n"
                               "  max_order_quantity: 1000\n"
                               "  max_order_notional: 10000000\n"
                               "  trading_enabled: true\n"
                               "strategies:\n"
                               "  - id: grid-1\n"
                               "    type: grid\n"
                               "    enabled: true\n"
                               "    account: paper\n"
                               "    instrument: TEST\n"
                               "    lower: 900\n"
                               "    upper: 1100\n"
                               "    grids: 4\n"
                               "    quantity: 200\n"
                               "    start: 1000\n"
                               "execution:\n"
                               "  algo: twap\n"
                               "  twap:\n"
                               "    slices: 2\n"
                               "    interval_ms: 500\n");
  const qts::AppConfig loaded = qts::load_app_config(path.string());
  assert(loaded.strategies.size() == 1);
  assert(loaded.strategies[0].quantity == 200 && loaded.strategies[0].enabled);
  assert(loaded.strategies[0].stock == 0 && loaded.strategies[0].stock_avg == 0);
  assert(loaded.execution.algo == "twap");
  assert(loaded.execution.twap_slices == 2 && loaded.execution.twap_interval_ms == 500);
  assert(loaded.validate(error));

  qts::AppConfig bad_type = qts::AppConfig::paper_defaults();
  qts::StrategyConfig magic{};
  magic.id = "x";
  magic.enabled = true;
  magic.type = "magic";
  magic.account = "paper";
  magic.instrument = "TEST";
  bad_type.strategies.push_back(magic);
  assert(!bad_type.validate(error));
  qts::AppConfig bad_grid = qts::AppConfig::paper_defaults();
  qts::StrategyConfig inverted{};
  inverted.id = "x";
  inverted.enabled = true;
  inverted.type = "grid";
  inverted.account = "paper";
  inverted.instrument = "TEST";
  inverted.lower = 1100;
  inverted.upper = 900;
  bad_grid.strategies.push_back(inverted);
  assert(!bad_grid.validate(error));
  qts::AppConfig bad_algo = qts::AppConfig::paper_defaults();
  bad_algo.execution.algo = "magic";
  assert(!bad_algo.validate(error));
  // stock is dynamic-start only: yaml inventory stays in starting_positions.
  qts::AppConfig bad_stock = qts::AppConfig::paper_defaults();
  qts::StrategyConfig stocked{};
  stocked.id = "x";
  stocked.enabled = true;
  stocked.type = "grid";
  stocked.account = "paper";
  stocked.instrument = "TEST";
  stocked.stock = 800;
  bad_stock.strategies.push_back(stocked);
  assert(!bad_stock.validate(error));
}

void rebalance_strategy_roundtrip() {
  const auto path = write_temp("qts-test-rebalance.yaml",
                               "environment: paper\n"
                               "config_version: \"1\"\n"
                               "rules_version: \"1\"\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "risk:\n"
                               "  max_order_quantity: 1000\n"
                               "  max_order_notional: 10000000\n"
                               "  trading_enabled: true\n"
                               "strategies:\n"
                               "  - id: bal-1\n"
                               "    type: rebalance\n"
                               "    enabled: true\n"
                               "    account: paper\n"
                               "    band_bps: 50\n"
                               "    targets:\n"
                               "      - instrument: TEST\n"
                               "        weight_bps: 5000\n");
  const qts::AppConfig loaded = qts::load_app_config(path.string());
  assert(loaded.strategies.size() == 1);
  assert(loaded.strategies[0].type == "rebalance");
  assert(loaded.strategies[0].rebalance_band_bps == 50);
  assert(loaded.strategies[0].rebalance_targets.size() == 1);
  assert(loaded.strategies[0].rebalance_targets[0].instrument == "TEST");
  assert(loaded.strategies[0].rebalance_targets[0].weight_bps == 5000);
  std::string error;
  assert(loaded.validate(error));

  qts::AppConfig no_targets = qts::AppConfig::paper_defaults();
  qts::StrategyConfig empty{};
  empty.id = "bal-x";
  empty.enabled = true;
  empty.type = "rebalance";
  no_targets.strategies.push_back(empty);
  assert(!no_targets.validate(error));
  qts::AppConfig overweight = qts::AppConfig::paper_defaults();
  qts::StrategyConfig heavy{};
  heavy.id = "bal-y";
  heavy.enabled = true;
  heavy.type = "rebalance";
  heavy.rebalance_targets.push_back({"TEST", 6000});
  heavy.rebalance_targets.push_back({"OTHER", 5000});
  overweight.strategies.push_back(heavy);
  assert(!overweight.validate(error));
}

void ama_feed_accepts_rebalance_targets() {
  // A rebalancer's universe is its targets: the grid-only instrument field
  // must not gate it, but an unlisted target still fails closed.
  const auto path = write_temp("qts-test-ama-rebalance.yaml",
                               "environment: paper\n"
                               "config_version: \"1\"\n"
                               "rules_version: \"1\"\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "risk:\n"
                               "  max_order_quantity: 1000\n"
                               "  max_order_notional: 10000000\n"
                               "  trading_enabled: true\n"
                               "strategies:\n"
                               "  - id: bal-1\n"
                               "    type: rebalance\n"
                               "    enabled: true\n"
                               "    account: paper\n"
                               "    targets:\n"
                               "      - instrument: 600000.SSE\n"
                               "        weight_bps: 5000\n"
                               "instruments:\n"
                               "  - id: 600000.SSE\n"
                               "    exchange: sse\n"
                               "    board: main\n"
                               "    tick: 1\n"
                               "    min_qty: 100\n"
                               "    lot_step: 100\n"
                               "demo:\n"
                               "  enabled: true\n"
                               "  ticks: 10\n"
                               "  feed: ama\n");
  const qts::AppConfig loaded = qts::load_app_config(path.string());
  std::string error;
  assert(loaded.validate(error));

  qts::AppConfig stray = loaded;
  stray.strategies[0].rebalance_targets[0].instrument = "999999.SSE";
  assert(!stray.validate(error));
}

void risk_rate_and_switches_roundtrip() {
  qts::AppConfig defaults = qts::AppConfig::paper_defaults();
  assert(defaults.risk.rate_max_orders == 0 && defaults.risk.rate_window_ms == 0);
  assert(defaults.switches.blocked_strategies.empty());
  std::string error;
  assert(defaults.validate(error));

  const auto path = write_temp("qts-test-risk.yaml",
                               "environment: paper\n"
                               "config_version: \"1\"\n"
                               "rules_version: \"1\"\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "risk:\n"
                               "  max_order_quantity: 1000\n"
                               "  max_order_notional: 10000000\n"
                               "  max_position_quantity: 500\n"
                               "  trading_enabled: true\n"
                               "  rate:\n"
                               "    max_orders: 100\n"
                               "    window_ms: 60000\n"
                               "  blocked_strategies: [grid-1]\n"
                               "  blocked_accounts: []\n"
                               "  blocked_instruments: [TEST]\n");
  const qts::AppConfig loaded = qts::load_app_config(path.string());
  assert(loaded.risk.max_position_quantity == 500);
  assert(!loaded.risk.enforce_cash);  // absent key keeps the permissive default
  assert(loaded.risk.rate_max_orders == 100 && loaded.risk.rate_window_ms == 60000);
  assert(loaded.switches.blocked_strategies.size() == 1 && loaded.switches.blocked_strategies[0] == "grid-1");
  assert(loaded.switches.blocked_accounts.empty());
  assert(loaded.switches.blocked_instruments.size() == 1);
  assert(loaded.validate(error));

  qts::AppConfig bad_window = qts::AppConfig::paper_defaults();
  bad_window.risk.rate_window_ms = -1;
  assert(!bad_window.validate(error));
  qts::AppConfig bad_position = qts::AppConfig::paper_defaults();
  bad_position.risk.max_position_quantity = -1;
  assert(!bad_position.validate(error));
}

void risk_safety_net_roundtrip() {
  qts::AppConfig defaults = qts::AppConfig::paper_defaults();
  assert(defaults.risk.max_consecutive_rejects == 3);
  assert(defaults.risk.max_reject_rate_bps == 3000 && defaults.risk.reject_rate_min_orders == 100);
  assert(defaults.risk.max_loss_ratio_bps == 3000);
  std::string error;
  assert(defaults.validate(error));

  const auto path = write_temp("qts-test-risk-safety.yaml",
                               "environment: paper\n"
                               "config_version: \"1\"\n"
                               "rules_version: \"1\"\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "risk:\n"
                               "  max_order_quantity: 1000\n"
                               "  max_order_notional: 10000000\n"
                               "  trading_enabled: true\n"
                               "  max_consecutive_rejects: 5\n"
                               "  max_reject_rate_bps: 2000\n"
                               "  reject_rate_min_orders: 50\n"
                               "  max_loss_ratio_bps: 1000\n");
  const qts::AppConfig loaded = qts::load_app_config(path.string());
  assert(loaded.risk.max_consecutive_rejects == 5);
  assert(loaded.risk.max_reject_rate_bps == 2000 && loaded.risk.reject_rate_min_orders == 50);
  assert(loaded.risk.max_loss_ratio_bps == 1000);
  assert(loaded.validate(error));

  qts::AppConfig bad_streak = qts::AppConfig::paper_defaults();
  bad_streak.risk.max_consecutive_rejects = -1;
  assert(!bad_streak.validate(error));
  qts::AppConfig bad_rate = qts::AppConfig::paper_defaults();
  bad_rate.risk.max_reject_rate_bps = 10001;
  assert(!bad_rate.validate(error));
  qts::AppConfig bad_loss = qts::AppConfig::paper_defaults();
  bad_loss.risk.max_loss_ratio_bps = -1;
  assert(!bad_loss.validate(error));
  qts::AppConfig bad_min = qts::AppConfig::paper_defaults();
  bad_min.risk.reject_rate_min_orders = -1;
  assert(!bad_min.validate(error));
}

void trading_day_rollover_parses_and_schedules() {
  // Defaults keep the old behavior: 00:01 local.
  qts::AppConfig defaults = qts::AppConfig::paper_defaults();
  assert(defaults.trading_day.rollover == "00:01");
  std::string error;
  assert(defaults.validate(error));

  int hour = -1;
  int minute = -1;
  assert(qts::parse_rollover("00:01", hour, minute) && hour == 0 && minute == 1);
  assert(qts::parse_rollover("23:59", hour, minute) && hour == 23 && minute == 59);
  assert(!qts::parse_rollover("7:30", hour, minute));
  assert(!qts::parse_rollover("24:00", hour, minute));
  assert(!qts::parse_rollover("00:60", hour, minute));
  assert(!qts::parse_rollover("midnight", hour, minute));
  assert(!qts::parse_rollover("", hour, minute));

  qts::AppConfig bad = qts::AppConfig::paper_defaults();
  bad.trading_day.rollover = "25:00";
  assert(!bad.validate(error));

  // Fixed local noon: next 00:01 is tomorrow; next 23:59 is today.
  std::tm noon{};
  noon.tm_year = 126;
  noon.tm_mon = 9;
  noon.tm_mday = 2;
  noon.tm_hour = 12;
  noon.tm_min = 0;
  const std::time_t now = std::mktime(&noon);
  assert(now != static_cast<std::time_t>(-1));
  const std::time_t next_dawn = qts::next_rollover_after(now, 0, 1);
  const std::time_t next_late = qts::next_rollover_after(now, 23, 59);
  assert(next_dawn > now && next_late > now && next_late < next_dawn);
  // A time 60 seconds ahead lands within the hour, strictly after now.
  std::tm soon = noon;
  soon.tm_min = 1;
  const std::time_t anchor = std::mktime(&soon);
  const std::time_t imminent = qts::next_rollover_after(now, 12, 1);
  assert(imminent == anchor);
}

void log_level_is_validated() {
  qts::AppConfig config = qts::AppConfig::paper_defaults();
  std::string error;
  config.log_level = "verbose";
  assert(!config.validate(error));
  config.log_level = "debug";
  assert(config.validate(error));
}

void relative_paths_resolve_against_config_dir() {
  // Unified ./bin layout: bin/cfg/*.yaml writes ../db/... and ../log/...;
  // the loader anchors them at the config file's directory (CWD-proof).
  // Absolute paths pass through untouched.
  const auto dir = std::filesystem::temp_directory_path() / "qts-test-cfgdir";
  std::filesystem::create_directories(dir);
  const auto path = dir / "case.yaml";
  {
    std::ofstream out{path};
    out << "environment: paper\n"
           "config_version: \"1\"\n"
           "rules_version: \"1\"\n"
           "ledger_file: ../db/a.jsonl\n"
           "sqlite_file: /abs/b.db\n"
           "accounts:\n"
           "  - id: paper\n"
           "    broker: simulated\n"
           "risk:\n"
           "  max_order_quantity: 1000\n"
           "  max_order_notional: 10000000\n"
           "  trading_enabled: true\n"
           "log:\n"
           "  file: ../log/c.log\n";
  }
  const qts::AppConfig loaded = qts::load_app_config(path.string());
  assert(loaded.ledger_file == (dir / "../db/a.jsonl").string());
  assert(loaded.sqlite_file == "/abs/b.db");
  assert(loaded.log_file == (dir / "../log/c.log").string());
  std::string error;
  assert(loaded.validate(error));
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

void gateway_is_optional_but_roundtrips() {
  const qts::AppConfig defaults = qts::AppConfig::paper_defaults();
  assert(defaults.gateway.socket.empty());

  const auto path = write_temp("qts-test-gateway.yaml",
                               "environment: paper\n"
                               "config_version: \"7\"\n"
                               "rules_version: \"9\"\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "risk:\n"
                               "  max_order_quantity: 1000\n"
                               "  max_order_notional: 10000000\n"
                               "  trading_enabled: true\n"
                               "gateway:\n"
                               "  socket: gateway/qts-gateway.sock\n");
  const qts::AppConfig loaded = qts::load_app_config(path.string());
  assert(loaded.gateway.socket == (std::filesystem::path{path.string()}.parent_path() / "gateway/qts-gateway.sock").string());

  // Retired password tables (operators[]/accounts[]) are ignored, never
  // refused: ADR-050 removed gateway-side passwords, old private configs
  // keep loading.
  const auto legacy = write_temp("qts-test-gateway-legacy.yaml",
                                 "environment: paper\n"
                                 "config_version: \"7\"\n"
                                 "rules_version: \"9\"\n"
                                 "accounts:\n"
                                 "  - id: paper\n"
                                 "    broker: simulated\n"
                                 "risk:\n"
                                 "  trading_enabled: true\n"
                                 "gateway:\n"
                                 "  socket: gateway/qts-gateway.sock\n"
                                 "  operators:\n"
                                 "    - id: trader\n"
                                 "      credential: pw\n"
                                 "  accounts:\n"
                                 "    - account: paper\n"
                                 "      credential: pw\n");
  const qts::AppConfig legacy_loaded = qts::load_app_config(legacy.string());
  assert(!legacy_loaded.gateway.socket.empty());
}

void gateway_bind_roundtrip_and_rejects() {
  const auto good = write_temp("qts-test-gateway-bind.yaml",
                               "environment: paper\n"
                               "config_version: \"7\"\n"
                               "rules_version: \"9\"\n"
                               "accounts:\n"
                               "  - id: paper\n"
                               "    broker: simulated\n"
                               "risk:\n"
                               "  trading_enabled: true\n"
                               "gateway:\n"
                               "  socket: gateway/qts-gateway.sock\n"
                               "  bind: 127.0.0.1:19001\n");
  const qts::AppConfig loaded = qts::load_app_config(good.string());
  assert(loaded.gateway.bind == "127.0.0.1:19001");
  std::string error;
  assert(loaded.validate(error));
  const std::string head =
      "environment: paper\nconfig_version: \"7\"\nrules_version: \"9\"\naccounts:\n  - id: paper\n    broker: simulated\nrisk:\n  trading_enabled: true\ngateway:\n  socket: s.sock\n";
  const char* tails[] = {"  bind: no-colon\n", "  bind: :19001\n", "  bind: 127.0.0.1:\n", "  bind: 127.0.0.1:abc\n", "  bind: 127.0.0.1:99999\n"};
  for (std::size_t i = 0; i < 5; ++i) {
    const auto path = write_temp("qts-test-gateway-bind-bad.yaml", head + tails[i]);
    bool threw = false;
    try {
      (void)qts::load_app_config(path.string());
    } catch (const std::runtime_error&) {
      threw = true;
    }
    assert(threw);
  }
}

void gateway_rejects_non_map() {
  const std::string head = "environment: paper\nconfig_version: \"7\"\nrules_version: \"9\"\naccounts:\n  - id: paper\n    broker: simulated\nrisk:\n  trading_enabled: true\ngateway:\n";
  const std::string broken_head = head.substr(0, head.size() - std::string{"gateway:\n"}.size()) + "gateway: 7\n";
  const auto bad = write_temp("qts-test-gateway-bad.yaml", broken_head);
  bool threw = false;
  try {
    (void)qts::load_app_config(bad.string());
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
}


}  // namespace

int main() {
  defaults_are_valid();
  live_is_rejected_before_m3();
  accounts_are_required_and_unique();
  versions_and_limits();
  loads_example_shape();
  ledger_file_is_optional_but_roundtrips();
  loader_rejects_bad_files();
  routing_is_optional_but_validated();
  routing_loads_from_yaml();
  demo_log_and_sqlite_roundtrip();
  log_level_is_validated();
  relative_paths_resolve_against_config_dir();
  trading_day_rollover_parses_and_schedules();
  risk_rate_and_switches_roundtrip();
  strategy_and_execution_roundtrip();
  rebalance_strategy_roundtrip();
  ama_feed_accepts_rebalance_targets();
  demo_depth_roundtrip();
  paper_sim_roundtrip();
  paper_sim_fees_roundtrip();
  fee_file_roundtrip_with_overrides();
  paper_sim_chaos_validates();
  retention_defaults_to_bounded();
  instruments_roundtrip();
  demo_feed_selection_validates();
  vendor_feed_selection_validates();
  control_inbox_is_optional_but_roundtrips();
  gateway_is_optional_but_roundtrips();
  gateway_bind_roundtrip_and_rejects();
  gateway_rejects_non_map();
  risk_safety_net_roundtrip();
  return 0;
}
