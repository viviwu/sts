// clang-format off
/**
  ******************************************************************************
  * @file           : credentials.cpp
  * @author         : vivi wu
  * @brief          : Environment credential loading with partial-secret fail-closed.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/config/credentials.hpp"

#include <cstdlib>
#include <stdexcept>

namespace qts {
namespace {

std::string getenv_value(const std::string& name) {
  const char* value = std::getenv(name.c_str());
  return value == nullptr ? std::string{} : std::string{value};
}

const char* presence(const std::string& value) { return value.empty() ? "empty" : "set"; }

}  // namespace

std::string BrokerCredentials::redacted() const {
  return "cust_id=" + std::string{presence(cust_id)} + " fund_account_id=" + std::string{presence(fund_account_id)} + " password=" + std::string{presence(password)};
}

BrokerCredentials load_broker_credentials(const std::string& prefix) {
  BrokerCredentials out{};
  if (!try_load_broker_credentials(prefix, out)) {
    throw std::runtime_error("no broker credentials in environment for prefix '" + prefix + "'");
  }
  return out;
}

bool try_load_broker_credentials(const std::string& prefix, BrokerCredentials& out) {
  const std::string cust = getenv_value(prefix + "_CUST_ID");
  const std::string fund = getenv_value(prefix + "_FUND_ACCOUNT_ID");
  const std::string password = getenv_value(prefix + "_PASSWORD");
  const bool any = !cust.empty() || !fund.empty() || !password.empty();
  const bool all = !cust.empty() && !fund.empty() && !password.empty();
  if (!any) {
    out = BrokerCredentials{};
    return false;
  }
  if (!all) throw std::runtime_error("partial broker credentials in environment for prefix '" + prefix + "'");
  out.cust_id = cust;
  out.fund_account_id = fund;
  out.password = password;
  return true;
}

}  // namespace qts
