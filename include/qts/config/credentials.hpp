// clang-format off
/**
  ******************************************************************************
  * @file           : credentials.hpp
  * @author         : vivi wu
  * @brief          : Environment-only broker credentials with redacted display.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <string>

namespace qts {

// Broker credentials come exclusively from the process environment
// (M3-3 key policy): <PREFIX>_CUST_ID, <PREFIX>_FUND_ACCOUNT_ID and
// <PREFIX>_PASSWORD. They never live in yaml configs, logs or the
// ledger; use redacted() for any human-facing output.
struct BrokerCredentials {
  std::string cust_id;
  std::string fund_account_id;
  std::string password;

  [[nodiscard]] bool empty() const { return cust_id.empty() && fund_account_id.empty() && password.empty(); }
  // "cust_id=<set|empty> fund_account_id=... password=<set|empty>": values never printed.
  [[nodiscard]] std::string redacted() const;
};

// Strict load: throws std::runtime_error when none of the three vars is
// set, or when only a subset is set (fail closed on partial secrets).
[[nodiscard]] BrokerCredentials load_broker_credentials(const std::string& prefix);

// Lenient probe for optional credentials (e.g. paper): returns false and
// leaves out empty when none of the three vars is set; throws on partial.
bool try_load_broker_credentials(const std::string& prefix, BrokerCredentials& out);

}  // namespace qts
