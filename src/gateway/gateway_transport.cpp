// clang-format off
/**
  ******************************************************************************
  * @file           : gateway_transport.cpp
  * @author         : vivi wu
  * @brief          : Async socket shell implementation over GatewayService.
  * @version        : 0.1.0
  * @date           : 10/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/gateway/gateway_transport.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <utility>

#include "nlohmann/json.hpp"

namespace sts::gateway {
namespace {

constexpr std::size_t kMaxPeers = 8;
constexpr std::size_t kMaxLineBytes = 65536;
constexpr std::size_t kMaxOutboxBytes = 1048576;

void set_nonblocking(int sock) {
  const int flags = ::fcntl(sock, F_GETFL, 0);
  if (flags >= 0) ::fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

const char* report_status_name(OrderStatus status) {
  switch (status) {
    case OrderStatus::pending_new:
      return "pending_new";
    case OrderStatus::live:
      return "live";
    case OrderStatus::partially_filled:
      return "partially_filled";
    case OrderStatus::filled:
      return "filled";
    case OrderStatus::pending_cancel:
      return "pending_cancel";
    case OrderStatus::cancelled:
      return "cancelled";
    case OrderStatus::rejected:
      return "rejected";
  }
  return "unknown";
}

nlohmann::json quote_to_json(const QuoteEvent& quote) {
  nlohmann::json node = nlohmann::json::object();
  node["instrument"] = quote.instrument_id;
  node["last"] = quote.last_price;
  node["bid"] = quote.bid_price;
  node["bid_qty"] = quote.bid_quantity;
  node["ask"] = quote.ask_price;
  node["ask_qty"] = quote.ask_quantity;
  node["sequence"] = quote.sequence;
  return node;
}

nlohmann::json report_to_json(const ExecutionReport& report) {
  nlohmann::json node = nlohmann::json::object();
  node["client_order_id"] = report.client_order_id;
  node["status"] = report_status_name(report.status);
  node["last_quantity"] = report.last_quantity;
  node["last_price"] = report.last_price;
  node["cumulative_quantity"] = report.cumulative_quantity;
  node["version"] = report.version;
  return node;
}

nlohmann::json settle_account_to_json(const AccountSettleReport& row) {
  nlohmann::json node = nlohmann::json::object();
  node["account"] = row.account;
  node["clean"] = row.diff.clean();
  node["cash_dirty"] = row.diff.cash_dirty();
  node["position_dirty"] = row.diff.position_dirty();
  nlohmann::json cash = nlohmann::json::array();
  for (const auto& entry : row.diff.cash) {
    nlohmann::json item = nlohmann::json::object();
    item["field"] = entry.field;
    item["sts"] = entry.sts_value;
    item["broker"] = entry.broker_value;
    item["comparable"] = entry.comparable;
    cash.push_back(std::move(item));
  }
  node["cash"] = std::move(cash);
  nlohmann::json positions = nlohmann::json::array();
  for (const auto& entry : row.diff.positions) {
    nlohmann::json item = nlohmann::json::object();
    item["instrument"] = entry.instrument;
    item["sts_total"] = entry.sts_total;
    item["broker_total"] = entry.broker_total;
    item["sts_sellable"] = entry.sts_sellable;
    item["broker_sellable"] = entry.broker_sellable;
    item["sts_avg"] = entry.sts_avg;
    item["broker_avg"] = entry.broker_avg;
    item["broker_frozen"] = entry.broker_frozen;
    positions.push_back(std::move(item));
  }
  node["positions"] = std::move(positions);
  node["fees"] = row.diff.sts_fees_disclosed;
  return node;
}

nlohmann::json strategy_to_json(const StrategyEvent& event) {
  nlohmann::json node = nlohmann::json::object();
  node["strategy"] = event.strategy_id;
  node["event"] = event.kind == StrategyEventKind::started ? "started" : "stopped";
  node["detail"] = event.detail;
  return node;
}

struct OpOutcome {
  bool ok{false};
  std::string message;
  nlohmann::json data = nlohmann::json::object();
};

OpOutcome refused_outcome(const std::string& reason) { return {false, reason, nlohmann::json::object()}; }

// Dispatch plumbing without touching the transport header: the member
// builds this context over one peer, file-local handlers do the JSON work.
struct DispatchContext {
  // Non-owning views into the live transport + peer (same-thread only):
  // the refs below never outlive one poll_once call.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  GatewayService& service;
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  GatewayTransport& transport;
  int peer_fd{0};
  std::function<void(const std::string& frame)> push;
  std::function<void(SubscriptionToken sub)> track_sub;
};

// Forward: subscription wiring is defined after handle_query_op (definition
// order follows the op groups); declared here so queries can route to it.
bool handle_subscribe_op(const std::string& operation, DispatchContext& context, OpOutcome& outcome);
bool handle_settle_report_op(DispatchContext& context, OpOutcome& outcome);

bool build_intent(const nlohmann::json& body, OrderIntent& intent, std::string& error) {
  intent.client_order_id = body.value("id", std::string{});
  intent.account_id = body.value("account", std::string{});
  intent.strategy_id = body.value("strategy", std::string{});
  intent.instrument_id = body.value("instrument", std::string{});
  const std::string side = body.value("side", std::string{});
  const std::string kind = body.value("type", std::string{});
  if (side == "buy") {
    intent.side = Side::buy;
  } else if (side == "sell") {
    intent.side = Side::sell;
  } else {
    error = "submit side must be buy|sell";
    return false;
  }
  if (kind == "limit") {
    intent.type = OrderType::limit;
  } else if (kind == "market") {
    intent.type = OrderType::market;
  } else {
    error = "submit type must be limit|market";
    return false;
  }
  const std::string venue = body.value("exchange", std::string{});
  if (venue.empty() || venue == "unknown") {
    intent.exchange = Exchange::unknown;
  } else if (venue == "sse") {
    intent.exchange = Exchange::sse;
  } else if (venue == "szse") {
    intent.exchange = Exchange::szse;
  } else if (venue == "bse") {
    intent.exchange = Exchange::bse;
  } else {
    error = "submit exchange must be sse|szse|bse";
    return false;
  }
  const std::string style = body.value("style", std::string{});
  if (intent.type == OrderType::limit) {
    if (!style.empty()) {
      error = "limit orders take no style";
      return false;
    }
  } else if (style == "best5-ioc") {
    intent.market_order_style = MarketOrderStyle::best_five_ioc;
  } else if (style == "best5-limit") {
    intent.market_order_style = MarketOrderStyle::best_five_to_limit;
  } else if (style == "own-best") {
    intent.market_order_style = MarketOrderStyle::own_best;
  } else if (style == "cp-best") {
    intent.market_order_style = MarketOrderStyle::counterparty_best;
  } else {
    error = "market orders need a style (best5-ioc|best5-limit|own-best|cp-best)";
    return false;
  }
  intent.quantity = static_cast<Quantity>(body.value("quantity", 0LL));
  intent.limit_price = static_cast<Price>(body.value("price", 0LL));
  intent.security_account_id = body.value("security", std::string{});
  intent.trace_id = "gateway-" + intent.client_order_id;
  return true;
}

bool build_strategy_config(const nlohmann::json& body, StrategyConfig& configured, std::string& error) {
  configured.id = body.value("id", std::string{});
  configured.enabled = true;
  configured.type = body.value("strategy_type", std::string{});
  configured.account = body.value("account", std::string{});
  configured.instrument = body.value("instrument", std::string{});
  configured.quantity = static_cast<Quantity>(body.value("quantity", 0LL));
  configured.lower = static_cast<Price>(body.value("lower", 0LL));
  configured.upper = static_cast<Price>(body.value("upper", 0LL));
  configured.grids = static_cast<int>(body.value("grids", 0LL));
  configured.start = static_cast<Price>(body.value("start", 0LL));
  configured.stock = static_cast<Quantity>(body.value("stock", 0LL));
  configured.stock_avg = static_cast<Price>(body.value("stock_avg", 0LL));
  if (configured.id.empty() || configured.account.empty() || configured.instrument.empty()) {
    error = "strategy_start needs id, account and instrument";
    return false;
  }
  if (configured.type.empty()) {
    error = "strategy_start needs a strategy_type";
    return false;
  }
  return true;
}

// ADR-050 account gate for bound sessions: fills the default account when
// the request omits it, refuses explicit strangers. Open traffic passes
// through untouched.
bool gate_session_account(DispatchContext& context, nlohmann::json& body, std::string& error) {
  std::string bound;
  if (!context.transport.bound_session_account(context.peer_fd, bound)) return true;
  std::string named = body.value("account", std::string{});
  if (named.empty()) {
    body["account"] = bound;
    return true;
  }
  if (named != bound) {
    error = "forbidden (account mismatch)";
    return false;
  }
  return true;
}

// Id-attributed ops (no account field): strangers refuse, unknowns fall
// through to the service's own "unknown" refusal. Empty owners (legacy
// tickets) match any session.
bool gate_order_owner(DispatchContext& context, const ClientOrderId& id, std::string& error) {
  std::string bound;
  if (!context.transport.bound_session_account(context.peer_fd, bound)) return true;
  AccountId owner;
  if (!context.service.order_account(id, owner)) return true;
  if (!owner.empty() && owner != bound) {
    error = "forbidden (account mismatch)";
    return false;
  }
  return true;
}

bool gate_strategy_owner(DispatchContext& context, const StrategyId& id, std::string& error) {
  std::string bound;
  if (!context.transport.bound_session_account(context.peer_fd, bound)) return true;
  AccountId owner;
  if (!context.service.strategy_account(id, owner)) return true;
  if (!owner.empty() && owner != bound) {
    error = "forbidden (account mismatch)";
    return false;
  }
  return true;
}

// Ping only (plus the P0 auth verbs below). Returns true when matched.
bool handle_ping_op(const std::string& operation, const nlohmann::json&, DispatchContext&, OpOutcome& outcome) {
  if (operation == "ping") {
    outcome = {true, "pong", nlohmann::json::object()};
    return true;
  }
  return false;
}

// Account auth verbs (ADR-050): login/logout/whoami. Passwords never
// touch the service or the logs; the transport owns sessions per peer.
bool handle_auth_op(const std::string& operation, const nlohmann::json& body, DispatchContext& context, OpOutcome& outcome) {
  if (operation == "login") {
    // Canonical key is account (operator stays an alias); empty means
    // "the session default", resolved by the caller presets.
    std::string name = body.value("account", std::string{});
    if (name.empty()) name = body.value("operator", std::string{});
    std::string bound;
    std::string error;
    if (!context.transport.login_peer(context.peer_fd, name, body.value("password", std::string{}), bound, error)) {
      outcome = refused_outcome(error);
      return true;
    }
    outcome.ok = true;
    outcome.message = "logged in";
    outcome.data["account"] = bound;
    outcome.data["operator"] = bound;
    return true;
  }
  if (operation == "logout") {
    context.transport.logout_peer(context.peer_fd);
    outcome = {true, "logged out", nlohmann::json::object()};
    return true;
  }
  if (operation == "whoami") {
    std::string name;
    std::string scope;
    std::string account;
    bool authed = false;
    context.transport.identity(context.peer_fd, name, scope, account, authed);
    outcome.ok = true;
    outcome.message = authed ? "ok" : "not logged in";
    outcome.data["operator"] = name;
    outcome.data["account"] = account;
    outcome.data["scope"] = scope;
    outcome.data["authed"] = authed;
    return true;
  }
  return false;
}

// Order/strategy ops: every one needs a session; the service re-checks
// scopes and tickets (defense in depth, same refusal shapes as in-proc).
bool handle_order_op(const std::string& operation, const nlohmann::json& body, DispatchContext& context, OpOutcome& outcome) {
  if (operation == "submit") {
    OrderIntent intent{};
    std::string error;
    if (!build_intent(body, intent, error)) {
      outcome = refused_outcome(error);
      return true;
    }
    const FrontResult result = context.service.submit_order(intent);
    outcome = {result.ok, result.message, nlohmann::json::object()};
    return true;
  }
  if (operation == "cancel") {
    std::string gate_error;
    if (!gate_order_owner(context, body.value("id", std::string{}), gate_error)) {
      outcome = refused_outcome(gate_error);
      return true;
    }
    const FrontResult result = context.service.cancel_order(body.value("id", std::string{}));
    outcome = {result.ok, result.message, nlohmann::json::object()};
    return true;
  }
  if (operation == "strategy_start") {
    StrategyConfig configured{};
    std::string error;
    if (!build_strategy_config(body, configured, error)) {
      outcome = refused_outcome(error);
      return true;
    }
    const FrontResult result = context.service.start_strategy(configured);
    outcome = {result.ok, result.message, nlohmann::json::object()};
    return true;
  }
  if (operation == "settle_confirm") {
    ReconcileAttestation claimed{};
    claimed.broker_snapshot_time = body.value("snapshot_time", std::string{});
    claimed.ledger_watermark = body.value("watermark", std::uint64_t{0});
    claimed.cash_dirty = body.value("cash_dirty", 0);
    claimed.position_dirty = body.value("position_dirty", 0);
    claimed.note = body.value("note", std::string{});
    const FrontResult result = context.service.settle_confirm(claimed);
    outcome = {result.ok, result.message, nlohmann::json::object()};
    return true;
  }
  if (operation == "strategy_stop") {
    std::string gate_error;
    if (!gate_strategy_owner(context, body.value("id", std::string{}), gate_error)) {
      outcome = refused_outcome(gate_error);
      return true;
    }
    const FrontResult result = context.service.stop_strategy(body.value("id", std::string{}));
    outcome = {result.ok, result.message, nlohmann::json::object()};
    return true;
  }
  if (operation == "strategy_list") {
    const FrontResult result = context.service.list_strategies();
    outcome.ok = result.ok;
    outcome.message = result.message;
    outcome.data["strategies"] = result.message;
    return true;
  }
  return false;
}

// Query/subscribe ops: reads need a valid session only; subscriptions bind
// the peer outbox as the push sink and are tracked for disconnect cleanup.
bool handle_query_op(const std::string& operation, const nlohmann::json& body, DispatchContext& context, OpOutcome& outcome) {
  if (operation == "cash") {
    Price cash = 0;
    const AccountId account = body.value("account", std::string{});
    const FrontResult result = context.service.account_cash(cash, account);
    if (!result.ok) {
      outcome = refused_outcome(result.message);
      return true;
    }
    outcome.ok = true;
    outcome.message = "ok";
    outcome.data["cash"] = cash;
    // Per-account channel books report shared=false; the legacy single
    // book (or multi-account views) stay shared=true (ADR-049). Bookless
    // live accounts serve the latest counter snapshot (ADR-054) instead.
    outcome.data["shared"] = context.service.book_shared(account);
    const std::string snapshot_time = context.service.counter_snapshot_time(account);
    if (!snapshot_time.empty()) outcome.data["snapshot_time"] = snapshot_time;
    return true;
  }
  if (operation == "open_orders") {
    std::vector<ExecutionReport> orders;
    const FrontResult result = context.service.open_orders(orders, body.value("account", std::string{}));
    if (!result.ok) {
      outcome = refused_outcome(result.message);
      return true;
    }
    nlohmann::json rows = nlohmann::json::array();
    for (const auto& order : orders) rows.push_back(report_to_json(order));
    outcome.ok = true;
    outcome.message = "ok";
    outcome.data["orders"] = std::move(rows);
    return true;
  }
  if (operation == "find_order") {
    std::string gate_error;
    if (!gate_order_owner(context, body.value("id", std::string{}), gate_error)) {
      outcome = refused_outcome(gate_error);
      return true;
    }
    ExecutionReport report{};
    const FrontResult result = context.service.find_order(body.value("id", std::string{}), report);
    if (!result.ok) {
      outcome = refused_outcome(result.message);
      return true;
    }
    outcome.ok = true;
    outcome.message = "ok";
    outcome.data["order"] = report_to_json(report);
    return true;
  }
  if (operation == "positions_for") {
    std::unordered_map<InstrumentId, PositionView> positions;
    const AccountId account = body.value("account", std::string{});
    const FrontResult result = context.service.positions_for(body.value("strategy", std::string{}), positions, account);
    if (!result.ok) {
      outcome = refused_outcome(result.message);
      return true;
    }
    nlohmann::json rows = nlohmann::json::array();
    for (const auto& entry : positions) {
      nlohmann::json row = nlohmann::json::object();
      row["instrument"] = entry.first;
      row["qty"] = entry.second.total;
      row["sellable"] = entry.second.sellable;
      row["avg"] = entry.second.average_cost;
      rows.push_back(std::move(row));
    }
    outcome.ok = true;
    outcome.message = "ok";
    outcome.data["positions"] = std::move(rows);
    outcome.data["shared"] = context.service.book_shared(account);
    return true;
  }
  if (operation == "fills") {
    std::vector<FillView> rows_in;
    bool truncated = false;
    const std::size_t limit = static_cast<std::size_t>(body.value("limit", 0LL));
    const FrontResult result = context.service.fills(body.value("account", std::string{}), body.value("strategy", std::string{}), limit, rows_in, truncated);
    if (!result.ok) {
      outcome = refused_outcome(result.message);
      return true;
    }
    nlohmann::json rows = nlohmann::json::array();
    for (const auto& view : rows_in) {
      nlohmann::json row = report_to_json(view.report);
      row["account"] = view.intent.account_id;
      row["strategy"] = view.intent.strategy_id;
      row["instrument"] = view.intent.instrument_id;
      row["seq"] = view.seq;
      rows.push_back(std::move(row));
    }
    outcome.ok = true;
    outcome.message = "ok";
    outcome.data["fills"] = std::move(rows);
    outcome.data["truncated"] = truncated;
    return true;
  }
  if (operation == "strategy_progress") {
    StrategyProgress progress{};
    const FrontResult result = context.service.strategy_progress(body.value("id", std::string{}), progress, body.value("account", std::string{}));
    if (!result.ok) {
      outcome = refused_outcome(result.message);
      return true;
    }
    outcome.ok = true;
    outcome.message = "ok";
    outcome.data["strategy"] = progress.strategy_id;
    outcome.data["running"] = progress.running;
    outcome.data["submitted"] = progress.submitted;
    outcome.data["open"] = progress.open;
    outcome.data["filled"] = progress.filled;
    outcome.data["cancelled"] = progress.cancelled;
    outcome.data["rejected"] = progress.rejected;
    outcome.data["position_qty"] = progress.position_quantity;
    outcome.data["realized"] = progress.realized;
    return true;
  }
  if (operation == "settle_report") return handle_settle_report_op(context, outcome);
  if (operation == "subscribe_quotes" || operation == "subscribe_reports" || operation == "subscribe_strategies") {
    return handle_subscribe_op(operation, context, outcome);
  }
  if (operation == "unsubscribe") {
    const auto sub = static_cast<SubscriptionToken>(body.value("sub", 0LL));
    if (sub == 0 || !context.service.unsubscribe(sub)) {
      outcome = refused_outcome("unknown subscription");
      return true;
    }
    outcome = {true, "unsubscribed", nlohmann::json::object()};
    return true;
  }
  return false;
}

// Subscription wiring, split out of handle_query_op for the cognitive
// complexity gate: binds the peer outbox as the push sink and tracks the
// subscription for disconnect cleanup.
bool handle_settle_report_op(DispatchContext& context, OpOutcome& outcome) {
  SettleReport settled{};
  const FrontResult result = context.service.settle_report(settled);
  if (!result.ok) {
    outcome = refused_outcome(result.message);
    return true;
  }
  nlohmann::json rows = nlohmann::json::array();
  for (const auto& row : settled.accounts) rows.push_back(settle_account_to_json(row));
  outcome.ok = true;
  outcome.message = settled.accounts.empty() ? "no snapshots" : "ok";
  outcome.data["snapshot_time"] = settled.captured_at;
  outcome.data["ledger_watermark"] = settled.ledger_watermark;
  outcome.data["accounts"] = std::move(rows);
  return true;
}
bool handle_subscribe_op(const std::string& operation, DispatchContext& context, OpOutcome& outcome) {
  std::string error;
  SubscriptionToken sub = 0;
  if (operation == "subscribe_quotes") {
    GatewayService::QuoteSubscription handler = [sink = context.push](const QuoteEvent& quote) {
      nlohmann::json frame = nlohmann::json::object();
      frame["push"] = "quote";
      frame["quote"] = quote_to_json(quote);
      sink(frame.dump() + "\n");
    };
    sub = context.service.subscribe_quotes(std::move(handler), error);
  } else if (operation == "subscribe_reports") {
    GatewayService::ReportSubscription handler = [sink = context.push](const ExecutionReport& report) {
      nlohmann::json frame = nlohmann::json::object();
      frame["push"] = "report";
      frame["report"] = report_to_json(report);
      sink(frame.dump() + "\n");
    };
    sub = context.service.subscribe_reports(std::move(handler), error);
  } else {
    GatewayService::StrategySubscription handler = [sink = context.push](const StrategyEvent& event) {
      nlohmann::json frame = nlohmann::json::object();
      frame["push"] = "strategy";
      frame["strategy"] = strategy_to_json(event);
      sink(frame.dump() + "\n");
    };
    sub = context.service.subscribe_strategies(std::move(handler), error);
  }
  if (sub == 0) {
    outcome = refused_outcome(error);
    return true;
  }
  context.track_sub(sub);
  outcome.ok = true;
  outcome.message = "subscribed";
  outcome.data["sub"] = sub;
  return true;
}

std::string dispatch_request(const std::string& line, DispatchContext& context) {
  nlohmann::json body;
  try {
    body = nlohmann::json::parse(line);
  } catch (const nlohmann::json::exception& parse_failure) {
    body = nlohmann::json::object();
    nlohmann::json reply = nlohmann::json::object();
    reply["req"] = 0;
    reply["ok"] = false;
    reply["message"] = std::string{"bad request line: "} + parse_failure.what();
    reply["data"] = nlohmann::json::object();
    return reply.dump() + "\n";
  }
  std::int64_t req = 0;
  if (body.is_object()) req = body.value("req", 0LL);
  OpOutcome outcome = refused_outcome("bad request: not an object");
  if (body.is_object()) {
    const std::string operation = body.value("op", std::string{});
    try {
      if (handle_ping_op(operation, body, context, outcome)) {
        // Matched above; outcome already filled.
      } else if (handle_auth_op(operation, body, context, outcome)) {
        // Matched above.
      } else {
        std::string gate_error;
        if (!context.transport.authorize(context.peer_fd, operation, body.value("account", std::string{}), gate_error)) {
          outcome = refused_outcome(gate_error);
        } else if (!gate_session_account(context, body, gate_error)) {
          outcome = refused_outcome(gate_error);
        } else if (handle_order_op(operation, body, context, outcome)) {
          // Matched above.
        } else if (handle_query_op(operation, body, context, outcome)) {
          // Matched above.
        } else if (operation.empty()) {
          outcome = refused_outcome("request needs an op");
        } else {
          outcome = refused_outcome("unknown op: " + operation);
        }
      }
    } catch (const std::exception& dispatch_failure) {
      outcome = refused_outcome(std::string{"request failed: "} + dispatch_failure.what());
    }
  }
  nlohmann::json reply = nlohmann::json::object();
  reply["req"] = req;
  reply["ok"] = outcome.ok;
  reply["message"] = outcome.message;
  reply["data"] = std::move(outcome.data);
  return reply.dump() + "\n";
}

}  // namespace

GatewayTransport::GatewayTransport(GatewayService& service, std::string socket_path, std::string bind, GatewayAuth auth)
    : service_(service),
      path_(std::move(socket_path)),
      auth_(std::move(auth)) {
  if (!path_.empty()) {
    if (path_.size() >= sizeof(sockaddr_un::sun_path)) throw std::runtime_error("gateway socket path too long: " + path_);
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);  // stale file from an unclean stop
    const int sock = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) throw std::runtime_error("gateway socket: socket() failed");
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, path_.c_str(), sizeof(address.sun_path) - 1);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): sockaddr_un downcast is the POSIX API shape.
    if (::bind(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || ::listen(sock, 4) != 0) {
      ::close(sock);
      throw std::runtime_error("gateway socket: bind/listen failed: " + path_);
    }
    set_nonblocking(sock);
    listen_fd_ = sock;
  }
  if (!bind.empty()) {
    // P1 TCP: strict host:port (config already validated the shape; direct
    // construction fails closed the same way instead of binding surprises).
    const std::size_t mark = bind.rfind(':');
    if (mark == std::string::npos || mark == 0 || mark + 1 >= bind.size()) throw std::runtime_error("gateway tcp: bind must be host:port");
    const std::string host = bind.substr(0, mark);
    unsigned long raw_port = 0;
    try {
      raw_port = std::stoul(bind.substr(mark + 1));
    } catch (const std::exception&) {
      throw std::runtime_error("gateway tcp: port must be numeric");
    }
    if (raw_port > 65535) throw std::runtime_error("gateway tcp: port out of range");
    const std::uint16_t port = static_cast<std::uint16_t>(raw_port);
    addrinfo wants{};
    wants.ai_family = AF_INET;
    wants.ai_socktype = SOCK_STREAM;
    addrinfo* resolved = nullptr;
    if (::getaddrinfo(host.c_str(), nullptr, &wants, &resolved) != 0 || resolved == nullptr) {
      throw std::runtime_error("gateway tcp: cannot resolve host: " + host);
    }
    int sock = -1;
    for (addrinfo* item = resolved; item != nullptr; item = item->ai_next) {
      sock = ::socket(item->ai_family, item->ai_socktype, item->ai_protocol);
      if (sock < 0) continue;
      int reuse = 1;
      ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
      auto* address = reinterpret_cast<sockaddr_in*>(item->ai_addr);
      address->sin_port = htons(port);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): sockaddr_in downcast is the POSIX API shape.
      if (::bind(sock, reinterpret_cast<sockaddr*>(address), sizeof(sockaddr_in)) == 0 && ::listen(sock, 4) == 0) break;
      ::close(sock);
      sock = -1;
    }
    ::freeaddrinfo(resolved);
    if (sock < 0) throw std::runtime_error("gateway tcp: bind/listen failed: " + bind);
    set_nonblocking(sock);
    tcp_fd_ = sock;
    // Actual endpoint (resolves port 0 to the ephemeral pick).
    sockaddr_in bound{};
    socklen_t bound_size = sizeof(bound);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): getsockname is the POSIX API shape.
    if (::getsockname(sock, reinterpret_cast<sockaddr*>(&bound), &bound_size) == 0) {
      char digits[INET_ADDRSTRLEN]{0};
      ::inet_ntop(AF_INET, &bound.sin_addr, digits, sizeof(digits));
      tcp_endpoint_ = std::string{digits} + ":" + std::to_string(ntohs(bound.sin_port));
    } else {
      tcp_endpoint_ = bind;
    }
  }
  if (listen_fd_ < 0 && tcp_fd_ < 0 && (!path_.empty() || !bind.empty())) throw std::runtime_error("gateway: no listener bound");
}

GatewayTransport::~GatewayTransport() {
  for (const auto& peer : peers_) {
    for (const auto sub : peer.subscriptions) service_.unsubscribe(sub);
    if (peer.fd >= 0) ::close(peer.fd);
  }
  if (listen_fd_ >= 0) ::close(listen_fd_);
  if (tcp_fd_ >= 0) ::close(tcp_fd_);
  if (!path_.empty()) {
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
  }
}

GatewayTransport::Peer* GatewayTransport::find_peer(int peer_fd) {
  for (auto& peer : peers_) {
    if (peer.fd == peer_fd) return &peer;
  }
  return nullptr;
}

bool GatewayTransport::login_peer(int peer_fd, const std::string& account, const std::string& credential, std::string& bound_account_out, std::string& error) {
  if (!auth_enabled()) {
    error = "auth disabled (paper mode)";
    return false;
  }
  if (account.empty() || credential.empty()) {
    error = "bad credential";
    return false;
  }
  // Single failure shape: unknown accounts and wrong passwords refuse
  // alike (no enumeration). ADR-053 lazy login: with counter_login set the
  // trader-supplied password drives the counter login on first use (and
  // session verify afterwards); otherwise it is checked against the broker
  // credential the server logged in with (auth_.verify, memory only).
  bool verified = false;
  std::string canonical = account;
  if (auth_.counter_login) {
    try {
      verified = auth_.counter_login(account, credential, canonical);
    } catch (const std::exception&) {
      verified = false;
    }
    if (canonical.empty()) verified = false;
  } else if (auth_.verify) {
    try {
      verified = auth_.verify(account, credential);
    } catch (const std::exception&) {
      verified = false;
    }
  }
  if (!verified) {
    error = "bad credential";
    return false;
  }
  Peer* peer = find_peer(peer_fd);
  if (peer == nullptr) {
    error = "peer gone";
    return false;
  }
  // Remote-login replace (v1): one account holds a single session. A new
  // login from elsewhere kicks the older peer(s) bound to the same account
  // (subscriptions dropped, connection closed); strategies keep running on
  // the server independently of any peer.
  for (std::size_t index = peers_.size(); index-- > 0;) {
    if (peers_[index].fd != peer_fd && peers_[index].authed && peers_[index].bound_account == canonical) {
      drop_peer(index);
      peer = find_peer(peer_fd);  // erase may have moved storage
      if (peer == nullptr) {
        error = "peer gone";
        return false;
      }
    }
  }
  peer->peer_operator = canonical;
  peer->scope = "admin";
  peer->bound_account = canonical;
  peer->authed = true;
  bound_account_out = canonical;
  return true;
}

void GatewayTransport::logout_peer(int peer_fd) {
  Peer* peer = find_peer(peer_fd);
  if (peer == nullptr) return;
  for (const auto sub : peer->subscriptions) service_.unsubscribe(sub);
  peer->subscriptions.clear();
  peer->peer_operator.clear();
  peer->scope.clear();
  peer->bound_account.clear();
  peer->authed = false;
}

bool GatewayTransport::authorize(int peer_fd, const std::string& operation, const std::string& account, std::string& error) {
  (void)operation;                   // scoping is per-account (ADR-050), not per-op
  if (!auth_enabled()) return true;  // paper mode: socket permissions only
  const Peer* peer = find_peer(peer_fd);
  if (peer != nullptr && peer->authed) return true;  // bound sessions pass (account scoping is per-op)
  if (!account.empty()) {
    for (const auto& gated : auth_.login_accounts) {
      if (gated == account) {
        error = "login required";
        return false;
      }
    }
    return true;  // paper account stays open
  }
  // Global views would mix live data in: they need a session whenever the
  // gate is on. Id-attributed money ops without an account are global too.
  error = "login required";
  return false;
}

bool GatewayTransport::bound_session_account(int peer_fd, std::string& account_out) {
  Peer* peer = find_peer(peer_fd);
  if (peer == nullptr || !peer->authed || peer->bound_account.empty()) return false;
  account_out = peer->bound_account;
  return true;
}

void GatewayTransport::identity(int peer_fd, std::string& name_out, std::string& scope_out, std::string& account_out, bool& authed_out) {
  if (!auth_enabled()) {
    name_out = kGatewayAdmin;
    scope_out = "admin";
    account_out.clear();
    authed_out = true;
    return;
  }
  const Peer* peer = find_peer(peer_fd);
  if (peer == nullptr || !peer->authed) {
    name_out.clear();
    scope_out.clear();
    account_out.clear();
    authed_out = false;
    return;
  }
  name_out = peer->peer_operator;
  scope_out = peer->scope;
  account_out = peer->bound_account;
  authed_out = true;
}

void GatewayTransport::queue_push(int peer_fd, const std::string& frame) {
  Peer* peer = find_peer(peer_fd);
  if (peer == nullptr) return;
  peer->outbox += frame;
  if (peer->outbox.size() <= kMaxOutboxBytes) return;
  // Slow consumer: shed the peer (with its subscriptions) instead of
  // growing without bound; the next poll would drop it anyway.
  for (auto index = peers_.begin(); index != peers_.end(); ++index) {
    if (index->fd != peer_fd) continue;
    drop_peer(static_cast<std::size_t>(index - peers_.begin()));
    break;
  }
}

void GatewayTransport::drop_peer(std::size_t index) {
  Peer& peer = peers_[index];
  for (const auto sub : peer.subscriptions) service_.unsubscribe(sub);
  if (peer.fd >= 0) ::close(peer.fd);
  peers_.erase(peers_.begin() + static_cast<std::ptrdiff_t>(index));
}

void GatewayTransport::poll_once() {
  if (listen_fd_ >= 0) accept_peers();
  if (tcp_fd_ >= 0) accept_tcp_peers();
  for (std::size_t index = 0; index < peers_.size();) {
    if (!pump_peer(index) || !flush_peer(index)) {
      drop_peer(index);
      continue;
    }
    ++index;
  }
}

void GatewayTransport::accept_peers() {
  for (;;) {
    const int conn = ::accept(listen_fd_, nullptr, nullptr);
    if (conn < 0) break;
    if (peers_.size() >= kMaxPeers) {
      ::close(conn);  // busy: shed load instead of queueing
      continue;
    }
    set_nonblocking(conn);
    Peer peer{};
    peer.fd = conn;
    peers_.push_back(std::move(peer));
  }
}

void GatewayTransport::accept_tcp_peers() {
  for (;;) {
    const int conn = ::accept(tcp_fd_, nullptr, nullptr);
    if (conn < 0) break;
    if (peers_.size() >= kMaxPeers) {
      ::close(conn);  // busy: shed load instead of queueing (shared cap)
      continue;
    }
    set_nonblocking(conn);
    Peer peer{};
    peer.fd = conn;
    peers_.push_back(std::move(peer));
  }
}

bool GatewayTransport::pump_peer(std::size_t index) {
  Peer& peer = peers_[index];
  for (;;) {
    char chunk[512]{0};
    const ssize_t done = ::recv(peer.fd, chunk, sizeof(chunk) - 1, 0);
    if (done > 0) {
      peer.pending.append(chunk, static_cast<std::size_t>(done));
      continue;
    }
    if (done == 0) return false;  // orderly shutdown
    if (done < 0 && errno != EAGAIN && errno != EWOULDBLOCK) return false;
    break;
  }
  for (;;) {
    const std::size_t end = peer.pending.find('\n');
    if (end == std::string::npos) break;
    const std::string line = peer.pending.substr(0, end);
    peer.pending.erase(0, end + 1);
    const int peer_fd = peer.fd;
    // The dispatch borrows the peer by index: no drops happen inside
    // (replies are small; pushes only queue from the tick fan-out, and
    // the handlers registered here never fire synchronously).
    DispatchContext context{service_, *this, peer_fd, [this, peer_fd](const std::string& frame) { queue_push(peer_fd, frame); },
                            [this, index](SubscriptionToken sub) { peers_[index].subscriptions.push_back(sub); }};
    peers_[index].outbox += dispatch_request(line, context);
    if (peers_[index].outbox.size() > kMaxOutboxBytes) return false;  // pipelined flood: shed
  }
  return peer.pending.size() <= kMaxLineBytes;  // never-terminated lines are shed
}

bool GatewayTransport::flush_peer(std::size_t index) {
  Peer& peer = peers_[index];
  for (;;) {
    if (peer.outbox.empty()) break;
    const ssize_t done = ::send(peer.fd, peer.outbox.data(), peer.outbox.size(), MSG_NOSIGNAL);
    if (done <= 0) {
      if (done < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;  // retry next tick
      return false;
    }
    peer.outbox.erase(0, static_cast<std::size_t>(done));
  }
  return peer.outbox.size() <= kMaxOutboxBytes;
}


GatewayClient::GatewayClient(std::string socket_path) {
  if (socket_path.empty()) throw std::runtime_error("gateway client needs a non-empty socket path");
  if (socket_path.size() >= sizeof(sockaddr_un::sun_path)) throw std::runtime_error("gateway client path too long: " + socket_path);
  const int sock = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0) throw std::runtime_error("gateway client: socket() failed");
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  std::strncpy(address.sun_path, socket_path.c_str(), sizeof(address.sun_path) - 1);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): sockaddr_un downcast is the POSIX API shape.
  if (::connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    ::close(sock);
    throw std::runtime_error("gateway client: cannot connect (server down?): " + socket_path);
  }
  sock_ = sock;
}

GatewayClient::GatewayClient(std::string host, std::uint16_t port) {
  if (host.empty() || port == 0) throw std::runtime_error("gateway client needs a non-empty host and port");
  addrinfo wants{};
  wants.ai_family = AF_INET;
  wants.ai_socktype = SOCK_STREAM;
  addrinfo* resolved = nullptr;
  if (::getaddrinfo(host.c_str(), nullptr, &wants, &resolved) != 0 || resolved == nullptr) {
    throw std::runtime_error("gateway client: cannot resolve host: " + host);
  }
  int sock = -1;
  for (addrinfo* item = resolved; item != nullptr; item = item->ai_next) {
    sock = ::socket(item->ai_family, item->ai_socktype, item->ai_protocol);
    if (sock < 0) continue;
    auto* address = reinterpret_cast<sockaddr_in*>(item->ai_addr);
    address->sin_port = htons(port);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): sockaddr_in downcast is the POSIX API shape.
    if (::connect(sock, reinterpret_cast<sockaddr*>(address), sizeof(sockaddr_in)) == 0) break;
    ::close(sock);
    sock = -1;
  }
  ::freeaddrinfo(resolved);
  if (sock < 0) throw std::runtime_error("gateway client: cannot connect (server down?): " + host);
  sock_ = sock;
}

GatewayClient::~GatewayClient() {
  if (sock_ >= 0) ::close(sock_);
}

void GatewayClient::send(const std::string& line) const {
  const std::string framed = line + "\n";
  std::size_t sent = 0;
  while (sent < framed.size()) {
    const ssize_t done = ::send(sock_, framed.data() + sent, framed.size() - sent, MSG_NOSIGNAL);
    if (done <= 0) throw std::runtime_error("gateway client: send failed (server gone?)");
    sent += static_cast<std::size_t>(done);
  }
}

std::string GatewayClient::recv_line(int timeout_ms) {
  std::string frame;
  if (poll_line(frame, timeout_ms)) return frame;
  throw std::runtime_error("gateway client: recv timed out");
}

bool GatewayClient::poll_line(std::string& out, int timeout_ms) {
  if (timeout_ms < 0) throw std::runtime_error("gateway client: negative timeout");
  timeval window{};
  window.tv_sec = timeout_ms / 1000;
  window.tv_usec = static_cast<suseconds_t>((timeout_ms % 1000) * 1000);
  if (::setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &window, sizeof(window)) != 0) throw std::runtime_error("gateway client: cannot arm timeout");
  std::string frame = std::move(stash_);
  stash_.clear();
  for (;;) {
    const std::size_t end = frame.find('\n');
    if (end != std::string::npos) {
      stash_ = frame.substr(end + 1);
      out = frame.substr(0, end);
      return true;
    }
    if (frame.size() > kMaxOutboxBytes) throw std::runtime_error("gateway client: frame too long");
    char chunk[512]{0};
    const ssize_t done = ::recv(sock_, chunk, sizeof(chunk) - 1, 0);
    if (done == 0) {
      stash_ = std::move(frame);  // keep bytes for the next reader, if any
      throw std::runtime_error("gateway client: server closed the connection");
    }
    if (done < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        stash_ = std::move(frame);  // quiet timeout: nothing consumed
        return false;
      }
      throw std::runtime_error("gateway client: recv failed");
    }
    frame.append(chunk, static_cast<std::size_t>(done));
  }
}

}  // namespace sts::gateway
