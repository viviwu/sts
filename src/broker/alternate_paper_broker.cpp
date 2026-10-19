// clang-format off
/**
  ******************************************************************************
  * @file           : alternate_paper_broker.cpp
  * @author         : vivi wu
  * @brief          : Alternate paper channel implementation (admission, session, drain, seq file).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/broker/alternate_paper_broker.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

namespace sts {

AlternatePaperBroker::AlternatePaperBroker(EventBus& events, std::string seq_path)
    : events_{events},
      seq_path_{std::move(seq_path)} {
  load_seq();
}

void AlternatePaperBroker::set_starting_cash(Price cash) { book_ = Portfolio(cash); }

void AlternatePaperBroker::set_fee_schedule(FeeSchedule schedule) { fee_table_.fallback = schedule; }

void AlternatePaperBroker::set_fee_override(InstrumentId instrument, FeeSchedule schedule) { fee_table_.overrides.insert_or_assign(std::move(instrument), schedule); }

void AlternatePaperBroker::fund_settled_position(const StrategyId& strategy, InstrumentId instrument, Quantity quantity, Price avg_cost) {
  book_.fund_settled(strategy, std::move(instrument), quantity, avg_cost);
}

void AlternatePaperBroker::set_reference_price(InstrumentId instrument, Price price) { reference_prices_.insert_or_assign(std::move(instrument), price); }

void AlternatePaperBroker::clear_reference_price(const InstrumentId& instrument) { reference_prices_.erase(instrument); }

void AlternatePaperBroker::set_session_open(bool open) { session_open_ = open; }

void AlternatePaperBroker::disconnect() { connected_ = false; }

void AlternatePaperBroker::reconnect() { connected_ = true; }

void AlternatePaperBroker::settle_day() { book_.settle_day(); }

void AlternatePaperBroker::drain() {
  while (!pending_.empty()) {
    const ExecutionReport report = pending_.front();
    pending_.pop_front();
    if (execution_handler_) {
      execution_handler_(report);
    } else {
      events_.publish(report);
    }
  }
}

std::string_view AlternatePaperBroker::channel() const noexcept { return "alternate"; }

bool AlternatePaperBroker::connected() const noexcept { return connected_; }

void AlternatePaperBroker::submit(const OrderIntent& intent) {
  require_tradable();
  if (orders_.find(intent.client_order_id) != orders_.end()) {
    emit_rejected(intent, "duplicate client order id");
    return;
  }
  if (intent.type == OrderType::limit && intent.limit_price % kPriceTick != 0) {
    emit_rejected(intent, "price violates venue tick");
    return;
  }
  if (intent.quantity % kLotSize != 0) {
    emit_rejected(intent, "quantity violates venue lot");
    return;
  }
  const std::uint64_t seq = next_seq();
  WorkingOrder order{intent, "ALT-" + std::to_string(seq), OrderStatus::live};
  orders_.emplace(intent.client_order_id, order);
  WorkingOrder& stored = orders_.at(intent.client_order_id);
  emit_live(stored);

  const auto ref = reference_prices_.find(intent.instrument_id);
  if (ref == reference_prices_.end()) return;
  const bool crosses = intent.type == OrderType::market || (intent.side == Side::buy && intent.limit_price >= ref->second) || (intent.side == Side::sell && intent.limit_price <= ref->second);
  if (crosses) emit_fill(stored, ref->second);
}

void AlternatePaperBroker::cancel(const ClientOrderId& client_order_id) {
  require_tradable();
  const auto iter = orders_.find(client_order_id);
  if (iter == orders_.end() || iter->second.state != OrderStatus::live) return;
  iter->second.state = OrderStatus::cancelled;
  emit_cancelled(iter->second);
}

void AlternatePaperBroker::set_execution_handler(ExecutionHandler handler) { execution_handler_ = std::move(handler); }

void AlternatePaperBroker::request_account_snapshot(const AccountId& account_id) {
  require_tradable();
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

void AlternatePaperBroker::request_open_orders(const AccountId& account_id) {
  require_tradable();
  for (const auto& entry : orders_) {
    const WorkingOrder& order = entry.second;
    if (order.state != OrderStatus::live || order.intent.account_id != account_id) continue;
    enqueue(ExecutionReport{order.order_id, order.intent.client_order_id, OrderStatus::live, 0, 0, "", "", 0, 0, "", 0});
  }
  if (recovery_complete_handler_) recovery_complete_handler_(account_id, true);
}

void AlternatePaperBroker::set_account_snapshot_handler(AccountSnapshotHandler handler) { account_snapshot_handler_ = std::move(handler); }

void AlternatePaperBroker::request_security_info(const AccountId& account_id, const InstrumentId& instrument) {
  require_tradable();
  // Paper channels carry no reference data: an empty, complete batch.
  if (security_info_handler_) security_info_handler_(account_id, {}, true);
  (void)instrument;
}

void AlternatePaperBroker::set_security_info_handler(SecurityInfoHandler handler) { security_info_handler_ = std::move(handler); }

void AlternatePaperBroker::request_asset_snapshot(const AccountId& account_id) {
  require_tradable();
  if (!asset_snapshot_handler_) return;
  TradeAssetSnapshot snapshot{};
  snapshot.account_id = account_id;
  snapshot.leaves_value = book_.cash();
  snapshot.available_t1 = book_.cash();
  asset_snapshot_handler_(snapshot);
}

void AlternatePaperBroker::set_asset_snapshot_handler(AssetSnapshotHandler handler) { asset_snapshot_handler_ = std::move(handler); }

void AlternatePaperBroker::set_recovery_complete_handler(RecoveryCompleteHandler handler) { recovery_complete_handler_ = std::move(handler); }

void AlternatePaperBroker::require_tradable() const {
  if (!connected_) throw std::runtime_error("alternate broker is disconnected");
  if (!session_open_) throw std::runtime_error("alternate channel session is closed");
}

void AlternatePaperBroker::enqueue(const ExecutionReport& report) { pending_.push_back(report); }

void AlternatePaperBroker::emit_live(const WorkingOrder& order) {
  enqueue(ExecutionReport{order.order_id, order.intent.client_order_id, OrderStatus::live, 0, 0, "", "ALT-ACK-" + order.intent.client_order_id, 0, 0, "", 0});
}

void AlternatePaperBroker::emit_fill(WorkingOrder& order, Price reference) {
  order.state = OrderStatus::filled;
  const Price notional = order.intent.quantity * reference;
  const Price fee = fill_fee(schedule_for(fee_table_, order.intent.instrument_id), order.intent.side, notional);
  book_.apply_fill(order.intent, order.intent.quantity, reference);
  book_.apply_fee(fee);
  enqueue(ExecutionReport{order.order_id, order.intent.client_order_id, OrderStatus::filled, order.intent.quantity, reference, "", "ALT-FILL-" + order.intent.client_order_id, order.intent.quantity, 0,
                          "", 0, fee});
}

void AlternatePaperBroker::emit_cancelled(const WorkingOrder& order) {
  enqueue(ExecutionReport{order.order_id, order.intent.client_order_id, OrderStatus::cancelled, 0, 0, "", "ALT-CXL-" + order.intent.client_order_id, 0, 0, "", 0});
}

void AlternatePaperBroker::emit_rejected(const OrderIntent& intent, const std::string& reason) {
  enqueue(ExecutionReport{"", intent.client_order_id, OrderStatus::rejected, 0, 0, reason, "", 0, 0, "", 0});
}

std::uint64_t AlternatePaperBroker::next_seq() {
  const std::uint64_t seq = next_seq_++;
  persist_seq();
  return seq;
}

void AlternatePaperBroker::persist_seq() const {
  std::ofstream out{seq_path_, std::ios::trunc};
  if (!out) throw std::runtime_error("cannot persist alternate broker sequence: " + seq_path_);
  out << next_seq_ << '\n';
  out.flush();
  if (!out) throw std::runtime_error("failed to persist alternate broker sequence: " + seq_path_);
}

void AlternatePaperBroker::load_seq() {
  std::ifstream input{seq_path_};
  if (!input) return;  // fresh channel: file is created on first accepted submit.
  std::uint64_t stored = 0;
  input >> stored;
  if (!input || stored == 0) throw std::runtime_error("corrupt alternate broker sequence file: " + seq_path_);
  next_seq_ = stored;
}

}  // namespace sts
