// clang-format off
/**
  ******************************************************************************
  * @file           : gateway_service.cpp
  * @author         : vivi wu
  * @brief          : Unified gateway facade implementation over OMS and books.
  * @version        : 0.1.0
  * @date           : 10/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/gateway/gateway_service.hpp"

#include <utility>

namespace qts::gateway {
namespace {

bool valid_order_ticket(const OrderIntent& intent, std::string& reason) {
  if (intent.client_order_id.empty() || intent.account_id.empty() || intent.instrument_id.empty()) {
    reason = "order needs id, account and instrument";
    return false;
  }
  if (intent.quantity <= 0) {
    reason = "order needs a positive quantity";
    return false;
  }
  if (intent.type == OrderType::limit) {
    if (intent.limit_price <= 0) {
      reason = "limit orders need a positive price";
      return false;
    }
    return true;
  }
  if (intent.market_order_style == MarketOrderStyle::none) {
    reason = "market orders need a style";
    return false;
  }
  return true;
}

FrontResult refused(const std::string& reason) { return {false, reason}; }

bool order_matches(const OrderIntent& intent, const AccountId& account, const StrategyId& strategy) {
  if (!account.empty() && intent.account_id != account) return false;
  if (!strategy.empty() && intent.strategy_id != strategy) return false;
  return true;
}

}  // namespace

GatewayService::GatewayService(control::ControlContext context, const Portfolio& book, std::vector<AccountId> accounts, std::unordered_map<AccountId, const Portfolio*> account_books)
    : context_(context),
      book_(book),
      accounts_(std::move(accounts)),
      account_books_(std::move(account_books)) {}

FrontResult GatewayService::submit_order(const OrderIntent& intent) {
  std::string reason;
  if (!valid_order_ticket(intent, reason)) return refused(reason);
  if (context_.oms.submit(intent)) return {true, "order " + intent.client_order_id + " accepted"};
  std::string why = context_.oms.last_reject_reason();
  if (why.empty()) why = "see ledger risk_rejected for the reason";
  return refused("order " + intent.client_order_id + " rejected: " + why);
}

FrontResult GatewayService::cancel_order(const ClientOrderId& client_order_id) {
  if (client_order_id.empty()) return refused("cancel needs an order id");
  // Parent ids never reach the OMS; the shared cancel path fans them out
  // through the execution algorithm, and true ghosts stay loud there.
  control::ControlCommand command{};
  command.name = "cancel";
  command.id = client_order_id;
  const control::ControlResult applied = control::apply_command(command, context_);
  return {applied.ok, applied.message};
}

FrontResult GatewayService::start_strategy(const StrategyConfig& configured) {
  if (configured.stock < 0 || configured.stock_avg < 0) return refused("stock allocation must be non-negative");
  if (configured.stock > 0 && !stock_funder_) return refused("no funding path for stock allocation");
  control::ControlCommand command{};
  command.name = "strategy_start";
  command.id = configured.id;
  command.account = configured.account;
  command.instrument = configured.instrument;
  command.strategy_type = configured.type;
  command.quantity = configured.quantity;
  command.lower = configured.lower;
  command.upper = configured.upper;
  command.grids = configured.grids;
  command.start = configured.start;
  const control::ControlResult applied = control::apply_command(command, context_);
  if (applied.ok) {
    if (configured.stock > 0) stock_funder_(configured.id, configured.instrument, configured.stock, configured.stock_avg);
    StrategyEvent event{};
    event.strategy_id = configured.id;
    event.kind = StrategyEventKind::started;
    event.detail = configured.type + " " + configured.account + " " + configured.instrument;
    on_strategy_event(event);
  }
  return {applied.ok, applied.message};
}

FrontResult GatewayService::stop_strategy(const StrategyId& strategy_id) {
  control::ControlCommand command{};
  command.name = "strategy_stop";
  command.id = strategy_id;
  const control::ControlResult applied = control::apply_command(command, context_);
  if (applied.ok) {
    StrategyEvent event{};
    event.strategy_id = strategy_id;
    event.kind = StrategyEventKind::stopped;
    event.detail = applied.message;
    on_strategy_event(event);
  }
  return {applied.ok, applied.message};
}

FrontResult GatewayService::list_strategies() {
  control::ControlCommand command{};
  command.name = "strategy_list";
  const control::ControlResult applied = control::apply_command(command, context_);
  return {applied.ok, applied.message};
}

FrontResult GatewayService::account_cash(Price& cash, const AccountId& account) const {
  const Portfolio* book = nullptr;
  std::string error;
  if (!resolve_book(account, book, error)) {
    // Bookless live accounts (ADR-054): serve the latest counter snapshot
    // instead of refusing; unknown accounts still refuse above.
    if (error.rfind("no book for account ", 0) != 0) return refused(error);
    CounterSnapshot snapshot;
    if (snapshot_cache_ == nullptr || !snapshot_cache_->lookup(account, snapshot) || !snapshot.has_account) {
      return refused("no counter snapshot for account " + account);
    }
    cash = snapshot.account.available_cash;
    return {true, "ok"};
  }
  cash = book->cash();
  return {true, "ok"};
}

void GatewayService::CounterSnapshotCache::update(const AccountId& account, const CounterSnapshot& snapshot) {
  const std::lock_guard<std::mutex> guard(mutex_);
  map_[account] = snapshot;
}

void GatewayService::CounterSnapshotCache::update_account(const AccountId& account, const TradeAccountSnapshot& snapshot, const std::string& captured_at) {
  const std::lock_guard<std::mutex> guard(mutex_);
  CounterSnapshot& entry = map_[account];
  entry.account = snapshot;
  entry.has_account = true;
  entry.captured_at = captured_at;
}

void GatewayService::CounterSnapshotCache::update_asset(const AccountId& account, const TradeAssetSnapshot& snapshot, const std::string& captured_at) {
  const std::lock_guard<std::mutex> guard(mutex_);
  CounterSnapshot& entry = map_[account];
  entry.asset = snapshot;
  entry.has_asset = true;
  entry.captured_at = captured_at;
}

bool GatewayService::CounterSnapshotCache::lookup(const AccountId& account, CounterSnapshot& snapshot) const {
  const std::lock_guard<std::mutex> guard(mutex_);
  const auto found = map_.find(account);
  if (found == map_.end()) return false;
  snapshot = found->second;
  return true;
}

void GatewayService::set_counter_snapshot_cache(std::shared_ptr<CounterSnapshotCache> cache) { snapshot_cache_ = std::move(cache); }

std::string GatewayService::counter_snapshot_time(const AccountId& account) const {
  CounterSnapshot snapshot;
  if (snapshot_cache_ == nullptr || !snapshot_cache_->lookup(account, snapshot)) return {};
  return snapshot.captured_at;
}

bool GatewayService::resolve_book(const AccountId& account, const Portfolio*& book, std::string& error) const {
  if (account.empty()) {
    book = &book_;
    return true;
  }
  const auto wired = account_books_.find(account);
  if (wired != account_books_.end() && wired->second != nullptr) {
    book = wired->second;
    return true;
  }
  bool known = accounts_.empty();
  for (const auto& seen : accounts_) {
    if (seen == account) {
      known = true;
      break;
    }
  }
  if (!known) {
    error = "unknown account " + account;
    return false;
  }
  if (account_books_.empty()) {
    book = &book_;  // legacy wiring: one shared default book
    return true;
  }
  error = "no book for account " + account;
  return false;
}

bool GatewayService::book_shared(const AccountId& account) const {
  if (account_books_.empty()) return true;  // legacy wiring: topology unknown
  if (account.empty()) return accounts_.size() > 1;
  const Portfolio* mine = nullptr;
  std::string error;
  // Known bookless live accounts serve their own counter snapshot
  // (shared=false); unknown accounts stay conservative (refused anyway).
  if (!resolve_book(account, mine, error)) return error.rfind("unknown account ", 0) == 0;
  std::size_t sharing = 0;
  for (const auto& seen : accounts_) {
    const Portfolio* other = nullptr;
    std::string ignored;
    if (resolve_book(seen, other, ignored) && other == mine) ++sharing;
  }
  return sharing > 1;
}

FrontResult GatewayService::open_orders(std::vector<ExecutionReport>& orders, const AccountId& account) const {
  if (!account.empty()) {
    const Portfolio* ignored = nullptr;
    std::string error;
    if (!resolve_book(account, ignored, error)) return refused(error);
  }
  orders = context_.oms.open_orders();
  if (!account.empty()) {
    std::vector<ExecutionReport> filtered;
    for (const auto& report : orders) {
      const OrderIntent* intent = context_.oms.find_intent(report.client_order_id);
      if (intent != nullptr && intent->account_id == account) filtered.push_back(report);
    }
    orders = std::move(filtered);
  }
  return {true, "ok"};
}

FrontResult GatewayService::find_order(const ClientOrderId& client_order_id, ExecutionReport& report) const {
  const ExecutionReport* found = context_.oms.find(client_order_id);
  if (found == nullptr) return refused("unknown order " + client_order_id);
  report = *found;
  return {true, "ok"};
}

bool GatewayService::order_account(const ClientOrderId& client_order_id, AccountId& account) const {
  const OrderIntent* intent = context_.oms.find_intent(client_order_id);
  if (intent == nullptr) return false;
  account = intent->account_id;
  return true;
}

bool GatewayService::strategy_account(const StrategyId& strategy_id, AccountId& account) const {
  if (context_.strategies == nullptr) return false;
  return context_.strategies->account_of(strategy_id, account);
}

FrontResult GatewayService::positions_for(const StrategyId& strategy, std::unordered_map<InstrumentId, PositionView>& positions, const AccountId& account) const {
  const Portfolio* book = nullptr;
  std::string error;
  if (!resolve_book(account, book, error)) return refused(error);
  positions = book->all_positions_for(strategy);
  return {true, "ok"};
}

FrontResult GatewayService::fills(const AccountId& account, const StrategyId& strategy, std::size_t limit, std::vector<FillView>& out, bool& truncated) const {
  if (!account.empty()) {
    const Portfolio* ignored = nullptr;
    std::string error;
    if (!resolve_book(account, ignored, error)) return refused(error);
  }
  if (limit == 0) limit = kDefaultFillLimit;
  if (limit > kMaxFillLimit) limit = kMaxFillLimit;
  const std::vector<OrderView> views = context_.oms.all_orders();
  std::vector<FillView> matched;
  for (const auto& view : views) {
    if (view.snapshot.cumulative_quantity <= 0) continue;
    if (!order_matches(view.intent, account, strategy)) continue;
    matched.push_back({view.intent, view.snapshot, view.seq});
  }
  truncated = matched.size() > limit;
  out.clear();
  // Newest first: all_orders arrives oldest-first, take the tail.
  std::size_t taken = 0;
  for (std::size_t index = matched.size(); index > 0 && taken < limit; --index, ++taken) out.push_back(matched[index - 1]);
  return {true, "ok"};
}

FrontResult GatewayService::strategy_progress(const StrategyId& strategy, StrategyProgress& progress, const AccountId& account) const {
  if (strategy.empty()) return refused("progress needs a strategy id");
  const Portfolio* book = nullptr;
  std::string error;
  if (!resolve_book(account, book, error)) return refused(error);
  progress = StrategyProgress{};
  progress.strategy_id = strategy;
  if (context_.strategies != nullptr) {
    for (const auto& active : context_.strategies->active_ids()) {
      if (active == strategy) {
        progress.running = true;
        break;
      }
    }
  }
  bool seen = progress.running;
  for (const auto& view : context_.oms.all_orders()) {
    if (view.intent.strategy_id != strategy) continue;
    if (!order_matches(view.intent, account, {})) continue;
    seen = true;
    ++progress.submitted;
    if (OrderManager::is_terminal(view.snapshot.status)) {
      switch (view.snapshot.status) {
        case OrderStatus::filled:
          ++progress.filled;
          break;
        case OrderStatus::cancelled:
          ++progress.cancelled;
          break;
        case OrderStatus::rejected:
          ++progress.rejected;
          break;
        default:
          break;
      }
    } else {
      ++progress.open;
    }
  }
  if (!seen) return refused("unknown strategy " + strategy);
  for (const auto& entry : book->all_positions_for(strategy)) progress.position_quantity += entry.second.total;
  progress.realized = book->realized_for(strategy);
  return {true, "ok"};
}

SubscriptionToken GatewayService::subscribe_quotes(QuoteSubscription handler, std::string& error) {
  if (!handler) {
    error = "quote subscription needs a handler";
    return 0;
  }
  const SubscriptionToken subscription = next_subscription_;
  ++next_subscription_;
  quote_subscribers_.emplace(subscription, std::move(handler));
  return subscription;
}

SubscriptionToken GatewayService::subscribe_reports(ReportSubscription handler, std::string& error) {
  if (!handler) {
    error = "report subscription needs a handler";
    return 0;
  }
  const SubscriptionToken subscription = next_subscription_;
  ++next_subscription_;
  report_subscribers_.emplace(subscription, std::move(handler));
  return subscription;
}

SubscriptionToken GatewayService::subscribe_strategies(StrategySubscription handler, std::string& error) {
  if (!handler) {
    error = "strategy subscription needs a handler";
    return 0;
  }
  const SubscriptionToken subscription = next_subscription_;
  ++next_subscription_;
  strategy_subscribers_.emplace(subscription, std::move(handler));
  return subscription;
}

bool GatewayService::unsubscribe(SubscriptionToken subscription) {
  if (quote_subscribers_.erase(subscription) > 0) return true;
  if (report_subscribers_.erase(subscription) > 0) return true;
  return strategy_subscribers_.erase(subscription) > 0;
}

void GatewayService::on_quote(const QuoteEvent& quote) {
  // Copy handlers before calling: a subscriber must never reenter, and a
  // mid-fan-out unsubscribe must not invalidate the iteration.
  std::vector<QuoteSubscription> handlers;
  handlers.reserve(quote_subscribers_.size());
  for (const auto& entry : quote_subscribers_) handlers.push_back(entry.second);
  for (const auto& handler : handlers) handler(quote);
}

void GatewayService::on_execution_report(const ExecutionReport& report) {
  std::vector<ReportSubscription> handlers;
  handlers.reserve(report_subscribers_.size());
  for (const auto& entry : report_subscribers_) handlers.push_back(entry.second);
  for (const auto& handler : handlers) handler(report);
}

void GatewayService::on_strategy_event(const StrategyEvent& event) {
  std::vector<StrategySubscription> handlers;
  handlers.reserve(strategy_subscribers_.size());
  for (const auto& entry : strategy_subscribers_) handlers.push_back(entry.second);
  for (const auto& handler : handlers) handler(event);
}

void GatewayService::set_snapshot_provider(AccountSnapshotProvider provider) { snapshot_provider_ = std::move(provider); }

void GatewayService::set_stock_funder(StockFunder funder) { stock_funder_ = std::move(funder); }

void GatewayService::set_attestation_ledger(EventLedger* ledger) { attestation_ledger_ = ledger; }

FrontResult GatewayService::settle_report(SettleReport& report) const {
  if (!snapshot_provider_) return refused("no snapshot provider: settlement is unavailable on this server");
  SnapshotBatch batch{};
  try {
    batch = snapshot_provider_();
  } catch (const std::exception& fetch_failure) {
    return refused(std::string{"snapshot fetch failed: "} + fetch_failure.what());
  }
  report.captured_at = batch.captured_at;
  report.ledger_watermark = attestation_ledger_ != nullptr ? attestation_ledger_->latest_event_id() : 0;
  for (const auto& snapshot : batch.snapshots) {
    AccountSettleReport row{};
    row.account = snapshot.account_id;
    row.diff = reconcile::diff_books(book_, snapshot);
    report.accounts.push_back(std::move(row));
  }
  return {true, "ok"};
}

FrontResult GatewayService::settle_confirm(const ReconcileAttestation& claimed) {
  if (attestation_ledger_ == nullptr) return refused("no attestation ledger: settlement is unavailable on this server");
  if (claimed.broker_snapshot_time.empty()) return refused("attestation needs the report snapshot time");
  if (claimed.cash_dirty < 0 || claimed.position_dirty < 0) return refused("attestation dirty counts must be non-negative");
  if (claimed.ledger_watermark > attestation_ledger_->latest_event_id()) return refused("cannot attest a ledger prefix that does not exist yet");
  LedgerEvent event{};
  event.type = LedgerEventType::reconcile_attested;
  event.attestation = claimed;
  event.attestation.operator_id = kGatewayAdmin;
  event.envelope.trace_id = "settle-confirm";
  const std::uint64_t event_id = attestation_ledger_->append(std::move(event));
  return {true, "attested at event " + std::to_string(event_id)};
}

}  // namespace qts::gateway
