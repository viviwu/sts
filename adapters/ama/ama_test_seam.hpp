// clang-format off
/**
  ******************************************************************************
  * @file           : ama_test_seam.hpp
  * @author         : vivi wu
  * @brief          : Vendor-free seam for AMA adapter unit tests (no SDK linkage).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

// Vendor-gated test seam for the AMA adapter (M7-2b).
//
// This is the single header outside the adapter .cpp that includes vendor
// headers, and it is only reachable from adapters/tests (built with
// QTS_ENABLE_VENDOR_SDKS=ON; CI never enables it). It exposes:
//   - pure vendor->fixture conversion (snapshot_to_fixture): the exact
//     field-for-field copy the SPI callback performs, callable without a
//     vendor session and without touching IAMDApi::FreeMemory;
//   - build_cfg/parse_instrument: the session/subscription assembly logic;
//   - inject_*: feed the adapter's queue and session state as if the vendor
//     callback threads had delivered data (friend seams on the adapter).
// Live session behaviour (Init/SubscribeData/QueryMDTick against the real
// SDK) is covered by the M7-2b 联调 run, not here.

#include <cstdint>
#include <string>

#include "ama.h"
#include "ama_quote_adapter.hpp"

namespace qts {
namespace ama_detail {

AmaSnapshotFixture snapshot_to_fixture(const amd::ama::MDSnapshot& snapshot);

// Fills a vendor Cfg from the session config. Returns false (leaving `out`
// untouched) when credentials exceed vendor array sizes or the UMS list is
// empty or longer than the vendor's 8-slot limit.
bool build_cfg(const AmaSessionConfig& config, amd::ama::Cfg& out);

// "600000.SSE" -> kSSE/"600000", "000001.SZSE" -> kSZSE, "920000.BSE" ->
// kNEEQ. Returns false for malformed ids/unknown suffixes (caller skips and
// counts them in subscriptions_invalid).
bool parse_instrument(const InstrumentId& id, std::int32_t& market, std::string& code);

}  // namespace ama_detail
}  // namespace qts
