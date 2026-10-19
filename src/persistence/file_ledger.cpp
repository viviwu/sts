// clang-format off
/**
  ******************************************************************************
  * @file           : file_ledger.cpp
  * @author         : vivi wu
  * @brief          : JSONL ledger implementation: append, resume, corrupt-line rejection.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/persistence/file_ledger.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <stdexcept>

#include "nlohmann/json.hpp"
#include "sts/persistence/event_store.hpp"

namespace sts {
namespace {

using nlohmann::json;

std::int64_t to_ns(Timestamp time) { return std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count(); }

Timestamp from_ns(std::int64_t value) { return Timestamp{std::chrono::nanoseconds{value}}; }

LedgerEventType event_type_from_string(const std::string& file, std::uint64_t line_no, const std::string& value) {
  if (value == "order_submitted") return LedgerEventType::order_submitted;
  if (value == "execution_applied") return LedgerEventType::execution_applied;
  if (value == "order_cancel_requested") return LedgerEventType::order_cancel_requested;
  if (value == "risk_rejected") return LedgerEventType::risk_rejected;
  if (value == "funding_deposited") return LedgerEventType::funding_deposited;
  if (value == "reconcile_attested") return LedgerEventType::reconcile_attested;
  if (value == "control_applied") return LedgerEventType::control_applied;
  throw std::runtime_error(file + ": line " + std::to_string(line_no) + ": unknown event type '" + value + "'");
}

json serialize_intent(const OrderIntent& intent) {
  return json{{"client_order_id", intent.client_order_id},
              {"account_id", intent.account_id},
              {"strategy_id", intent.strategy_id},
              {"instrument_id", intent.instrument_id},
              {"side", static_cast<int>(intent.side)},
              {"type", static_cast<int>(intent.type)},
              {"time_in_force", static_cast<int>(intent.time_in_force)},
              {"quantity", intent.quantity},
              {"limit_price", intent.limit_price},
              {"trace_id", intent.trace_id},
              {"exchange", static_cast<int>(intent.exchange)},
              {"board", static_cast<int>(intent.board)},
              {"security_account_id", intent.security_account_id},
              {"trading_business", static_cast<int>(intent.trading_business)},
              {"market_order_style", static_cast<int>(intent.market_order_style)},
              {"protection_price", intent.protection_price},
              {"parent_client_order_id", intent.parent_client_order_id}};
}

OrderIntent parse_intent(const json& node) {
  OrderIntent intent{};
  intent.client_order_id = node.at("client_order_id").get<std::string>();
  intent.account_id = node.at("account_id").get<std::string>();
  intent.strategy_id = node.at("strategy_id").get<std::string>();
  intent.instrument_id = node.at("instrument_id").get<std::string>();
  intent.side = static_cast<Side>(node.at("side").get<int>());
  intent.type = static_cast<OrderType>(node.at("type").get<int>());
  intent.time_in_force = static_cast<TimeInForce>(node.at("time_in_force").get<int>());
  intent.quantity = node.at("quantity").get<Quantity>();
  intent.limit_price = node.at("limit_price").get<Price>();
  intent.trace_id = node.at("trace_id").get<std::string>();
  intent.exchange = static_cast<Exchange>(node.at("exchange").get<int>());
  intent.board = static_cast<AshareBoard>(node.at("board").get<int>());
  intent.security_account_id = node.at("security_account_id").get<std::string>();
  intent.trading_business = static_cast<TradingBusiness>(node.at("trading_business").get<int>());
  intent.market_order_style = static_cast<MarketOrderStyle>(node.at("market_order_style").get<int>());
  intent.protection_price = node.at("protection_price").get<Price>();
  intent.parent_client_order_id = node.at("parent_client_order_id").get<std::string>();
  return intent;
}

json serialize_snapshot(const ExecutionReport& report) {
  return json{{"order_id", report.order_id},
              {"client_order_id", report.client_order_id},
              {"status", static_cast<int>(report.status)},
              {"last_quantity", report.last_quantity},
              {"last_price", report.last_price},
              {"reason", report.reason},
              {"execution_id", report.execution_id},
              {"cumulative_quantity", report.cumulative_quantity},
              {"version", report.version},
              {"broker_order_id", report.broker_order_id},
              {"reject_code", report.reject_code},
              {"fee", report.fee}};
}

ExecutionReport parse_snapshot(const json& node) {
  ExecutionReport report{};
  report.order_id = node.at("order_id").get<std::string>();
  report.client_order_id = node.at("client_order_id").get<std::string>();
  report.status = static_cast<OrderStatus>(node.at("status").get<int>());
  report.last_quantity = node.at("last_quantity").get<Quantity>();
  report.last_price = node.at("last_price").get<Price>();
  report.reason = node.at("reason").get<std::string>();
  report.execution_id = node.at("execution_id").get<std::string>();
  report.cumulative_quantity = node.at("cumulative_quantity").get<Quantity>();
  report.version = node.at("version").get<std::uint64_t>();
  // ADR-032 fields are optional: ledgers written before ADR-032 must replay.
  report.broker_order_id = node.value("broker_order_id", std::string{});
  report.reject_code = node.value("reject_code", 0);
  // M11-6 fee likewise defaults to zero for pre-fee ledgers.
  report.fee = node.value("fee", Price{0});
  return report;
}

json serialize_event(const LedgerEvent& event) {
  return json{{"event_id", event.envelope.event_id},
              {"type", event_type_name(event.type)},
              {"trace_id", event.envelope.trace_id},
              {"source_time_ns", to_ns(event.envelope.source_time)},
              {"receive_time_ns", to_ns(event.envelope.receive_time)},
              {"config_version", event.envelope.config_version},
              {"rules_version", event.envelope.rules_version},
              {"source_message_id", event.envelope.source_message_id},
              {"trade_date", event.envelope.trade_date},
              {"intent", serialize_intent(event.intent)},
              {"snapshot", serialize_snapshot(event.snapshot)},
              {"funding",
               {{"strategy_id", event.funding.strategy},
                {"instrument_id", event.funding.instrument},
                {"quantity", event.funding.quantity},
                {"average_cost", event.funding.average_cost},
                {"cash", event.funding.cash}}},
              {"attestation",
               {{"operator_id", event.attestation.operator_id},
                {"broker_snapshot_time", event.attestation.broker_snapshot_time},
                {"ledger_watermark", event.attestation.ledger_watermark},
                {"cash_dirty", event.attestation.cash_dirty},
                {"position_dirty", event.attestation.position_dirty},
                {"disclosed_fees", event.attestation.disclosed_fees},
                {"note", event.attestation.note}}},
              {"control", {{"command", event.control.command}, {"target", event.control.target}, {"blocked_after", event.control.blocked_after}, {"source", event.control.source}}}};
}

FundingDeposit parse_funding(const json& node) {
  FundingDeposit funding{};
  funding.strategy = node.value("strategy_id", std::string{});
  funding.instrument = node.value("instrument_id", std::string{});
  funding.quantity = node.value("quantity", Quantity{0});
  funding.average_cost = node.value("average_cost", Price{0});
  funding.cash = node.value("cash", Price{0});
  return funding;
}

ControlRecord parse_control(const json& node) {
  ControlRecord record{};
  record.command = node.value("command", std::string{});
  record.target = node.value("target", std::string{});
  record.blocked_after = node.value("blocked_after", false);
  record.source = node.value("source", std::string{});
  return record;
}

ReconcileAttestation parse_attestation(const json& node) {
  ReconcileAttestation attestation{};
  attestation.operator_id = node.value("operator_id", std::string{});
  attestation.broker_snapshot_time = node.value("broker_snapshot_time", std::string{});
  attestation.ledger_watermark = node.value("ledger_watermark", std::uint64_t{0});
  attestation.cash_dirty = node.value("cash_dirty", 0);
  attestation.position_dirty = node.value("position_dirty", 0);
  attestation.disclosed_fees = node.value("disclosed_fees", Price{0});
  attestation.note = node.value("note", std::string{});
  return attestation;
}

LedgerEvent parse_event(const std::string& file, std::uint64_t line_no, const json& node) {
  LedgerEvent event{};
  event.envelope.event_id = node.at("event_id").get<std::uint64_t>();
  event.type = event_type_from_string(file, line_no, node.at("type").get<std::string>());
  event.envelope.trace_id = node.at("trace_id").get<std::string>();
  event.envelope.source_time = from_ns(node.at("source_time_ns").get<std::int64_t>());
  event.envelope.receive_time = from_ns(node.at("receive_time_ns").get<std::int64_t>());
  event.envelope.config_version = node.at("config_version").get<std::string>();
  event.envelope.rules_version = node.at("rules_version").get<std::string>();
  event.envelope.source_message_id = node.at("source_message_id").get<std::string>();
  // trade_date is optional: ledgers written before the field existed replay
  // with "" (unknown), same pattern as the ADR-032 keys above.
  event.envelope.trade_date = node.value("trade_date", std::string{});
  event.intent = parse_intent(node.at("intent"));
  event.snapshot = parse_snapshot(node.at("snapshot"));
  // Funding node is optional: ledgers written before M11-7 replay with an
  // empty deposit, same pattern as the ADR-032 keys above.
  if (node.contains("funding")) event.funding = parse_funding(node.at("funding"));
  // Attestation node is optional: ledgers written before M14 replay with an
  // empty attestation, same pattern as the funding node above.
  if (node.contains("attestation")) event.attestation = parse_attestation(node.at("attestation"));
  // Control node is optional: ledgers written before M15 replay with an
  // empty record, same pattern as the nodes above.
  if (node.contains("control")) event.control = parse_control(node.at("control"));
  return event;
}

}  // namespace

FileEventLedger::FileEventLedger(std::string path)
    : path_{std::move(path)} {
  std::ifstream input{path_};
  if (!input) return;  // fresh ledger: file is created on first append.
  std::string line;
  std::uint64_t line_no = 0;
  std::uint64_t max_id = 0;
  while (std::getline(input, line)) {
    ++line_no;
    if (line.empty()) continue;
    json node;
    try {
      node = json::parse(line);
    } catch (const json::exception& e) {
      throw std::runtime_error(path_ + ": line " + std::to_string(line_no) + ": json parse error: " + e.what());
    }
    try {
      LedgerEvent event = parse_event(path_, line_no, node);
      if (event.envelope.event_id == 0) {
        throw std::runtime_error("event_id must be non-zero");
      }
      max_id = std::max(max_id, event.envelope.event_id);
      events_.push_back(std::move(event));
    } catch (const json::exception& e) {
      throw std::runtime_error(path_ + ": line " + std::to_string(line_no) + ": bad event: " + e.what());
    }
  }
  if (max_id > 0) next_id_ = max_id + 1;
}

std::uint64_t FileEventLedger::append(LedgerEvent event) {
  std::scoped_lock lock{mutex_};
  const std::uint64_t event_id = next_id_++;
  event.envelope.event_id = event_id;
  const std::string line = serialize_event(event).dump();
  std::ofstream out{path_, std::ios::app};
  if (!out) throw std::runtime_error("cannot open ledger file for append: " + path_);
  out << line << '\n';
  out.flush();
  if (!out) throw std::runtime_error("failed to append ledger file: " + path_);
  events_.push_back(std::move(event));
  return event_id;
}

std::vector<LedgerEvent> FileEventLedger::replay(std::uint64_t from_event_id) const {
  std::scoped_lock lock{mutex_};
  std::vector<LedgerEvent> out;
  for (const auto& event : events_) {
    if (event.envelope.event_id >= from_event_id) out.push_back(event);
  }
  return out;
}

std::uint64_t FileEventLedger::latest_event_id() const {
  std::scoped_lock lock{mutex_};
  return events_.empty() ? 0 : events_.back().envelope.event_id;
}

void write_open_snapshot(const std::string& path, const OpenSnapshot& snapshot) {
  json opens = json::array();
  for (const auto& entry : snapshot.opens) {
    opens.push_back({{"intent", serialize_intent(entry.first)}, {"snapshot", serialize_snapshot(entry.second)}});
  }
  const json node{{"ledger_bytes", snapshot.ledger_bytes},
                  {"last_event_id", snapshot.last_event_id},
                  {"config_version", snapshot.config_version},
                  {"rules_version", snapshot.rules_version},
                  {"opens", std::move(opens)}};
  std::ofstream out{path, std::ios::trunc};
  if (!out) throw std::runtime_error("cannot open snapshot file for write: " + path);
  out << node.dump() << '\n';
  out.flush();
  if (!out) throw std::runtime_error("failed to write snapshot file: " + path);
}

OpenSnapshot read_open_snapshot(const std::string& path) {
  std::ifstream input{path};
  if (!input) throw std::runtime_error("cannot open snapshot file: " + path);
  std::string text;
  if (!std::getline(input, text) || text.empty()) throw std::runtime_error("snapshot file is empty: " + path);
  json node;
  try {
    node = json::parse(text);
  } catch (const json::exception& e) {
    throw std::runtime_error(path + ": snapshot json parse error: " + e.what());
  }
  try {
    OpenSnapshot snapshot{};
    snapshot.ledger_bytes = node.at("ledger_bytes").get<std::uint64_t>();
    snapshot.last_event_id = node.at("last_event_id").get<std::uint64_t>();
    snapshot.config_version = node.at("config_version").get<std::string>();
    snapshot.rules_version = node.at("rules_version").get<std::string>();
    for (const auto& entry : node.at("opens")) {
      snapshot.opens.emplace_back(parse_intent(entry.at("intent")), parse_snapshot(entry.at("snapshot")));
    }
    return snapshot;
  } catch (const json::exception& e) {
    throw std::runtime_error(path + ": bad snapshot: " + e.what());
  }
}

}  // namespace sts
