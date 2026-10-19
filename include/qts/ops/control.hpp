// clang-format off
/**
  ******************************************************************************
  * @file           : control.hpp
  * @author         : vivi wu
  * @brief          : Operator control plane: file-inbox commands and dispatch.
  * @version        : 0.1.0
  * @date           : 03/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "qts/config/config.hpp"
#include "qts/core/types.hpp"
#include "qts/execution/execution_api.hpp"
#include "qts/oms/order_manager.hpp"
#include "qts/risk/risk_engine.hpp"
#include "qts/strategy/strategy_api.hpp"

namespace qts::control {

// Operator control plane (M11-4, zero IPC).
//
// The running server polls an append-only inbox file (one JSON object per
// line, see format_command); qts_admin send appends commands while the
// server runs. Order commands flow through the same OMS/execution paths as
// the shutdown fan-out, so cancels land in the ledger audit trail like any
// other cancel. Switch commands (block/halt) take effect in the risk engine
// immediately and are audit-logged as structured log lines; they have no
// ledger event type yet (documented gap, see runbook).
//
// Supported cmd values:
//   cancel <id>              withdraw one order (parent fans out, else direct)
//   cancel_all               withdraw every working order
//   block_strategy <id>      / unblock_strategy <id>
//   block_instrument <id>    / unblock_instrument <id>
//   block_account <id>       / unblock_account <id>
//   halt / resume            global trading switch
//   replace <id> <quantity> [price]  re-plan a parent via any live child id
//     (price 0 keeps the child price; the parent id is taken from the child)
//   order <id> <account> <instrument> <buy|sell> <limit|market> <quantity>
//     <price> <sse|szse|bse> <security-account> [style]
//     submit one manual order straight into OMS (same synchronous risk gate
//     as every other submit; empty strategy normalizes to passthrough).
//     price is the limit price, or the protection price for market orders
//     (0 keeps main-board market orders unprotected). style names a market
//     order style and is required exactly for market orders.
//   strategy_start <id> <grid|rebalance> <account> <instrument> <lower>
//     <upper> <grids> <quantity> [start]
//     create a live strategy instance from trader input (grid bounds are
//     Price ticks, quantity is shares; rebalance ignores lower/upper/grids
//     and reads its targets from the server config). The instance joins the
//     per-tick fan-out with a unique id prefix immediately.
//   strategy_stop <id>    block the strategy in risk and withdraw its open
//     parents through the execution algorithm (same fan-out as shutdown).
//   strategy_list         list live strategy instances (message only).
struct ControlCommand {
  std::string name;
  std::string id;
  Quantity quantity{0};
  Price price{0};
  std::string account;
  std::string instrument;
  std::string side;
  std::string order_type;
  std::string exchange;
  std::string security_account;
  std::string style;
  std::string strategy_type;
  Price lower{0};
  Price upper{0};
  int grids{0};
  Price start{0};
};

struct ControlResult {
  bool ok{false};
  std::string message;
};

// Strict parse of one inbox line: unknown cmd, missing id, or non-positive
// replace quantity fails (fail closed, loudly). Manual orders carry their
// full ticket (id/account/instrument/side/type/quantity/price/exchange/
// security account, plus style for market orders). Returns false with reason.
[[nodiscard]] bool parse_command(const std::string& line, ControlCommand& command, std::string& error);
// Canonical one-line encoding consumed by parse_command (admin send writes it).
[[nodiscard]] std::string format_command(const ControlCommand& command);

// Live-trading authorization (ADR-051/053): no approval files, no configured
// passwords. The trader passes identity+password at gateway login and the
// counter session builds lazily (v1: one session per process, surviving
// gateway logout); headless automation pre-logs in from gateway-side env
// through the same path. Old approval files are inert data.

// Live strategy registry: trader-created instances join the same per-tick
// fan-out vector the yaml-configured strategies run in. The gateway owns
// the vector; the registry only appends/erases through it (single loop
// thread, no extra locking). Id prefixes carry a per-strategy generation so
// a restarted instance never reuses a previous run's parent numbering.
class StrategyRegistry {
 public:
  using Factory = std::function<std::unique_ptr<Strategy>(const StrategyConfig& configured, const std::string& id_prefix)>;
  StrategyRegistry(std::vector<std::unique_ptr<Strategy>>& live, Factory factory, std::string prefix_seed);
  // Adopts a yaml-configured instance already in the fan-out vector (no
  // creation, descriptor only) so list/stop/duplicate checks see it.
  void adopt(const StrategyConfig& configured, Strategy* instance);
  // Starts one instance from trader input; rejects duplicates, unknown
  // types and inconsistent grid ranges (fail closed, loudly).
  ControlResult start(const StrategyConfig& configured);
  // Blocks the strategy in risk, withdraws its open parents through the
  // execution algorithm, then detaches the instance. Unknown ids fail.
  ControlResult stop(const StrategyId& strategy_id, OrderManager& oms, ExecutionAlgorithm& execution, RiskEngine& risk);
  // One "id type account instrument" line per live instance (message only).
  [[nodiscard]] std::string list() const;
  // Owning account of a live instance for the gateway account gate
  // (ADR-050): false when the id is unknown.
  [[nodiscard]] bool account_of(const StrategyId& strategy_id, AccountId& account) const;
  // Live ids for per-tick risk-basket sync (T+1 sellable must see runtime
  // baskets, not just the yaml-configured ones).
  [[nodiscard]] std::vector<StrategyId> active_ids() const;

 private:
  struct Entry {
    StrategyConfig config;
    Strategy* instance{nullptr};
  };
  // Non-owning borrowed views: the gateway owns the fan-out vector (same
  // pattern as ControlContext above); the registry only appends/erases.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  std::vector<std::unique_ptr<Strategy>>& live_;
  Factory factory_;
  std::string prefix_seed_;
  std::unordered_map<StrategyId, std::size_t> generations_;
  std::vector<Entry> entries_;
};

struct ControlContext {
  // Non-owning borrowed views (mandatory injected dependencies, same pattern
  // as SimulatedBroker::events_): the gateway owns all three lifetimes.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  OrderManager& oms;
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  ExecutionAlgorithm& execution;
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  RiskEngine& risk;
  // Optional live-strategy registry (strategy_start/stop/list need it; null
  // keeps every previously constructed context compiling and working).
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  StrategyRegistry* strategies{nullptr};
  // Optional switch-command journal (M15): successful halt/block/unblock
  // applications append a control_applied event here. Null keeps every
  // previously constructed context compiling and silent; the member stays
  // last so existing aggregate initializers keep working.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  EventLedger* ledger{nullptr};
};

// Executes a parsed command against live state. Never throws for unknown
// order ids (reported as ok=false); programmer errors (empty parent links)
// are reported the same way, never fatal to the server loop.
ControlResult apply_command(const ControlCommand& command, ControlContext& context);

// Withdraws every working order: parents fan out through the execution
// algorithm first (unknown parents tolerated per ADR-024), stragglers and
// parentless orders fall through to the direct sweep. Returns withdrawn
// order count for summaries.
std::size_t cancel_all_open(OrderManager& oms, ExecutionAlgorithm& execution);

// M13 safety-net drain: replays risk-queued auto actions through the same
// paths as operator commands (strategy halts as strategy_stop = block +
// withdraw + detach, global halts withdraw everything). Recovery stays
// manual (unblock + strategy restart). Returns a one-line human summary,
// empty when the queue was idle (the common case: no log spam).
std::string apply_auto_actions(ControlContext& context);

}  // namespace qts::control
