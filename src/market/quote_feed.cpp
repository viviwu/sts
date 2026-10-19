// clang-format off
/**
  ******************************************************************************
  * @file           : quote_feed.cpp
  * @author         : vivi wu
  * @brief          : Simulated L1 quote walk with phase windows and fault injection.
  * @version        : 0.1.0
  * @date           : 03/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/market/quote_feed.hpp"

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace qts {

SimulatedQuoteFeed::SimulatedQuoteFeed(std::uint64_t seed, Price start_price, Price step, Price low, Price high)
    : start_{start_price},
      current_{start_price},
      step_{step},
      low_{low},
      high_{high},
      rng_{seed} {}

void SimulatedQuoteFeed::set_price_limits(Price limit_down, Price limit_up) noexcept {
  limit_down_ = limit_down;
  limit_up_ = limit_up;
}

SimulatedQuoteFeed::SymbolState& SimulatedQuoteFeed::state_for(const InstrumentId& instrument_id) {
  auto found = symbols_.find(instrument_id);
  if (found != symbols_.end()) return found->second;
  SymbolState state{};
  state.current = start_;
  state.cumulative_volume = 0;
  state.seeded = true;
  const auto inserted = symbols_.emplace(instrument_id, state);
  return inserted.first->second;
}

Price SimulatedQuoteFeed::current_for(const InstrumentId& instrument_id) const {
  const auto found = symbols_.find(instrument_id);
  if (found == symbols_.end()) return start_;
  return found->second.current;
}

QuoteEvent SimulatedQuoteFeed::next(const InstrumentId& instrument_id, std::uint64_t sequence) {
  SymbolState& state = state_for(instrument_id);
  const int move = draw_(rng_);
  Price candidate = state.current + (static_cast<Price>(move) * step_);
  if (candidate > high_) candidate = high_ - step_;
  if (candidate < low_) candidate = low_ + step_;
  state.current = candidate;
  current_ = candidate;
  ++emitted_;

  // Deterministic volume: 100 shares per tick so POV/VWAP budgets move.
  state.cumulative_volume += 100;
  const Price turnover = state.cumulative_volume * candidate;

  QuoteEvent quote{};
  quote.instrument_id = instrument_id;
  quote.last_price = candidate;
  quote.bid_price = candidate - step_;
  quote.bid_quantity = 100;
  quote.ask_price = candidate + step_;
  quote.ask_quantity = 100;
  quote.cumulative_volume = state.cumulative_volume;
  quote.cumulative_turnover = turnover;
  quote.limit_down = limit_down_;
  quote.limit_up = limit_up_;
  quote.bid_levels.push_back({quote.bid_price, quote.bid_quantity});
  quote.ask_levels.push_back({quote.ask_price, quote.ask_quantity});
  quote.sequence = sequence;
  const auto now = std::chrono::system_clock::now();
  quote.exchange_time = now;
  quote.receive_time = now;
  quote.quality = QuoteQuality::valid;

  // Phase windows: leading auction, configured halt window, else forced.
  if (faults_.auction_ticks > 0 && emitted_ <= faults_.auction_ticks) {
    quote.market_phase = MarketPhase::opening_auction;
  } else if (faults_.halt_for > 0 && faults_.halt_after > 0 && emitted_ >= faults_.halt_after && emitted_ < faults_.halt_after + faults_.halt_for) {
    quote.market_phase = MarketPhase::halted;
  } else {
    quote.market_phase = forced_phase_;
  }

  // Fault injection (deterministic by emission count, not sequence).
  if (faults_.crossed_every > 0 && (emitted_ % faults_.crossed_every) == 0) {
    quote.bid_price = quote.ask_price + step_;
    if (!quote.bid_levels.empty()) quote.bid_levels[0].price = quote.bid_price;
    quote.quality = QuoteQuality::crossed;
  }
  if (faults_.stale_every > 0 && (emitted_ % faults_.stale_every) == 0) {
    quote.quality = QuoteQuality::stale;
    quote.receive_time = now - std::chrono::seconds{10};
  }
  return quote;
}

namespace {

Price parse_price_field(const std::string& text, const std::string& path, std::size_t line_no) {
  try {
    std::size_t used = 0;
    const std::int64_t value = std::stoll(text, &used);
    if (used != text.size() || value <= 0) throw std::invalid_argument{"range"};
    return value;
  } catch (const std::exception&) {
    throw std::runtime_error(path + ": line " + std::to_string(line_no) + " needs positive integer prices/quantities");
  }
}

Quantity parse_quantity_field(const std::string& text, const std::string& path, std::size_t line_no) { return parse_price_field(text, path, line_no); }

QuoteEvent parse_file_row(const std::string& line, const std::string& path, std::size_t line_no, std::uint64_t sequence) {
  std::vector<std::string> fields;
  std::string field;
  std::istringstream row{line};
  while (std::getline(row, field, ',')) fields.push_back(field);
  if (fields.size() != 6 || fields[0].empty()) {
    throw std::runtime_error(path + ": line " + std::to_string(line_no) + " must be instrument,last,bid,ask,bid_qty,ask_qty");
  }
  const Price last = parse_price_field(fields[1], path, line_no);
  const Price bid = parse_price_field(fields[2], path, line_no);
  const Price ask = parse_price_field(fields[3], path, line_no);
  const Quantity bid_qty = parse_quantity_field(fields[4], path, line_no);
  const Quantity ask_qty = parse_quantity_field(fields[5], path, line_no);
  if (bid > last || last > ask) {
    throw std::runtime_error(path + ": line " + std::to_string(line_no) + " needs bid <= last <= ask");
  }
  QuoteEvent quote{};
  quote.instrument_id = fields[0];
  quote.last_price = last;
  quote.bid_price = bid;
  quote.bid_quantity = bid_qty;
  quote.ask_price = ask;
  quote.ask_quantity = ask_qty;
  quote.cumulative_volume = bid_qty + ask_qty;
  quote.cumulative_turnover = quote.cumulative_volume * last;
  quote.bid_levels.push_back({bid, bid_qty});
  quote.ask_levels.push_back({ask, ask_qty});
  quote.sequence = sequence;
  const auto now = std::chrono::system_clock::now();
  quote.exchange_time = now;
  quote.receive_time = now;
  quote.quality = QuoteQuality::valid;
  quote.market_phase = MarketPhase::continuous;
  return quote;
}

}  // namespace

FileQuoteFeed::FileQuoteFeed(const std::string& path) {
  std::ifstream input{path};
  if (!input.is_open()) throw std::runtime_error("quote file not readable: " + path);
  std::string line;
  std::size_t line_no = 0;
  while (std::getline(input, line)) {
    ++line_no;
    if (line.empty() || line[0] == '#') continue;
    quotes_.push_back(parse_file_row(line, path, line_no, quotes_.size() + 1));
  }
  if (quotes_.empty()) throw std::runtime_error("quote file has no quote rows: " + path);
}

std::optional<QuoteEvent> FileQuoteFeed::next() {
  if (cursor_ >= quotes_.size()) return std::nullopt;
  return quotes_[cursor_++];
}

}  // namespace qts
