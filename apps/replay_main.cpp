// clang-format off
/**
  ******************************************************************************
  * @file           : replay_main.cpp
  * @author         : vivi wu
  * @brief          : sts_replay offline replay CLI: ledger report and strict double-rebuild verify.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "sts/ops/log.hpp"
#include "sts/persistence/file_ledger.hpp"
#include "sts/research/ledger_verify.hpp"
#include "sts/research/report.hpp"

namespace {

struct Options {
  std::string command;
  std::string ledger;
  std::string config_version{"1"};
  std::string rules_version{"1"};
  std::string log_file;
  std::string log_level{"info"};
};

bool parse_args(int argc, char** argv, Options& out) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--ledger" && i + 1 < argc) {
      out.ledger = argv[++i];
    } else if (arg == "--config-version" && i + 1 < argc) {
      out.config_version = argv[++i];
    } else if (arg == "--rules-version" && i + 1 < argc) {
      out.rules_version = argv[++i];
    } else if (arg == "--log-file" && i + 1 < argc) {
      out.log_file = argv[++i];
    } else if (arg == "--log-level" && i + 1 < argc) {
      out.log_level = argv[++i];
    } else if (!arg.empty() && arg[0] != '-' && out.command.empty()) {
      out.command = arg;
    } else {
      return false;
    }
  }
  return !out.command.empty() && !out.ledger.empty();
}

void usage() {
  std::cerr << "usage: sts_replay <report|verify> --ledger FILE [--config-version V --rules-version V] [--log-file PAT] [--log-level L]\n"
               "  report: RunReport over the ledger file (pure function): fills, gross, fees, net, drawdown, versions.\n"
               "  verify: rebuild OMS twice from the file; every event must apply legally and both\n"
               "          rebuilds must converge field-by-field (M2 replay-consistency acceptance).\n";
}

// M9-4: offline reader over the append-only JSONL file, same race-free
// property as sts_admin (safe while a gateway appends: the snapshot is
// taken at open).
std::vector<sts::LedgerEvent> load_ledger(const std::string& path) {
  sts::FileEventLedger ledger{path};
  return ledger.replay(1);
}

int run_report(const Options& opts, sts::log::Logger& log) {
  const auto events = load_ledger(opts.ledger);
  const sts::RunReport report = sts::summarize_ledger(events, opts.config_version, opts.rules_version);
  std::cout << "ledger: events=" << events.size() << '\n';
  std::cout << "submitted=" << report.submitted << " filled=" << report.filled << " cancelled=" << report.cancelled << " rejected=" << report.rejected << " fills=" << report.fills << '\n';
  std::cout << "bought=" << report.bought_quantity << " sold=" << report.sold_quantity << " gross_buy=" << report.gross_buy << " gross_sell=" << report.gross_sell << " gross_pnl=" << report.gross_pnl
            << " fees=" << report.total_fees << " net_pnl=" << report.net_pnl << " drawdown=" << report.max_drawdown << '\n';
  std::cout << "versions_match=" << (report.versions_match ? "true" : "false") << " (expected " << opts.config_version << "/" << opts.rules_version << ")\n";
  for (const auto& reason : report.rejection_reasons) {
    std::cout << "rejected: " << reason.first << " count=" << reason.second << '\n';
  }
  std::cout << "report ok\n";
  log.info("replay", "report finished",
           {{"ledger_events", std::to_string(events.size())},
            {"filled", std::to_string(report.filled)},
            {"gross_pnl", std::to_string(report.gross_pnl)},
            {"fees", std::to_string(report.total_fees)},
            {"net_pnl", std::to_string(report.net_pnl)},
            {"versions_match", report.versions_match ? "true" : "false"}});
  return 0;
}

int run_verify(const Options& opts, sts::log::Logger& log) {
  const auto events = load_ledger(opts.ledger);
  const sts::replay::VerifyResult result = sts::replay::verify_ledger_replay(events);
  std::cout << "verify: events=" << result.events << " orders=" << result.orders << " open=" << result.open << '\n';
  if (!result.ok) {
    std::cerr << "verify FAILED: " << result.error << '\n';
    log.error("replay", "verify failed", {{"ledger_events", std::to_string(result.events)}, {"error", result.error}});
    return 1;
  }
  std::cout << "verify ok: all events apply legally, two rebuilds converge\n";
  log.info("replay", "verify finished", {{"ledger_events", std::to_string(result.events)}, {"orders", std::to_string(result.orders)}, {"open", std::to_string(result.open)}});
  return 0;
}

}  // namespace

// sts_replay: offline replayer over ledger files (no broker, no IPC).
int main(int argc, char** argv) {
  Options opts{};
  if (!parse_args(argc, argv, opts)) {
    usage();
    return 2;
  }
  sts::log::Level minimum = sts::log::Level::info;
  if (!sts::log::parse_level(opts.log_level, minimum)) {
    std::cerr << "sts_replay: bad --log-level '" << opts.log_level << "' (want debug/info/warn/error)\n";
    return 2;
  }
  sts::log::set_level(minimum);
  try {
    sts::log::Logger log{opts.log_file};
    if (opts.command == "report") return run_report(opts, log);
    if (opts.command == "verify") return run_verify(opts, log);
  } catch (const std::exception& e) {
    std::cerr << "sts_replay: " << e.what() << '\n';
    return 2;
  }
  usage();
  return 2;
}
