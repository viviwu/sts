// clang-format off
/**
  ******************************************************************************
  * @file           : risk_engine.cpp
  * @author         : vivi wu
  * @brief          : Risk-engine implementation: bands, positions, frequency, allowlists, switches.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/risk/risk_engine.hpp"

#include <cstdlib>
#include <limits>
#include <utility>

namespace sts {

RiskEngine::RiskEngine(RiskLimits limits)
    : limits_{limits} {}

RiskDecision RiskEngine::check(const OrderIntent& intent) const {
  RiskDecision switches = check_switches(intent);
  if (!switches.allowed) return switches;
  RiskDecision shape = check_shape(intent);
  if (!shape.allowed) return shape;
  RiskDecision band = check_band(intent);
  if (!band.allowed) return band;
  RiskDecision notional = check_notional(intent);
  if (!notional.allowed) return notional;
  RiskDecision position = check_position(intent);
  if (!position.allowed) return position;
  RiskDecision sellable = check_sellable(intent);
  if (!sellable.allowed) return sellable;
  RiskDecision cash = check_cash(intent);
  if (!cash.allowed) return cash;
  RiskDecision loss = check_loss_limit(intent);
  if (!loss.allowed) return loss;
  RiskDecision venue = check_venue(intent);
  if (!venue.allowed) return venue;
  return check_rate();
}

RiskDecision RiskEngine::check_switches(const OrderIntent& intent) const {
  if (!limits_.trading_enabled) return {false, "trading is disabled"};
  if (blocked_strategies_.find(intent.strategy_id) != blocked_strategies_.end()) {
    return {false, "strategy is blocked"};
  }
  if (blocked_accounts_.find(intent.account_id) != blocked_accounts_.end()) {
    return {false, "account is blocked"};
  }
  if (blocked_instruments_.find(intent.instrument_id) != blocked_instruments_.end()) {
    return {false, "instrument is blocked"};
  }
  if (intent.client_order_id.empty() || intent.instrument_id.empty()) return {false, "missing order identity"};
  return {true, ""};
}

RiskDecision RiskEngine::check_shape(const OrderIntent& intent) const {
  if (intent.quantity <= 0) return {false, "quantity must be positive"};
  if (intent.type == OrderType::limit && intent.limit_price <= 0) return {false, "limit price must be positive"};
  if (!instrument_allowlist_.empty() && instrument_allowlist_.find(intent.instrument_id) == instrument_allowlist_.end()) {
    return {false, "instrument is not allowlisted"};
  }
  return {true, ""};
}

RiskDecision RiskEngine::check_band(const OrderIntent& intent) const {
  if (intent.type != OrderType::limit) return {true, ""};
  const auto band = price_bands_.find(intent.instrument_id);
  if (band == price_bands_.end()) return {true, ""};
  if (band->second.limit_down > 0 && intent.limit_price < band->second.limit_down) {
    return {false, "limit price below price band"};
  }
  if (band->second.limit_up > 0 && intent.limit_price > band->second.limit_up) {
    return {false, "limit price above price band"};
  }
  return {true, ""};
}

RiskDecision RiskEngine::check_notional(const OrderIntent& intent) const {
  if (limits_.max_order_quantity > 0 && intent.quantity > limits_.max_order_quantity) return {false, "order quantity exceeds limit"};
  if (intent.type == OrderType::limit && limits_.max_order_notional > 0) {
    if (intent.quantity > std::numeric_limits<Price>::max() / intent.limit_price) return {false, "order notional overflow"};
    if (intent.quantity * intent.limit_price > limits_.max_order_notional) return {false, "order notional exceeds limit"};
  }
  return {true, ""};
}

RiskDecision RiskEngine::check_position(const OrderIntent& intent) const {
  if (limits_.max_position_quantity <= 0) return {true, ""};
  const Quantity cap = limits_.max_position_quantity;
  Quantity net = 0;
  const auto pos = positions_.find(intent.instrument_id);
  if (pos != positions_.end()) net = pos->second;
  // Overflow-safe breach test: |net| <= cap keeps (cap +/- net) in range
  // for any realistic cap; absurd stored values fail closed.
  if (net == std::numeric_limits<Quantity>::min() || std::llabs(net) > cap) {
    return {false, "position limit would be breached"};
  }
  const bool breach = (intent.side == Side::buy) ? (intent.quantity > cap - net) : (intent.quantity > cap + net);
  if (breach) return {false, "position limit would be breached"};
  return {true, ""};
}

RiskDecision RiskEngine::check_sellable(const OrderIntent& intent) const {
  if (!limits_.enforce_sellable || intent.side != Side::sell) return {true, ""};
  const StrategyId basket = intent.strategy_id.empty() ? StrategyId{kPassthroughStrategy} : intent.strategy_id;
  Quantity sellable = 0;
  const auto pos = sellable_.find({basket, intent.instrument_id});
  if (pos != sellable_.end()) sellable = pos->second;
  const auto held = sell_reserved_.find({basket, intent.instrument_id});
  if (held != sell_reserved_.end()) sellable -= held->second;
  if (sellable < 0) sellable = 0;
  if (intent.quantity > sellable) return {false, "insufficient sellable inventory (T+1)"};
  return {true, ""};
}

RiskDecision RiskEngine::check_loss_limit(const OrderIntent& intent) const {
  if (limits_.daily_loss_limit <= 0 || day_pnl_ > -limits_.daily_loss_limit) return {true, ""};
  // Breached: only pure closes pass (smaller or flat same-sign net; flips
  // open fresh exposure and stay blocked). Unknown/empty books count as
  // flat, so any open is blocked.
  Quantity net = 0;
  const auto pos = positions_.find(intent.instrument_id);
  if (pos != positions_.end()) net = pos->second;
  const Quantity after = (intent.side == Side::buy) ? net + intent.quantity : net - intent.quantity;
  const bool same_side = after == 0 || (after > 0 && net > 0) || (after < 0 && net < 0);
  const Quantity magnitude_before = net < 0 ? -net : net;
  const Quantity magnitude_after = after < 0 ? -after : after;
  if (!same_side || magnitude_after > magnitude_before) return {false, "daily loss limit breached"};
  return {true, ""};
}

RiskDecision RiskEngine::check_rate() const {
  if (rate_max_orders_ <= 0 || rate_clock_ == nullptr) return {true, ""};
  const Timestamp now = rate_clock_->now();
  while (!rate_attempts_.empty() && now - rate_attempts_.front() >= rate_window_) {
    rate_attempts_.pop_front();
  }
  if (rate_attempts_.size() >= rate_max_orders_) return {false, "order rate limit exceeded"};
  rate_attempts_.push_back(now);
  return {true, ""};
}

RiskDecision RiskEngine::check_ashare(const OrderIntent& intent) const {
  // Legacy single-ruleset entry point, kept for existing callers/tests.
  // When the single ruleset is absent but the master covers the instrument,
  // validate against the master entry so both APIs converge.
  if (!ashare_rules_.has_value()) {
    const auto found = ashare_master_.find(intent.instrument_id);
    if (found == ashare_master_.end()) return {false, "ashare rules are not configured"};
    RiskDecision base = check(intent);
    if (!base.allowed) return base;
    const OrderValidationResult master_result = AshareOrderValidator::validate(intent, found->second);
    if (!master_result.valid) return {false, master_result.reason};
    return {true, ""};
  }
  // Single-ruleset path: run the generic gate without double venue work by
  // inlining the pre-venue checks is overkill; check() venue passes when the
  // master is empty or covers this instrument consistently.
  RiskDecision base = check(intent);
  if (!base.allowed) return base;
  const OrderValidationResult result = AshareOrderValidator::validate(intent, *ashare_rules_);
  if (!result.valid) return {false, result.reason};
  return {true, ""};
}

RiskDecision RiskEngine::check_venue(const OrderIntent& intent) const {
  if (ashare_master_.empty()) return {true, ""};
  const auto found = ashare_master_.find(intent.instrument_id);
  if (found == ashare_master_.end()) return {false, "unknown instrument (no ashare rules)"};
  const OrderValidationResult result = AshareOrderValidator::validate(intent, found->second);
  if (!result.valid) return {false, result.reason};
  return {true, ""};
}

void RiskEngine::set_trading_enabled(bool enabled) noexcept { limits_.trading_enabled = enabled; }

void RiskEngine::set_limits(RiskLimits limits) noexcept { limits_ = limits; }

void RiskEngine::set_switches(std::unordered_set<StrategyId> strategies, std::unordered_set<AccountId> accounts, std::unordered_set<InstrumentId> instruments) {
  blocked_strategies_ = std::move(strategies);
  blocked_accounts_ = std::move(accounts);
  blocked_instruments_ = std::move(instruments);
}

void RiskEngine::set_strategy_blocked(const StrategyId& strategy_id, bool blocked) {
  if (blocked) {
    blocked_strategies_.insert(strategy_id);
  } else {
    blocked_strategies_.erase(strategy_id);
  }
}

void RiskEngine::set_account_blocked(const AccountId& account_id, bool blocked) {
  if (blocked) {
    blocked_accounts_.insert(account_id);
  } else {
    blocked_accounts_.erase(account_id);
  }
}

void RiskEngine::set_instrument_blocked(const InstrumentId& instrument_id, bool blocked) {
  if (blocked) {
    blocked_instruments_.insert(instrument_id);
  } else {
    blocked_instruments_.erase(instrument_id);
  }
}

void RiskEngine::set_ashare_rules(AshareInstrumentRules rules) { ashare_rules_ = std::move(rules); }

void RiskEngine::clear_ashare_rules() { ashare_rules_.reset(); }

void RiskEngine::set_ashare_master(std::unordered_map<InstrumentId, AshareInstrumentRules> master) { ashare_master_ = std::move(master); }

void RiskEngine::clear_ashare_master() { ashare_master_.clear(); }

void RiskEngine::set_price_band(InstrumentId instrument_id, PriceBand band) { price_bands_.insert_or_assign(std::move(instrument_id), band); }

void RiskEngine::clear_price_band(const InstrumentId& instrument_id) { price_bands_.erase(instrument_id); }

void RiskEngine::set_position(InstrumentId instrument_id, Quantity net_quantity) { positions_.insert_or_assign(std::move(instrument_id), net_quantity); }

void RiskEngine::set_day_pnl(Price day_pnl) { day_pnl_ = day_pnl; }

void RiskEngine::note_submit_accepted(const StrategyId& strategy) {
  ++day_submits_;
  consecutive_rejects_.erase(strategy);
}

void RiskEngine::note_submit_rejected(const StrategyId& strategy) {
  ++day_submits_;
  ++day_rejects_;
  ++consecutive_rejects_[strategy];
  evaluate_reject_gates(strategy);
}

void RiskEngine::note_broker_reject(const StrategyId& strategy) {
  // The submit was already counted at submit time; only the reject side and
  // the streak move here. Callers count the first broker rejection only
  // (duplicate reports must not extend the streak twice).
  ++day_rejects_;
  ++consecutive_rejects_[strategy];
  evaluate_reject_gates(strategy);
}

void RiskEngine::evaluate_reject_gates(const StrategyId& strategy) {
  if (limits_.max_consecutive_rejects > 0) {
    const auto streak = consecutive_rejects_.find(strategy);
    const int count = streak == consecutive_rejects_.end() ? 0 : streak->second;
    if (count >= limits_.max_consecutive_rejects && blocked_strategies_.find(strategy) == blocked_strategies_.end()) {
      blocked_strategies_.insert(strategy);
      AutoAction action{};
      action.kind = AutoActionKind::halt_strategy;
      action.strategy_id = strategy;
      action.reason = "strategy auto-stopped after " + std::to_string(count) + " consecutive rejects";
      auto_actions_.push_back(std::move(action));
    }
  }
  if (limits_.max_reject_rate_bps > 0 && limits_.reject_rate_min_orders > 0 && limits_.trading_enabled && day_submits_ >= static_cast<std::uint64_t>(limits_.reject_rate_min_orders)) {
    // Overflow-safe breach test: rejects*10000 vs bps*submits, saturated on
    // either side (same fail-closed shape as check_position).
    const std::uint64_t cap = std::numeric_limits<std::uint64_t>::max() / 10'000;
    const std::uint64_t scaled = day_rejects_ > cap ? std::numeric_limits<std::uint64_t>::max() : day_rejects_ * 10'000;
    const std::uint64_t bar = day_submits_ > cap ? 0 : static_cast<std::uint64_t>(limits_.max_reject_rate_bps) * day_submits_;
    if (scaled >= bar) {
      limits_.trading_enabled = false;
      AutoAction action{};
      action.kind = AutoActionKind::halt_global;
      action.reason = "global reject rate breached (" + std::to_string(day_rejects_) + "/" + std::to_string(day_submits_) + ")";
      auto_actions_.push_back(std::move(action));
    }
  }
}

void RiskEngine::set_equity_base(Price base) { equity_base_ = base; }

void RiskEngine::set_equity(Price current) {
  if (limits_.max_loss_ratio_bps <= 0 || equity_base_ <= 0 || !limits_.trading_enabled) return;
  const Price loss = equity_base_ - current;
  if (loss <= 0) return;
  // Breach test without division: loss/base >= bps/10000, overflow-guarded
  // on both products (same fail-closed shape as check_position).
  bool breached = loss >= equity_base_;  // 100%+ drawdown breaches any threshold
  if (!breached) {
    const Price cap = std::numeric_limits<Price>::max() / 10'000;
    const Price scaled = loss > cap ? std::numeric_limits<Price>::max() : loss * 10'000;
    const Price bar = equity_base_ > cap ? 0 : static_cast<Price>(limits_.max_loss_ratio_bps) * equity_base_;
    breached = scaled >= bar;
  }
  if (!breached) return;
  limits_.trading_enabled = false;
  evaluate_loss_gate();
}

void RiskEngine::evaluate_loss_gate() {
  AutoAction action{};
  action.kind = AutoActionKind::halt_global;
  action.reason = "day loss ratio breached (equity base " + std::to_string(equity_base_) + ")";
  auto_actions_.push_back(std::move(action));
}

void RiskEngine::settle_day() {
  consecutive_rejects_.clear();
  day_submits_ = 0;
  day_rejects_ = 0;
  day_pnl_ = 0;
}

std::vector<AutoAction> RiskEngine::drain_auto_actions() {
  std::vector<AutoAction> actions;
  actions.swap(auto_actions_);
  return actions;
}

void RiskEngine::set_sellable(const StrategyId& strategy, const InstrumentId& instrument, Quantity sellable_quantity) {
  const StrategyId basket = strategy.empty() ? StrategyId{kPassthroughStrategy} : strategy;
  sellable_.insert_or_assign({basket, instrument}, sellable_quantity);
}

void RiskEngine::set_sell_reserved(SellMap reserved) { sell_reserved_ = std::move(reserved); }

void RiskEngine::set_cash(Price cash) { cash_ = cash; }

void RiskEngine::set_buy_reserved(Price reserved_notional) { buy_reserved_ = reserved_notional; }

RiskDecision RiskEngine::check_cash(const OrderIntent& intent) const {
  if (!limits_.enforce_cash || intent.side != Side::buy) return {true, ""};
  if (intent.type != OrderType::limit || intent.limit_price <= 0) return {true, ""};  // unsizable: venue/lot gates still apply
  const Price available = cash_ - buy_reserved_;
  if (available < 0) return {false, "insufficient buying power (cash)"};
  if (intent.quantity > std::numeric_limits<Price>::max() / intent.limit_price) return {false, "order notional overflow"};
  if (intent.quantity * intent.limit_price > available) return {false, "insufficient buying power (cash)"};
  return {true, ""};
}

void RiskEngine::set_instrument_allowlist(std::unordered_set<InstrumentId> allowlist) { instrument_allowlist_ = std::move(allowlist); }

void RiskEngine::set_rate_limit(std::size_t max_orders, std::chrono::nanoseconds window, const Clock* clock) {
  rate_max_orders_ = (clock == nullptr) ? 0 : max_orders;
  rate_window_ = window;
  rate_clock_ = clock;
  rate_attempts_.clear();
}

}  // namespace sts
