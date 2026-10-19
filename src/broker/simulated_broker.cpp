// clang-format off
/**
  ******************************************************************************
  * @file           : simulated_broker.cpp
  * @author         : vivi wu
  * @brief          : Simulated paper matching implementation (latency, slippage, fees, books).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/broker/simulated_broker.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace sts {

SimulatedBroker::SimulatedBroker(EventBus& events)
    : events_{events} {}

void SimulatedBroker::set_starting_cash(Price cash) { book_ = Portfolio(cash); }

void SimulatedBroker::fund_settled_position(const StrategyId& strategy, InstrumentId instrument, Quantity quantity, Price avg_cost) {
  book_.fund_settled(strategy, std::move(instrument), quantity, avg_cost);
}

void SimulatedBroker::set_reference_price(InstrumentId instrument, Price price) { reference_prices_.insert_or_assign(std::move(instrument), price); }

void SimulatedBroker::clear_reference_price(const InstrumentId& instrument) { reference_prices_.erase(instrument); }

void SimulatedBroker::set_order_book(InstrumentId instrument, OrderBook book) {
  auto iter = books_.insert_or_assign(instrument, std::move(book)).first;
  match_resting(iter->first, iter->second);
}

void SimulatedBroker::clear_order_book(const InstrumentId& instrument) { books_.erase(instrument); }

const OrderBook* SimulatedBroker::order_book(const InstrumentId& instrument) const {
  const auto iter = books_.find(instrument);
  return iter == books_.end() ? nullptr : &iter->second;
}

void SimulatedBroker::disconnect() { connected_ = false; }
void SimulatedBroker::reconnect() { connected_ = true; }
void SimulatedBroker::settle_day() { book_.settle_day(); }

void SimulatedBroker::set_clock(const Clock* clock) { clock_ = clock; }
void SimulatedBroker::set_latency(std::chrono::nanoseconds latency) { latency_ = latency; }
void SimulatedBroker::set_slippage_per_unit(Price slippage) { slippage_per_unit_ = slippage; }
void SimulatedBroker::set_fee_rate_bp(int fee_rate_bp) {
  fee_table_ = FeeTable{};
  fee_table_.fallback.commission_ppm = static_cast<std::int64_t>(fee_rate_bp) * 100;
}

void SimulatedBroker::set_fee_schedule(FeeSchedule schedule) { fee_table_.fallback = schedule; }

void SimulatedBroker::set_fee_override(InstrumentId instrument, FeeSchedule schedule) { fee_table_.overrides.insert_or_assign(std::move(instrument), schedule); }

void SimulatedBroker::deliver_due() {
  if (!connected_) return;  // held for reconnect
  while (!pending_.empty()) {
    if (clock_ != nullptr && pending_.front().deliver_at > clock_->now()) return;
    const ExecutionReport report = pending_.front().report;
    pending_.pop_front();
    emit_now(report);
  }
}

std::string_view SimulatedBroker::channel() const noexcept { return "simulated"; }
bool SimulatedBroker::connected() const noexcept { return connected_; }

void SimulatedBroker::set_execution_handler(ExecutionHandler handler) { execution_handler_ = std::move(handler); }

void SimulatedBroker::request_account_snapshot(const AccountId& account_id) {
  require_connected();
  if (!account_snapshot_handler_) return;
  TradeAccountSnapshot snapshot{};
  snapshot.account_id = account_id;
  snapshot.total_asset = book_.cash();
  snapshot.available_cash = book_.cash();
  snapshot.withdrawable_cash = book_.cash();
  for (const auto& entry : book_.all_positions()) {
    PositionSnapshot position{};
    position.instrument_id = entry.second.instrument_id;
    position.total_quantity = entry.second.total;
    position.sellable_quantity = entry.second.sellable;
    position.average_cost = entry.second.average_cost;
    snapshot.positions.push_back(std::move(position));
  }
  account_snapshot_handler_(snapshot);
}

void SimulatedBroker::request_open_orders(const AccountId& account_id) {
  require_connected();
  // Re-emit working orders so OMS can reconcile; terminal orders are
  // never replayed from here. Partials re-emit with their cumulative so a
  // recovering OMS converges to the true state instead of live/zero.
  for (const auto& entry : orders_) {
    const WorkingOrder& order = entry.second;
    if (order.state != OrderStatus::live || order.intent.account_id != account_id) continue;
    const OrderStatus status = order.filled > 0 ? OrderStatus::partially_filled : OrderStatus::live;
    deliver(ExecutionReport{order.order_id, order.intent.client_order_id, status, 0, 0, "", "", order.filled, 0, "", 0});
  }
  if (recovery_complete_handler_) recovery_complete_handler_(account_id, true);
}

void SimulatedBroker::set_account_snapshot_handler(AccountSnapshotHandler handler) { account_snapshot_handler_ = std::move(handler); }

void SimulatedBroker::request_security_info(const AccountId& account_id, const InstrumentId& instrument) {
  require_connected();
  // Paper channels carry no reference data: an empty, complete batch.
  if (security_info_handler_) security_info_handler_(account_id, {}, true);
  (void)instrument;
}

void SimulatedBroker::set_security_info_handler(SecurityInfoHandler handler) { security_info_handler_ = std::move(handler); }

void SimulatedBroker::request_asset_snapshot(const AccountId& account_id) {
  require_connected();
  if (!asset_snapshot_handler_) return;
  TradeAssetSnapshot snapshot{};
  snapshot.account_id = account_id;
  snapshot.leaves_value = book_.cash();
  snapshot.available_t1 = book_.cash();
  asset_snapshot_handler_(snapshot);
}

void SimulatedBroker::set_asset_snapshot_handler(AssetSnapshotHandler handler) { asset_snapshot_handler_ = std::move(handler); }

void SimulatedBroker::set_recovery_complete_handler(RecoveryCompleteHandler handler) { recovery_complete_handler_ = std::move(handler); }

void SimulatedBroker::submit(const OrderIntent& intent) {
  require_connected();
  if (orders_.find(intent.client_order_id) != orders_.end()) {
    // Defensive: OMS guards duplicates, but a replayed submit must surface
    // as an explicit rejection rather than corrupting the book.
    deliver(ExecutionReport{"", intent.client_order_id, OrderStatus::rejected, 0, 0, "duplicate client order id", "", 0, 0, "", 0});
    return;
  }
  WorkingOrder order{intent, "SIM-" + intent.client_order_id, OrderStatus::live};
  order.seq = next_seq_++;
  orders_.emplace(intent.client_order_id, order);
  WorkingOrder& stored = orders_.at(intent.client_order_id);
  emit_live(stored);

  if (books_.find(intent.instrument_id) != books_.end()) {
    emit_book_fill(stored);
    return;
  }
  const auto ref = reference_prices_.find(intent.instrument_id);
  if (ref == reference_prices_.end()) return;
  const bool crosses = intent.type == OrderType::market || (intent.side == Side::buy && intent.limit_price >= ref->second) || (intent.side == Side::sell && intent.limit_price <= ref->second);
  if (crosses) emit_fill(stored, ref->second);
}

void SimulatedBroker::cancel(const ClientOrderId& client_order_id) {
  require_connected();
  const auto iter = orders_.find(client_order_id);
  if (iter == orders_.end() || iter->second.state != OrderStatus::live) return;  // filled/unknown: nothing to do
  iter->second.state = OrderStatus::cancelled;
  emit_cancelled(iter->second);
}

void SimulatedBroker::require_connected() const {
  if (!connected_) throw std::runtime_error("simulated broker is disconnected");
}

void SimulatedBroker::deliver(const ExecutionReport& report) {
  if (clock_ != nullptr && latency_ > std::chrono::nanoseconds{0}) {
    pending_.push_back({clock_->now() + latency_, report});
    return;
  }
  emit_now(report);
}

void SimulatedBroker::emit_now(const ExecutionReport& report) {
  if (execution_handler_) {
    execution_handler_(report);
  } else {
    events_.publish(report);
  }
}

void SimulatedBroker::emit_live(const WorkingOrder& order) {
  deliver(ExecutionReport{order.order_id, order.intent.client_order_id, OrderStatus::live, 0, 0, "", "SIM-ACK-" + order.intent.client_order_id, 0, 0, "", 0});
}

void SimulatedBroker::emit_fill(WorkingOrder& order, Price reference) {
  order.state = OrderStatus::filled;
  const Price execution_price = reference + (order.intent.side == Side::buy ? slippage_per_unit_ : -slippage_per_unit_);
  const Price notional = order.intent.quantity * execution_price;
  const Price fee = fill_fee(schedule_for(fee_table_, order.intent.instrument_id), order.intent.side, notional);
  book_.apply_fill(order.intent, order.intent.quantity, execution_price);
  book_.apply_fee(fee);
  deliver(ExecutionReport{order.order_id, order.intent.client_order_id, OrderStatus::filled, order.intent.quantity, execution_price, "", "SIM-FILL-" + order.intent.client_order_id,
                          order.intent.quantity, 0, "", 0, fee});
}

// Book-aware match (M6-0): consume standing depth once at submit time.
// Full consumption fills; a partial leaves the remainder live; zero touch
// keeps the live ack only. Execution ids carry a per-order fill sequence
// so OMS dedup (stable execution id) never collapses distinct fills.
void SimulatedBroker::emit_book_fill(WorkingOrder& order) {
  OrderBook& book = books_.at(order.intent.instrument_id);
  const BookFill fill = book.take(order.intent.side, order.intent.type, order.intent.limit_price, order.intent.quantity);
  if (fill.filled == 0) return;
  const Price execution_price = fill.average_price + (order.intent.side == Side::buy ? slippage_per_unit_ : -slippage_per_unit_);
  const Price notional = fill.filled * execution_price;
  const Price fee = fill_fee(schedule_for(fee_table_, order.intent.instrument_id), order.intent.side, notional);
  book_.apply_fill(order.intent, fill.filled, execution_price);
  book_.apply_fee(fee);
  order.filled = fill.filled;
  ++order.fill_seq;
  const bool complete = fill.filled == order.intent.quantity;
  if (complete) order.state = OrderStatus::filled;
  deliver(ExecutionReport{order.order_id, order.intent.client_order_id, complete ? OrderStatus::filled : OrderStatus::partially_filled, fill.filled, execution_price, "",
                          "SIM-FILL-" + order.intent.client_order_id + "-" + std::to_string(order.fill_seq), fill.filled, 0, "", 0, fee});
}

void SimulatedBroker::match_resting(const InstrumentId& instrument, OrderBook& book) {
  std::vector<ClientOrderId> resting;
  for (const auto& entry : orders_) {
    if (entry.second.state == OrderStatus::live && entry.second.intent.instrument_id == instrument) resting.push_back(entry.first);
  }
  std::sort(resting.begin(), resting.end(), [&](const ClientOrderId& lhs, const ClientOrderId& rhs) { return orders_.at(lhs).seq < orders_.at(rhs).seq; });
  for (const auto& order_id : resting) {
    WorkingOrder& order = orders_.at(order_id);
    const Quantity remaining = order.intent.quantity - order.filled;
    if (remaining <= 0) continue;
    const BookFill fill = book.take(order.intent.side, order.intent.type, order.intent.limit_price, remaining);
    if (fill.filled == 0) continue;
    const Price execution_price = fill.average_price + (order.intent.side == Side::buy ? slippage_per_unit_ : -slippage_per_unit_);
    const Price notional = fill.filled * execution_price;
    const Price fee = fill_fee(schedule_for(fee_table_, order.intent.instrument_id), order.intent.side, notional);
    book_.apply_fill(order.intent, fill.filled, execution_price);
    book_.apply_fee(fee);
    order.filled += fill.filled;
    ++order.fill_seq;
    const bool complete = order.filled == order.intent.quantity;
    if (complete) order.state = OrderStatus::filled;
    deliver(ExecutionReport{order.order_id, order.intent.client_order_id, complete ? OrderStatus::filled : OrderStatus::partially_filled, fill.filled, execution_price, "",
                            "SIM-FILL-" + order.intent.client_order_id + "-" + std::to_string(order.fill_seq), order.filled, 0, "", 0, fee});
  }
}

void SimulatedBroker::emit_cancelled(const WorkingOrder& order) {
  deliver(ExecutionReport{order.order_id, order.intent.client_order_id, OrderStatus::cancelled, 0, 0, "", "SIM-CXL-" + order.intent.client_order_id, order.filled, 0, "", 0});
}

}  // namespace sts
