// clang-format off
/**
  ******************************************************************************
  * @file           : control.cpp
  * @author         : vivi wu
  * @brief          : Operator command parse, format and live dispatch implementation.
  * @version        : 0.1.0
  * @date           : 03/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/ops/control.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <stdexcept>

#include "nlohmann/json.hpp"
#include "qts/persistence/ledger.hpp"

namespace qts::control {
namespace {

bool is_terminal_state(OrderStatus status) noexcept { return status == OrderStatus::filled || status == OrderStatus::cancelled || status == OrderStatus::rejected; }

// True once the order left the working set (withdrawn or already terminal).
bool withdrawn(OrderManager& oms, const ClientOrderId& order_id) {
  const ExecutionReport* found = oms.find(order_id);
  if (found == nullptr) return false;
  return found->status == OrderStatus::pending_cancel || found->status == OrderStatus::cancelled || is_terminal_state(found->status);
}

ControlResult cancel_one(ControlContext& context, const std::string& order_id) {
  if (order_id.empty()) return {false, "cancel needs an order id"};
  // Parent fan-out first: execution knows the live children. Unknown ids
  // throw per ADR-024 and fall through to the direct child path below.
  try {
    const std::vector<ClientOrderId> children = context.execution.cancel_parent(order_id);
    std::size_t done = 0;
    for (const auto& child : children) {
      context.oms.cancel(child);
      if (withdrawn(context.oms, child)) ++done;
    }
    return {true, "parent " + order_id + ": " + std::to_string(done) + "/" + std::to_string(children.size()) + " children withdrawn"};
  } catch (const std::exception&) {
  }
  context.oms.cancel(order_id);
  if (withdrawn(context.oms, order_id)) return {true, "order " + order_id + " withdrawal requested"};
  return {false, "unknown order " + order_id};
}

ControlResult replace_parent(ControlContext& context, const std::string& child_id, Quantity quantity, Price price) {
  if (child_id.empty()) return {false, "replace needs a live child id"};
  if (quantity <= 0) return {false, "replace needs a positive quantity"};
  if (price < 0) return {false, "replace price must be non-negative (0 keeps)"};
  const OrderIntent* child = context.oms.find_intent(child_id);
  if (child == nullptr) return {false, "unknown order " + child_id};
  if (child->parent_client_order_id.empty()) return {false, "order " + child_id + " is not an algorithm child"};
  OrderIntent updated = *child;
  updated.client_order_id = child->parent_client_order_id;
  updated.quantity = quantity;
  if (price > 0) updated.limit_price = price;
  std::vector<ClientOrderId> recalled;
  try {
    recalled = context.execution.replace_parent(updated);
  } catch (const std::exception& e) {
    return {false, std::string{"replace refused: "} + e.what()};
  }
  for (const auto& live : recalled) context.oms.cancel(live);
  return {true, "parent " + updated.client_order_id + " re-planned, " + std::to_string(recalled.size()) + " recalled"};
}

}  // namespace

bool parse_side(const std::string& text, Side& side) {
  if (text == "buy") {
    side = Side::buy;
    return true;
  }
  if (text == "sell") {
    side = Side::sell;
    return true;
  }
  return false;
}

bool parse_order_type(const std::string& text, OrderType& type) {
  if (text == "limit") {
    type = OrderType::limit;
    return true;
  }
  if (text == "market") {
    type = OrderType::market;
    return true;
  }
  return false;
}

bool parse_market_style(const std::string& text, MarketOrderStyle& style) {
  if (text == "best5-ioc") {
    style = MarketOrderStyle::best_five_ioc;
    return true;
  }
  if (text == "best5-limit") {
    style = MarketOrderStyle::best_five_to_limit;
    return true;
  }
  if (text == "own-best") {
    style = MarketOrderStyle::own_best;
    return true;
  }
  if (text == "cp-best") {
    style = MarketOrderStyle::counterparty_best;
    return true;
  }
  return false;
}

bool parse_ticket_exchange(const std::string& text, Exchange& exchange) {
  if (text == "sse") {
    exchange = Exchange::sse;
    return true;
  }
  if (text == "szse") {
    exchange = Exchange::szse;
    return true;
  }
  if (text == "bse") {
    exchange = Exchange::bse;
    return true;
  }
  return false;
}

// One manual ticket straight into OMS. The submit carries the same
// synchronous risk gate as strategy children (OrderManager::submit
// checks first); rejection lands in the ledger audit trail and the
// operator reads the reason via qts_admin (rejected submits stay loud
// here, never silent).
ControlResult submit_manual(ControlContext& context, const ControlCommand& command) {
  Side side = Side::buy;
  OrderType type = OrderType::limit;
  Exchange exchange = Exchange::unknown;
  MarketOrderStyle style = MarketOrderStyle::none;
  if (!parse_side(command.side, side) || !parse_order_type(command.order_type, type) || !parse_ticket_exchange(command.exchange, exchange)) {
    return {false, "order has an inconsistent ticket (reparse it)"};
  }
  OrderIntent intent{};
  intent.client_order_id = command.id;
  intent.account_id = command.account;
  intent.strategy_id = "";  // OMS normalizes empty to the passthrough basket
  intent.instrument_id = command.instrument;
  intent.side = side;
  intent.type = type;
  intent.time_in_force = TimeInForce::day;
  intent.quantity = command.quantity;
  intent.exchange = exchange;
  intent.security_account_id = command.security_account;
  intent.trace_id = "manual-" + command.id;
  if (type == OrderType::limit) {
    intent.limit_price = command.price;
  } else {
    if (!parse_market_style(command.style, style)) return {false, "order has an inconsistent ticket (reparse it)"};
    intent.market_order_style = style;
    intent.protection_price = command.price;
  }
  if (context.oms.submit(intent)) return {true, "order " + command.id + " accepted"};
  std::string why = context.oms.last_reject_reason();
  if (why.empty()) why = "see ledger risk_rejected for the reason";
  return {false, "order " + command.id + " rejected: " + why};
}

bool validate_order_ticket(const ControlCommand& command, std::string& error) {
  Side side = Side::buy;
  OrderType type = OrderType::limit;
  Exchange exchange = Exchange::unknown;
  if (command.id.empty() || command.account.empty() || command.instrument.empty() || command.security_account.empty()) {
    error = "order needs id, account, instrument and security account";
    return false;
  }
  if (!parse_side(command.side, side)) {
    error = "order side must be buy|sell";
    return false;
  }
  if (!parse_order_type(command.order_type, type)) {
    error = "order type must be limit|market";
    return false;
  }
  if (command.quantity <= 0) {
    error = "order needs a positive quantity";
    return false;
  }
  if (!parse_ticket_exchange(command.exchange, exchange)) {
    error = "order exchange must be sse|szse|bse";
    return false;
  }
  if (type == OrderType::limit) {
    if (command.price <= 0) {
      error = "limit orders need a positive price";
      return false;
    }
    if (!command.style.empty()) {
      error = "limit orders take no style";
      return false;
    }
    return true;
  }
  if (command.price < 0) {
    error = "market protection price must be non-negative";
    return false;
  }
  MarketOrderStyle style = MarketOrderStyle::none;
  if (!parse_market_style(command.style, style)) {
    error = "market orders need a style (best5-ioc|best5-limit|own-best|cp-best)";
    return false;
  }
  return true;
}

bool validate_strategy_ticket(const ControlCommand& command, std::string& error) {
  if (command.id.empty() || command.account.empty() || command.instrument.empty()) {
    error = "strategy_start needs id, account and instrument";
    return false;
  }
  if (command.strategy_type != "grid" && command.strategy_type != "rebalance") {
    error = "strategy_start type must be grid|rebalance";
    return false;
  }
  if (command.quantity <= 0) {
    error = "strategy_start needs a positive quantity";
    return false;
  }
  if (command.strategy_type == "grid") {
    if (command.lower <= 0 || command.upper <= command.lower) {
      error = "strategy_start grid needs 0 < lower < upper";
      return false;
    }
    if (command.grids <= 0) {
      error = "strategy_start grid needs positive grids";
      return false;
    }
    if (command.start < 0) {
      error = "strategy_start start must be non-negative";
      return false;
    }
  }
  return true;
}

bool parse_command(const std::string& line, ControlCommand& command, std::string& error) {
  command = ControlCommand{};
  nlohmann::json value;
  try {
    value = nlohmann::json::parse(line);
  } catch (const nlohmann::json::exception& e) {
    error = std::string{"bad control line: "} + e.what();
    return false;
  }
  if (!value.is_object()) {
    error = "bad control line: not an object";
    return false;
  }
  command.name = value.value("cmd", std::string{});
  command.id = value.value("id", std::string{});
  command.quantity = value.value("quantity", Quantity{0});
  command.price = value.value("price", Price{0});
  command.account = value.value("account", std::string{});
  command.instrument = value.value("instrument", std::string{});
  command.side = value.value("side", std::string{});
  command.order_type = value.value("type", std::string{});
  command.exchange = value.value("exchange", std::string{});
  command.security_account = value.value("security", std::string{});
  command.style = value.value("style", std::string{});
  command.strategy_type = value.value("strategy_type", std::string{});
  command.lower = value.value("lower", Price{0});
  command.upper = value.value("upper", Price{0});
  command.grids = value.value("grids", 0);
  command.start = value.value("start", Price{0});
  if (command.name != "cancel" && command.name != "cancel_all" && command.name != "block_strategy" && command.name != "unblock_strategy" && command.name != "block_instrument" &&
      command.name != "unblock_instrument" && command.name != "block_account" && command.name != "unblock_account" && command.name != "halt" && command.name != "resume" && command.name != "replace" &&
      command.name != "order" && command.name != "strategy_start" && command.name != "strategy_stop" && command.name != "strategy_list") {
    error = "unknown control cmd: " + command.name;
    return false;
  }
  if ((command.name == "cancel" || command.name == "block_strategy" || command.name == "unblock_strategy" || command.name == "block_instrument" || command.name == "unblock_instrument" ||
       command.name == "block_account" || command.name == "unblock_account") &&
      command.id.empty()) {
    error = command.name + " needs an id";
    return false;
  }
  if (command.name == "replace" && (command.id.empty() || command.quantity <= 0)) {
    error = "replace needs a live child id and a positive quantity";
    return false;
  }
  if (command.name == "order") return validate_order_ticket(command, error);
  if (command.name == "strategy_stop" && command.id.empty()) {
    error = "strategy_stop needs a strategy id";
    return false;
  }
  if (command.name == "strategy_start") return validate_strategy_ticket(command, error);
  return true;
}

std::string format_command(const ControlCommand& command) {
  nlohmann::json value;
  value["cmd"] = command.name;
  if (!command.id.empty()) value["id"] = command.id;
  if (command.quantity != 0) value["quantity"] = command.quantity;
  if (command.price != 0) value["price"] = command.price;
  if (!command.account.empty()) value["account"] = command.account;
  if (!command.instrument.empty()) value["instrument"] = command.instrument;
  if (!command.side.empty()) value["side"] = command.side;
  if (!command.order_type.empty()) value["type"] = command.order_type;
  if (!command.exchange.empty()) value["exchange"] = command.exchange;
  if (!command.security_account.empty()) value["security"] = command.security_account;
  if (!command.style.empty()) value["style"] = command.style;
  if (!command.strategy_type.empty()) value["strategy_type"] = command.strategy_type;
  if (command.lower != 0) value["lower"] = command.lower;
  if (command.upper != 0) value["upper"] = command.upper;
  if (command.grids != 0) value["grids"] = command.grids;
  if (command.start != 0) value["start"] = command.start;
  return value.dump();
}

ControlResult apply_registry_command(const ControlCommand& command, ControlContext& context) {
  if (context.strategies == nullptr) return {false, command.name + " unavailable: no live registry"};
  if (command.name == "strategy_start") {
    StrategyConfig configured{};
    configured.id = command.id;
    configured.enabled = true;
    configured.type = command.strategy_type;
    configured.account = command.account;
    configured.instrument = command.instrument;
    configured.lower = command.lower;
    configured.upper = command.upper;
    configured.grids = command.grids;
    configured.quantity = command.quantity;
    configured.start = command.start;
    return context.strategies->start(configured);
  }
  if (command.name == "strategy_stop") return context.strategies->stop(command.id, context.oms, context.execution, context.risk);
  return {true, context.strategies->list()};
}

// Switch-command journal (M15): successful applications append a
// control_applied event carrying the verb, target and resulting state.
// Ledger-less contexts (tests, offline tools) stay silent.
void record_control(ControlContext& context, const std::string& name, const std::string& target, bool blocked_after) {
  if (context.ledger == nullptr) return;
  LedgerEvent event{};
  event.type = LedgerEventType::control_applied;
  event.control.command = name;
  event.control.target = target;
  event.control.blocked_after = blocked_after;
  event.control.source = "inbox";
  event.envelope.trace_id = "control-" + name;
  context.ledger->append(std::move(event));
}

ControlResult apply_command(const ControlCommand& command, ControlContext& context) {
  if (command.name == "cancel") return cancel_one(context, command.id);
  if (command.name == "cancel_all") {
    const std::size_t done = cancel_all_open(context.oms, context.execution);
    return {true, "cancel all: " + std::to_string(done) + " orders withdrawn"};
  }
  if (command.name == "block_strategy") {
    context.risk.set_strategy_blocked(command.id, true);
    record_control(context, "block_strategy", command.id, true);
    return {true, "strategy blocked: " + command.id};
  }
  if (command.name == "unblock_strategy") {
    context.risk.set_strategy_blocked(command.id, false);
    record_control(context, "unblock_strategy", command.id, false);
    return {true, "strategy unblocked: " + command.id};
  }
  if (command.name == "block_instrument") {
    context.risk.set_instrument_blocked(command.id, true);
    record_control(context, "block_instrument", command.id, true);
    return {true, "instrument blocked: " + command.id};
  }
  if (command.name == "unblock_instrument") {
    context.risk.set_instrument_blocked(command.id, false);
    record_control(context, "unblock_instrument", command.id, false);
    return {true, "instrument unblocked: " + command.id};
  }
  if (command.name == "block_account") {
    context.risk.set_account_blocked(command.id, true);
    record_control(context, "block_account", command.id, true);
    return {true, "account blocked: " + command.id};
  }
  if (command.name == "unblock_account") {
    context.risk.set_account_blocked(command.id, false);
    record_control(context, "unblock_account", command.id, false);
    return {true, "account unblocked: " + command.id};
  }
  if (command.name == "halt") {
    context.risk.set_trading_enabled(false);
    record_control(context, "halt", "", true);
    return {true, "global trading halted"};
  }
  if (command.name == "resume") {
    context.risk.set_trading_enabled(true);
    record_control(context, "resume", "", false);
    return {true, "global trading resumed"};
  }
  if (command.name == "replace") return replace_parent(context, command.id, command.quantity, command.price);
  if (command.name == "order") return submit_manual(context, command);
  if (command.name == "strategy_start" || command.name == "strategy_stop" || command.name == "strategy_list") return apply_registry_command(command, context);
  return {false, "unknown control cmd: " + command.name};
}

std::size_t cancel_all_open(OrderManager& oms, ExecutionAlgorithm& execution) {
  std::size_t done = 0;
  // Deterministic sweep order (M2 replay parity): the working set iterates
  // in hash order, so two identical runs would otherwise cancel in a
  // different sequence and diverge in the ledger tail.
  auto parents = oms.open_parent_ids();
  std::sort(parents.begin(), parents.end());
  for (const auto& parent : parents) {
    std::vector<ClientOrderId> children;
    try {
      children = execution.cancel_parent(parent);
    } catch (const std::exception&) {
      continue;  // unknown to the algorithm: the sweep below still gets it
    }
    for (const auto& child : children) {
      oms.cancel(child);
      if (withdrawn(oms, child)) ++done;
    }
  }
  auto opens = oms.open_orders();
  std::sort(opens.begin(), opens.end(), [](const ExecutionReport& lhs, const ExecutionReport& rhs) { return lhs.client_order_id < rhs.client_order_id; });
  for (const auto& order : opens) {
    oms.cancel(order.client_order_id);
    if (withdrawn(oms, order.client_order_id)) ++done;
  }
  return done;
}

std::string apply_auto_actions(ControlContext& context) {
  std::vector<AutoAction> actions = context.risk.drain_auto_actions();
  if (actions.empty()) return {};
  std::string summary;
  for (const auto& action : actions) {
    if (!summary.empty()) summary += "; ";
    if (action.kind == AutoActionKind::halt_strategy) {
      ControlCommand command{};
      command.name = "strategy_stop";
      command.id = action.strategy_id;
      const ControlResult result = apply_command(command, context);
      summary += action.reason + " -> " + result.message;
    } else {
      const std::size_t done = cancel_all_open(context.oms, context.execution);
      summary += action.reason + " -> withdrew " + std::to_string(done) + " orders";
    }
  }
  return summary;
}

StrategyRegistry::StrategyRegistry(std::vector<std::unique_ptr<Strategy>>& live, Factory factory, std::string prefix_seed)
    : live_(live),
      factory_(std::move(factory)),
      prefix_seed_(std::move(prefix_seed)) {}

void StrategyRegistry::adopt(const StrategyConfig& configured, Strategy* instance) {
  generations_[configured.id] = 0;
  entries_.push_back(Entry{configured, instance});
}

ControlResult StrategyRegistry::start(const StrategyConfig& configured) {
  for (const auto& entry : entries_) {
    if (entry.config.id == configured.id) return {false, "strategy_start refused: duplicate id " + configured.id};
  }
  if (configured.type != "grid" && configured.type != "rebalance") return {false, "strategy_start refused: unknown type " + configured.type};
  const std::size_t generation = ++generations_[configured.id];
  const std::string prefix = configured.id + "-r" + std::to_string(generation) + "-" + prefix_seed_ + "-";
  std::unique_ptr<Strategy> instance;
  try {
    instance = factory_(configured, prefix);
  } catch (const std::exception& e) {
    generations_[configured.id] = generation - 1;
    return {false, std::string{"strategy_start refused: "} + e.what()};
  }
  if (instance == nullptr) return {false, "strategy_start refused: factory returned null"};
  Strategy* raw = instance.get();
  live_.push_back(std::move(instance));
  entries_.push_back(Entry{configured, raw});
  return {true, "strategy started: " + configured.id + " (" + configured.type + ")"};
}

ControlResult StrategyRegistry::stop(const StrategyId& strategy_id, OrderManager& oms, ExecutionAlgorithm& execution, RiskEngine& risk) {
  std::size_t found = entries_.size();
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].config.id == strategy_id) {
      found = i;
      break;
    }
  }
  if (found == entries_.size()) {
    return {false, "strategy_stop refused: unknown id " + strategy_id};
  }
  risk.set_strategy_blocked(strategy_id, true);
  // Withdraw this strategy's open parents only: parent ids carry the
  // "<strategy>-" prefix (yaml runs and runtime generations alike), so the
  // sweep stays scoped while manual passthrough orders keep working.
  const std::string prefix = strategy_id + "-";
  auto parents = oms.open_parent_ids();
  std::sort(parents.begin(), parents.end());
  std::size_t withdrawn_count = 0;
  for (const auto& parent : parents) {
    if (parent.compare(0, prefix.size(), prefix) != 0) {
      continue;
    }
    std::vector<ClientOrderId> children;
    try {
      children = execution.cancel_parent(parent);
    } catch (const std::exception&) {
      continue;  // unknown to the algorithm: the sweep below still gets it
    }
    for (const auto& child : children) {
      oms.cancel(child);
      if (withdrawn(oms, child)) {
        ++withdrawn_count;
      }
    }
  }
  Strategy* raw = entries_[found].instance;
  live_.erase(std::remove_if(live_.begin(), live_.end(), [raw](const std::unique_ptr<Strategy>& held) { return held.get() == raw; }), live_.end());
  entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(found));
  // Stragglers the algorithm never owned (parentless direct/manual orders
  // under the same prefix): direct OMS withdrawal, like cancel_all_open.
  for (const auto& open : oms.open_orders()) {
    if (open.client_order_id.compare(0, prefix.size(), prefix) != 0) {
      continue;
    }
    oms.cancel(open.client_order_id);
    if (withdrawn(oms, open.client_order_id)) {
      ++withdrawn_count;
    }
  }
  return {true, "strategy stopped: " + strategy_id + " (" + std::to_string(withdrawn_count) + " orders withdrawn)"};
}

std::string StrategyRegistry::list() const {
  if (entries_.empty()) return "no live strategies";
  std::string out;
  for (const auto& entry : entries_) {
    if (!out.empty()) {
      out += '\n';
    }
    out += entry.config.id + " " + entry.config.type + " " + entry.config.account + " " + entry.config.instrument;
  }
  return out;
}

std::vector<StrategyId> StrategyRegistry::active_ids() const {
  std::vector<StrategyId> ids;
  ids.reserve(entries_.size());
  for (const auto& entry : entries_) {
    ids.push_back(entry.config.id);
  }
  return ids;
}

bool StrategyRegistry::account_of(const StrategyId& strategy_id, AccountId& account) const {
  for (const auto& entry : entries_) {
    if (entry.config.id == strategy_id) {
      account = entry.config.account;
      return true;
    }
  }
  return false;
}

}  // namespace qts::control
