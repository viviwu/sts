// clang-format off
/**
  ******************************************************************************
  * @file           : broker_gateway.hpp
  * @author         : vivi wu
  * @brief          : Deprecated compatibility alias over TradeApi (new code uses TradeApi).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include "qts/broker/trade_api.hpp"

namespace qts {

// Compatibility alias. New core code must depend on TradeApi; broker-specific
// adapters implement TradeApi and, independently, QuoteApi.
using BrokerGateway [[deprecated("Use TradeApi")]] = TradeApi;

}  // namespace qts
