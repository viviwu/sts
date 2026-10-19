// clang-format off
/**
  ******************************************************************************
  * @file           : gateway_service.hpp
  * @author         : vivi wu
  * @brief          : Unified gateway facade over OMS, risk, strategies and books.
  * @version        : 0.1.0
  * @date           : 10/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "sts/broker/quote_api.hpp"
#include "sts/broker/trade_api.hpp"
#include "sts/core/types.hpp"
#include "sts/ops/control.hpp"
#include "sts/persistence/ledger.hpp"
#include "sts/portfolio/portfolio.hpp"
#include "sts/research/reconcile.hpp"

namespace sts::gateway {

// Unified gateway facade (ADR-041, M12-0).
//
// One typed entry point for operator apps (test/demo/remote clients): session
// login, manual orders, strategy lifecycle, book queries and quote/report
// subscriptions. Commands run the same OMS + synchronous risk path as the
// control inbox (OrderManager::submit checks first); queries read only the
// in-memory OMS/Portfolio snapshots, never the hot-path channels.
//
// Threading: the server tick thread owns the service. on_quote and
// on_execution_report fan out from that thread; subscriber handlers receive
// value copies and must not block or reenter the service (same discipline
// as the vendor adapter drain pumps). No extra thread, no shared-state lock.
using SubscriptionToken = std::uint64_t;

struct FrontResult {
  bool ok{false};
  std::string message;
};

// Strategy lifecycle event (M12-3): emitted to strategy subscribers when a
// start/stop succeeds through the service (inbox-driven changes bypass the
// service and stay visible via list_strategies, documented gap).
enum class StrategyEventKind : std::uint8_t { started, stopped };

struct StrategyEvent {
  StrategyId strategy_id;
  StrategyEventKind kind{StrategyEventKind::started};
  std::string detail;
};

// Settlement report plumbing (M14): one broker snapshot batch plus the
// per-account book diffs against the live book. Multi-account servers
// compare every snapshot against the single strategy book and say so;
// per-account books are a later milestone, not a silent assumption.
struct SnapshotBatch {
  std::vector<TradeAccountSnapshot> snapshots;
  std::string captured_at;  // ISO-8601 UTC of fetch completion
};
struct AccountSettleReport {
  AccountId account;
  reconcile::BooksDiff diff;
};

struct SettleReport {
  std::string captured_at;
  std::uint64_t ledger_watermark{0};
  std::vector<AccountSettleReport> accounts;
};

// M16: single default admin, no sessions. Socket file permissions are the
// access control (same model as the query socket); every request runs as
// admin. Per-operator scopes return in the phase-2 RBAC milestone.
inline constexpr char kGatewayAdmin[] = "admin";

// P0 (ADR-049): fills paging bounds. Queries read the retained OMS set
// (bounded by the session's order count, never by the limit); the limit
// only caps one reply frame, overflow reports truncated=true.
inline constexpr std::size_t kDefaultFillLimit = 200;
inline constexpr std::size_t kMaxFillLimit = 1000;

// One retained order with its submission intent: the intent carries the
// (account, strategy) sub-account dimensions the snapshot lacks.
struct FillView {
  OrderIntent intent;
  ExecutionReport report;
  std::uint64_t seq{0};
};

// Per-strategy (sub-account) execution progress: order-state counters over
// the retained OMS set plus the basket position. running reflects the live
// registry; inbox-driven changes bypass the service events but stay
// visible here (documented gap, same as list_strategies).
struct StrategyProgress {
  StrategyId strategy_id;
  bool running{false};
  std::size_t submitted{0};
  std::size_t open{0};
  std::size_t filled{0};
  std::size_t cancelled{0};
  std::size_t rejected{0};
  Quantity position_quantity{0};
  Price realized{0};
};

class GatewayService {
 public:
  using QuoteSubscription = std::function<void(const QuoteEvent&)>;
  using ReportSubscription = std::function<void(const ExecutionReport&)>;
  using StrategySubscription = std::function<void(const StrategyEvent&)>;

  // Borrowed views (same pattern as control::ControlContext): the server
  // owns the OMS/execution/risk lifetimes and the book; the service only
  // drives them from the tick thread. accounts lists the known top-level
  // accounts for query validation (empty disables validation, keeping
  // tests and legacy callers working); unknown accounts refuse loudly.
  // account_books resolves an account to its channel book for real
  // per-account cash/positions (empty map keeps the legacy shared-default
  // behavior); a known account with no wired book refuses honestly instead
  // of serving another account's numbers.
  GatewayService(control::ControlContext context, const Portfolio& book, std::vector<AccountId> accounts = {}, std::unordered_map<AccountId, const Portfolio*> account_books = {});
  ~GatewayService() = default;

  GatewayService(const GatewayService&) = delete;
  GatewayService& operator=(const GatewayService&) = delete;
  GatewayService(GatewayService&&) = delete;
  GatewayService& operator=(GatewayService&&) = delete;

  // Order path: straight into OMS (same synchronous risk gate as strategy
  // children; empty strategy_id normalizes to the passthrough basket at
  // the OMS entry point). Ticket validation fails loudly, never silently.
  FrontResult submit_order(const OrderIntent& intent);
  FrontResult cancel_order(const ClientOrderId& client_order_id);

  // Strategy lifecycle: needs the live registry (null registry refuses
  // loudly, like the inbox path).
  FrontResult start_strategy(const StrategyConfig& configured);
  FrontResult stop_strategy(const StrategyId& strategy_id);
  FrontResult list_strategies();

  // Queries: copies out of the live OMS/Portfolio snapshots. Empty
  // account means no filtering; a non-empty unknown account refuses
  // loudly (fail closed). Cash and strategy baskets come from the
  // account's own channel book when wired (shared=false); otherwise they
  // are the single channel-book shared view (shared=true, see book_shared).
  FrontResult account_cash(Price& cash, const AccountId& account = {}) const;
  FrontResult open_orders(std::vector<ExecutionReport>& orders, const AccountId& account = {}) const;
  FrontResult find_order(const ClientOrderId& client_order_id, ExecutionReport& report) const;
  // Owning-account lookups for the transport account gate (ADR-050):
  // false when the id is unknown (fail closed at the gate). Empty intent
  // accounts (legacy tickets) match any session.
  bool order_account(const ClientOrderId& client_order_id, AccountId& account) const;
  bool strategy_account(const StrategyId& strategy_id, AccountId& account) const;
  FrontResult positions_for(const StrategyId& strategy, std::unordered_map<InstrumentId, PositionView>& positions, const AccountId& account = {}) const;
  // Latest counter snapshots (ADR-054): the server tick thread updates,
  // the gateway reads. Bookless live accounts serve cash from here
  // (shared=false plus the snapshot time); absent entries refuse loudly
  // like before. Thread-safe: tick thread writes, gateway reads.
  struct CounterSnapshot {
    TradeAccountSnapshot account;
    TradeAssetSnapshot asset;
    std::string captured_at;
    bool has_account{false};
    bool has_asset{false};
  };
  class CounterSnapshotCache {
   public:
    void update(const AccountId& account, const CounterSnapshot& snapshot);
    void update_account(const AccountId& account, const TradeAccountSnapshot& snapshot, const std::string& captured_at);
    void update_asset(const AccountId& account, const TradeAssetSnapshot& snapshot, const std::string& captured_at);
    [[nodiscard]] bool lookup(const AccountId& account, CounterSnapshot& snapshot) const;

   private:
    mutable std::mutex mutex_;
    std::unordered_map<AccountId, CounterSnapshot> map_;
  };
  void set_counter_snapshot_cache(std::shared_ptr<CounterSnapshotCache> cache);
  // Snapshot time backing the last served live cash ("" when inapplicable).
  // Only call after a successful account_cash on a bookless account.
  [[nodiscard]] std::string counter_snapshot_time(const AccountId& account) const;
  // True when the number just served mixes (or may mix) several accounts:
  // empty wiring (legacy), the unfiltered global view, or several accounts
  // resolving to one book. Only call after the matching query succeeded.
  [[nodiscard]] bool book_shared(const AccountId& account) const;
  // Fills history (newest first): retained orders with cumulative fills,
  // filtered by (account, strategy); empty means no filter. limit 0 selects
  // the default; over-limit replies set truncated.
  FrontResult fills(const AccountId& account, const StrategyId& strategy, std::size_t limit, std::vector<FillView>& out, bool& truncated) const;
  // Structured per-strategy progress; unknown strategies (no retained
  // orders and not live) refuse loudly. Empty account aggregates across
  // accounts; a set account scopes counters and the basket to it.
  FrontResult strategy_progress(const StrategyId& strategy, StrategyProgress& progress, const AccountId& account = {}) const;

  // Subscriptions: returns 0 with error on a null handler. Handlers run
  // inline on the tick thread: copy what is needed, return.
  SubscriptionToken subscribe_quotes(QuoteSubscription handler, std::string& error);
  SubscriptionToken subscribe_reports(ReportSubscription handler, std::string& error);
  SubscriptionToken subscribe_strategies(StrategySubscription handler, std::string& error);
  bool unsubscribe(SubscriptionToken subscription);

  // Settlement reconciliation (M14, option A): read-only diff plus an
  // attested confirmation. The snapshot provider (server-wired, bounded
  // fetch) and the attestation ledger are both optional: unset providers
  // refuse loudly instead of inventing data. Confirmations record the
  // default admin operator.
  using AccountSnapshotProvider = std::function<SnapshotBatch()>;
  void set_snapshot_provider(AccountSnapshotProvider provider);
  void set_attestation_ledger(EventLedger* ledger);
  // Settled-inventory funding for dynamic starts (position allocation):
  // wired by the server to the primary strategy book (paper channel fund
  // hook). Unset + position>0 refuses loudly instead of starting flat.
  using StockFunder = std::function<void(const StrategyId&, const InstrumentId&, Quantity, Price)>;
  void set_stock_funder(StockFunder funder);
  FrontResult settle_report(SettleReport& report) const;
  FrontResult settle_confirm(const ReconcileAttestation& claimed);

  // Tick-thread fan-out entry points (called by the server loop, never by
  // subscribers). Value copies only, no vendor pointers cross here.
  void on_quote(const QuoteEvent& quote);
  void on_execution_report(const ExecutionReport& report);
  void on_strategy_event(const StrategyEvent& event);

 private:
  // Non-owning borrowed views (mandatory injected dependencies, same pattern
  // as control::ControlContext above): the server owns all lifetimes.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  control::ControlContext context_;
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  const Portfolio& book_;
  // Known top-level accounts for query validation (empty = no validation).
  std::vector<AccountId> accounts_;
  // Per-account channel books (non-owning, server-owned like book_).
  std::unordered_map<AccountId, const Portfolio*> account_books_;
  StockFunder stock_funder_;
  // Latest counter snapshots (ADR-054, server-owned, outlives the service).
  std::shared_ptr<CounterSnapshotCache> snapshot_cache_;

  // Resolves account to its channel book (default book when empty, or a
  // known account when no per-account wiring exists). Returns false with
  // error for unknown accounts, or known accounts without a wired book.
  bool resolve_book(const AccountId& account, const Portfolio*& book, std::string& error) const;
  AccountSnapshotProvider snapshot_provider_;
  // Attestation ledger (optional, server-owned like the OMS ledger; must
  // outlive the service). Null refuses confirmations instead of dropping
  // attestations on the floor.
  EventLedger* attestation_ledger_{nullptr};
  std::uint64_t next_subscription_{1};
  std::unordered_map<SubscriptionToken, QuoteSubscription> quote_subscribers_;
  std::unordered_map<SubscriptionToken, ReportSubscription> report_subscribers_;
  std::unordered_map<SubscriptionToken, StrategySubscription> strategy_subscribers_;
};

}  // namespace sts::gateway
