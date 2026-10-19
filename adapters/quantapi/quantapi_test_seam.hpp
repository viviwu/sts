// clang-format off
/**
  ******************************************************************************
  * @file           : quantapi_test_seam.hpp
  * @author         : vivi wu
  * @brief          : Vendor-free seam for QuantAPI adapter unit tests (no SDK linkage).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
// SPDX-License-Identifier: MIT
// Vendor-gated test seam for the QuantAPI adapter (M7-2c).
//
// Unlike the AMA seam this header pulls in no vendor headers: the adapter
// converts every vendor callback into a value fixture (quantapi_fixtures.hpp)
// before any logic runs, so the inject_* functions declared as friends on the
// adapter (quantapi_trade_adapter.hpp) exercise the full attribution,
// mapping, sidecar and drain pipeline with zero SDK involvement at test
// level. It is only reachable from adapters/tests (vendor-gated target; CI
// never builds it, ADR-033).
//
// Covered here: fixture feeding (login/ack/rtn/cancel rsp/query rows/fund/
// shares), the submit bookkeeping seed, and the pure mappings asserted
// directly (parse_instrument / map_ord_status / map_order_type). NOT covered:
// ATPQuantAPI::Init/Login and real Req* sends — that is the M7-2c 联调 run
// with credentials in the Linux environment, plus sts_quantapi_readonly.
#pragma once

#include "sts/broker/quantapi_fixtures.hpp"
#include "quantapi_trade_adapter.hpp"
