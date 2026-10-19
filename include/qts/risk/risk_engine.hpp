// clang-format off
/**
  ******************************************************************************
  * @file           : risk_engine.hpp
  * @author         : vivi wu
  * @brief          : Pre-trade risk checks, limits and four-level kill switches (fail-closed).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "qts/core/clock.hpp"
#include "qts/core/types.hpp"
#include "qts/market/ashare.hpp"

namespace qts {

struct RiskLimits {
  Quantity max_order_quantity{0};
  Price max_order_notional{0};
  bool trading_enabled{true};
  // Kept last so existing {qty, notional, enabled} initializers keep working.
  Quantity max_position_quantity{0};  // on resulting |net|; 0 means no limit
  // Sliding-window submission throttle; 0/0 disables. The engine needs a
  // live clock, wired by the gateway from this config.
  std::size_t rate_max_orders{0};
  std::int64_t rate_window_ms{0};
  // T+1 sellable enforcement for sells (off by default: paper strategies
  // like the drill grid assume starting inventory; enable for strict
  // spot semantics once opening inventory is funded).
  bool enforce_sellable{false};
  // Daily loss limit on day Arithmetic PnL (0 disables). Breached days
  // reject order opens but still allow risk-reducing closes; the gateway
  // feeds day PnL and resets the baseline on rollover.
  Price daily_loss_limit{0};
  // Buying-power enforcement for buys (off by default): buy notional must
  // fit inside synced cash minus live (unfilled) buy exposure. Fee cushion
  // is not modeled; market orders carry no limit price and skip sizing.
  bool enforce_cash{false};
  // M13 safety net (all 0 disables): consecutive risk/broker rejects per
  // strategy auto-block it; day reject-rate and day loss-ratio halt all
  // trading. All three ride set_limits, so SIGHUP reloads apply them.
  int max_consecutive_rejects{3};   // per strategy; 0 disables
  int max_reject_rate_bps{3000};    // day rejects per 10k submits; 0 disables
  int reject_rate_min_orders{100};  // day submits before the rate gate arms
  int max_loss_ratio_bps{3000};     // day drawdown per 10k of day-start equity
};

// Initial kill-switch state from config (operational toggles; the engine
// also exposes runtime setters). Empty lists block nothing.
struct RiskSwitches {
  std::vector<StrategyId> blocked_strategies;
  std::vector<AccountId> blocked_accounts;
  std::vector<InstrumentId> blocked_instruments;
};

struct RiskDecision {
  bool allowed{false};
  std::string reason;
};

// M13 safety-net action (risk decides, the gateway acts): the engine only
// ever sets blocks internally and queues these for the tick driver, which
// withdraws working orders through the existing cancel paths. Recovery is
// always manual (unblock + strategy restart); the engine never unblocks.
enum class AutoActionKind : std::uint8_t { halt_strategy, halt_global };

struct AutoAction {
  AutoActionKind kind{AutoActionKind::halt_global};
  StrategyId strategy_id;  // set for halt_strategy, empty for halt_global
  std::string reason;
};

struct PriceBand {
  Price limit_down{0};  // <= 0 means no lower bound
  Price limit_up{0};    // <= 0 means no upper bound
};

class RiskEngine {
 public:
  explicit RiskEngine(RiskLimits limits);
  // Generic pre-trade gate. Order of evaluation: global, strategy, account
  // and instrument switches; identity; quantity; limit-price positivity;
  // price band; quantity/notional caps; position cap; allowlist; rate.
  // The rate quota counts only orders that passed every earlier gate.
  // A-share venue rules are enforced inline when the instrument has an
  // entry in the injected master (unknown instruments fail closed once the
  // master is non-empty; an empty master keeps the legacy venue-agnostic
  // path so existing drills stay green).
  [[nodiscard]] RiskDecision check(const OrderIntent& intent) const;
  // Generic gate plus the injected A-share ruleset. Fail-closed: rejects
  // when no ruleset has been configured.
  [[nodiscard]] RiskDecision check_ashare(const OrderIntent& intent) const;
  void set_trading_enabled(bool enabled) noexcept;
  // Wholesale limit replacement for SIGHUP config reloads (atomic from the
  // checker's view: single assignment, no torn reads on this thread model).
  void set_limits(RiskLimits limits) noexcept;
  void set_strategy_blocked(const StrategyId& strategy_id, bool blocked);
  void set_account_blocked(const AccountId& account_id, bool blocked);
  void set_instrument_blocked(const InstrumentId& instrument_id, bool blocked);
  // Wholesale switch replacement (clears first): removed-from-config ids
  // unblock, which per-id setters alone cannot express.
  void set_switches(std::unordered_set<StrategyId> strategies, std::unordered_set<AccountId> accounts, std::unordered_set<InstrumentId> instruments);
  // Versioned instrument rules are injected, never hard-coded. Clearing
  // returns check_ashare to fail-closed mode.
  void set_ashare_rules(AshareInstrumentRules rules);
  void clear_ashare_rules();
  // Multi-instrument master for the generic check() path (M11-1): one entry
  // per tradeable symbol. Empty master disables venue enforcement.
  void set_ashare_master(std::unordered_map<InstrumentId, AshareInstrumentRules> master);
  void clear_ashare_master();
  [[nodiscard]] bool has_ashare_master() const noexcept { return !ashare_master_.empty(); }

  // Static venue bands (refreshed from the instrument master by the owner).
  // Limit orders on a banded instrument must trade inside the band.
  void set_price_band(InstrumentId instrument_id, PriceBand band);
  void clear_price_band(const InstrumentId& instrument_id);
  // Signed net position per instrument, fed from portfolio/broker snapshots.
  void set_position(InstrumentId instrument_id, Quantity net_quantity);
  // Day Arithmetic PnL for the daily loss limit (gateway-fed from the book
  // relative to the rollover baseline). Zero/unset means no loss yet.
  void set_day_pnl(Price day_pnl);
  // M13 safety-net outcome feed (called by the OMS submit/report paths with
  // the normalized strategy id): every attempt counts a day submit; every
  // refusal (risk gate, submit transport, broker report) counts a day reject
  // and extends the strategy's consecutive streak, while an accept clears
  // the streak. Threshold breaches block internally and queue AutoActions.
  void note_submit_accepted(const StrategyId& strategy);
  void note_submit_rejected(const StrategyId& strategy);
  void note_broker_reject(const StrategyId& strategy);
  // Day-start net-asset baseline for the loss-ratio gate (gateway-fed from
  // book equity at startup/rollover). Non-positive baselines disable the
  // ratio gate (a ratio over nothing is undefined, never a halt).
  void set_equity_base(Price base);
  // Current net-asset equity (gateway-fed every sync). Breaching the ratio
  // halts all trading and queues the global action.
  void set_equity(Price current);
  // New trading day: day submit/reject counters and per-strategy streaks
  // reset. Blocks (manual or automatic) persist until explicitly lifted:
  // surviving a rollover never silently re-arms a stopped strategy.
  void settle_day();
  // Drains queued auto actions (cleared on read). The gateway acts on them
  // once per tick, after the submit phase.
  [[nodiscard]] std::vector<AutoAction> drain_auto_actions();
  // T+1 sellable inventory, scoped per strategy basket (pod-level
  // discipline, stricter than the broker's account gate): a strategy may
  // only sell what is sellable inside its own basket, so concurrent live
  // sells from different strategies can never pool the same inventory.
  // Enforced only when RiskLimits::enforce_sellable is set. Empty strategy
  // normalizes to passthrough, matching OMS entry normalization.
  using SellKey = std::pair<StrategyId, InstrumentId>;
  using SellMap = std::map<SellKey, Quantity>;
  void set_sellable(const StrategyId& strategy, const InstrumentId& instrument, Quantity sellable_quantity);
  // True when sell reservation matters: T+1 enforcement on. The OMS submit
  // path consults it to skip the working-set scan otherwise (hot-path cost).
  [[nodiscard]] bool sell_reservation_active() const noexcept { return limits_.enforce_sellable; }
  // Buying-power state: synced cash plus live buy-exposure reservation
  // (same absolute-set discipline as sells). Unknown cash counts as zero
  // once enforcement is on (fail-closed); the gateway syncs every tick.
  void set_cash(Price cash);
  void set_buy_reserved(Price reserved_notional);
  [[nodiscard]] bool cash_reservation_active() const noexcept { return limits_.enforce_cash; }
  // Live (unfilled) sell exposure per instrument, refreshed from the OMS
  // working set every sync: concurrent live sells each sized against the
  // same sellable would otherwise overshoot on fill. Absolute set (not
  // incremental), so fills/cancels release automatically on the next sync.
  void set_sell_reserved(SellMap reserved);
  // Non-empty allowlist rejects everything not listed. Empty allows all.
  void set_instrument_allowlist(std::unordered_set<InstrumentId> allowlist);
  // Attempt-based rate gate over a sliding window. Disabled when max_orders
  // is 0 or clock is null; the clock must outlive the engine.
  void set_rate_limit(std::size_t max_orders, std::chrono::nanoseconds window, const Clock* clock);

 private:
  [[nodiscard]] RiskDecision check_switches(const OrderIntent& intent) const;
  [[nodiscard]] RiskDecision check_shape(const OrderIntent& intent) const;
  [[nodiscard]] RiskDecision check_band(const OrderIntent& intent) const;
  [[nodiscard]] RiskDecision check_notional(const OrderIntent& intent) const;
  [[nodiscard]] RiskDecision check_position(const OrderIntent& intent) const;
  [[nodiscard]] RiskDecision check_sellable(const OrderIntent& intent) const;
  [[nodiscard]] RiskDecision check_cash(const OrderIntent& intent) const;
  [[nodiscard]] RiskDecision check_loss_limit(const OrderIntent& intent) const;
  [[nodiscard]] RiskDecision check_venue(const OrderIntent& intent) const;
  [[nodiscard]] RiskDecision check_rate() const;
  // M13 safety-net evaluation (non-const: blocks and queues on breach).
  // Called after every counted outcome; check() itself stays a pure reader
  // over the resulting blocks, so the pre-trade gate never mutates.
  void evaluate_reject_gates(const StrategyId& strategy);
  void evaluate_loss_gate();

  RiskLimits limits_;
  std::unordered_set<StrategyId> blocked_strategies_;
  std::unordered_set<AccountId> blocked_accounts_;
  std::unordered_set<InstrumentId> blocked_instruments_;
  std::optional<AshareInstrumentRules> ashare_rules_;
  std::unordered_map<InstrumentId, AshareInstrumentRules> ashare_master_;
  std::unordered_map<InstrumentId, PriceBand> price_bands_;
  std::unordered_map<InstrumentId, Quantity> positions_;
  SellMap sellable_;
  SellMap sell_reserved_;
  Price cash_{0};
  Price buy_reserved_{0};
  Price day_pnl_{0};
  // M13 safety-net state: per-strategy consecutive rejects, day cumulative
  // submits/rejects, day-start equity baseline, and queued auto actions.
  std::unordered_map<StrategyId, int> consecutive_rejects_;
  std::uint64_t day_submits_{0};
  std::uint64_t day_rejects_{0};
  Price equity_base_{0};
  std::vector<AutoAction> auto_actions_;
  std::unordered_set<InstrumentId> instrument_allowlist_;
  std::size_t rate_max_orders_{0};
  std::chrono::nanoseconds rate_window_{0};
  const Clock* rate_clock_{nullptr};
  mutable std::deque<Timestamp> rate_attempts_;
};

}  // namespace qts
