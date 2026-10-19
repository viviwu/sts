// clang-format off
/**
  ******************************************************************************
  * @file           : config.cpp
  * @author         : vivi wu
  * @brief          : Yaml config loader: relative-path anchoring, optional keys and validation.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/config/config.hpp"

#include <cctype>
#include <cstddef>
#include <ctime>
#include <stdexcept>

#include "yaml-cpp/yaml.h"

namespace qts {
namespace {

bool is_absolute_path(const std::string& path) { return !path.empty() && path[0] == '/'; }

// Unified ./bin layout: ledger/sqlite/log paths written relative in
// bin/cfg/*.yaml resolve against the config file's own directory, so the
// gateway works from any working directory. Absolute paths pass through.
std::string resolve_against(const std::string& config_path, std::string value) {
  if (value.empty() || is_absolute_path(value)) return value;
  const std::size_t slash = config_path.find_last_of('/');
  const std::string dir = slash == std::string::npos ? "." : config_path.substr(0, slash);
  return dir + "/" + value;
}

template <typename T>
T required(const YAML::Node& node, const std::string& file, const char* key) {
  const YAML::Node child = node[key];
  if (!child.IsDefined()) throw std::runtime_error(file + ": missing required key '" + key + "'");
  try {
    return child.as<T>();
  } catch (const YAML::Exception& e) {
    throw std::runtime_error(file + ": bad value for '" + key + "': " + e.what());
  }
}

template <typename T>
T optional(const YAML::Node& node, const char* key, T fallback) {
  const YAML::Node child = node[key];
  if (!child.IsDefined()) return fallback;
  return child.as<T>(fallback);
}

}  // namespace

bool parse_rollover(const std::string& text, int& hour, int& minute) {
  // Strict shape HH:MM, digits only; "7:30" and "24:00" are rejected.
  if (text.size() != 5 || text[2] != ':') return false;
  for (const int pos : {0, 1, 3, 4}) {
    if (std::isdigit(static_cast<unsigned char>(text[static_cast<std::size_t>(pos)])) == 0) return false;
  }
  const int parsed_hour = ((text[0] - '0') * 10) + (text[1] - '0');
  const int parsed_minute = ((text[3] - '0') * 10) + (text[4] - '0');
  if (parsed_hour > 23 || parsed_minute > 59) return false;
  hour = parsed_hour;
  minute = parsed_minute;
  return true;
}

std::time_t next_rollover_after(std::time_t now, int hour, int minute) {
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  local.tm_hour = hour;
  local.tm_min = minute;
  local.tm_sec = 0;
  std::time_t candidate = std::mktime(&local);
  if (candidate == static_cast<std::time_t>(-1) || candidate <= now) {
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    local.tm_mday += 1;
    local.tm_hour = hour;
    local.tm_min = minute;
    local.tm_sec = 0;
    candidate = std::mktime(&local);
  }
  return candidate;
}

void parse_log_section(const YAML::Node& root, const std::string& path, AppConfig& config) {
  const YAML::Node log = root["log"];
  if (!log.IsDefined()) return;
  if (!log.IsMap()) throw std::runtime_error(path + ": 'log' must be a mapping");
  config.log_file = resolve_against(path, optional<std::string>(log, "file", ""));
  config.log_level = optional<std::string>(log, "level", "info");
}

void parse_demo_section(const YAML::Node& root, const std::string& path, AppConfig& config) {
  const YAML::Node demo = root["demo"];
  if (!demo.IsDefined()) return;
  if (!demo.IsMap()) throw std::runtime_error(path + ": 'demo' must be a mapping");
  config.demo.enabled = optional<bool>(demo, "enabled", false);
  config.demo.ticks = optional<std::uint64_t>(demo, "ticks", 200);
  config.demo.interval_ms = optional<std::uint64_t>(demo, "interval_ms", 0);
  config.demo.seed = optional<std::uint64_t>(demo, "seed", 42);
  config.demo.depth_levels = optional<std::uint64_t>(demo, "depth_levels", 0);
  config.demo.depth_quantity = optional<Quantity>(demo, "depth_quantity", 0);
  config.demo.depth_tick = optional<Price>(demo, "depth_tick", 1);
  config.demo.auction_ticks = optional<std::uint64_t>(demo, "auction_ticks", 0);
  config.demo.halt_after = optional<std::uint64_t>(demo, "halt_after", 0);
  config.demo.halt_for = optional<std::uint64_t>(demo, "halt_for", 0);
  config.demo.stale_every = optional<std::uint64_t>(demo, "stale_every", 0);
  config.demo.crossed_every = optional<std::uint64_t>(demo, "crossed_every", 0);
  config.demo.feed = optional<std::string>(demo, "feed", "simulated");
  config.demo.file = resolve_against(path, optional<std::string>(demo, "file", ""));
}

void parse_ama_section(const YAML::Node& root, const std::string& path, AppConfig& config) {
  const YAML::Node ama = root["ama"];
  if (!ama.IsDefined()) return;
  if (!ama.IsMap()) throw std::runtime_error(path + ": 'ama' must be a mapping");
  config.ama.enabled = optional<bool>(ama, "enabled", false);
  config.ama.staleness_ms = optional<std::int64_t>(ama, "staleness_ms", 3000);
  const YAML::Node servers = ama["ums_servers"];
  if (servers.IsDefined()) {
    if (!servers.IsSequence()) throw std::runtime_error(path + ": 'ama.ums_servers' must be a sequence");
    for (const auto& item : servers) config.ama.ums_servers.push_back(item.as<std::string>(""));
  }
}

void parse_quantapi_section(const YAML::Node& root, const std::string& path, AppConfig& config) {
  const YAML::Node quantapi = root["quantapi"];
  if (!quantapi.IsDefined()) return;
  if (!quantapi.IsMap()) throw std::runtime_error(path + ": 'quantapi' must be a mapping");
  config.quantapi.locations = optional<std::string>(quantapi, "locations", "");
  config.quantapi.order_way = optional<std::string>(quantapi, "order_way", "4");
  config.quantapi.sidecar_path = resolve_against(path, optional<std::string>(quantapi, "sidecar_path", ""));
  config.quantapi.agw_user = optional<std::string>(quantapi, "agw_user", "");
  config.quantapi.agw_password = optional<std::string>(quantapi, "agw_password", "");
}

void parse_trading_day_section(const YAML::Node& root, const std::string& path, AppConfig& config) {
  const YAML::Node trading_day = root["trading_day"];
  if (!trading_day.IsDefined()) return;
  if (!trading_day.IsMap()) throw std::runtime_error(path + ": 'trading_day' must be a mapping");
  config.trading_day.rollover = optional<std::string>(trading_day, "rollover", "00:01");
}

void parse_paper_sim_section(const YAML::Node& root, const std::string& path, AppConfig& config) {
  const YAML::Node paper = root["paper_sim"];
  if (!paper.IsDefined()) return;
  if (!paper.IsMap()) throw std::runtime_error(path + ": 'paper_sim' must be a mapping");
  config.paper_sim.starting_cash = optional<Price>(paper, "starting_cash", 1'000'000);
  config.paper_sim.starting_cash_set = paper["starting_cash"].IsDefined();
  config.paper_sim.latency_ms = optional<std::int64_t>(paper, "latency_ms", 0);
  config.paper_sim.slippage_per_unit = optional<Price>(paper, "slippage", 0);
  config.paper_sim.fee_rate_bp = optional<int>(paper, "fee_bp", 0);
  const YAML::Node fees = paper["fees"];
  if (fees.IsDefined()) {
    if (!fees.IsMap()) throw std::runtime_error(path + ": 'paper_sim.fees' must be a mapping");
    config.paper_sim.use_fees = true;
    config.paper_sim.fees.commission_ppm = optional<std::int64_t>(fees, "commission_ppm", 0);
    config.paper_sim.fees.commission_minimum = optional<Price>(fees, "commission_minimum", 0);
    config.paper_sim.fees.stamp_ppm = optional<std::int64_t>(fees, "stamp_ppm", 0);
    config.paper_sim.fees.transfer_ppm = optional<std::int64_t>(fees, "transfer_ppm", 0);
  }
  const auto fee_file = optional<std::string>(paper, "fee_file", "");
  if (!fee_file.empty()) {
    config.paper_sim.fee_file = resolve_against(path, fee_file);
    config.paper_sim.fee_table = load_fee_table(config.paper_sim.fee_file);
    config.paper_sim.use_fee_table = true;
  }
  const YAML::Node positions = paper["starting_positions"];
  if (positions.IsDefined()) {
    if (!positions.IsSequence()) throw std::runtime_error(path + ": 'paper_sim.starting_positions' must be a sequence");
    for (const auto& item : positions) {
      StartingPosition position{};
      position.instrument = required<std::string>(item, path, "instrument");
      position.quantity = required<Quantity>(item, path, "quantity");
      position.avg_cost = optional<Price>(item, "avg_cost", 0);
      position.strategy = optional<std::string>(item, "strategy", "");
      config.paper_sim.starting_positions.push_back(std::move(position));
    }
  }
  config.paper_sim.chaos_every_ticks = optional<std::uint64_t>(paper, "chaos_every_ticks", 0);
  config.paper_sim.chaos_outage_ticks = optional<std::uint64_t>(paper, "chaos_outage_ticks", 0);
  const YAML::Node chaos_channels = paper["chaos_channels"];
  if (chaos_channels.IsDefined()) {
    if (!chaos_channels.IsSequence()) throw std::runtime_error(path + ": 'paper_sim.chaos_channels' must be a sequence");
    for (const auto& item : chaos_channels) config.paper_sim.chaos_channels.push_back(item.as<std::string>(""));
  }
}

void parse_control_section(const YAML::Node& root, const std::string& path, AppConfig& config) {
  const YAML::Node control = root["control"];
  if (!control.IsDefined()) return;
  if (!control.IsMap()) throw std::runtime_error(path + ": 'control' must be a mapping");
  config.control_inbox = resolve_against(path, optional<std::string>(control, "inbox", ""));
  config.control_socket = resolve_against(path, optional<std::string>(control, "socket", ""));
  // Retired approval_file (ADR-051) stays ignored, never refused: old
  // private configs keep loading without it authorizing anything.
}

void parse_gateway_section(const YAML::Node& root, const std::string& path, AppConfig& config) {
  const YAML::Node gateway = root["gateway"];
  if (!gateway.IsDefined()) return;
  if (!gateway.IsMap()) throw std::runtime_error(path + ": 'gateway' must be a mapping");
  config.gateway.socket = resolve_against(path, optional<std::string>(gateway, "socket", ""));
  const std::string bind = optional<std::string>(gateway, "bind", "");
  if (!bind.empty()) {
    // Strict host:port (last colon separates the port): garbage fails
    // closed at startup instead of listening on a surprise interface.
    const std::size_t mark = bind.rfind(':');
    if (mark == std::string::npos || mark == 0 || mark + 1 >= bind.size()) throw std::runtime_error(path + ": 'gateway.bind' must be host:port");
    const std::string port_text = bind.substr(mark + 1);
    for (const char digit : port_text) {
      if (digit < '0' || digit > '9') throw std::runtime_error(path + ": 'gateway.bind' port must be numeric");
    }
    unsigned long port = 0;
    try {
      port = std::stoul(port_text);
    } catch (const std::exception&) {
      throw std::runtime_error(path + ": 'gateway.bind' port out of range");
    }
    if (port > 65535) throw std::runtime_error(path + ": 'gateway.bind' port out of range");
    config.gateway.bind = bind;
  }
  // Retired password tables (operators[]/accounts[]) stay ignored, never
  // refused: ADR-050 removed gateway-side passwords (the broker password is
  // the single credential), old private configs keep loading.
}

void parse_postgres_section(const YAML::Node& root, const std::string& path, AppConfig& config) {
  const YAML::Node postgres = root["postgres"];
  if (!postgres.IsDefined()) return;
  if (!postgres.IsMap()) throw std::runtime_error(path + ": 'postgres' must be a mapping");
  config.postgres.conninfo_env = optional<std::string>(postgres, "conninfo_env", "");
}

void parse_metrics_endpoint_section(const YAML::Node& root, const std::string& path, AppConfig& config) {
  const YAML::Node metrics = root["metrics"];
  if (!metrics.IsDefined()) return;
  if (!metrics.IsMap()) throw std::runtime_error(path + ": 'metrics' must be a mapping");
  config.metrics_endpoint.listen = optional<std::string>(metrics, "listen", "");
}

void parse_retention_section(const YAML::Node& root, const std::string& path, AppConfig& config) {
  const YAML::Node retention = root["retention"];
  if (!retention.IsDefined()) return;
  if (!retention.IsMap()) throw std::runtime_error(path + ": 'retention' must be a mapping");
  config.retention.log_files = optional<std::size_t>(retention, "log_files", 20);
  config.retention.ledger_rotate_mb = optional<std::uint64_t>(retention, "ledger_rotate_mb", 64);
}

void parse_accounts_section(const YAML::Node& root, const std::string& path, AppConfig& config) {
  const YAML::Node accounts = root["accounts"];
  if (!accounts.IsDefined() || !accounts.IsSequence()) {
    throw std::runtime_error(path + ": 'accounts' must be a sequence");
  }
  for (const auto& item : accounts) {
    AccountConfig account{};
    account.id = required<std::string>(item, path, "id");
    account.broker = required<std::string>(item, path, "broker");
    account.security_account_id = optional<std::string>(item, "security_account_id", "");
    config.accounts.push_back(std::move(account));
  }
}

void parse_id_list(const YAML::Node& risk, const std::string& path, const char* key, std::vector<std::string>& out) {
  const YAML::Node list = risk[key];
  if (!list.IsDefined()) return;
  if (!list.IsSequence()) throw std::runtime_error(path + ": 'risk." + key + "' must be a sequence");
  for (const auto& item : list) {
    try {
      out.push_back(item.as<std::string>());
    } catch (const YAML::Exception& e) {
      throw std::runtime_error(path + ": bad value in 'risk." + key + "': " + e.what());
    }
  }
}

void parse_risk_section(const YAML::Node& root, const std::string& path, AppConfig& config) {
  const YAML::Node risk = root["risk"];
  if (!risk.IsDefined() || !risk.IsMap()) throw std::runtime_error(path + ": 'risk' must be a mapping");
  config.risk.max_order_quantity = optional<Quantity>(risk, "max_order_quantity", 0);
  config.risk.max_order_notional = optional<Price>(risk, "max_order_notional", 0);
  config.risk.max_position_quantity = optional<Quantity>(risk, "max_position_quantity", 0);
  config.risk.trading_enabled = optional<bool>(risk, "trading_enabled", true);
  config.risk.enforce_sellable = optional<bool>(risk, "enforce_sellable", false);
  config.risk.enforce_cash = optional<bool>(risk, "enforce_cash", false);
  config.risk.daily_loss_limit = optional<Price>(risk, "daily_loss_limit", 0);
  config.risk.daily_loss_limit = optional<Price>(risk, "daily_loss_limit", 0);
  config.risk.max_consecutive_rejects = optional<int>(risk, "max_consecutive_rejects", 3);
  config.risk.max_reject_rate_bps = optional<int>(risk, "max_reject_rate_bps", 3000);
  config.risk.reject_rate_min_orders = optional<int>(risk, "reject_rate_min_orders", 100);
  config.risk.max_loss_ratio_bps = optional<int>(risk, "max_loss_ratio_bps", 3000);
  const YAML::Node rate = risk["rate"];
  if (rate.IsDefined()) {
    if (!rate.IsMap()) throw std::runtime_error(path + ": 'risk.rate' must be a mapping");
    config.risk.rate_max_orders = optional<std::size_t>(rate, "max_orders", 0);
    config.risk.rate_window_ms = optional<std::int64_t>(rate, "window_ms", 0);
  }
  parse_id_list(risk, path, "blocked_strategies", config.switches.blocked_strategies);
  parse_id_list(risk, path, "blocked_accounts", config.switches.blocked_accounts);
  parse_id_list(risk, path, "blocked_instruments", config.switches.blocked_instruments);
}

void parse_strategies_section(const YAML::Node& root, const std::string& path, AppConfig& config) {
  const YAML::Node strategies = root["strategies"];
  if (!strategies.IsDefined()) return;
  if (!strategies.IsSequence()) throw std::runtime_error(path + ": 'strategies' must be a sequence");
  for (const auto& item : strategies) {
    StrategyConfig strategy{};
    strategy.id = required<std::string>(item, path, "id");
    strategy.enabled = optional<bool>(item, "enabled", false);
    strategy.type = optional<std::string>(item, "type", "grid");
    strategy.account = optional<std::string>(item, "account", "paper");
    strategy.instrument = optional<std::string>(item, "instrument", "TEST");
    strategy.lower = optional<Price>(item, "lower", 900);
    strategy.upper = optional<Price>(item, "upper", 1'100);
    strategy.grids = optional<int>(item, "grids", 4);
    strategy.quantity = optional<Quantity>(item, "quantity", 100);
    strategy.start = optional<Price>(item, "start", 1'000);
    strategy.stock = optional<Quantity>(item, "stock", 0);
    strategy.stock_avg = optional<Price>(item, "stock_avg", 0);
    if (strategy.stock < 0) throw std::runtime_error(path + ": 'strategies[].stock' must be non-negative");
    if (strategy.stock_avg < 0) throw std::runtime_error(path + ": 'strategies[].stock_avg' must be non-negative");
    const YAML::Node targets = item["targets"];
    if (targets.IsDefined()) {
      if (!targets.IsSequence()) throw std::runtime_error(path + ": 'strategies[].targets' must be a sequence");
      for (const auto& entry : targets) {
        StrategyConfig::TargetWeight weight{};
        weight.instrument = required<std::string>(entry, path, "instrument");
        weight.weight_bps = optional<int>(entry, "weight_bps", 0);
        strategy.rebalance_targets.push_back(std::move(weight));
      }
    }
    strategy.rebalance_band_bps = optional<int>(item, "band_bps", 0);
    config.strategies.push_back(std::move(strategy));
  }
}

void parse_execution_section(const YAML::Node& root, const std::string& path, AppConfig& config) {
  const YAML::Node execution = root["execution"];
  if (!execution.IsDefined()) return;
  if (!execution.IsMap()) throw std::runtime_error(path + ": 'execution' must be a mapping");
  config.execution.algo = optional<std::string>(execution, "algo", "immediate");
  const YAML::Node twap = execution["twap"];
  if (twap.IsDefined()) {
    if (!twap.IsMap()) throw std::runtime_error(path + ": 'execution.twap' must be a mapping");
    config.execution.twap_slices = optional<int>(twap, "slices", 4);
    config.execution.twap_interval_ms = optional<std::int64_t>(twap, "interval_ms", 1'000);
  }
  const YAML::Node pov = execution["pov"];
  if (pov.IsDefined()) {
    if (!pov.IsMap()) throw std::runtime_error(path + ": 'execution.pov' must be a mapping");
    config.execution.pov_participation_bps = optional<int>(pov, "participation_bps", 1'000);
  }
  const YAML::Node vwap = execution["vwap"];
  if (vwap.IsDefined()) {
    if (!vwap.IsMap()) throw std::runtime_error(path + ": 'execution.vwap' must be a mapping");
    const YAML::Node weights = vwap["weights"];
    if (weights.IsDefined()) {
      if (!weights.IsSequence()) throw std::runtime_error(path + ": 'execution.vwap.weights' must be a sequence");
      config.execution.vwap_weights.clear();
      for (const auto& weight : weights) config.execution.vwap_weights.push_back(weight.as<int>(0));
    }
    config.execution.vwap_interval_ms = optional<std::int64_t>(vwap, "interval_ms", 1'000);
  }
  const YAML::Node iceberg = execution["iceberg"];
  if (iceberg.IsDefined()) {
    if (!iceberg.IsMap()) throw std::runtime_error(path + ": 'execution.iceberg' must be a mapping");
    config.execution.iceberg_display = optional<Quantity>(iceberg, "display", 100);
  }
}

void parse_routing_section(const YAML::Node& root, const std::string& path, AppConfig& config) {
  const YAML::Node routing = root["routing"];
  if (!routing.IsDefined()) return;
  if (!routing.IsMap()) throw std::runtime_error(path + ": 'routing' must be a mapping");
  const YAML::Node weights = routing["weights"];
  if (weights.IsDefined()) {
    if (!weights.IsMap()) throw std::runtime_error(path + ": 'routing.weights' must be a mapping");
    config.routing.weight_fee = optional<std::int64_t>(weights, "fee", 1);
    config.routing.weight_latency = optional<std::int64_t>(weights, "latency", 1);
    config.routing.weight_fill = optional<std::int64_t>(weights, "fill", 1);
  }
  config.routing.ewma_alpha_bps = optional<std::int64_t>(routing, "ewma_alpha_bps", 2000);
  const YAML::Node channels = routing["channels"];
  if (!channels.IsDefined()) return;
  if (!channels.IsSequence()) throw std::runtime_error(path + ": 'routing.channels' must be a sequence");
  for (const auto& item : channels) {
    ChannelStatsConfig channel{};
    channel.channel = required<std::string>(item, path, "channel");
    channel.fee_bps = optional<std::int64_t>(item, "fee_bps", 0);
    channel.fill_rate_bps = optional<std::int64_t>(item, "fill_rate_bps", 0);
    channel.latency_us = optional<std::int64_t>(item, "latency_us", 0);
    channel.split_weight = optional<std::int64_t>(item, "split_weight", 0);
    config.routing.channels.push_back(std::move(channel));
  }
}

void parse_instruments_section(const YAML::Node& root, const std::string& path, AppConfig& config) {
  const YAML::Node instruments = root["instruments"];
  if (!instruments.IsDefined()) return;
  if (!instruments.IsSequence()) throw std::runtime_error(path + ": 'instruments' must be a sequence");
  for (const auto& item : instruments) {
    InstrumentConfig entry{};
    entry.id = required<std::string>(item, path, "id");
    entry.exchange = optional<std::string>(item, "exchange", "sse");
    entry.board = optional<std::string>(item, "board", "main");
    entry.tick = optional<Price>(item, "tick", 1);
    entry.min_qty = optional<Quantity>(item, "min_qty", 100);
    entry.lot_step = optional<Quantity>(item, "lot_step", 100);
    entry.max_qty = optional<Quantity>(item, "max_qty", 0);
    entry.sell_odd_lot_only = optional<bool>(item, "sell_odd_lot_only", false);
    entry.market_orders_allowed = optional<bool>(item, "market_orders_allowed", true);
    entry.limit_down = optional<Price>(item, "limit_down", 0);
    entry.limit_up = optional<Price>(item, "limit_up", 0);
    config.instruments.push_back(std::move(entry));
  }
}

Exchange parse_exchange(const std::string& text) {
  if (text == "sse") return Exchange::sse;
  if (text == "szse") return Exchange::szse;
  if (text == "bse") return Exchange::bse;
  throw std::runtime_error("unknown exchange: " + text);
}

AshareBoard parse_board(const std::string& text) {
  if (text == "main") return AshareBoard::main;
  if (text == "star") return AshareBoard::star;
  if (text == "chinext") return AshareBoard::chinext;
  if (text == "beijing_select") return AshareBoard::beijing_select;
  throw std::runtime_error("unknown board: " + text);
}

AshareInstrumentRules instrument_rules_from(const InstrumentConfig& entry) {
  AshareInstrumentRules rules{};
  rules.instrument_id = entry.id;
  rules.exchange = parse_exchange(entry.exchange);
  rules.board = parse_board(entry.board);
  rules.price_tick = entry.tick;
  rules.minimum_quantity = entry.min_qty;
  rules.quantity_step = entry.lot_step;
  rules.maximum_quantity = entry.max_qty;
  rules.sell_odd_lot_only = entry.sell_odd_lot_only;
  rules.market_orders_allowed = entry.market_orders_allowed;
  rules.limit_down = entry.limit_down;
  rules.limit_up = entry.limit_up;
  return rules;
}

void parse_fee_schedule_into(const YAML::Node& node, const std::string& path, const char* where, FeeSchedule& schedule) {
  if (!node.IsMap()) throw std::runtime_error(path + ": '" + where + "' must be a mapping");
  schedule.commission_ppm = optional<std::int64_t>(node, "commission_ppm", 0);
  schedule.commission_minimum = optional<Price>(node, "commission_minimum", 0);
  schedule.stamp_ppm = optional<std::int64_t>(node, "stamp_ppm", 0);
  schedule.transfer_ppm = optional<std::int64_t>(node, "transfer_ppm", 0);
  if (schedule.commission_ppm < 0 || schedule.commission_minimum < 0 || schedule.stamp_ppm < 0 || schedule.transfer_ppm < 0) {
    throw std::runtime_error(path + ": '" + where + "' rates and minimum must be non-negative");
  }
}

FeeTable load_fee_table(const std::string& path) {
  YAML::Node root;
  try {
    root = YAML::LoadFile(path);
  } catch (const YAML::BadFile&) {
    throw std::runtime_error("cannot open fee file: " + path);
  } catch (const YAML::Exception& e) {
    throw std::runtime_error(path + ": yaml parse error: " + e.what());
  }
  if (!root.IsMap()) throw std::runtime_error(path + ": fee file root must be a mapping");
  FeeTable table{};
  const YAML::Node defaults = root["default"];
  if (defaults.IsDefined()) parse_fee_schedule_into(defaults, path, "default", table.fallback);
  const YAML::Node instruments = root["instruments"];
  if (instruments.IsDefined()) {
    if (!instruments.IsMap()) throw std::runtime_error(path + ": fee file 'instruments' must be a mapping");
    for (const auto& entry : instruments) {
      const auto instrument_id = entry.first.as<std::string>("");
      if (instrument_id.empty()) throw std::runtime_error(path + ": fee file instrument id must be non-empty");
      FeeSchedule schedule{};
      parse_fee_schedule_into(entry.second, path, instrument_id.c_str(), schedule);
      table.overrides.insert_or_assign(instrument_id, schedule);
    }
  }
  return table;
}

AppConfig load_app_config(const std::string& path) {
  YAML::Node root;
  try {
    root = YAML::LoadFile(path);
  } catch (const YAML::BadFile&) {
    throw std::runtime_error("cannot open config file: " + path);
  } catch (const YAML::Exception& e) {
    throw std::runtime_error(path + ": yaml parse error: " + e.what());
  }
  if (!root.IsMap()) throw std::runtime_error(path + ": root must be a mapping");

  AppConfig config{};
  config.environment = required<std::string>(root, path, "environment");
  config.config_version = required<std::string>(root, path, "config_version");
  config.rules_version = required<std::string>(root, path, "rules_version");
  config.ledger_file = resolve_against(path, optional<std::string>(root, "ledger_file", ""));
  config.sqlite_file = resolve_against(path, optional<std::string>(root, "sqlite_file", ""));
  config.state_file = resolve_against(path, optional<std::string>(root, "state_file", ""));
  parse_log_section(root, path, config);
  parse_control_section(root, path, config);
  parse_gateway_section(root, path, config);
  parse_demo_section(root, path, config);
  parse_ama_section(root, path, config);
  parse_quantapi_section(root, path, config);
  parse_trading_day_section(root, path, config);
  parse_paper_sim_section(root, path, config);
  parse_postgres_section(root, path, config);
  parse_metrics_endpoint_section(root, path, config);
  parse_retention_section(root, path, config);
  parse_accounts_section(root, path, config);
  parse_risk_section(root, path, config);
  parse_strategies_section(root, path, config);
  parse_instruments_section(root, path, config);
  parse_execution_section(root, path, config);
  parse_routing_section(root, path, config);

  std::string error;
  if (!config.validate(error)) throw std::runtime_error(path + ": invalid config: " + error);
  return config;
}

}  // namespace qts
