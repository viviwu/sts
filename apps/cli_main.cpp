// clang-format off
/**
  ******************************************************************************
  * @file           : cli_main.cpp
  * @author         : vivi wu
  * @brief          : sts_cli resident gateway REPL: commands plus live pushes.
  * @version        : 0.1.0
  * @date           : 11/10/26
  ******************************************************************************
  */
// clang-format on
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#ifdef _WIN32
  #include <io.h>
#else
  #include <termios.h>
  #include <unistd.h>
#endif

#include "nlohmann/json.hpp"
#include "sts/gateway/gateway_transport.hpp"
#include "yaml-cpp/yaml.h"

namespace {

bool stdin_is_tty() {
#ifdef _WIN32
  return _isatty(_fileno(stdin)) != 0;
#else
  return ::isatty(STDIN_FILENO) != 0;
#endif
}

struct CliOptions {
  std::string socket;
  std::string tcp;  // P1: host:port, alternative to --socket
  std::string prompt{"sts> "};
  // sts_app.yaml presets (used when the command omits them).
  std::string default_operator;
  std::string default_account;
  std::string default_strategy;
  std::string default_instrument;
  // quick: mapping of per-ticket prompted defaults.
  std::map<std::string, std::string> quick_defaults;
};

bool parse_cli_args(int argc, char** argv, CliOptions& out) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if ((arg == "--socket" || arg == "--tcp" || arg == "--prompt") && i + 1 < argc) {
      const std::string value = argv[++i];
      if (arg == "--socket") {
        out.socket = value;
      } else if (arg == "--tcp") {
        out.tcp = value;
      } else {
        out.prompt = value;
      }
    } else {
      return false;
    }
  }
  if (!out.socket.empty() && !out.tcp.empty()) return false;
  return true;  // empty endpoint falls back to cfg/sts_app.yaml in main
}

// Client defaults file (<exe-dir>/cfg/sts_app.yaml): connection endpoint
// plus operator/account/strategy presets. Explicit argv wins; missing file
// with no argv endpoint is a usage error (never a silent default server).
bool load_app_defaults(const std::string& app_path, CliOptions& out, std::string& error) {
  YAML::Node root;
  try {
    root = YAML::LoadFile(app_path);
  } catch (const std::exception& load_failure) {
    error = std::string{"cannot open app config file: "} + app_path + " (" + load_failure.what() + ")";
    return false;
  }
  if (root["gateway"]) {
    const YAML::Node gateway = root["gateway"];
    if (!gateway.IsMap()) {
      error = app_path + ": 'gateway' must be a mapping";
      return false;
    }
    if (gateway["socket"] && out.socket.empty()) out.socket = gateway["socket"].as<std::string>("");
    if (gateway["tcp"] && out.tcp.empty()) out.tcp = gateway["tcp"].as<std::string>("");
  }
  if (root["operator"]) out.default_operator = root["operator"].as<std::string>("");
  if (root["account"]) out.default_account = root["account"].as<std::string>("");
  if (root["strategy"]) out.default_strategy = root["strategy"].as<std::string>("");
  if (root["instrument"]) out.default_instrument = root["instrument"].as<std::string>("");
  if (root["quick"] && root["quick"].IsMap()) {
    for (auto item = root["quick"].begin(); item != root["quick"].end(); ++item) {
      if (item->second.IsScalar()) out.quick_defaults[item->first.as<std::string>()] = item->second.as<std::string>();
    }
  }
  if (!out.socket.empty() && !out.tcp.empty()) {
    error = app_path + ": set either gateway.socket or gateway.tcp";
    return false;
  }
  if (out.socket.empty() && out.tcp.empty()) {
    error = app_path + ": gateway.socket or gateway.tcp is required";
    return false;
  }
  return true;
}

void usage() {
  std::cerr << "usage: sts_cli [--socket PATH|--tcp host:port] [--prompt TEXT]\n"
            << "  No endpoint flag reads <exe-dir>/cfg/sts_app.yaml (connection +\n"
            << "  account/strategy presets). Resident gateway shell (paper accounts\n"
            << "  stay open; live-broker accounts log in with their broker password):\n"
            << "  reads commands from stdin (one per line, piped scripts work),\n"
            << "  prints replies and interleaved pushes.\n"
            << "  Commands: login logout whoami buy sell trade strategy account quick ping cash orders order positions fills\n"
            << "  progress list submit cancel start stop watch sub unsub help quit.\n"
            << "  Args are k=v (id= | account= | strategy= | instrument= | side= |\n"
            << "  type= | quantity= | price= | exchange= | security= | style= |\n"
            << "  strategy_type= | lower= | upper= | grids= | start= | stock= | stock_avg= |\n"
            << "  limit= | interval= | count= | what= | menu= | sub=). Bare tokens fill\n"
            << "  the verb's default key (id, or account for login, sub for strategy,\n"
            << "  menu for quick, what for sub/watch, strategy for positions).\n"
            << "  login takes password= or reads $STS_GW_PW (tty hidden prompt fallback).\n"
            << "  watch what=cash|orders|positions|fills|progress [interval=2] [count=5]\n"
            << "  re-runs one query (plus account=/strategy=/id= filters).\n"
            << "  quick strategy|buy|sell [k=v ...] [confirm=y]: guided ticket with\n"
            << "  presets as defaults (tty prompts per missing field, then confirm).\n"
            << "  Examples:\n"
            << "    login account=10014939\n"
            << "    cash account=paper\n"
            << "    fills account=paper strategy=grid-1 limit=50\n"
            << "    progress grid-1\n"
            << "    sub what=reports\n";
}

// Hidden password prompt (P2): tty-only, no echo; piped stdin falls back
// to a plain read (scripts should prefer $STS_GW_PW anyway).
std::string read_secret(const std::string& prompt_text) {
#ifdef _WIN32
  (void)prompt_text;
  throw std::runtime_error("hidden password prompt needs a POSIX tty (use $STS_GW_PW)");
#else
  termios saved{};
  if (::tcgetattr(STDIN_FILENO, &saved) != 0) {
    std::string line;
    std::getline(std::cin, line);
    return line;
  }
  termios quiet = saved;
  quiet.c_lflag &= static_cast<unsigned>(~ECHO);
  ::tcsetattr(STDIN_FILENO, TCSANOW, &quiet);
  std::string line;
  std::getline(std::cin, line);
  ::tcsetattr(STDIN_FILENO, TCSANOW, &saved);
  std::cout << '\n';
  return line;
#endif
}

// Frame pump: the only socket reader. Replies route to the waiting command
// by req; pushes print immediately. Replies for unknown reqs are dropped,
// never misdelivered.
class FramePump {
 public:
  FramePump(sts::gateway::GatewayClient& client, std::mutex& print, std::string prompt, bool tty)
      : client_(client),
        print_(print),
        prompt_(std::move(prompt)),
        tty_(tty) {}

  void start() {
    worker_ = std::thread([this] { run(); });
  }

  void stop() {
    stop_.store(true);
    if (worker_.joinable()) worker_.join();
  }

  nlohmann::json wait_reply(std::int64_t req, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{timeout_ms};
    std::unique_lock<std::mutex> guard(mutex_);
    for (;;) {
      const auto found = replies_.find(req);
      if (found != replies_.end()) {
        nlohmann::json reply = std::move(found->second);
        replies_.erase(found);
        return reply;
      }
      if (dead_.load()) throw std::runtime_error("sts_cli: server closed the connection");
      if (cv_.wait_until(guard, deadline) == std::cv_status::timeout) throw std::runtime_error("sts_cli: no reply (timed out)");
    }
  }

 private:
  void run() {
    for (;;) {
      if (stop_.load()) return;
      std::string frame;
      bool has_frame = false;
      try {
        has_frame = client_.poll_line(frame, 500);
      } catch (const std::exception& pump_failure) {
        std::lock_guard<std::mutex> guard(print_);
        std::cout << "<< closed (" << pump_failure.what() << ")\n";
        dead_.store(true);
        cv_.notify_all();
        return;
      }
      if (!has_frame) continue;
      nlohmann::json parsed;
      try {
        parsed = nlohmann::json::parse(frame);
      } catch (const std::exception&) {
        std::lock_guard<std::mutex> guard(print_);
        std::cout << "<< " << frame << '\n';
        continue;
      }
      if (parsed.contains("push")) {
        std::lock_guard<std::mutex> guard(print_);
        std::cout << "<< " << frame << '\n';
        if (tty_) std::cout << prompt_ << std::flush;
        continue;
      }
      const std::int64_t reply_to = parsed.value("req", 0LL);
      if (reply_to != 0) {
        std::lock_guard<std::mutex> guard(mutex_);
        replies_[reply_to] = std::move(parsed);
        cv_.notify_all();
      }
    }
  }

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  sts::gateway::GatewayClient& client_;
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  std::mutex& print_;
  std::string prompt_;
  bool tty_{false};
  std::mutex mutex_;
  std::condition_variable cv_;
  std::map<std::int64_t, nlohmann::json> replies_;
  std::atomic<bool> stop_{false};
  std::atomic<bool> dead_{false};
  std::thread worker_;
};

struct Session {
  std::unique_ptr<sts::gateway::GatewayClient> owned;
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  sts::gateway::GatewayClient& client;
  FramePump pump;
  std::mutex print;
  std::int64_t req{0};
  // Monotonic ticket counter: auto order ids must stay unique across
  // restarts (the ledger dedups by id, so req-based ids collided).
  std::int64_t auto_seq{0};
  std::vector<std::uint64_t> subs;
  bool tty{false};
  std::string prompt;
  // sts_app.yaml presets (fill command args when omitted).
  std::string default_operator;
  std::string default_account;
  std::string default_strategy;
  std::string default_instrument;
  std::map<std::string, std::string> quick_defaults;

  Session(std::unique_ptr<sts::gateway::GatewayClient> dialed, const CliOptions& opts, bool tty_in)
      : owned(std::move(dialed)),
        client(*owned),
        pump(client, print, opts.prompt, tty_in),
        tty(tty_in),
        prompt(opts.prompt),
        default_operator(opts.default_operator),
        default_account(opts.default_account),
        default_strategy(opts.default_strategy),
        default_instrument(opts.default_instrument),
        quick_defaults(opts.quick_defaults) {}
};

std::unique_ptr<sts::gateway::GatewayClient> dial_session(const CliOptions& opts) {
  if (!opts.tcp.empty()) {
    const std::size_t mark = opts.tcp.rfind(':');
    if (mark == std::string::npos || mark == 0 || mark + 1 >= opts.tcp.size()) throw std::runtime_error("sts_cli --tcp must be host:port");
    unsigned long port = 0;
    try {
      port = std::stoul(opts.tcp.substr(mark + 1));
    } catch (const std::exception&) {
      throw std::runtime_error("sts_cli --tcp port must be numeric");
    }
    if (port == 0 || port > 65535) throw std::runtime_error("sts_cli --tcp port out of range");
    return std::make_unique<sts::gateway::GatewayClient>(opts.tcp.substr(0, mark), static_cast<std::uint16_t>(port));
  }
  return std::make_unique<sts::gateway::GatewayClient>(opts.socket);
}

nlohmann::json roundtrip(Session& session, nlohmann::json body) {
  body["req"] = ++session.req;
  session.client.send(body.dump());
  return session.pump.wait_reply(session.req, 5000);
}

void checked_reply(const nlohmann::json& reply) {
  if (!reply.value("ok", false)) throw std::runtime_error("refused: " + reply.value("message", std::string{"?"}));
}

std::map<std::string, std::string> parse_kv(const std::vector<std::string>& tokens, const std::string& verb) {
  std::string fallback = "id";
  if (verb == "positions") {
    fallback = "strategy";
  } else if (verb == "login") {
    fallback = "account";
  } else if (verb == "strategy") {
    fallback = "sub";
  } else if (verb == "quick") {
    fallback = "menu";
  } else if (verb == "sub" || verb == "watch") {
    fallback = "what";
  }
  std::map<std::string, std::string> args;
  for (const auto& piece : tokens) {
    const std::size_t mark = piece.find('=');
    if (mark == std::string::npos) {
      args[fallback] = piece;
      continue;
    }
    args[piece.substr(0, mark)] = piece.substr(mark + 1);
  }
  return args;
}

std::int64_t number_for(const std::map<std::string, std::string>& args, const std::string& key) {
  const auto found = args.find(key);
  if (found == args.end() || found->second.empty()) return 0;
  // Strict integers only: "4.45" must refuse here, never truncate to 4.
  const std::string& text = found->second;
  std::size_t head = 0;
  if (text[head] == '-' || text[head] == '+') head = 1;
  if (head == text.size()) throw std::runtime_error("bad number for " + key + ": " + text);
  for (std::size_t i = head; i < text.size(); ++i) {
    if (text[i] < '0' || text[i] > '9') throw std::runtime_error("bad number for " + key + ": " + text);
  }
  try {
    return std::stoll(text);
  } catch (const std::exception&) {
    throw std::runtime_error("bad number for " + key + ": " + text);
  }
}

std::string text_for(const std::map<std::string, std::string>& args, const std::string& key) {
  const auto found = args.find(key);
  return found == args.end() ? std::string{} : found->second;
}

// Auto order ids: timestamp + pid + session sequence, unique across client
// restarts (per-session req counters collided with ledger history).
std::string auto_order_id(Session& session, const std::string& prefix) {
  const std::time_t now = std::time(nullptr);
  char stamp[16]{0};
  std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", std::localtime(&now));
#ifdef _WIN32
  const int pid = _getpid();
#else
  const int pid = static_cast<int>(::getpid());
#endif
  return prefix + "-" + stamp + "-" + std::to_string(pid) + "-" + std::to_string(++session.auto_seq);
}

nlohmann::json command_body(const std::string& verb, const std::map<std::string, std::string>& args) {
  nlohmann::json body = nlohmann::json::object();
  if (verb == "ping" || verb == "logout" || verb == "whoami") {
    body["op"] = verb;
  } else if (verb == "login") {
    body["op"] = "login";
    body["account"] = text_for(args, "account");
    body["password"] = text_for(args, "password");
  } else if (verb == "cash") {
    body["op"] = verb;
    body["account"] = text_for(args, "account");
  } else if (verb == "orders") {
    body["op"] = "open_orders";
    body["account"] = text_for(args, "account");
  } else if (verb == "list") {
    body["op"] = "strategy_list";
  } else if (verb == "order") {
    body["op"] = "find_order";
    body["id"] = text_for(args, "id");
  } else if (verb == "positions") {
    body["op"] = "positions_for";
    const std::string basket = text_for(args, "strategy");
    body["strategy"] = basket.empty() ? std::string{sts::kPassthroughStrategy} : basket;
    body["account"] = text_for(args, "account");
  } else if (verb == "fills") {
    body["op"] = "fills";
    body["account"] = text_for(args, "account");
    body["strategy"] = text_for(args, "strategy");
    body["limit"] = number_for(args, "limit");
  } else if (verb == "progress") {
    body["op"] = "strategy_progress";
    body["id"] = text_for(args, "id");
  } else if (verb == "submit") {
    body["op"] = "submit";
    body["id"] = text_for(args, "id");
    body["account"] = text_for(args, "account");
    body["strategy"] = text_for(args, "strategy");
    body["instrument"] = text_for(args, "instrument");
    body["side"] = text_for(args, "side");
    body["type"] = text_for(args, "type");
    body["quantity"] = number_for(args, "quantity");
    body["price"] = number_for(args, "price");
    body["exchange"] = text_for(args, "exchange");
    body["security"] = text_for(args, "security");
    body["style"] = text_for(args, "style");
  } else if (verb == "cancel") {
    body["op"] = "cancel";
    body["id"] = text_for(args, "id");
  } else if (verb == "stop") {
    body["op"] = "strategy_stop";
    body["id"] = text_for(args, "id");
  } else if (verb == "start") {
    body["op"] = "strategy_start";
    body["id"] = text_for(args, "id");
    body["strategy_type"] = text_for(args, "strategy_type");
    body["account"] = text_for(args, "account");
    body["instrument"] = text_for(args, "instrument");
    body["quantity"] = number_for(args, "quantity");
    body["lower"] = number_for(args, "lower");
    body["upper"] = number_for(args, "upper");
    body["grids"] = number_for(args, "grids");
    body["start"] = number_for(args, "start");
    body["stock"] = number_for(args, "stock");
    body["stock_avg"] = number_for(args, "stock_avg");
  } else {
    throw std::runtime_error("unknown command (try help): " + verb);
  }
  return body;
}

void print_reply(Session& session, const std::string& verb, const nlohmann::json& reply) {
  checked_reply(reply);
  std::lock_guard<std::mutex> guard(session.print);
  const nlohmann::json& data = reply.value("data", nlohmann::json::object());
  if (verb == "ping") {
    std::cout << "ok: pong\n";
  } else if (verb == "login") {
    std::cout << "ok: logged in as " << data.value("account", data.value("operator", std::string{"?"})) << '\n';
  } else if (verb == "logout") {
    std::cout << "ok: logged out\n";
  } else if (verb == "whoami") {
    std::cout << "ok: operator=" << data.value("operator", std::string{"-"}) << " scope=" << data.value("scope", std::string{"-"}) << " authed=" << (data.value("authed", false) ? "yes" : "no")
              << '\n';
  } else if (verb == "cash") {
    std::cout << "ok: cash=" << data.value("cash", 0) << (data.value("shared", false) ? " (shared book)" : "") << '\n';
  } else if (verb == "orders") {
    for (const auto& order : data.value("orders", nlohmann::json::array())) {
      std::cout << "order: " << order.value("client_order_id", std::string{"?"}) << " status=" << order.value("status", std::string{"?"}) << " cum=" << order.value("cumulative_quantity", 0) << '\n';
    }
  } else if (verb == "fills") {
    for (const auto& fill : data.value("fills", nlohmann::json::array())) {
      std::cout << "fill: " << fill.value("client_order_id", std::string{"?"}) << " status=" << fill.value("status", std::string{"?"}) << " cum=" << fill.value("cumulative_quantity", 0) << " "
                << fill.value("account", std::string{"?"}) << "/" << fill.value("strategy", std::string{"?"}) << " seq=" << fill.value("seq", 0) << '\n';
    }
    if (data.value("truncated", false)) std::cout << "fill: ... truncated (raise limit=)\n";
  } else if (verb == "progress") {
    std::cout << "progress: " << data.value("strategy", std::string{"?"}) << " running=" << (data.value("running", false) ? "yes" : "no") << " submitted=" << data.value("submitted", 0)
              << " open=" << data.value("open", 0) << " filled=" << data.value("filled", 0) << " cancelled=" << data.value("cancelled", 0) << " rejected=" << data.value("rejected", 0)
              << " pos=" << data.value("position_qty", 0) << " realized=" << data.value("realized", 0) << '\n';
  } else if (verb == "order") {
    const nlohmann::json& order = data.value("order", nlohmann::json::object());
    std::cout << "order: " << order.value("client_order_id", std::string{"?"}) << " status=" << order.value("status", std::string{"?"}) << " cum=" << order.value("cumulative_quantity", 0) << '\n';
  } else if (verb == "positions") {
    for (const auto& row : data.value("positions", nlohmann::json::array())) {
      std::cout << "position: " << row.value("instrument", std::string{"?"}) << " qty=" << row.value("qty", 0) << " sellable=" << row.value("sellable", 0) << " avg=" << row.value("avg", 0) << '\n';
    }
  } else {
    std::cout << "ok: " << reply.value("message", std::string{"ok"}) << '\n';
  }
}

// One REPL line. Returns false on quit; throws loudly on refused commands
// (the session stays up, like the socket shell itself).
bool run_line(Session& session, const std::string& line) {
  std::istringstream words(line);
  std::string verb;
  words >> verb;
  if (verb.empty()) return true;
  if (verb == "quit" || verb == "exit") return false;
  if (verb == "help") {
    std::lock_guard<std::mutex> guard(session.print);
    std::cout << "一级菜单: login logout whoami buy sell trade strategy account quick ping cash orders order positions fills progress list submit cancel start stop watch sub unsub help quit\n"
              << "  缺参数时逐项提示（预设/默认显示在 [] 中，回车即用；成交类动词提示后需 confirm）\n"
              << "  buy/sell: 现货买卖 (instrument quantity price [type=limit] [account] [strategy])\n"
              << "  trade: 现货买卖 side 作输入 (side=buy|sell instrument quantity price [type=limit])\n"
              << "  strategy grid: 网格策略 (id account instrument quantity lower upper grids start [stock] [stock_avg])\n"
              << "  account: 查询账户 (资金+持仓+今日委托+今日成交，需 account=)\n"
              << "  quick buy|sell|strategy: 一键下单（预设+确认）\n"
              << "  cash/orders/positions/fills: 查询可附 account=/strategy= 过滤，回车即不过滤\n";
    return true;
  }
  std::vector<std::string> tokens;
  std::string piece;
  while (words >> piece) tokens.push_back(piece);
  std::map<std::string, std::string> args = parse_kv(tokens, verb);
  // sts_app.yaml presets fill omitted keys (explicit k=v always wins).
  if (args["operator"].empty() && !session.default_operator.empty()) args["operator"] = session.default_operator;
  if (args["account"].empty() && !session.default_account.empty()) args["account"] = session.default_account;
  if (args["strategy"].empty() && !session.default_strategy.empty()) args["strategy"] = session.default_strategy;
  if (args["instrument"].empty() && !session.default_instrument.empty()) args["instrument"] = session.default_instrument;
  // Guided interaction (demo parity): on a tty, missing fields are prompted
  // with presets/defaults shown (Enter accepts); piped scripts must pass
  // every field as k=v (fail closed, wire behavior unchanged). confirm_ticket
  // asks once when anything was prompted; fully-specified lines run directly.
  bool prompted_any = false;
  auto prompt_into = [&](std::map<std::string, std::string>& fields, const std::string& key, const std::string& def, bool required, const std::string& label = "") {
    const auto found = fields.find(key);
    if (found != fields.end() && !found->second.empty()) return found->second;
    std::string value = def;
    if (session.tty) {
      prompted_any = true;
      std::lock_guard<std::mutex> guard(session.print);
      std::cout << (label.empty() ? key : label) << " [" << (def.empty() ? "-" : def) << "]: " << std::flush;
      std::string line;
      std::getline(std::cin, line);
      if (!line.empty()) value = line;
    }
    if (value.empty() && required) throw std::runtime_error(verb + " needs " + key + "=");
    fields[key] = value;
    return value;
  };
  auto prompt_field = [&](const std::string& key, const std::string& def, bool required, const std::string& label = "") { return prompt_into(args, key, def, required, label); };
  // Order-entry discipline (login precedent, ADR-052): presets must never
  // silently decide a ticket on a tty. Unless explicitly typed as k=v, the
  // trader is asked with the preset as default (Enter keeps). Scripts keep
  // the preset fill above, fail closed when empty.
  auto explicit_key = [&](const std::string& key) {
    for (const auto& piece : tokens) {
      if (piece.rfind(key + "=", 0) == 0) return true;
    }
    return false;
  };
  auto confirm_field = [&](const std::string& key, bool required) {
    std::string current = text_for(args, key);
    if (!session.tty || explicit_key(key)) {
      if (current.empty() && required) throw std::runtime_error(verb + " needs " + key + "=");
      return current;
    }
    prompted_any = true;
    std::lock_guard<std::mutex> guard(session.print);
    std::cout << key << " [" << (current.empty() ? "-" : current) << "]: " << std::flush;
    std::string line;
    std::getline(std::cin, line);
    if (!line.empty()) {
      args[key] = line;
      return line;
    }
    if (current.empty() && required) throw std::runtime_error(verb + " needs " + key + "=");
    return current;
  };
  auto confirm_ticket = [&](const std::string& summary) {
    if (!prompted_any || !session.tty) return true;
    std::lock_guard<std::mutex> guard(session.print);
    std::cout << summary << '\n' << "confirm? [y/N]: " << std::flush;
    std::string line;
    std::getline(std::cin, line);
    if (line != "y" && line != "Y") {
      std::cout << "cancelled\n";
      return false;
    }
    return true;
  };
  if (verb == "login") {
    // Account discipline (ADR-050/052): explicit account= (or a bare token)
    // always wins; otherwise a tty prompts with the preset as default so a
    // bare `login` never silently authenticates as the preset account.
    // Scripts (non-tty) keep the preset fill above, fail closed when empty.
    bool explicit_account = false;
    for (const auto& piece : tokens) {
      if (piece.rfind("account=", 0) == 0 || piece.find('=') == std::string::npos) {
        explicit_account = true;
        break;
      }
    }
    std::string account = text_for(args, "account");
    if (!explicit_account && session.tty) {
      std::lock_guard<std::mutex> guard(session.print);
      std::cout << "account [" << (account.empty() ? "-" : account) << "]: " << std::flush;
      std::string line;
      std::getline(std::cin, line);
      if (!line.empty()) account = line;
      args["account"] = account;
    }
    // Password discipline: explicit password= for scripts, else $STS_GW_PW,
    // else a tty hidden prompt. Never echoed, never logged.
    std::string password = text_for(args, "password");
    if (password.empty()) {
      const char* env = std::getenv("STS_GW_PW");
      if (env != nullptr) password = env;
    }
    if (password.empty()) {
      if (!session.tty) throw std::runtime_error("login needs password= or $STS_GW_PW");
      std::lock_guard<std::mutex> guard(session.print);
      std::cout << "password for " << (account.empty() ? "-" : account) << ": " << std::flush;
      password = read_secret("password: ");
    }
    if (text_for(args, "account").empty()) throw std::runtime_error("login needs account=");
    if (password.empty()) throw std::runtime_error("login needs a non-empty password");
    nlohmann::json body = nlohmann::json::object();
    body["op"] = "login";
    body["account"] = text_for(args, "account");
    body["password"] = password;
    print_reply(session, verb, roundtrip(session, body));
    return true;
  }
  // 查询账户 dashboard: 资金 + 持仓(逐策略) + 今日委托 + 今日成交，一屏看完。
  if (verb == "account") {
    prompt_field("account", "", true);
    print_reply(session, "cash", roundtrip(session, command_body("cash", args)));
    const nlohmann::json listed = roundtrip(session, command_body("list", args));
    checked_reply(listed);
    std::istringstream lines(listed["data"].value("strategies", std::string{}));
    std::string row;
    while (std::getline(lines, row)) {
      std::istringstream words(row);
      std::string sid;
      if (!(words >> sid) || sid == "no") continue;  // "no live strategies"
      std::map<std::string, std::string> query = args;
      query["strategy"] = sid;
      print_reply(session, "positions", roundtrip(session, command_body("positions", query)));
    }
    print_reply(session, "orders", roundtrip(session, command_body("orders", args)));
    print_reply(session, "fills", roundtrip(session, command_body("fills", args)));
    return true;
  }
  // 现货买卖 shortcut: side 定死，价格类型默认限价，单号自动编.
  // tty 缺参数逐项提示（demo 式交互），齐参直发；脚本行为不变.
  if (verb == "buy" || verb == "sell") {
    args["side"] = verb;
    if (text_for(args, "type").empty()) args["type"] = "limit";
    confirm_field("account", false);
    confirm_field("instrument", true);
    prompt_field("instrument", "", true);
    prompt_field("quantity", "100", true);
    prompt_field("price", "", true, "price(定点整数,4.45元=44500)");
    if (number_for(args, "quantity") <= 0) throw std::runtime_error(verb + " needs quantity=");
    if (number_for(args, "price") <= 0) throw std::runtime_error(verb + " needs price=");
    if (text_for(args, "id").empty()) args["id"] = auto_order_id(session, verb == "buy" ? "b" : "s");
    if (!confirm_ticket("ticket: " + verb + " " + args["quantity"] + " " + args["instrument"] + " @ " + args["price"] + " (" + args["id"] + ")")) return true;
    print_reply(session, "submit", roundtrip(session, command_body("submit", args)));
    return true;
  }
  // 统一现货买卖：side 作输入，价格类型默认限价.
  if (verb == "trade") {
    const std::string side = prompt_field("side", "", false);
    if (side != "buy" && side != "sell") throw std::runtime_error("trade needs side=buy|sell");
    args["side"] = side;
    if (text_for(args, "type").empty()) args["type"] = "limit";
    confirm_field("account", false);
    confirm_field("instrument", true);
    prompt_field("instrument", "", true);
    prompt_field("quantity", "100", true);
    prompt_field("price", "", true, "price(定点整数,4.45元=44500)");
    if (number_for(args, "quantity") <= 0) throw std::runtime_error("trade needs quantity=");
    if (number_for(args, "price") <= 0) throw std::runtime_error("trade needs price=");
    if (text_for(args, "id").empty()) args["id"] = auto_order_id(session, "t");
    if (!confirm_ticket("ticket: " + side + " " + args["quantity"] + " " + args["instrument"] + " @ " + args["price"] + " (" + args["id"] + ")")) return true;
    print_reply(session, "submit", roundtrip(session, command_body("submit", args)));
    return true;
  }
  // 策略二级菜单: strategy grid ... (strategy_type 固定 grid).
  if (verb == "strategy") {
    const std::string sub = prompt_field("sub", "grid", false);
    if (sub != "grid") throw std::runtime_error("strategy needs sub=grid");
    args["strategy_type"] = "grid";
    if (text_for(args, "id").empty()) args["id"] = auto_order_id(session, "g");
    confirm_field("account", true);
    confirm_field("instrument", true);
    prompt_field("account", "", true);
    prompt_field("instrument", "", true);
    prompt_field("quantity", "100", true);
    prompt_field("lower", "", true);
    prompt_field("upper", "", true);
    prompt_field("grids", "8", true);
    prompt_field("start", args["lower"], true);
    prompt_field("stock", "0", true);
    if (!confirm_ticket("ticket: start " + args["id"] + " grid " + args["account"] + " " + args["instrument"])) return true;
    print_reply(session, "start", roundtrip(session, command_body("start", args)));
    return true;
  }
  // Guided quick-trade menu: quick strategy|buy|sell. Missing fields are
  // prompted interactively (tty, defaults shown, Enter accepts); piped
  // scripts must pass every field as k=v plus confirm=y (fail closed).
  if (verb == "quick") {
    const std::string menu = prompt_field("menu", "", false);
    // quick: presets pre-fill prompted defaults (explicit k=v wins).
    for (const auto& preset : session.quick_defaults) {
      if (args[preset.first].empty()) args[preset.first] = preset.second;
    }
    if (menu == "strategy") {
      args["strategy_type"] = prompt_field("strategy_type", "grid", true);
      args["id"] = prompt_field("id", auto_order_id(session, "quick-" + args["strategy_type"]), true);
      confirm_field("account", true);
      confirm_field("instrument", true);
      prompt_field("quantity", "100", true);
      if (args["strategy_type"] == "grid") {
        prompt_field("lower", "", true);
        prompt_field("upper", "", true);
        prompt_field("grids", "8", true);
        prompt_field("start", args["lower"], true);
      }
      prompt_field("stock", "0", true);
      if (number_for(args, "stock") > 0) prompt_field("stock_avg", "0", true);
      std::cout << "ticket: start " << args["id"] << " " << args["strategy_type"] << " " << args["account"] << " " << args["instrument"] << '\n';
      if (session.tty) {
        std::cout << "confirm? [y/N]: " << std::flush;
        std::string line;
        std::getline(std::cin, line);
        if (line != "y" && line != "Y") {
          std::cout << "cancelled\n";
          return true;
        }
      } else if (text_for(args, "confirm") != "y") {
        throw std::runtime_error("quick strategy needs confirm=y in scripts");
      }
      print_reply(session, "start", roundtrip(session, command_body("start", args)));
      return true;
    }
    if (menu == "buy" || menu == "sell") {
      args["side"] = menu;
      args["id"] = prompt_field("id", auto_order_id(session, "q"), true);
      confirm_field("account", true);
      prompt_field("strategy", "", false);
      confirm_field("instrument", true);
      prompt_field("quantity", "100", true);
      args["type"] = "limit";
      prompt_field("price", "", true, "price(定点整数,4.45元=44500)");
      args["exchange"] = prompt_field("exchange", "sse", false);
      args["security"] = prompt_field("security", "", false);
      std::cout << "ticket: " << menu << " " << args["quantity"] << " " << args["instrument"] << " @ " << args["price"] << " (" << args["id"] << ")\n";
      if (session.tty) {
        std::cout << "confirm? [y/N]: " << std::flush;
        std::string line;
        std::getline(std::cin, line);
        if (line != "y" && line != "Y") {
          std::cout << "cancelled\n";
          return true;
        }
      } else if (text_for(args, "confirm") != "y") {
        throw std::runtime_error("quick buy/sell needs confirm=y in scripts");
      }
      print_reply(session, "submit", roundtrip(session, command_body("submit", args)));
      return true;
    }
    throw std::runtime_error("quick needs menu=strategy|buy|sell");
  }
  // Guided queries: tty prompts for missing filters (Enter = unfiltered,
  // same wire as today); scripts pass k=v as before. Presets do not apply
  // here (same as the previous fallthrough: explicit filters only).
  if (verb == "cash" || verb == "orders") {
    std::map<std::string, std::string> query = parse_kv(tokens, verb);
    prompt_into(query, "account", "", false);
    print_reply(session, verb, roundtrip(session, command_body(verb, query)));
    return true;
  }
  if (verb == "positions") {
    std::map<std::string, std::string> query = parse_kv(tokens, verb);
    prompt_into(query, "strategy", "", session.tty);  // tty must name a basket; scripts keep the passthrough default
    prompt_into(query, "account", "", false);
    print_reply(session, verb, roundtrip(session, command_body(verb, query)));
    return true;
  }
  if (verb == "fills") {
    std::map<std::string, std::string> query = parse_kv(tokens, verb);
    prompt_into(query, "account", "", false);
    prompt_into(query, "strategy", "", false);
    prompt_into(query, "limit", "", false);
    print_reply(session, verb, roundtrip(session, command_body(verb, query)));
    return true;
  }
  if (verb == "progress" || verb == "order" || verb == "cancel" || verb == "stop") {
    std::map<std::string, std::string> query = parse_kv(tokens, verb);
    prompt_into(query, "id", "", true);
    print_reply(session, verb, roundtrip(session, command_body(verb, query)));
    return true;
  }
  if (verb == "watch") {
    prompt_field("what", "", false);
    const std::string what = text_for(args, "what");
    if (what != "cash" && what != "orders" && what != "positions" && what != "fills" && what != "progress") {
      throw std::runtime_error("watch needs what=cash|orders|positions|fills|progress");
    }
    std::int64_t interval = number_for(args, "interval");
    if (interval <= 0) interval = 2;
    std::int64_t count = number_for(args, "count");
    if (count <= 0 && text_for(args, "count").empty()) count = 5;
    std::int64_t shown = 0;
    while (count == 0 || shown < count) {
      std::map<std::string, std::string> query = args;
      query.erase("what");
      query.erase("interval");
      query.erase("count");
      print_reply(session, what, roundtrip(session, command_body(what, query)));
      ++shown;
      if (count != 0 && shown >= count) break;
      std::this_thread::sleep_for(std::chrono::seconds{interval});
    }
    return true;
  }
  if (verb == "sub" || verb == "unsub") {
    const auto base = parse_kv(tokens, verb);
    std::map<std::string, std::string> args = base;
    prompt_into(args, verb == "sub" ? "what" : "id", "", false);
    if (verb == "sub") {
      const std::string what = text_for(args, "what");
      if (what != "quotes" && what != "reports" && what != "strategies") throw std::runtime_error("sub needs what=quotes|reports|strategies");
      nlohmann::json body = nlohmann::json::object();
      body["op"] = "subscribe_" + what;
      const nlohmann::json reply = roundtrip(session, body);
      checked_reply(reply);
      const auto sub = static_cast<std::uint64_t>(reply["data"].value("sub", 0LL));
      session.subs.push_back(sub);
      {
        std::lock_guard<std::mutex> guard(session.print);
        std::cout << "ok: subscribed " << what << " sub=" << sub << '\n';
      }
    } else {
      const std::string target = text_for(args, "id");
      if (target == "all") {
        for (const auto sub : session.subs) {
          nlohmann::json body = nlohmann::json::object();
          body["op"] = "unsubscribe";
          body["sub"] = sub;
          checked_reply(roundtrip(session, body));
        }
        session.subs.clear();
        {
          std::lock_guard<std::mutex> guard(session.print);
          std::cout << "ok: unsubscribed all\n";
        }
      } else {
        nlohmann::json body = nlohmann::json::object();
        body["op"] = "unsubscribe";
        body["sub"] = number_for(args, "id");
        checked_reply(roundtrip(session, body));
        {
          std::lock_guard<std::mutex> guard(session.print);
          std::cout << "ok: unsubscribed " << target << '\n';
        }
      }
    }
    return true;
  }
  print_reply(session, verb, roundtrip(session, command_body(verb, parse_kv(tokens, verb))));
  return true;
}

}  // namespace

// sts_cli (P2): resident downstream shell over one gateway session.
// Login is per-connection via the login verb (password by $STS_GW_PW,
// explicit password=, or a tty hidden prompt; never logged); stdin is the
// script (piped runs are deterministic apart from push interleaving).
int main(int argc, char** argv) {
  CliOptions opts{};
  if (!parse_cli_args(argc, argv, opts)) {
    usage();
    return 2;
  }
  if (opts.socket.empty() && opts.tcp.empty()) {
    // No endpoint on argv: fall back to <exe-dir>/cfg/sts_app.yaml, so
    // bare ./sts_cli works straight from bin/.
    std::string self = argv[0];
    const std::size_t mark = self.find_last_of("/\\");
    const std::string exe_dir = mark == std::string::npos ? std::string{} : self.substr(0, mark);
    const std::string app_path = exe_dir.empty() ? "cfg/sts_app.yaml" : exe_dir + "/cfg/sts_app.yaml";
    std::string error;
    if (!load_app_defaults(app_path, opts, error)) {
      std::cerr << error << '\n';
      usage();
      return 2;
    }
  }
  const bool tty = stdin_is_tty();
  try {
    Session session{dial_session(opts), opts, tty};
    session.pump.start();
    {
      std::lock_guard<std::mutex> guard(session.print);
      std::cout << "connected (login when the server requires it)" << '\n';
    }
    bool running = true;
    std::string line;
    while (running) {
      if (tty) {
        std::lock_guard<std::mutex> guard(session.print);
        std::cout << opts.prompt << std::flush;
      }
      if (!std::getline(std::cin, line)) break;
      // No print lock across run_line: roundtrip blocks, and the pump needs
      // the same lock to print pushes (holding it here would starve reads
      // into socket back-pressure and fake every reply timeout).
      try {
        running = run_line(session, line);
      } catch (const std::exception& command_failure) {
        std::lock_guard<std::mutex> guard(session.print);
        std::cout << "error: " << command_failure.what() << '\n';
      }
    }
    session.pump.stop();
  } catch (const std::exception& e) {
    std::cerr << "sts_cli: " << e.what() << '\n';
    return 2;
  }
  return 0;
}
