// clang-format off
/**
  ******************************************************************************
  * @file           : admin_main.cpp
  * @author         : vivi wu
  * @brief          : qts_admin query CLI: offline file reads plus the live gateway client.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"
#include "qts/gateway/gateway_transport.hpp"
#include "qts/ops/admin_socket.hpp"
#include "qts/ops/admin_view.hpp"
#include "qts/ops/control.hpp"
#include "qts/persistence/event_store.hpp"
#include "qts/persistence/file_ledger.hpp"

namespace {

std::string status_name(qts::OrderStatus status) {
  switch (status) {
    case qts::OrderStatus::pending_new:
      return "pending_new";
    case qts::OrderStatus::live:
      return "live";
    case qts::OrderStatus::partially_filled:
      return "partially_filled";
    case qts::OrderStatus::filled:
      return "filled";
    case qts::OrderStatus::pending_cancel:
      return "pending_cancel";
    case qts::OrderStatus::cancelled:
      return "cancelled";
    case qts::OrderStatus::rejected:
      return "rejected";
  }
  return "unknown";
}

struct Options {
  std::string command;
  std::string sub;
  std::string ledger;
  std::string sqlite;
  std::string pg_env;
  std::string approve_out;
  std::string state_file;
  std::string socket;
  std::string tcp;  // P1: host:port, alternative to --socket
  std::string inbox;
  std::string id;
  std::vector<std::string> approve_channels;
  qts::Quantity quantity{0};
  qts::Price price{0};
  std::string account;
  std::string instrument;
  std::string side;
  std::string order_type;
  std::string approve_date;
  std::string approver;
  std::string approve_comment;
  std::string exchange;
  std::string security_account;
  std::string style;
  std::string strategy_type;
  std::string strategy;
  std::string snapshot_time;
  std::uint64_t watermark{0};
  int cash_dirty{0};
  int position_dirty{0};
  std::string settle_note;
  qts::Price lower{0};
  qts::Price upper{0};
  int grids{0};
  qts::Price start{0};
  qts::Quantity stock{0};
  qts::Price stock_avg{0};
  qts::Price starting_cash{1'000'000};
  std::size_t lines{10};
  // P2 live auth + fills paging.
  std::string live_account;
  std::string credential_env;
  std::size_t fill_limit{200};
};

bool parse_order_flag(int argc, char** argv, int& index, Options& out) {
  const std::string arg = argv[index];
  if (arg == "--account" && index + 1 < argc) {
    out.account = argv[++index];
  } else if (arg == "--instrument" && index + 1 < argc) {
    out.instrument = argv[++index];
  } else if (arg == "--side" && index + 1 < argc) {
    out.side = argv[++index];
  } else if (arg == "--type" && index + 1 < argc) {
    out.order_type = argv[++index];
  } else if (arg == "--exchange" && index + 1 < argc) {
    out.exchange = argv[++index];
  } else if (arg == "--security" && index + 1 < argc) {
    out.security_account = argv[++index];
  } else if (arg == "--style" && index + 1 < argc) {
    out.style = argv[++index];
  } else if (arg == "--strategy-type" && index + 1 < argc) {
    out.strategy_type = argv[++index];
  } else if (arg == "--lower" && index + 1 < argc) {
    out.lower = std::stoll(argv[++index]);
  } else if (arg == "--upper" && index + 1 < argc) {
    out.upper = std::stoll(argv[++index]);
  } else if (arg == "--grids" && index + 1 < argc) {
    out.grids = std::stoi(argv[++index]);
  } else if (arg == "--start" && index + 1 < argc) {
    out.start = std::stoll(argv[++index]);
  } else if (arg == "--stock" && index + 1 < argc) {
    out.stock = std::stoll(argv[++index]);
  } else if (arg == "--stock-avg" && index + 1 < argc) {
    out.stock_avg = std::stoll(argv[++index]);
  } else {
    return false;
  }
  return true;
}

bool parse_approve_flag(int argc, char** argv, int& index, Options& out) {
  const std::string arg = argv[index];
  if (arg == "--out" && index + 1 < argc) {
    out.approve_out = argv[++index];
  } else if (arg == "--channel" && index + 1 < argc) {
    out.approve_channels.emplace_back(argv[++index]);
  } else if (arg == "--date" && index + 1 < argc) {
    out.approve_date = argv[++index];
  } else if (arg == "--approver" && index + 1 < argc) {
    out.approver = argv[++index];
  } else if (arg == "--comment" && index + 1 < argc) {
    out.approve_comment = argv[++index];
  } else {
    return false;
  }
  return true;
}

bool parse_source_flag(int argc, char** argv, int& index, Options& out) {
  const std::string arg = argv[index];
  if (arg == "--ledger" && index + 1 < argc) {
    out.ledger = argv[++index];
  } else if (arg == "--sqlite" && index + 1 < argc) {
    out.sqlite = argv[++index];
  } else if (arg == "--pg-env" && index + 1 < argc) {
    out.pg_env = argv[++index];
  } else if (arg == "--state-file" && index + 1 < argc) {
    out.state_file = argv[++index];
  } else if (arg == "--socket" && index + 1 < argc) {
    out.socket = argv[++index];
  } else if (arg == "--tcp" && index + 1 < argc) {
    out.tcp = argv[++index];
  } else if (arg == "--inbox" && index + 1 < argc) {
    out.inbox = argv[++index];
  } else {
    return false;
  }
  return true;
}

bool parse_live_flag(int argc, char** argv, int& index, Options& out) {
  const std::string arg = argv[index];
  if (arg == "--strategy" && index + 1 < argc) {
    out.strategy = argv[++index];
  } else if ((arg == "--account-id" || arg == "--login-account") && index + 1 < argc) {
    out.live_account = argv[++index];
  } else if (arg == "--credential-env" && index + 1 < argc) {
    out.credential_env = argv[++index];
  } else if (arg == "--limit" && index + 1 < argc) {
    out.fill_limit = static_cast<std::size_t>(std::stoull(argv[++index]));
  } else if (arg == "--snapshot-time" && index + 1 < argc) {
    out.snapshot_time = argv[++index];
  } else if (arg == "--watermark" && index + 1 < argc) {
    out.watermark = static_cast<std::uint64_t>(std::stoull(argv[++index]));
  } else if (arg == "--cash-dirty" && index + 1 < argc) {
    out.cash_dirty = std::stoi(argv[++index]);
  } else if (arg == "--position-dirty" && index + 1 < argc) {
    out.position_dirty = std::stoi(argv[++index]);
  } else if (arg == "--note" && index + 1 < argc) {
    out.settle_note = argv[++index];
  } else {
    return false;
  }
  return true;
}

bool parse_flag(int argc, char** argv, int& index, Options& out) {
  if (parse_order_flag(argc, argv, index, out)) return true;
  if (parse_approve_flag(argc, argv, index, out)) return true;
  if (parse_source_flag(argc, argv, index, out)) return true;
  if (parse_live_flag(argc, argv, index, out)) return true;
  const std::string arg = argv[index];
  if (arg == "--id" && index + 1 < argc) {
    out.id = argv[++index];
  } else if (arg == "--quantity" && index + 1 < argc) {
    out.quantity = std::stoll(argv[++index]);
  } else if (arg == "--price" && index + 1 < argc) {
    out.price = std::stoll(argv[++index]);
  } else if (arg == "--cash" && index + 1 < argc) {
    out.starting_cash = std::stoll(argv[++index]);
  } else if ((arg == "-n" || arg == "--lines") && index + 1 < argc) {
    out.lines = static_cast<std::size_t>(std::stoull(argv[++index]));
  } else {
    return false;
  }
  return true;
}

bool parse_args(int argc, char** argv, Options& out) {
  for (int i = 1; i < argc; ++i) {
    if (parse_flag(argc, argv, i, out)) continue;
    const std::string arg = argv[i];
    if (!arg.empty() && arg[0] != '-' && out.command.empty()) {
      out.command = arg;
    } else if (!arg.empty() && arg[0] != '-' && out.sub.empty()) {
      out.sub = arg;
    } else {
      return false;
    }
  }
  return !out.command.empty();
}

void usage() {
  std::cerr
      << "usage: qts_admin <orders|positions|tail|reconcile|db|state|trace|send|approve>\n"
      << "  [--ledger FILE] [--sqlite FILE] [--pg-env NAME] [--state-file FILE] [--socket PATH] [--cash N] [-n N] [id]\n"
      << "  state shows the server's live state: --socket PATH queries the\n"
      << "  running server, --state-file FILE reads its snapshot file.\n"
      << "  orders|positions|tail|reconcile read the JSONL ledger file (offline, zero IPC).\n"
      << "  db shows the SQLite (--sqlite FILE) or PostgreSQL (--pg-env NAME, DSN from that env var) projection watermark and table counts.\n"
      << "  trace <id> follows one order (parent or child) intent -> children -> fills -> final state.\n"
      << "  send --inbox FILE "
         "<cancel|cancel-all|block-strategy|unblock-strategy|block-instrument|unblock-instrument|block-account|unblock-account|halt|resume|replace|order|strategy-start|strategy-stop|strategy-list>\n"
      << "    [--id ID] [--quantity N] [--price P] appends one operator command for the running server (zero IPC).\n"
      << "    order also needs --account A --instrument SYM --side buy|sell --type limit|market\n"
      << "    --exchange sse|szse|bse --security SECID [--style best5-ioc|best5-limit|own-best|cp-best, market only].\n"
      << "  approve --out FILE --channel NAME [--channel ...] --date YYYY-MM-DD --approver NAME [--comment TEXT]\n"
      << "    retired (ADR-051): always refuses, live authorization is the broker password.\n"
      << "  live --socket PATH|--tcp host:port <verb> [verb flags] [--account-id NAME --credential-env ENV]\n"
      << "    one-shot gateway client (P1 TCP or unix socket; live accounts log in\n"
      << "    with their broker password, by env only). Verbs: ping|whoami|cash|orders|order|\n"
      << "    positions|fills|progress|list|submit|cancel|start|stop|watch|account|\n"
      << "    settle-report|settle-confirm. account prints cash+orders+fills (--account);\n"
      << "    order/cancel/stop/progress need --id; cash/orders take\n"
      << "    --account; positions/fills take --strategy (default passthrough) and\n"
      << "    --account; fills takes --limit N (default 200); submit/start take the\n"
      << "    order/strategy ticket flags (--account --instrument --side --type\n"
      << "    --quantity --price --exchange --security [--style] / --strategy-type\n"
      << "    --lower --upper --grids --start); watch needs --id quotes|reports|\n"
      << "    strategies and prints N frames (--lines N, default 10, Ctrl-C stops).\n"
      << "    settle-report prints the broker-vs-book diff; settle-confirm attests it\n"
      << "    (--snapshot-time TIME --watermark N --cash-dirty N --position-dirty N [--note T]).\n";
}

// M9-2: offline reader over the append-only JSONL file. Safe to run while
// the gateway appends: FileEventLedger replays the snapshot at open.
std::vector<qts::LedgerEvent> load_ledger(const std::string& path) {
  if (path.empty()) throw std::runtime_error("this command needs --ledger FILE");
  qts::FileEventLedger ledger{path};
  return ledger.replay(1);
}

int run_orders(const Options& opts) {
  const auto events = load_ledger(opts.ledger);
  const auto views = qts::admin::summarize_orders(events);
  for (const auto& view : views) {
    std::cout << view.client_order_id << " status=" << status_name(view.status) << " cum=" << view.cumulative_quantity << " version=" << view.version << " event=" << view.last_event_id << '\n';
  }
  std::cout << "orders: count=" << views.size() << " ledger_events=" << events.size() << '\n';
  return 0;
}

int run_positions(const Options& opts) {
  const auto events = load_ledger(opts.ledger);
  qts::Portfolio book = qts::admin::rebuild_positions(events, opts.starting_cash);
  for (const auto& entry : book.all_positions()) {
    std::cout << entry.second.instrument_id << " total=" << entry.second.total << " sellable=" << entry.second.sellable << " avg_cost=" << entry.second.average_cost << '\n';
  }
  std::cout << "positions: cash=" << book.cash() << " realized=" << book.realized_pnl() << " fees=" << book.total_fees() << '\n';
  return 0;
}

int run_tail(const Options& opts) {
  const auto events = load_ledger(opts.ledger);
  const std::size_t total = events.size();
  const std::size_t shown = opts.lines < total ? opts.lines : total;
  for (std::size_t i = total - shown; i < total; ++i) {
    const auto& event = events[i];
    const auto client = event.snapshot.client_order_id.empty() ? event.intent.client_order_id : event.snapshot.client_order_id;
    std::cout << event.envelope.event_id << " " << qts::event_type_name(event.type) << " " << client << " " << status_name(event.snapshot.status) << '\n';
  }
  std::cout << "tail: shown=" << shown << " ledger_events=" << total << '\n';
  return 0;
}

int run_reconcile(const Options& opts) {
  const auto events = load_ledger(opts.ledger);
  const auto open = qts::admin::open_orders(events);
  for (const auto& view : open) {
    std::cout << "open " << view.client_order_id << " status=" << status_name(view.status) << " cum=" << view.cumulative_quantity << '\n';
  }
  if (open.empty()) {
    std::cout << "reconcile ok: open=0\n";
    return 0;
  }
  std::cout << "reconcile dirty: open=" << open.size() << '\n';
  return 1;
}

int run_db(const Options& opts) {
  // DSN travels by env name only (never argv/logs): same secret discipline
  // as the server's postgres.conninfo_env resolution.
  std::string pg_conninfo;
  if (!opts.pg_env.empty()) {
    const char* resolved = std::getenv(opts.pg_env.c_str());
    if (resolved == nullptr || *resolved == '\0') throw std::runtime_error("db: env " + opts.pg_env + " is not set");
    pg_conninfo = resolved;
  }
  if (opts.sqlite.empty() && pg_conninfo.empty()) throw std::runtime_error("db needs --sqlite FILE or --pg-env NAME");
  const std::unique_ptr<qts::EventStore> store = qts::open_event_store(opts.sqlite, pg_conninfo);
  const auto counts = store->counts();
  std::cout << "db: watermark=" << store->latest_event_id() << " events=" << counts.events << " orders=" << counts.orders << " executions=" << counts.executions << '\n';
  for (const auto& entry : store->strategy_order_counts()) {
    std::cout << "strategy: " << (entry.first.empty() ? "<untracked>" : entry.first) << " orders=" << entry.second << '\n';
  }
  for (const auto& row : store->positions()) {
    std::cout << "position: " << (row.strategy_id.empty() ? "<untracked>" : row.strategy_id) << " " << row.instrument_id << " qty=" << row.quantity << " avg=" << row.average_cost
              << " realized=" << row.realized_pnl << '\n';
  }
  return 0;
}

int run_state(const Options& opts) {
  nlohmann::json state;
  if (!opts.socket.empty()) {
    try {
      state = nlohmann::json::parse(qts::query_live_state(opts.socket));
    } catch (const nlohmann::json::exception& e) {
      throw std::runtime_error("bad state reply from " + opts.socket + ": " + e.what());
    }
  } else {
    if (opts.state_file.empty()) throw std::runtime_error("state needs --state-file PATH or --socket PATH");
    std::ifstream input{opts.state_file};
    if (!input) throw std::runtime_error("cannot open state file: " + opts.state_file);
    try {
      state = nlohmann::json::parse(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
    } catch (const nlohmann::json::exception& e) {
      throw std::runtime_error("bad state file " + opts.state_file + ": " + e.what());
    }
  }
  std::cout << "state: tick=" << state.value("tick", 0) << " date=" << state.value("trade_date", std::string{"?"}) << " submitted=" << state.value("submitted", 0)
            << " rejected=" << state.value("rejected", 0) << " ledger_events=" << state.value("ledger_events", 0) << " cash=" << state.value("cash", 0) << '\n';
  if (state.contains("open")) {
    for (const auto& order : state["open"]) {
      std::cout << "open: " << order.value("id", std::string{"?"}) << " status=" << order.value("status", std::string{"?"}) << " cum=" << order.value("cum", 0) << '\n';
    }
  }
  if (state.contains("positions")) {
    for (const auto& position : state["positions"]) {
      std::cout << "position: " << position.value("strategy", std::string{"?"}) << " " << position.value("instrument", std::string{"?"}) << " qty=" << position.value("qty", 0)
                << " avg=" << position.value("avg", 0) << " realized=" << position.value("realized", 0) << '\n';
    }
  }
  return 0;
}

// Live-trading approval issuance (M7-2d, ADR-039): offline, pre-startup.
// The server validates the file against its startup trade date; a
// well-formed date that never matches simply never authorizes (fail
// closed), so only the YYYY-MM-DD shape is enforced here.
bool valid_approval_date(const std::string& date) {
  if (date.size() != 10 || date[4] != '-' || date[7] != '-') return false;
  for (std::size_t i = 0; i < date.size(); ++i) {
    if (i == 4 || i == 7) continue;
    if (date[i] < '0' || date[i] > '9') return false;
  }
  const int month = (date[5] - '0') * 10 + (date[6] - '0');
  const int day = (date[8] - '0') * 10 + (date[9] - '0');
  return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

int run_approve(const Options& opts) {
  (void)opts;
  throw std::runtime_error("approve retired (ADR-051): live authorization is the broker password (env), no approval files");
}

int run_send(const Options& opts) {
  if (opts.inbox.empty()) throw std::runtime_error("send needs --inbox FILE");
  if (opts.sub.empty()) throw std::runtime_error("send needs an operator verb (cancel, cancel-all, block-strategy, halt, replace, ...)");
  qts::control::ControlCommand command{};
  if (opts.sub == "cancel-all") {
    command.name = "cancel_all";
  } else if (opts.sub == "block-strategy") {
    command.name = "block_strategy";
  } else if (opts.sub == "unblock-strategy") {
    command.name = "unblock_strategy";
  } else if (opts.sub == "block-instrument") {
    command.name = "block_instrument";
  } else if (opts.sub == "unblock-instrument") {
    command.name = "unblock_instrument";
  } else if (opts.sub == "block-account") {
    command.name = "block_account";
  } else if (opts.sub == "unblock-account") {
    command.name = "unblock_account";
  } else if (opts.sub == "cancel" || opts.sub == "halt" || opts.sub == "resume" || opts.sub == "replace") {
    command.name = opts.sub;
  } else if (opts.sub == "order") {
    command.name = "order";
    command.account = opts.account;
    command.instrument = opts.instrument;
    command.side = opts.side;
    command.order_type = opts.order_type;
    command.exchange = opts.exchange;
    command.security_account = opts.security_account;
    command.style = opts.style;
  } else if (opts.sub == "strategy-start") {
    command.name = "strategy_start";
    command.account = opts.account;
    command.instrument = opts.instrument;
    command.strategy_type = opts.strategy_type;
    command.lower = opts.lower;
    command.upper = opts.upper;
    command.grids = opts.grids;
    command.start = opts.start;
  } else if (opts.sub == "strategy-stop" || opts.sub == "strategy-list") {
    command.name = opts.sub == "strategy-stop" ? "strategy_stop" : "strategy_list";
  } else {
    throw std::runtime_error("unknown send verb: " + opts.sub);
  }
  command.id = opts.id;
  command.quantity = opts.quantity;
  command.price = opts.price;
  // Validate exactly what the server will parse: typos fail here, loudly.
  const std::string line = qts::control::format_command(command);
  qts::control::ControlCommand checked{};
  std::string error;
  if (!qts::control::parse_command(line, checked, error)) throw std::runtime_error(std::string{"send refused: "} + error);
  std::ofstream out{opts.inbox, std::ios::app};
  if (!out) throw std::runtime_error("cannot open inbox: " + opts.inbox);
  out << line << '\n';
  out.flush();
  if (!out) throw std::runtime_error("failed to append inbox: " + opts.inbox);
  std::cout << "sent: " << line << '\n';
  return 0;
}

// Live gateway client (P2): one-shot login + verb over the gateway socket
// or TCP endpoint. The password travels by env name only (never argv/logs),
// same discipline as db --pg-env. Each invocation logs in (when --account-id
// is set), runs one verb (or streams pushes for watch), then logs out.
std::unique_ptr<qts::gateway::GatewayClient> dial_live(const Options& opts) {
  if (!opts.tcp.empty()) {
    const std::size_t mark = opts.tcp.rfind(':');
    if (mark == std::string::npos || mark == 0 || mark + 1 >= opts.tcp.size()) throw std::runtime_error("live --tcp must be host:port");
    unsigned long port = 0;
    try {
      port = std::stoul(opts.tcp.substr(mark + 1));
    } catch (const std::exception&) {
      throw std::runtime_error("live --tcp port must be numeric");
    }
    if (port == 0 || port > 65535) throw std::runtime_error("live --tcp port out of range");
    return std::make_unique<qts::gateway::GatewayClient>(opts.tcp.substr(0, mark), static_cast<std::uint16_t>(port));
  }
  if (opts.socket.empty()) throw std::runtime_error("live needs --socket PATH or --tcp host:port (gateway.socket/bind)");
  return std::make_unique<qts::gateway::GatewayClient>(opts.socket);
}

void live_login(qts::gateway::GatewayClient& client, std::int64_t& req, const Options& opts) {
  if (opts.live_account.empty()) return;
  if (opts.credential_env.empty()) throw std::runtime_error("live --account-id needs --credential-env NAME (password by env, never argv)");
  const char* password = std::getenv(opts.credential_env.c_str());
  if (password == nullptr || std::string{password}.empty()) throw std::runtime_error(std::string{"live: env "} + opts.credential_env + " is empty");
  nlohmann::json body = nlohmann::json::object();
  body["req"] = ++req;
  body["op"] = "login";
  body["account"] = opts.live_account;
  body["password"] = password;
  client.send(body.dump());
  const nlohmann::json reply = nlohmann::json::parse(client.recv_line());
  if (!reply.value("ok", false)) throw std::runtime_error("live login refused: " + reply.value("message", std::string{"?"}));
}

void live_logout(qts::gateway::GatewayClient& client, std::int64_t& req, const Options& opts) {
  if (opts.live_account.empty()) return;
  nlohmann::json body = nlohmann::json::object();
  body["req"] = ++req;
  body["op"] = "logout";
  try {
    client.send(body.dump());
    (void)client.recv_line();
  } catch (const std::exception&) {
    // Best effort: the verb already ran.
  }
}

nlohmann::json live_request_body(const Options& opts) {
  nlohmann::json body = nlohmann::json::object();
  const std::string& verb = opts.sub;
  if (verb == "ping") {
    body["op"] = verb;
    return body;
  }
  if (verb == "whoami") {
    body["op"] = verb;
    return body;
  }
  if (verb == "cash") {
    body["op"] = verb;
    body["account"] = opts.account;
    return body;
  }
  if (verb == "orders") {
    body["op"] = "open_orders";
    body["account"] = opts.account;
    return body;
  }
  if (verb == "list") {
    body["op"] = "strategy_list";
    return body;
  }
  if (verb == "order") {
    if (opts.id.empty()) throw std::runtime_error("live order needs --id ID");
    body["op"] = "find_order";
    body["id"] = opts.id;
    return body;
  }
  if (verb == "positions") {
    body["op"] = "positions_for";
    body["strategy"] = opts.strategy.empty() ? std::string{qts::kPassthroughStrategy} : opts.strategy;
    body["account"] = opts.account;
    return body;
  }
  if (verb == "fills") {
    body["op"] = "fills";
    body["account"] = opts.account;
    body["strategy"] = opts.strategy;
    body["limit"] = opts.fill_limit;
    return body;
  }
  if (verb == "progress") {
    if (opts.id.empty()) throw std::runtime_error("live progress needs --id STRATEGY");
    body["op"] = "strategy_progress";
    body["id"] = opts.id;
    return body;
  }
  if (verb == "submit") {
    body["op"] = "submit";
    body["id"] = opts.id;
    body["account"] = opts.account;
    body["strategy"] = opts.strategy;
    body["instrument"] = opts.instrument;
    body["side"] = opts.side;
    body["type"] = opts.order_type;
    body["quantity"] = opts.quantity;
    body["price"] = opts.price;
    body["exchange"] = opts.exchange;
    body["security"] = opts.security_account;
    body["style"] = opts.style;
    return body;
  }
  if (verb == "cancel" || verb == "stop") {
    if (opts.id.empty()) throw std::runtime_error("live " + verb + " needs --id ID");
    body["op"] = verb == "stop" ? "strategy_stop" : "cancel";
    body["id"] = opts.id;
    return body;
  }
  if (verb == "start") {
    if (opts.id.empty()) throw std::runtime_error("live start needs --id ID");
    body["op"] = "strategy_start";
    body["id"] = opts.id;
    body["strategy_type"] = opts.strategy_type;
    body["account"] = opts.account;
    body["instrument"] = opts.instrument;
    body["quantity"] = opts.quantity;
    body["lower"] = opts.lower;
    body["upper"] = opts.upper;
    body["grids"] = opts.grids;
    body["start"] = opts.start;
    body["stock"] = opts.stock;
    body["stock_avg"] = opts.stock_avg;
    return body;
  }
  if (verb == "settle-report") {
    body["op"] = "settle_report";
    return body;
  }
  if (verb == "settle-confirm") {
    if (opts.snapshot_time.empty()) throw std::runtime_error("live settle-confirm needs --snapshot-time TIME (from settle-report)");
    body["op"] = "settle_confirm";
    body["snapshot_time"] = opts.snapshot_time;
    body["watermark"] = opts.watermark;
    body["cash_dirty"] = opts.cash_dirty;
    body["position_dirty"] = opts.position_dirty;
    body["note"] = opts.settle_note;
    return body;
  }
  throw std::runtime_error("unknown live verb: " + verb + " (ping|whoami|cash|orders|order|positions|fills|progress|list|submit|cancel|start|stop|watch|settle-report|settle-confirm)");
}

void print_settle_position(const nlohmann::json& position) {
  const bool dirty = position.value("qts_total", 0) != position.value("broker_total", 0) || position.value("qts_sellable", 0) != position.value("broker_sellable", 0) ||
                     position.value("qts_avg", 0) != position.value("broker_avg", 0);
  if (!dirty) return;
  std::cout << "settle-position-dirty: " << position.value("instrument", std::string{"?"}) << " qts=" << position.value("qts_total", 0) << "/" << position.value("qts_sellable", 0) << "/"
            << position.value("qts_avg", 0) << " broker=" << position.value("broker_total", 0) << "/" << position.value("broker_sellable", 0) << "/" << position.value("broker_avg", 0) << '\n';
}

void print_settle_account(const nlohmann::json& row) {
  std::cout << "settle-account: " << row.value("account", std::string{"?"}) << " clean=" << (row.value("clean", false) ? "yes" : "no") << " cash_dirty=" << row.value("cash_dirty", 0)
            << " position_dirty=" << row.value("position_dirty", 0) << " fees=" << row.value("fees", 0) << '\n';
  for (const auto& cash : row.value("cash", nlohmann::json::array())) {
    if (!cash.value("comparable", false) || cash.value("qts", 0) == cash.value("broker", 0)) continue;
    std::cout << "settle-cash-dirty: " << cash.value("field", std::string{"?"}) << " qts=" << cash.value("qts", 0) << " broker=" << cash.value("broker", 0) << '\n';
  }
  for (const auto& position : row.value("positions", nlohmann::json::array())) print_settle_position(position);
}

void print_settle_report(const nlohmann::json& data) {
  std::cout << "settle: snapshot=" << data.value("snapshot_time", std::string{"?"}) << " watermark=" << data.value("ledger_watermark", 0) << '\n';
  for (const auto& row : data.value("accounts", nlohmann::json::array())) print_settle_account(row);
}

void print_live_reply(const std::string& verb, const nlohmann::json& reply) {
  if (!reply.value("ok", false)) throw std::runtime_error("live refused: " + reply.value("message", std::string{"?"}));
  const nlohmann::json& data = reply.value("data", nlohmann::json::object());
  if (verb == "ping") {
    std::cout << "live: pong\n";
  } else if (verb == "cash") {
    std::cout << "live: cash=" << data.value("cash", 0) << (data.value("shared", false) ? " (shared book)" : "") << '\n';
  } else if (verb == "whoami") {
    std::cout << "live: account=" << data.value("account", data.value("operator", std::string{"-"})) << " authed=" << (data.value("authed", false) ? "yes" : "no") << '\n';
  } else if (verb == "fills") {
    for (const auto& fill : data.value("fills", nlohmann::json::array())) {
      std::cout << "live-fill: " << fill.value("client_order_id", std::string{"?"}) << " status=" << fill.value("status", std::string{"?"}) << " cum=" << fill.value("cumulative_quantity", 0) << " "
                << fill.value("account", std::string{"?"}) << "/" << fill.value("strategy", std::string{"?"}) << " seq=" << fill.value("seq", 0) << '\n';
    }
    std::cout << "live: fills=" << data.value("fills", nlohmann::json::array()).size() << " truncated=" << (data.value("truncated", false) ? "yes" : "no") << '\n';
  } else if (verb == "progress") {
    std::cout << "live-progress: " << data.value("strategy", std::string{"?"}) << " running=" << (data.value("running", false) ? "yes" : "no") << " submitted=" << data.value("submitted", 0)
              << " open=" << data.value("open", 0) << " filled=" << data.value("filled", 0) << " cancelled=" << data.value("cancelled", 0) << " rejected=" << data.value("rejected", 0)
              << " pos=" << data.value("position_qty", 0) << " realized=" << data.value("realized", 0) << '\n';
  } else if (verb == "orders") {
    for (const auto& order : data.value("orders", nlohmann::json::array())) {
      std::cout << "live-order: " << order.value("client_order_id", std::string{"?"}) << " status=" << order.value("status", std::string{"?"}) << " cum=" << order.value("cumulative_quantity", 0)
                << '\n';
    }
    std::cout << "live: orders=" << data.value("orders", nlohmann::json::array()).size() << '\n';
  } else if (verb == "order") {
    const nlohmann::json& order = data.value("order", nlohmann::json::object());
    std::cout << "live-order: " << order.value("client_order_id", std::string{"?"}) << " status=" << order.value("status", std::string{"?"}) << " cum=" << order.value("cumulative_quantity", 0)
              << '\n';
  } else if (verb == "positions") {
    for (const auto& row : data.value("positions", nlohmann::json::array())) {
      std::cout << "live-position: " << row.value("instrument", std::string{"?"}) << " qty=" << row.value("qty", 0) << " sellable=" << row.value("sellable", 0) << " avg=" << row.value("avg", 0)
                << '\n';
    }
  } else if (verb == "settle-report") {
    print_settle_report(reply.value("data", nlohmann::json::object()));
  } else {
    std::cout << "live: " << reply.value("message", std::string{"ok"}) << '\n';
  }
}

nlohmann::json live_roundtrip(qts::gateway::GatewayClient& client, std::int64_t& req, const Options& opts, nlohmann::json body);
void run_live_watch(qts::gateway::GatewayClient& client, std::int64_t& req, const Options& opts);

int run_live(const Options& opts) {
  if (!opts.socket.empty() && !opts.tcp.empty()) throw std::runtime_error("live takes either --socket PATH or --tcp host:port");
  if (opts.sub.empty()) throw std::runtime_error("live needs a verb: ping|whoami|cash|orders|order|positions|fills|progress|list|submit|cancel|start|stop|watch|account");
  auto client = dial_live(opts);
  std::int64_t req = 0;
  live_login(*client, req, opts);
  if (opts.sub == "watch") {
    run_live_watch(*client, req, opts);
  } else if (opts.sub == "account") {
    // 查询账户 dashboard: 资金 + 今日委托 + 今日成交 (positions needs a strategy, use positions verb).
    if (opts.account.empty()) throw std::runtime_error("live account needs --account ID");
    Options cash_opts = opts;
    cash_opts.sub = "cash";
    print_live_reply("cash", live_roundtrip(*client, req, opts, live_request_body(cash_opts)));
    Options orders_opts = opts;
    orders_opts.sub = "orders";
    print_live_reply("orders", live_roundtrip(*client, req, opts, live_request_body(orders_opts)));
    Options fills_opts = opts;
    fills_opts.sub = "fills";
    print_live_reply("fills", live_roundtrip(*client, req, opts, live_request_body(fills_opts)));
  } else {
    print_live_reply(opts.sub, live_roundtrip(*client, req, opts, live_request_body(opts)));
  }
  live_logout(*client, req, opts);
  return 0;
}

nlohmann::json live_roundtrip(qts::gateway::GatewayClient& client, std::int64_t& req, const Options& opts, nlohmann::json body) {
  body["req"] = ++req;
  client.send(body.dump());
  const std::string endpoint = opts.tcp.empty() ? opts.socket : opts.tcp;
  for (int waited = 0; waited < 64; ++waited) {
    nlohmann::json reply;
    try {
      reply = nlohmann::json::parse(client.recv_line());
    } catch (const std::exception& recv_failure) {
      throw std::runtime_error(std::string{"live: no reply from "} + endpoint + ": " + recv_failure.what());
    }
    if (reply.value("req", 0LL) == req) return reply;
  }
  throw std::runtime_error("live: req mismatch from " + endpoint);
}

void run_live_watch(qts::gateway::GatewayClient& client, std::int64_t& req, const Options& opts) {
  if (opts.id != "quotes" && opts.id != "reports" && opts.id != "strategies") throw std::runtime_error("live watch needs --id quotes|reports|strategies");
  nlohmann::json subscribe = nlohmann::json::object();
  subscribe["op"] = "subscribe_" + opts.id;
  const nlohmann::json sub_reply = live_roundtrip(client, req, opts, subscribe);
  if (!sub_reply.value("ok", false)) throw std::runtime_error("live refused: " + sub_reply.value("message", std::string{"?"}));
  std::size_t shown = 0;
  for (;;) {
    if (opts.lines != 0 && shown >= opts.lines) break;
    std::string frame;
    try {
      frame = client.recv_line();
    } catch (const std::exception&) {
      std::cout << "watch: stream ended after " << shown << " frames\n";
      break;
    }
    std::cout << frame << '\n';
    ++shown;
  }
  std::cout << "watch: shown=" << shown << '\n';
}

}  // namespace

// qts_admin (M9-2, ADR-034; live client since M12-3): read-only projector
// over ledger/SQLite files (zero IPC), plus a one-shot client for the
// running gateway (live ... over the gateway socket or TCP, login when the
// server has operators).
int main(int argc, char** argv) {
  Options opts{};
  if (!parse_args(argc, argv, opts)) {
    usage();
    return 2;
  }
  try {
    if (opts.command == "orders") return run_orders(opts);
    if (opts.command == "positions") return run_positions(opts);
    if (opts.command == "tail") return run_tail(opts);
    if (opts.command == "reconcile") return run_reconcile(opts);
    if (opts.command == "db") return run_db(opts);
    if (opts.command == "state") return run_state(opts);
    if (opts.command == "send") return run_send(opts);
    if (opts.command == "approve") return run_approve(opts);
    if (opts.command == "live") return run_live(opts);
  } catch (const std::exception& e) {
    std::cerr << "qts_admin: " << e.what() << '\n';
    return 2;
  }
  usage();
  return 2;
}
