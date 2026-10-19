// clang-format off
/**
  ******************************************************************************
  * @file           : grid_strategy_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for grid parent-order ids and per-run prefixes.
  * @version        : 0.1.0
  * @date           : 03/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/strategy/grid_strategy.hpp"

#include <cassert>
#include <string>
#include <vector>

#include "sts/market/instrument.hpp"

namespace {

sts::GridConfig grid_config() {
  sts::GridConfig config{};
  config.strategy_id = "grid-1";
  config.account_id = "paper";
  config.instrument_id = "TEST";
  config.lower = 900;
  config.upper = 1'100;
  config.grids = 4;
  config.quantity_per_order = 100;
  return config;
}

sts::QuoteEvent make_quote(sts::Price last, std::uint64_t sequence) {
  sts::QuoteEvent quote{};
  quote.instrument_id = "TEST";
  quote.last_price = last;
  quote.sequence = sequence;
  quote.receive_time = sts::Timestamp{};
  quote.quality = sts::QuoteQuality::valid;
  return quote;
}

std::vector<std::string> collect_ids(sts::GridStrategy& strategy) {
  const sts::Portfolio book{0};
  std::vector<std::string> ids;
  std::uint64_t sequence = 0;
  for (const sts::Price price : {1'000, 950, 900}) {
    for (const auto& intent : strategy.on_quote(make_quote(price, ++sequence), book)) ids.push_back(intent.client_order_id);
  }
  return ids;
}

void default_ids_keep_legacy_shape() {
  sts::GridStrategy strategy{grid_config(), 1'000};
  const auto ids = collect_ids(strategy);
  assert(!ids.empty());
  assert(ids[0] == "grid-1");
  for (const auto& id : ids) assert(id.rfind("grid-", 0) == 0);
}

void prefix_makes_ids_run_unique() {
  sts::GridConfig config = grid_config();
  config.id_prefix = "grid-1-20261002-1234-";
  sts::GridStrategy strategy{config, 1'000};
  const auto ids = collect_ids(strategy);
  assert(!ids.empty());
  for (const auto& id : ids) assert(id.rfind("grid-1-20261002-1234-", 0) == 0);
}

void same_prefix_replays_identically() {
  sts::GridConfig config = grid_config();
  config.id_prefix = "grid-1-20261002-1234-";
  sts::GridStrategy first{config, 1'000};
  sts::GridStrategy second{config, 1'000};
  assert(collect_ids(first) == collect_ids(second));
}

}  // namespace

int main() {
  default_ids_keep_legacy_shape();
  prefix_makes_ids_run_unique();
  same_prefix_replays_identically();
  return 0;
}
