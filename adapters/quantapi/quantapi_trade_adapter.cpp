// clang-format off
/**
  ******************************************************************************
  * @file           : quantapi_trade_adapter.cpp
  * @author         : vivi wu
  * @brief          : QuantAPI TradeApi adapter implementation (callback fixtures plus single-pump drain).
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
// SPDX-License-Identifier: MIT
// QuantAPI TradeApi adapter implementation (M7-2c, ADR-029/ADR-032/ADR-033).
//
// Scope decision (M7-2): TCP transport only — prop::kConnectionProtocol =
// kTCPProtocol with "ip:port" locations; the RDMA/shm transports are out of
// scope, which is also why libatpquantapi.so links without rudplib.
//
// Threading (archforce.md §3.4, repo drain discipline): the ATPQuantHandler
// callbacks run on vendor receive threads; each callback converts its
// non-copyable vendor message into a value fixture immediately and calls the
// on_* entry point, which attributes, maps and enqueues — never touching the
// handler directly. A single pump thread calls drain() to deliver. Send
// paths (Req*/Login/Logout) serialize on send_ because Req* concurrency is
// not promised by the vendor. state_ guards attribution/query/stats maps and
// is never held across a vendor call or a handler invocation; queue_ guards
// the three delivery deques and is also never nested with state_.
//
// Sidecar (archforce.md §3.2): cl_ord_no is broker-generated, so the adapter
// persists its own monotonic request_id sequence plus the cl_ord_no ->
// client_order_id attribution map in an append-only sidecar file
// ("seq <n>" / "map <cl_ord_no> <client_order_id>", one record per line,
// restart-safe attribution). client_order_id itself never changes.
//
// Status mapping (archforce.md §2, ATPOrdStatusConst):
//   kNew/kUnSend/kSended (0/9/10)          -> pending_new
//   kPartiallyFilled (1)                   -> partially_filled
//   kFilled (2)                            -> filled
//   kPartiallyFilledPartiallyCancelled/
//   kCancelled/kPartiallyCancelled (3/4/5) -> cancelled
//   kReject/kInternalReject (8/90)         -> rejected
//   kWaitCancelled/kPartiallyFilledWaitCancelled (11/12) -> pending_cancel
//   kInternalCanceledOrder/kCanceledSuccess (91/92)       -> cancelled
//   anything else -> fail closed (dropped, counted in status_unknown_drops).
// Known gap (recorded in dev_scheme §7): a kWaitCancelled report arriving
// while OMS still holds pending_new is dropped by OrderManager's transition
// table; OMS is not changed — the terminal cancelled report still lands.
//
// Reject codes: adapter-local rejections carry reject_code = 0 with an
// "adapter: " reason prefix (the integer space belongs to vendor codes);
// gateway rejections pass the vendor error/reject code through.
#include "quantapi_trade_adapter.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#if defined(__linux__)
  #include <arpa/inet.h>
  #include <dirent.h>
  #include <ifaddrs.h>
  #include <net/if.h>
  #include <netinet/in.h>
  #include <netpacket/packet.h>
  #include <sys/socket.h>
#endif

#include "atp_quant_api.h"
#include "atp_quant_constant.h"
#include "atp_quant_msg.h"

using namespace atp::quant_api;  // NOLINT(build/namespaces) — vendor TU-local alias

namespace sts {
namespace {

constexpr std::int32_t kSendUnavailable = -100;   // adapter synthetic: no live session
constexpr int kLoginWaitTicks = 30;               // 30 * 100ms = 3s (demo convention)
constexpr std::size_t kSecInfoDrainChunk = 2000;  // ADR-054: rows per drain (50k sweeps never stall a tick)

const char* market_suffix(std::int32_t market) {
  switch (market) {
    case 101:
      return ".SSE";
    case 102:
      return ".SZSE";
    case 104:
      return ".BSE";
    default:
      return "";
  }
}

bool is_terminal(OrderStatus status) { return status == OrderStatus::filled || status == OrderStatus::cancelled || status == OrderStatus::rejected; }

}  // namespace

namespace quantapi_detail {

bool parse_instrument(const InstrumentId& id, std::int32_t& market, std::string& code) {
  const std::size_t dot = id.rfind('.');
  if (dot == std::string::npos || dot == 0) return false;
  std::string suffix = id.substr(dot + 1);
  code = id.substr(0, dot);
  if (code.size() > 6) return false;
  for (const char c : code) {
    if (c < '0' || c > '9') return false;
  }
  if (suffix == "SSE") {
    market = quantapi_fixtures::kMarketSse;
  } else if (suffix == "SZSE") {
    market = quantapi_fixtures::kMarketSzse;
  } else if (suffix == "BSE") {
    market = quantapi_fixtures::kMarketBse;
  } else {
    return false;
  }
  return true;
}

bool map_ord_status(std::int32_t ord_status, OrderStatus& out) {
  using namespace quantapi_fixtures;  // NOLINT — numeric mirror of ATPOrdStatusConst
  switch (ord_status) {
    case kOrdNew:
    case kOrdUnSend:
    case kOrdSended:
      out = OrderStatus::pending_new;
      return true;
    case kOrdPartiallyFilled:
      out = OrderStatus::partially_filled;
      return true;
    case kOrdFilled:
      out = OrderStatus::filled;
      return true;
    case kOrdPartiallyFilledCancelled:
    case kOrdCancelled:
    case kOrdPartiallyCancelled:
    case kOrdInternalCancelled:
    case kOrdCancelSuccess:
      out = OrderStatus::cancelled;
      return true;
    case kOrdReject:
    case kOrdInternalReject:
      out = OrderStatus::rejected;
      return true;
    case kOrdWaitCancelled:
    case kOrdPartiallyFilledWaitCancelled:
      out = OrderStatus::pending_cancel;
      return true;
    default:
      return false;
  }
}

char map_order_type(const OrderIntent& intent) {
  if (intent.type == OrderType::limit) return ATPOrderTypeConst::kFixedNew;  // 'a'
  // Market orders express style as order type b~g (archforce.md §1). The
  // five-level mappings are tentative per the vendor comments; confirm the
  // exact五档 semantics with the broker during 联调 before market orders run.
  if (intent.type == OrderType::market) {
    switch (intent.market_order_style) {
      case MarketOrderStyle::own_best:
        return ATPOrderTypeConst::kLocalOptimalNew;  // 'b' 本方最优
      case MarketOrderStyle::counterparty_best:
        return ATPOrderTypeConst::kCountPartyOptimalTransferFixed;  // 'c' 对手方最优转限价
      case MarketOrderStyle::best_five_ioc:
        return ATPOrderTypeConst::kOptimalFiveLevelFullDealTransferCancel;  // 'f' 五档全额成交剩余撤销
      case MarketOrderStyle::best_five_to_limit:
        return ATPOrderTypeConst::kOptimalFiveLevelImmediateDealTransferFixed;  // 'g' 五档即时成交剩余转限价
      case MarketOrderStyle::none:
        return 0;  // fail closed: explicit style required
    }
  }
  return 0;
}

// Terminal fingerprint per the mandated format (ADR-053):
//   #<APP>IIP:<ipv4>;MAC:<mac>;HD:<disk-serial>
// Missing parts stay empty but the shape is kept, so the vendor always sees
// a well-formed code. Best effort, stdlib/POSIX only (no new deps).
std::string local_ipv4(std::string& mac_out) {
  mac_out.clear();
#if defined(__linux__)
  ifaddrs* list = nullptr;
  if (::getifaddrs(&list) != 0) return {};
  std::string ip;
  std::string ifname;
  for (ifaddrs* it = list; it != nullptr; it = it->ifa_next) {
    if (it->ifa_addr == nullptr || it->ifa_addr->sa_family != AF_INET) continue;
    if ((it->ifa_flags & IFF_UP) == 0 || (it->ifa_flags & IFF_LOOPBACK) != 0) continue;
    char buf[INET_ADDRSTRLEN]{0};
    const auto* addr = reinterpret_cast<sockaddr_in*>(it->ifa_addr);
    if (::inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf)) == nullptr) continue;
    ip = buf;
    ifname = it->ifa_name != nullptr ? it->ifa_name : "";
    break;
  }
  if (!ifname.empty()) {
    for (ifaddrs* it = list; it != nullptr; it = it->ifa_next) {
      if (it->ifa_addr == nullptr || it->ifa_addr->sa_family != AF_PACKET) continue;
      const std::string name = it->ifa_name != nullptr ? it->ifa_name : "";
      if (name != ifname) continue;
      const auto* link = reinterpret_cast<sockaddr_ll*>(it->ifa_addr);
      if (link->sll_halen != 6) continue;
      char hex[13]{0};
      for (int i = 0; i < 6; ++i) std::snprintf(hex + 2 * i, 3, "%02x", link->sll_addr[i]);
      mac_out = hex;
      break;
    }
  }
  ::freeifaddrs(list);
  return ip;
#else
  return {};
#endif
}

std::string disk_serial() {
#if defined(__linux__)
  DIR* sys = ::opendir("/sys/block");
  if (sys == nullptr) return {};
  std::string serial;
  while (dirent* entry = ::readdir(sys)) {
    const std::string name = entry->d_name != nullptr ? entry->d_name : "";
    if (name.empty() || name[0] == '.' || name.rfind("loop", 0) == 0 || name.rfind("ram", 0) == 0 || name.rfind("dm-", 0) == 0) continue;
    for (const std::string& path : {"/sys/block/" + name + "/serial", "/sys/block/" + name + "/device/serial"}) {
      std::ifstream in(path);
      std::string line;
      if (in.is_open() && std::getline(in, line)) {
        // Trim trailing whitespace/newlines.
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        if (!line.empty()) {
          serial = line;
          break;
        }
      }
    }
    if (!serial.empty()) break;
  }
  ::closedir(sys);
  return serial;
#else
  return {};
#endif
}

std::string generate_feature_code(const std::string& app) {
  std::string mac;
  const std::string ip = local_ipv4(mac);
  return "#" + app + "IIP:" + ip + ";MAC:" + mac + ";HD:" + disk_serial();
}

bool const_time_equal(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  unsigned char diff = 0;
  for (std::size_t i = 0; i < a.size(); ++i) diff |= static_cast<unsigned char>(a[i] ^ b[i]);
  return diff == 0;
}

}  // namespace quantapi_detail

// ---------------------------------------------------------------------------
// Impl: all adapter state lives here. It doubles as the vendor callback
// handler (nested class of the adapter, so it may call the private on_*
// entry points directly).
// ---------------------------------------------------------------------------
struct QuantapiTradeAdapter::Impl final : public ATPQuantHandler {
  struct FundValues {
    double leaves{0.0};
    double init_leaves{0.0};
    double frozen{0.0};
    double available_t0{0.0};  // 当日可取
    double available_t1{0.0};  // 当日可用
  };

  explicit Impl(QuantapiSessionConfig config)
      : config_(std::move(config)) {
    open_sidecar();
  }
  ~Impl() override { stop(); }

  // ---- lifecycle (ADR-053: standby-first) ---------------------------------
  // Standby: validate non-secret config only. No vendor interaction and no
  // counter session here; the first gateway login builds the session via
  // login(). Customer identity/password are never required at startup.
  bool start() {
    if (config_.locations.empty() || config_.order_way.empty() || config_.order_way.size() > 4 || config_.sidecar_path.empty()) {
      return false;
    }
    if (config_.agw_user.empty() != config_.agw_password.empty()) return false;  // pair or nothing
    if (!sidecar_ok_) return false;
    return true;
  }

  // Vendor session bring-up (Init + construct: TCP ready, still no
  // counter login). Standby keeps this state until a gateway login.
  bool vendor_up() {
    if (api_ != nullptr) return true;
    props_ = new ATPProperties();
    props_->SetValue(prop::kLogLevel, ATPLogLevel::kInfo);
    props_->SetValue(prop::kCommonLogPath, "");
    props_->SetValue(prop::kIndicatorLogPath, "");
    props_->SetValue(prop::kIoLogPath, "");
    props_->SetValue(prop::kEncryptSchema, ATPEncryptModeConst::kEncryptMode);
    props_->SetValue(prop::kIsEnableLatency, false);

    {
      const std::lock_guard<std::mutex> lock(state_);
      ++stats_.init_calls;
    }
    const ATPErrorCodeType init_rc = ATPQuantAPI::Init(props_);
    if (init_rc != ATPErrorCode::kSuccess) {
      {
        const std::lock_guard<std::mutex> lock(state_);
        stats_.init_rc = static_cast<std::int32_t>(init_rc);
      }
      delete props_;
      props_ = nullptr;
      return false;
    }
    init_done_ = true;

    // Connection properties: TCP only, quick retransmit, demo parity.
    props_->SetValue(prop::kRecevieThreadCpu, static_cast<std::uint8_t>(255));
    props_->SetValue(prop::kSendThreadCpu, static_cast<std::uint8_t>(255));
    props_->SetValue(prop::kIsTcpDirect, false);
    props_->SetValue(prop::kBindIpAddress, "");
    props_->SetValue(prop::kLocations, config_.locations.c_str());
    props_->SetValue(prop::kRetransmitMode, ATPRetransmitModeConst::kQuickMode);
    props_->SetValue(prop::kMultiChannelFlag, ATPMultiChannelFlagConst::kDefault);
    props_->SetValue(prop::kConnectionProtocol, ATPConnectionProtocolConst::kTCPProtocol);
    props_->SetValue(prop::kOrderWay, config_.order_way.c_str());
    // Terminal fingerprint: pinned override wins, otherwise generated per
    // the mandated #<APP>IIP:<ip>;MAC:<mac>;HD:<hd> shape (ADR-053).
    const std::string feature = config_.client_feature_code.empty() ? quantapi_detail::generate_feature_code("STS") : config_.client_feature_code;
    props_->SetValue(prop::kClientFeatureCode, feature.c_str());
    if (!config_.agw_user.empty()) {
      props_->SetValue(prop::kAgwUser, config_.agw_user.c_str());
      props_->SetValue(prop::kAgwPassword, config_.agw_password.c_str());
    }
    api_ = new ATPQuantAPI(this, props_);
    return true;
  }

  bool counter_login(const std::string& identity, const std::string& password, std::uint8_t mode) {
    ATPLoginProperty* login = ATPLoginProperty::NewMessage();
    login->SetUserId(identity.c_str());
    login->SetPassword(password.c_str());
    login->SetLoginMode(mode);
    login->SetConnectChannel(ATPConnectChannelConst::kSoftChannel);
    {
      const std::lock_guard<std::mutex> lock(state_);
      ++stats_.login_calls;
    }
    const ATPErrorCodeType login_rc = vendor_call([&] { return api_->Login(login); });
    ATPLoginProperty::DeleteMessage(login);
    if (login_rc != ATPErrorCode::kSuccess) return false;
    for (int i = 0; i < kLoginWaitTicks && !logged_in_.load(); ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!logged_in_.load()) return false;
    {
      const std::lock_guard<std::mutex> lock(state_);
      session_password_ = password;  // memory only, zeroed on stop()
    }
    return true;
  }

  // state_ must be held.
  bool knows_identity_locked(const std::string& identity) const {
    return identity == cust_id_ || identity == fund_account_id_ || identity == account_sse_ || identity == account_szse_ || identity == account_bse_;
  }

  // Message stamping triple: session values win (ADR-053 lazy login), config
  // keeps compat fallbacks for pre-login shape checks (never sent: every
  // send path requires a live session first).
  void session_triple(std::string& cust, std::string& password) const {
    const std::lock_guard<std::mutex> lock(state_);
    cust = cust_id_.empty() ? config_.cust_id : cust_id_;
    password = session_password_;
  }

  // Counter login on first use (cust mode, then fund mode), session verify
  // afterwards with no counter traffic. Failed counter attempts count at
  // the counter: callers pass through what the trader typed, verbatim.
  bool login(const std::string& identity, const std::string& password) {
    if (identity.empty() || password.empty()) return false;
    {
      const std::lock_guard<std::mutex> lock(state_);
      if (logged_in_.load()) {
        if (!knows_identity_locked(identity)) return false;
        return quantapi_detail::const_time_equal(password, session_password_);
      }
    }
    if (!vendor_up()) return false;
    if (counter_login(identity, password, ATPLoginModeConst::kCustIDMode)) return true;
    if (counter_login(identity, password, ATPLoginModeConst::kFundAccountIDMode)) return true;
    return false;
  }

  // Standby connection: vendor session up, counter login deferred to the
  // first gateway login. Safe to call at server boot (no secrets needed).
  bool connect() { return vendor_up(); }

  std::string session_fund() const {
    const std::lock_guard<std::mutex> lock(state_);
    if (!logged_in_.load()) return {};
    if (!fund_account_id_.empty()) return fund_account_id_;
    return cust_id_;
  }

  void stop() {
    const std::lock_guard<std::recursive_mutex> lock(send_);
    if (api_ != nullptr) {
      if (logged_in_.load()) {
        api_->Logout();
        logged_in_.store(false);
      }
      api_->Release();
      delete api_;
      api_ = nullptr;
    }
    if (props_ != nullptr) {
      delete props_;
      props_ = nullptr;
    }
    if (init_done_) {
      ATPQuantAPI::Stop();
      init_done_ = false;
    }
    logged_in_.store(false);
    {
      // Session password lives in memory only: zero it on teardown.
      const std::lock_guard<std::mutex> state_lock(state_);
      std::fill(session_password_.begin(), session_password_.end(), '\0');
      session_password_.clear();
    }
  }

  bool started() const noexcept { return api_ != nullptr; }
  bool connected() const noexcept { return api_ != nullptr && logged_in_.load(); }

  // ---- handlers -----------------------------------------------------------
  void set_execution_handler(TradeApi::ExecutionHandler handler) {
    const std::lock_guard<std::mutex> lock(state_);
    execution_handler_ = std::move(handler);
  }
  void set_account_snapshot_handler(TradeApi::AccountSnapshotHandler handler) {
    const std::lock_guard<std::mutex> lock(state_);
    account_snapshot_handler_ = std::move(handler);
  }
  void set_security_info_handler(TradeApi::SecurityInfoHandler handler) {
    const std::lock_guard<std::mutex> lock(state_);
    secinfo_handler_ = std::move(handler);
  }
  void set_asset_snapshot_handler(TradeApi::AssetSnapshotHandler handler) {
    const std::lock_guard<std::mutex> lock(state_);
    asset_snapshot_handler_ = std::move(handler);
  }
  void set_recovery_complete_handler(TradeApi::RecoveryCompleteHandler handler) {
    const std::lock_guard<std::mutex> lock(state_);
    recovery_complete_handler_ = std::move(handler);
  }

  // ---- TradeApi hot path --------------------------------------------------
  void submit(const OrderIntent& intent) {
    {
      const std::lock_guard<std::mutex> lock(state_);
      ++stats_.submit_calls;
    }
    const ClientOrderId& coi = intent.client_order_id;

    // 1) Shape validation (vendor-free, fail closed) — runs before the
    //    session check so bad orders are always attributable to their shape.
    std::int32_t market = 0;
    std::string code;
    if (!quantapi_detail::parse_instrument(intent.instrument_id, market, code)) {
      reject_locally(coi, "adapter: unsupported instrument '" + intent.instrument_id + "'");
      return;
    }
    if (intent.trading_business != TradingBusiness::cash) {
      reject_locally(coi, "adapter: only cash trading business is supported");
      return;
    }
    if (intent.time_in_force != TimeInForce::day) {
      reject_locally(coi, "adapter: only day time-in-force is supported");
      return;
    }
    if (intent.quantity <= 0) {
      reject_locally(coi, "adapter: non-positive quantity");
      return;
    }
    if (intent.type == OrderType::limit && intent.limit_price <= 0) {
      reject_locally(coi, "adapter: non-positive limit price");
      return;
    }
    const char order_type = quantapi_detail::map_order_type(intent);
    if (order_type == 0) {
      reject_locally(coi, "adapter: unsupported order type or missing market_order_style");
      return;
    }
    // 2) Session.
    if (!logged_in_.load()) {
      reject_locally(coi, "adapter: not connected");
      return;
    }
    // 3) Account triple + duplicates + request_id allocation (archforce §3.2).
    std::string account;
    std::string branch;
    std::string fund;
    std::string local_reason;
    std::int64_t seq = 0;
    {
      const std::lock_guard<std::mutex> lock(state_);
      account = intent.security_account_id.empty() ? account_for_market_locked(market) : intent.security_account_id;
      branch = branch_id_;
      fund = fund_account_id_.empty() ? config_.fund_account_id : fund_account_id_;
      const bool duplicate = inflight_cois_.count(coi) > 0 || coi_to_cl_.count(coi) > 0;
      if (account.empty()) {
        local_reason = "adapter: shareholder account unavailable for market " + std::to_string(market);
      } else if (branch.empty()) {
        local_reason = "adapter: branch id unavailable (login incomplete)";
      } else if (duplicate) {
        local_reason = "adapter: duplicate client order id";
      }
      if (local_reason.empty()) {
        seq = alloc_seq_locked();
        inflight_req_[seq] = coi;
        inflight_cois_.insert(coi);
      }
    }
    if (!local_reason.empty()) {
      reject_locally(coi, local_reason);
      return;
    }
    // 4) Send. The synchronous return is acceptance only (archforce §1:
    //    never treat a send as the business result).
    const ATPErrorCodeType rc = send_order(intent, market, code, order_type, account, branch, fund, seq);
    if (rc == ATPErrorCode::kSuccess) return;
    {
      const std::lock_guard<std::mutex> lock(state_);
      inflight_req_.erase(seq);
      inflight_cois_.erase(coi);
      ++stats_.local_rejects;
    }
    const bool unavailable = rc == kSendUnavailable;
    const std::string reason = unavailable ? std::string("adapter: send failed: channel unavailable") : "adapter: send failed (code=" + std::to_string(rc) + ")";
    const std::int32_t code_out = unavailable ? quantapi_fixtures::kNoRejectCode : rc;
    enqueue_report(make_report("", coi, OrderStatus::rejected, 0, 0, reason, "", 0, "", code_out));
  }

  void cancel(const ClientOrderId& coi) {
    {
      const std::lock_guard<std::mutex> lock(state_);
      ++stats_.cancel_calls;
    }
    std::int64_t cl = 0;
    {
      const std::lock_guard<std::mutex> lock(state_);
      const auto it = coi_to_cl_.find(coi);
      if (it == coi_to_cl_.end()) {
        // No broker id yet: defer the cancel to the ack (it carries cl_ord_no).
        if (inflight_cois_.count(coi) > 0) cancel_requested_.insert(coi);
        return;
      }
      cl = it->second;
    }
    send_cancel(coi, cl);
  }

  // ---- control/recovery path ---------------------------------------------
  void request_open_orders(const AccountId& account_id) {
    std::vector<std::pair<std::int32_t, std::string>> markets;
    bool fail_now = false;
    {
      const std::lock_guard<std::mutex> lock(state_);
      if (!logged_in_.load()) {
        fail_now = true;
      } else {
        order_query_failed_ = false;
        recovery_account_id_ = account_id;
        pending_order_queries_.clear();
        collect_markets_locked(markets);
        if (markets.empty()) {
          recovery_account_id_.clear();
          fail_now = true;
        } else {
          // Reserve one request_id per market before any send so callbacks
          // can never beat their bookkeeping.
          for (std::size_t i = 0; i < markets.size(); ++i) pending_order_queries_.insert(alloc_seq_locked());
        }
      }
    }
    if (fail_now) {
      enqueue_recovery(account_id, false);
      return;
    }
    std::vector<std::int64_t> ids;
    {
      const std::lock_guard<std::mutex> lock(state_);
      ids.assign(pending_order_queries_.begin(), pending_order_queries_.end());
    }
    for (std::size_t i = 0; i < markets.size(); ++i) {
      // Each reserved id is sent exactly once (set order vs. vector order
      // pairing is arbitrary but complete: every market queried once).
      const ATPErrorCodeType rc = send_order_query(ids[i], markets[i].first, markets[i].second);
      if (rc != ATPErrorCode::kSuccess) {
        const std::lock_guard<std::mutex> lock(state_);
        pending_order_queries_.erase(ids[i]);
        order_query_failed_ = true;
      }
    }
    bool done = false;
    bool success = false;
    {
      const std::lock_guard<std::mutex> lock(state_);
      if (pending_order_queries_.empty() && !recovery_account_id_.empty()) {
        done = true;
        success = !order_query_failed_;
        recovery_account_id_.clear();
      }
    }
    if (done) enqueue_recovery(account_id, success);
  }

  void request_account_snapshot(const AccountId& account_id) {
    struct ShareQuery {
      std::int64_t seq;
      std::int32_t market;
      std::string account;
    };
    std::vector<ShareQuery> shares;
    std::int64_t fund_seq = 0;
    std::int32_t fund_market = 0;
    std::string fund_account;
    bool ready = false;
    {
      const std::lock_guard<std::mutex> lock(state_);
      if (!logged_in_.load()) return;  // fail closed: no snapshot without a session
      snapshot_failed_ = false;
      fund_valid_ = false;
      fund_ = FundValues{};
      snapshot_positions_.clear();
      snapshot_mv_sum_ = 0;
      snapshot_account_id_ = account_id;
      pending_snapshot_queries_.clear();
      std::vector<std::pair<std::int32_t, std::string>> markets;
      collect_markets_locked(markets);
      if (markets.empty()) return;
      // Fund query: one market only (demo queries 深市; fund account is shared).
      fund_market = quantapi_fixtures::kMarketSzse;
      fund_account = account_for_market_locked(fund_market);
      if (fund_account.empty()) {
        fund_market = markets.front().first;
        fund_account = markets.front().second;
      }
      fund_seq = alloc_seq_locked();
      pending_snapshot_queries_.insert(fund_seq);
      for (const auto& entry : markets) {
        const std::int64_t seq = alloc_seq_locked();
        pending_snapshot_queries_.insert(seq);
        shares.push_back(ShareQuery{seq, entry.first, entry.second});
      }
      ready = true;
    }
    if (!ready) return;
    // Send outside the lock; each failure marks the whole snapshot fail-closed.
    const ATPErrorCodeType fund_rc = send_fund_query(fund_seq, fund_market, fund_account);
    if (fund_rc != ATPErrorCode::kSuccess) mark_snapshot_failure(fund_seq);
    for (const ShareQuery& share : shares) {
      const ATPErrorCodeType rc = send_share_query(share.seq, share.market, share.account);
      if (rc != ATPErrorCode::kSuccess) mark_snapshot_failure(share.seq);
    }
  }

  // ---- reference-data path (ADR-054; bounded drain, never the hot path) --
  void request_security_info(const AccountId& account_id, const InstrumentId& instrument) {
    struct SecInfoQuery {
      std::int64_t seq;
      std::int32_t market;
      std::string account;
      std::string code;  // naked code, or empty for a full-market sweep leg
    };
    std::vector<SecInfoQuery> queries;
    {
      const std::lock_guard<std::mutex> lock(state_);
      if (!logged_in_.load()) {
        // Fail-closed completion signal (no rows): callers never hang.
        enqueue_secinfo(account_id, {}, true);
        return;
      }
      secinfo_failed_ = false;
      secinfo_account_id_ = account_id;
      secinfo_rows_.clear();
      pending_secinfo_queries_.clear();
      std::vector<std::pair<std::int32_t, std::string>> markets;
      collect_markets_locked(markets);
      if (markets.empty()) {
        enqueue_secinfo(account_id, {}, true);
        return;
      }
      if (instrument.empty()) {
        for (const auto& entry : markets) {
          const std::int64_t seq = alloc_seq_locked();
          pending_secinfo_queries_.insert(seq);
          queries.push_back(SecInfoQuery{seq, entry.first, entry.second, {}});
        }
      } else {
        std::int32_t market = 0;
        std::string code;
        if (!quantapi_detail::parse_instrument(instrument, market, code)) {
          enqueue_secinfo(account_id, {}, true);
          return;
        }
        const std::string account = account_for_market_locked(market);
        if (account.empty()) {
          enqueue_secinfo(account_id, {}, true);
          return;
        }
        const std::int64_t seq = alloc_seq_locked();
        pending_secinfo_queries_.insert(seq);
        queries.push_back(SecInfoQuery{seq, market, account, code});
      }
    }
    for (const SecInfoQuery& query : queries) {
      const ATPErrorCodeType rc = send_secinfo_query(query.seq, query.market, query.account, query.code);
      if (rc != ATPErrorCode::kSuccess) mark_secinfo_failure(query.seq);
    }
  }

  void request_asset_snapshot(const AccountId& account_id) {
    std::int64_t seq = 0;
    std::int32_t market = 0;
    std::string account;
    {
      const std::lock_guard<std::mutex> lock(state_);
      if (!logged_in_.load()) return;  // fail closed: mirrors the fund snapshot
      asset_failed_ = false;
      asset_ = AssetValues{};
      asset_account_id_ = account_id;
      pending_asset_queries_.clear();
      std::vector<std::pair<std::int32_t, std::string>> markets;
      collect_markets_locked(markets);
      if (markets.empty()) return;
      // Single leg (demo queries one market; fund account is shared).
      market = quantapi_fixtures::kMarketSzse;
      account = account_for_market_locked(market);
      if (account.empty()) {
        market = markets.front().first;
        account = markets.front().second;
      }
      seq = alloc_seq_locked();
      pending_asset_queries_.insert(seq);
    }
    if (send_asset_query(seq, market, account) != ATPErrorCode::kSuccess) mark_asset_failure(seq);
  }

  // ---- pump ---------------------------------------------------------------
  std::size_t drain() {
    {
      const std::lock_guard<std::mutex> lock(state_);
      ++stats_.drains;
    }
    std::size_t delivered = 0;
    for (;;) {
      ExecutionReport report;
      {
        const std::lock_guard<std::mutex> lock(queue_);
        if (reports_.empty()) break;
        report = reports_.front();
      }
      TradeApi::ExecutionHandler handler;
      {
        const std::lock_guard<std::mutex> lock(state_);
        handler = execution_handler_;
      }
      if (!handler) break;  // keep queued until a handler exists (AMA pattern)
      {
        const std::lock_guard<std::mutex> lock(queue_);
        reports_.pop_front();
      }
      handler(report);
      ++delivered;
      const std::lock_guard<std::mutex> lock(state_);
      ++stats_.reports_delivered;
    }
    for (;;) {
      TradeAccountSnapshot snapshot;
      {
        const std::lock_guard<std::mutex> lock(queue_);
        if (snapshots_.empty()) break;
        snapshot = snapshots_.front();
      }
      TradeApi::AccountSnapshotHandler handler;
      {
        const std::lock_guard<std::mutex> lock(state_);
        handler = account_snapshot_handler_;
      }
      if (!handler) break;
      {
        const std::lock_guard<std::mutex> lock(queue_);
        snapshots_.pop_front();
      }
      handler(snapshot);
      const std::lock_guard<std::mutex> lock(state_);
      ++stats_.snapshots_delivered;
    }
    // Reference-data batches (ADR-054): chunked so a 50k-row sweep never
    // stalls a tick; is_last lands with the final chunk only.
    for (;;) {
      TradeApi::SecurityInfoHandler handler;
      {
        const std::lock_guard<std::mutex> lock(state_);
        handler = secinfo_handler_;
      }
      if (!handler) break;  // keep queued until a handler exists (AMA pattern)
      AccountId account;
      std::vector<SecurityInfoRow> chunk;
      bool done = false;
      {
        const std::lock_guard<std::mutex> lock(queue_);
        if (secinfo_batches_.empty()) break;
        SecInfoBatch& batch = secinfo_batches_.front();
        account = batch.account;
        const std::size_t take = std::min<std::size_t>(batch.rows.size(), kSecInfoDrainChunk);
        chunk.assign(batch.rows.begin(), batch.rows.begin() + static_cast<std::ptrdiff_t>(take));
        batch.rows.erase(batch.rows.begin(), batch.rows.begin() + static_cast<std::ptrdiff_t>(take));
        if (batch.rows.empty() && batch.complete) {
          secinfo_batches_.pop_front();
          done = true;
        }
      }
      handler(account, chunk, done);
      const std::lock_guard<std::mutex> lock(state_);
      if (done) ++stats_.secinfo_batches_delivered;
    }
    for (;;) {
      TradeAssetSnapshot snapshot;
      {
        const std::lock_guard<std::mutex> lock(queue_);
        if (assets_.empty()) break;
        snapshot = assets_.front();
      }
      TradeApi::AssetSnapshotHandler handler;
      {
        const std::lock_guard<std::mutex> lock(state_);
        handler = asset_snapshot_handler_;
      }
      if (!handler) break;
      {
        const std::lock_guard<std::mutex> lock(queue_);
        assets_.pop_front();
      }
      handler(snapshot);
      const std::lock_guard<std::mutex> lock(state_);
      ++stats_.assets_delivered;
    }
    for (;;) {
      std::pair<AccountId, bool> recovery;
      {
        const std::lock_guard<std::mutex> lock(queue_);
        if (recoveries_.empty()) break;
        recovery = recoveries_.front();
      }
      TradeApi::RecoveryCompleteHandler handler;
      {
        const std::lock_guard<std::mutex> lock(state_);
        handler = recovery_complete_handler_;
      }
      if (!handler) break;
      {
        const std::lock_guard<std::mutex> lock(queue_);
        recoveries_.pop_front();
      }
      handler(recovery.first, recovery.second);
      const std::lock_guard<std::mutex> lock(state_);
      ++stats_.recovery_completes;
    }
    return delivered;
  }

  QuantapiAdapterStats stats() const {
    const std::lock_guard<std::mutex> lock(state_);
    return stats_;
  }

  const QuantapiSessionConfig& config() const noexcept { return config_; }

  // ---- fixture entry points (vendor callbacks and test seams) -------------
  void on_login(const QuantapiLoginFixture& fixture) {
    const std::lock_guard<std::mutex> lock(state_);
    if (!fixture.cust_id.empty()) cust_id_ = fixture.cust_id;
    // Seam-only session password (see fixture docs); production vendor
    // callbacks never carry one — login() caches the trader-supplied one.
    if (!fixture.session_password.empty()) session_password_ = fixture.session_password;
    if (!fixture.fund_account_id.empty()) fund_account_id_ = fixture.fund_account_id;
    if (!fixture.branch_id.empty()) branch_id_ = fixture.branch_id;
    account_sse_ = fixture.account_id_sse;
    account_szse_ = fixture.account_id_szse;
    account_bse_ = fixture.account_id_bse;
    logged_in_.store(true);
  }

  void on_ack(const QuantapiAckFixture& fixture) {
    ClientOrderId coi;
    bool attributed = false;
    bool deferred_cancel = false;
    bool emit = false;
    ExecutionReport report;
    std::vector<QuantapiRtnFixture> stray;
    std::int64_t cl = fixture.cl_ord_no;
    {
      const std::lock_guard<std::mutex> lock(state_);
      ++stats_.acks_received;
      const auto it = inflight_req_.find(fixture.request_id);
      if (it != inflight_req_.end()) {
        coi = it->second;
        attributed = true;
        inflight_req_.erase(it);
        inflight_cois_.erase(coi);
      }
      if (!attributed) {
        // A restart can lose the in-flight record; the sidecar map still
        // attributes later RTNs, this ack itself is counted and dropped.
        ++stats_.unmatched_acks;
        return;
      }
      if (cl > 0) {
        cl_to_coi_[cl] = coi;
        coi_to_cl_[coi] = cl;
        append_map_locked(cl, coi);
        const auto stray_it = stray_rtns_.find(cl);
        if (stray_it != stray_rtns_.end()) {
          stray = std::move(stray_it->second);
          stray_rtns_.erase(stray_it);
        }
      }
      const std::string order_id = cl > 0 ? std::to_string(cl) : std::string{};
      if (fixture.error_id != 0) {
        ++stats_.gateway_rejects;
        std::string reason = "gateway: ";
        reason += fixture.error_msg.empty() ? ("order rejected (code=" + std::to_string(fixture.error_id) + ")") : fixture.error_msg;
        report = make_report(order_id, coi, OrderStatus::rejected, 0, 0, reason, "", 0, order_id, fixture.error_id);
        emit = true;
      } else if (cl <= 0) {
        ++stats_.local_rejects;
        report = make_report("", coi, OrderStatus::rejected, 0, 0, "adapter: ack carried no cl_ord_no", "", 0, "", quantapi_fixtures::kNoRejectCode);
        emit = true;
      } else {
        report = make_report(order_id, coi, OrderStatus::pending_new, 0, 0, "", "", 0, order_id, quantapi_fixtures::kNoRejectCode);
        last_status_[coi] = OrderStatus::pending_new;
        last_cum_[coi] = 0;
        emit = true;
        deferred_cancel = cancel_requested_.erase(coi) > 0;
      }
    }
    if (emit) enqueue_report(report);
    for (const QuantapiRtnFixture& rtn : stray) on_rtn(rtn);
    if (deferred_cancel && cl > 0) send_cancel(coi, cl);
  }

  void on_rtn(const QuantapiRtnFixture& fixture) {
    bool emit = false;
    ExecutionReport report;
    {
      const std::lock_guard<std::mutex> lock(state_);
      ++stats_.rtns_received;
      const auto it = cl_to_coi_.find(fixture.cl_ord_no);
      if (it == cl_to_coi_.end()) {
        // RTN can precede the ack; stash it and flush when the ack binds
        // cl_ord_no -> client_order_id (or after a restart via the sidecar).
        ++stats_.unattributed_rtns;
        stray_rtns_[fixture.cl_ord_no].push_back(fixture);
        return;
      }
      const ClientOrderId& coi = it->second;
      OrderStatus status;
      if (!quantapi_detail::map_ord_status(fixture.ord_status, status)) {
        ++stats_.status_unknown_drops;
        return;
      }
      const std::string order_id = std::to_string(fixture.cl_ord_no);
      const Quantity cumulative = quantapi_quantity_from_vendor(fixture.cum_qty);
      const Quantity last = quantapi_quantity_from_vendor(fixture.last_qty);
      const Price last_price = last > 0 ? quantapi_price_from_vendor(fixture.last_px) : Price{0};
      std::string reason;
      std::int32_t reject_code = quantapi_fixtures::kNoRejectCode;
      if (status == OrderStatus::rejected) {
        ++stats_.gateway_rejects;
        reject_code = fixture.reject_reason_code;
        reason = "gateway: ";
        reason += fixture.ord_rej_reason.empty() ? ("order rejected (code=" + std::to_string(fixture.reject_reason_code) + ")") : fixture.ord_rej_reason;
      }
      report = make_report(order_id, coi, status, last, last_price, reason, fixture.exec_id, cumulative, order_id, reject_code);
      last_status_[coi] = status;
      last_cum_[coi] = cumulative;
      emit = true;
    }
    if (emit) enqueue_report(report);
  }

  void on_cancel_rsp(const QuantapiCancelRspFixture& fixture) {
    bool emit = false;
    ExecutionReport report;
    {
      const std::lock_guard<std::mutex> lock(state_);
      ++stats_.cancel_resps_received;
      const auto it = cl_to_coi_.find(fixture.orig_cl_ord_no);
      if (it == cl_to_coi_.end()) {
        ++stats_.unmatched_acks;
        return;
      }
      if (fixture.error_id != 0) {
        // The order stays open; OMS already sits in pending_cancel, so emit
        // same-status with the reason (pending_cancel -> pending_cancel).
        ++stats_.gateway_rejects;
        const std::string order_id = std::to_string(fixture.orig_cl_ord_no);
        std::string reason = "gateway: cancel rejected (code=" + std::to_string(fixture.error_id);
        if (!fixture.error_msg.empty()) reason += ": " + fixture.error_msg;
        reason += ")";
        report = make_report(order_id, it->second, OrderStatus::pending_cancel, 0, 0, reason, "", 0, order_id, quantapi_fixtures::kNoRejectCode);
        emit = true;
      }
    }
    if (emit) enqueue_report(report);
  }

  void on_order_query(const QuantapiOrderQueryFixture& fixture) {
    bool emit = false;
    bool recovery_done = false;
    bool recovery_ok = false;
    AccountId recovery_account;
    ExecutionReport report;
    {
      const std::lock_guard<std::mutex> lock(state_);
      if (fixture.error_id != 0) order_query_failed_ = true;
      if (fixture.has_row) {
        ++stats_.query_rows;
        const auto it = cl_to_coi_.find(fixture.cl_ord_no);
        if (it == cl_to_coi_.end()) {
          // Unattributable broker rows are invisible to OMS reconcile; counted
          // with the other unattributed broker events (archforce §3.2).
          ++stats_.unattributed_rtns;
        } else {
          OrderStatus status;
          if (!quantapi_detail::map_ord_status(fixture.ord_status, status)) {
            ++stats_.status_unknown_drops;
          } else if (!is_terminal(status)) {
            // request_open_orders feeds recovery: open rows only, terminal
            // rows stay with the live RTN stream.
            const ClientOrderId& coi = it->second;
            const std::string order_id = std::to_string(fixture.cl_ord_no);
            const Quantity cumulative = quantapi_quantity_from_vendor(fixture.cum_qty);
            report = make_report(order_id, coi, status, 0, 0, "", "", cumulative, order_id, quantapi_fixtures::kNoRejectCode);
            last_status_[coi] = status;
            last_cum_[coi] = cumulative;
            emit = true;
          }
        }
      }
      if (fixture.is_last) {
        pending_order_queries_.erase(fixture.request_id);
        if (!recovery_account_id_.empty() && pending_order_queries_.empty()) {
          recovery_done = true;
          recovery_ok = !order_query_failed_;
          recovery_account = recovery_account_id_;
          recovery_account_id_.clear();
        }
      }
    }
    if (emit) enqueue_report(report);
    if (recovery_done) enqueue_recovery(recovery_account, recovery_ok);
  }

  void on_fund(const QuantapiFundFixture& fixture) {
    bool deliver = false;
    TradeAccountSnapshot snapshot;
    {
      const std::lock_guard<std::mutex> lock(state_);
      if (fixture.error_id != 0) {
        snapshot_failed_ = true;
      } else if (fixture.has_row) {
        // Data row only: the empty is_last marker must not zero the cache.
        ++stats_.query_rows;
        fund_.leaves = fixture.leaves_value;
        fund_.init_leaves = fixture.init_leaves_value;
        fund_.frozen = fixture.frozen_all;
        fund_.available_t0 = fixture.available_t0;
        fund_.available_t1 = fixture.available_t1;
        fund_valid_ = true;
      }
      if (fixture.is_last) {
        pending_snapshot_queries_.erase(fixture.request_id);
        if (pending_snapshot_queries_.empty() && !snapshot_failed_ && fund_valid_) {
          snapshot = assemble_snapshot_locked();
          deliver = true;
        }
      }
    }
    if (deliver) enqueue_snapshot(snapshot);
  }

  void on_share(const QuantapiShareFixture& fixture) {
    bool deliver = false;
    TradeAccountSnapshot snapshot;
    {
      const std::lock_guard<std::mutex> lock(state_);
      if (fixture.error_id != 0) {
        snapshot_failed_ = true;
      } else if (fixture.has_row) {
        ++stats_.query_rows;
        const char* suffix = market_suffix(fixture.market_id);
        if (*suffix == '\0') {
          ++stats_.status_unknown_drops;  // unknown market: fail closed, row dropped
        } else {
          PositionSnapshot position;
          position.instrument_id = fixture.security_id + suffix;
          position.total_quantity = quantapi_quantity_from_vendor(fixture.leaves_qty);
          position.sellable_quantity = quantapi_quantity_from_vendor(fixture.available_qty);
          position.frozen_quantity = position.total_quantity > position.sellable_quantity ? position.total_quantity - position.sellable_quantity : Quantity{0};
          position.average_cost = quantapi_price_from_vendor(fixture.cost_price);
          // ADR-032: today/yesterday split stays 0 (unreported) until the
          // GetStockBuy/GetStockSale semantics are confirmed in 联调.
          position.today_quantity = 0;
          snapshot_positions_[position.instrument_id] = position;
          snapshot_mv_sum_ += quantapi_price_from_vendor(fixture.market_value);
        }
      }
      if (fixture.is_last) {
        pending_snapshot_queries_.erase(fixture.request_id);
        if (pending_snapshot_queries_.empty() && !snapshot_failed_ && fund_valid_) {
          snapshot = assemble_snapshot_locked();
          deliver = true;
        }
      }
    }
    if (deliver) enqueue_snapshot(snapshot);
  }

  void on_asset(const QuantapiAssetFixture& fixture) {
    bool deliver = false;
    TradeAssetSnapshot snapshot;
    {
      const std::lock_guard<std::mutex> lock(state_);
      if (fixture.error_id != 0) {
        asset_failed_ = true;
      } else if (fixture.has_row) {
        ++stats_.query_rows;
        asset_.leaves = fixture.leaves_value;
        asset_.init_leaves = fixture.init_leaves_value;
        asset_.frozen = fixture.frozen_all;
        asset_.available_t0 = fixture.available_t0;
        asset_.available_t1 = fixture.available_t1;
        asset_.total_asset = fixture.total_asset;
        asset_.market_value = fixture.market_value;
        asset_valid_ = true;
      }
      if (fixture.is_last) {
        pending_asset_queries_.erase(fixture.request_id);
        if (pending_asset_queries_.empty() && !asset_failed_ && asset_valid_) {
          snapshot.account_id = asset_account_id_.empty() ? config_.fund_account_id : asset_account_id_;
          snapshot.total_asset = quantapi_price_from_vendor(asset_.total_asset);
          snapshot.market_value = quantapi_price_from_vendor(asset_.market_value);
          snapshot.leaves_value = quantapi_price_from_vendor(asset_.leaves);
          snapshot.frozen = quantapi_price_from_vendor(asset_.frozen);
          snapshot.available_t0 = quantapi_price_from_vendor(asset_.available_t0);
          snapshot.available_t1 = quantapi_price_from_vendor(asset_.available_t1);
          deliver = true;
        }
      }
    }
    if (deliver) enqueue_asset(snapshot);
  }

  void on_secinfo(const QuantapiSecInfoFixture& fixture) {
    bool finish = false;
    {
      const std::lock_guard<std::mutex> lock(state_);
      // Same single-flight discipline as fund/share rows: a new request
      // clears the accumulation, so late rows land in the current sweep.
      if (fixture.error_id != 0) {
        secinfo_failed_ = true;
      } else if (fixture.has_row) {
        ++stats_.secinfo_rows;
        const char* suffix = market_suffix(fixture.market_id);
        if (*suffix != '\0' && !fixture.security_id.empty()) {
          SecurityInfoRow row;
          row.instrument_id = fixture.security_id + suffix;
          row.symbol = fixture.symbol;
          row.security_type = fixture.security_type;
          row.price_tick = quantapi_price_from_vendor(fixture.price_tick);
          row.buy_unit = quantapi_quantity_from_vendor(fixture.buy_qty_unit);
          row.sell_unit = quantapi_quantity_from_vendor(fixture.sell_qty_unit);
          row.buy_lower = quantapi_quantity_from_vendor(fixture.buy_qty_lower);
          row.sell_lower = quantapi_quantity_from_vendor(fixture.sell_qty_lower);
          row.limit_up = quantapi_price_from_vendor(fixture.price_upper_limit);
          row.limit_down = quantapi_price_from_vendor(fixture.price_lower_limit);
          row.prev_close = quantapi_price_from_vendor(fixture.prev_close);
          row.day_trading = fixture.day_trading;
          row.status = fixture.security_status;
          secinfo_rows_.push_back(std::move(row));
        }
      }
      if (fixture.is_last) {
        pending_secinfo_queries_.erase(fixture.request_id);
        if (pending_secinfo_queries_.empty()) finish = true;
      }
    }
    if (finish) {
      // Move the accumulation out; the batch (possibly empty on failure)
      // goes through the bounded drain below.
      std::vector<SecurityInfoRow> rows;
      AccountId account;
      {
        const std::lock_guard<std::mutex> lock(state_);
        if (!secinfo_failed_) rows = std::move(secinfo_rows_);
        account = secinfo_account_id_.empty() ? config_.fund_account_id : secinfo_account_id_;
        secinfo_rows_.clear();
        secinfo_account_id_.clear();
      }
      enqueue_secinfo(account, std::move(rows), true);
    }
  }

  void on_logout(const std::string& /*desc*/) {
    logged_in_.store(false);
    const std::lock_guard<std::mutex> lock(state_);
    ++stats_.logout_events;
  }

  void on_recovering(const std::string& /*desc*/) {
    const std::lock_guard<std::mutex> lock(state_);
    ++stats_.recovering_events;
  }

  // ---- vendor callback overrides (vendor thread -> value fixture) ---------
  void OnLogin(const ATPCustomerInfo& msg) override {
    QuantapiLoginFixture fixture;
    const char* cust = msg.GetCustId();
    fixture.cust_id = cust != nullptr ? cust : "";
    for (std::uint32_t i = 0; i < msg.FundAccountArraySize(); ++i) {
      if (i == 0) {
        const char* fund = msg.FundAccountArray_GetFundAccountId(i);
        const char* branch = msg.FundAccountArray_GetBranchId(i);
        fixture.fund_account_id = fund != nullptr ? fund : "";
        fixture.branch_id = branch != nullptr ? branch : "";
      }
      for (std::uint32_t j = 0; j < msg.FundAccountArray_AccountArraySize(i); ++j) {
        const std::uint16_t market = msg.FundAccountArray_AccountArray_GetMarketId(i, j);
        const char* account = msg.FundAccountArray_AccountArray_GetAccountId(i, j);
        const std::string value = account != nullptr ? account : "";
        if (market == static_cast<std::uint16_t>(quantapi_fixtures::kMarketSse)) {
          fixture.account_id_sse = value;
        } else if (market == static_cast<std::uint16_t>(quantapi_fixtures::kMarketSzse)) {
          fixture.account_id_szse = value;
        } else if (market == static_cast<std::uint16_t>(quantapi_fixtures::kMarketBse)) {
          fixture.account_id_bse = value;
        }
      }
    }
    on_login(fixture);
  }

  void OnLogout(const char* desc) override { on_logout(desc != nullptr ? desc : ""); }

  void OnRecovering(const char* desc) override { on_recovering(desc != nullptr ? desc : ""); }

  void OnRspCashAuctionOrder(const ATPRspCashAuctionOrderMsg& msg, const ATPRspErrorInfo& error_info, const std::int64_t request_id) override {
    QuantapiAckFixture fixture;
    fixture.request_id = request_id;
    fixture.cl_ord_no = msg.GetClOrdNo();
    fixture.error_id = error_info.error_id;
    fixture.error_msg = error_info.GetErrorMsg() != nullptr ? error_info.GetErrorMsg() : "";
    fixture.security_id = msg.GetSecurityId() != nullptr ? msg.GetSecurityId() : "";
    fixture.market_id = static_cast<std::int32_t>(msg.GetMarketId());
    fixture.side = msg.GetSide();
    fixture.order_type = msg.GetOrderType();
    fixture.price = msg.GetPrice();
    fixture.order_qty = msg.GetOrderQty();
    on_ack(fixture);
  }

  void OnRtnCashAuctionOrder(const ATPRtnCashAuctionOrderMsg& msg) override {
    QuantapiRtnFixture fixture;
    fixture.cl_ord_no = msg.GetClOrdNo();
    fixture.orig_cl_ord_no = msg.GetOrigClOrdNo();
    fixture.ord_status = static_cast<std::int32_t>(msg.GetOrdStatus());
    fixture.security_id = msg.GetSecurityId() != nullptr ? msg.GetSecurityId() : "";
    fixture.market_id = static_cast<std::int32_t>(msg.GetMarketId());
    fixture.side = msg.GetSide();
    fixture.price = msg.GetPrice();
    fixture.order_qty = msg.GetOrderQty();
    fixture.leaves_qty = msg.GetLeavesQty();
    fixture.cum_qty = msg.GetCumQty();
    fixture.last_px = msg.GetLastPx();
    fixture.last_qty = msg.GetLastQty();
    fixture.exec_id = msg.GetExecId() != nullptr ? msg.GetExecId() : "";
    fixture.reject_reason_code = msg.GetRejectReasonCode();
    fixture.ord_rej_reason = msg.GetOrdRejReason() != nullptr ? msg.GetOrdRejReason() : "";
    fixture.transact_time = msg.GetTransactTime();
    on_rtn(fixture);
  }

  void OnRspCashCancelOrder(const ATPRspCashCancelOrderMsg& msg, const ATPRspErrorInfo& error_info, const std::int64_t request_id) override {
    QuantapiCancelRspFixture fixture;
    fixture.request_id = request_id;
    fixture.cl_ord_no = msg.GetClOrdNo();
    fixture.orig_cl_ord_no = msg.GetOrigClOrdNo();
    fixture.error_id = error_info.error_id;
    fixture.error_msg = error_info.GetErrorMsg() != nullptr ? error_info.GetErrorMsg() : "";
    on_cancel_rsp(fixture);
  }

  void OnRspCashOrderQueryResult(const ATPRspCashOrderQueryResultMsg& msg, const std::int64_t request_id, const ATPRspErrorInfo& error_info, const bool is_last) override {
    QuantapiOrderQueryFixture fixture;
    fixture.request_id = request_id;
    fixture.is_last = is_last;
    fixture.error_id = error_info.error_id;
    fixture.error_msg = error_info.GetErrorMsg() != nullptr ? error_info.GetErrorMsg() : "";
    fixture.has_row = msg.GetClOrdNo() != 0;
    fixture.cl_ord_no = msg.GetClOrdNo();
    fixture.orig_cl_ord_no = msg.GetOrigClOrdNo();
    fixture.ord_status = static_cast<std::int32_t>(msg.GetOrdStatus());
    fixture.security_id = msg.GetSecurityId() != nullptr ? msg.GetSecurityId() : "";
    fixture.market_id = static_cast<std::int32_t>(msg.GetMarketId());
    fixture.side = msg.GetSide();
    fixture.order_price = msg.GetOrderPrice();
    fixture.order_qty = msg.GetOrderQty();
    fixture.leaves_qty = msg.GetLeavesQty();
    fixture.cum_qty = msg.GetCumQty();
    fixture.order_status_info = msg.GetOrderStatusInfo() != nullptr ? msg.GetOrderStatusInfo() : "";
    on_order_query(fixture);
  }

  void OnRspCashFundQueryResult(const ATPRspCashFundQueryResultMsg& msg, const std::int64_t request_id, const ATPRspErrorInfo& error_info, const bool is_last) override {
    QuantapiFundFixture fixture;
    fixture.request_id = request_id;
    fixture.is_last = is_last;
    fixture.error_id = error_info.error_id;
    // Identity field distinguishes a data row from the empty is_last marker.
    fixture.has_row = msg.GetAccountId() != nullptr && *msg.GetAccountId() != '\0';
    fixture.leaves_value = msg.GetLeavesValue();
    fixture.init_leaves_value = msg.GetInitLeavesValue();
    fixture.frozen_all = msg.GetFrozenAll();
    fixture.available_t0 = msg.GetAvailableT0();
    fixture.available_t1 = msg.GetAvailableT1();
    on_fund(fixture);
  }

  void OnRspCashShareQueryResult(const ATPRspCashShareQueryResultMsg& msg, const std::int64_t request_id, const ATPRspErrorInfo& error_info, const bool is_last) override {
    QuantapiShareFixture fixture;
    fixture.request_id = request_id;
    fixture.is_last = is_last;
    fixture.error_id = error_info.error_id;
    fixture.has_row = msg.GetSecurityId() != nullptr && *msg.GetSecurityId() != '\0';
    fixture.security_id = msg.GetSecurityId() != nullptr ? msg.GetSecurityId() : "";
    fixture.market_id = static_cast<std::int32_t>(msg.GetMarketId());
    fixture.init_qty = msg.GetInitQty();
    fixture.leaves_qty = msg.GetLeavesQty();
    fixture.available_qty = msg.GetAvailableQty();
    fixture.cost_price = msg.GetCostPrice();
    fixture.market_value = msg.GetMarketValue();
    fixture.stock_buy = msg.GetStockBuy();
    fixture.stock_sale = msg.GetStockSale();
    on_share(fixture);
  }

  void OnRspCashAssetQueryResult(const ATPRspCashAssetQueryResultMsg& msg, const std::int64_t request_id, const ATPRspErrorInfo& error_info, const bool is_last) override {
    QuantapiAssetFixture fixture;
    fixture.request_id = request_id;
    fixture.is_last = is_last;
    fixture.error_id = error_info.error_id;
    fixture.has_row = msg.GetAccountId() != nullptr && *msg.GetAccountId() != '\0';
    fixture.leaves_value = msg.GetLeavesValue();
    fixture.init_leaves_value = msg.GetInitLeavesValue();
    fixture.frozen_all = msg.GetFrozenAll();
    fixture.available_t0 = msg.GetAvailableT0();
    fixture.available_t1 = msg.GetAvailableT1();
    fixture.total_asset = msg.GetTotalAsset();
    fixture.market_value = msg.GetMarketValue();
    fixture.fund_buy = msg.GetFundBuy();
    fixture.fund_sale = msg.GetFundSale();
    on_asset(fixture);
  }

  void OnRspCashExtQueryResultSecurityInfo(const ATPRspCashExtQueryResultSecurityInfoMsg& msg, const std::int64_t request_id, const ATPRspErrorInfo& error_info, const bool isLast) override {
    QuantapiSecInfoFixture fixture;
    fixture.request_id = request_id;
    fixture.is_last = isLast;
    fixture.error_id = error_info.error_id;
    fixture.has_row = msg.GetSecurityId() != nullptr && *msg.GetSecurityId() != '\0';
    fixture.security_id = msg.GetSecurityId() != nullptr ? msg.GetSecurityId() : "";
    fixture.market_id = static_cast<std::int32_t>(msg.GetMarketId());
    fixture.symbol = msg.GetSecuritySymbol() != nullptr ? msg.GetSecuritySymbol() : "";
    fixture.security_type = msg.GetSecurityType();
    fixture.prev_close = msg.GetPrevClosePrice();
    fixture.price_upper_limit = msg.GetPriceUpperLimit();
    fixture.price_lower_limit = msg.GetPriceLowerLimit();
    fixture.price_tick = msg.GetPriceTick();
    fixture.buy_qty_lower = msg.GetBuyQtyLowerLimit();
    fixture.sell_qty_lower = msg.GetSellQtyLowerLimit();
    fixture.day_trading = msg.GetDayTrading();
    fixture.security_status = msg.GetSecurityStatus();
    fixture.buy_qty_unit = msg.GetBuyQtyUnit();
    fixture.sell_qty_unit = msg.GetSellQtyUnit();
    on_secinfo(fixture);
  }

  // ---- internals ----------------------------------------------------------
  template <typename Fn>
  ATPErrorCodeType vendor_call(Fn&& fn) {
    const std::lock_guard<std::recursive_mutex> lock(send_);
    if (api_ == nullptr) return kSendUnavailable;
    return fn();
  }

  ATPErrorCodeType send_order(
      const OrderIntent& intent, std::int32_t market, const std::string& code, char order_type, const std::string& account, const std::string& branch, const std::string& fund, std::int64_t seq) {
    const char side = intent.side == Side::buy ? ATPSideConst::kBuy : ATPSideConst::kSell;
    const double price = intent.type == OrderType::limit ? quantapi_price_to_vendor(intent.limit_price) : quantapi_price_to_vendor(intent.protection_price);
    const double quantity = quantapi_quantity_to_vendor(intent.quantity);
    std::string cust;
    std::string password;
    session_triple(cust, password);
    return vendor_call([&] {
      ATPReqCashAuctionOrderMsg* msg = ATPReqCashAuctionOrderMsg::NewMessage(ATPBusinessTypeConst::kCashAuction);
      msg->SetCustId(cust.c_str());
      msg->SetFundAccountId(fund.c_str());
      msg->SetBranchId(branch.c_str());
      msg->SetAccountId(account.c_str());
      msg->SetPassword(password.c_str());
      msg->SetSecurityId(code.c_str());
      msg->SetMarketId(static_cast<std::uint16_t>(market));
      msg->SetSide(side);
      msg->SetOrderQty(quantity);
      msg->SetPrice(price);
      msg->SetOrderType(order_type);
      // batch_cl_ord_no intentionally not set on orders (0 = system reserved).
      const ATPErrorCodeType rc = api_->ReqCashAuctionOrder(msg, seq);
      ATPReqCashAuctionOrderMsg::DeleteMessage(msg);
      return rc;
    });
  }

  ATPErrorCodeType send_cancel(const ClientOrderId& coi, std::int64_t cl) {
    std::int64_t seq = 0;
    {
      const std::lock_guard<std::mutex> lock(state_);
      seq = alloc_seq_locked();
    }
    const std::string fund = fund_account_id_.empty() ? config_.fund_account_id : fund_account_id_;
    std::string cust;
    std::string password;
    session_triple(cust, password);
    const ATPErrorCodeType rc = vendor_call([&] {
      ATPReqCashCancelOrderMsg* msg = ATPReqCashCancelOrderMsg::NewMessage();
      msg->SetCustId(cust.c_str());
      msg->SetFundAccountId(fund.c_str());
      msg->SetOrigClOrdNo(cl);
      msg->SetPassword(password.c_str());
      msg->SetBatchClOrdNo(1);  // demo convention for cancel/query
      const ATPErrorCodeType result = api_->ReqCashCancelOrder(msg, seq);
      ATPReqCashCancelOrderMsg::DeleteMessage(msg);
      return result;
    });
    if (rc == ATPErrorCode::kSuccess) return rc;
    // Cancel path emits pending_cancel (OMS cancel() has already moved the
    // record there; pending_cancel -> pending_cancel is always accepted).
    const bool unavailable = rc == kSendUnavailable;
    const std::string reason = unavailable ? std::string("adapter: cancel failed: channel unavailable") : "adapter: cancel failed (code=" + std::to_string(rc) + ")";
    const std::string order_id = std::to_string(cl);
    enqueue_report(make_report(order_id, coi, OrderStatus::pending_cancel, 0, 0, reason, "", 0, order_id, quantapi_fixtures::kNoRejectCode));
    return rc;
  }

  ATPErrorCodeType send_order_query(std::int64_t seq, std::int32_t market, const std::string& account) {
    const std::string fund = fund_account_id_.empty() ? config_.fund_account_id : fund_account_id_;
    const std::string branch = branch_id_;
    std::string cust;
    std::string password;
    session_triple(cust, password);
    return vendor_call([&] {
      ATPReqCashOrderQueryMsg* msg = ATPReqCashOrderQueryMsg::NewMessage();
      msg->SetCustId(cust.c_str());
      msg->SetFundAccountId(fund.c_str());
      msg->SetBranchId(branch.c_str());
      msg->SetPassword(password.c_str());
      msg->SetReturnNum(0);     // 0 = all (demo)
      msg->SetClOrdNo(0);       // 0 = all (demo)
      msg->SetBatchClOrdNo(1);  // demo convention
      msg->SetMarketId(static_cast<std::uint16_t>(market));
      msg->SetAccountId(account.c_str());
      const ATPErrorCodeType rc = api_->ReqCashOrderQuery(msg, seq);
      ATPReqCashOrderQueryMsg::DeleteMessage(msg);
      return rc;
    });
  }

  ATPErrorCodeType send_share_query(std::int64_t seq, std::int32_t market, const std::string& account) {
    const std::string fund = fund_account_id_.empty() ? config_.fund_account_id : fund_account_id_;
    std::string cust;
    std::string unused;
    session_triple(cust, unused);
    return vendor_call([&] {
      ATPReqCashShareQueryMsg* msg = ATPReqCashShareQueryMsg::NewMessage();
      msg->SetCustId(cust.c_str());
      msg->SetFundAccountId(fund.c_str());
      msg->SetReturnNum(0);
      msg->SetBusinessType(0);  // 0 = all markets (demo)
      msg->SetMarketId(static_cast<std::uint16_t>(market));
      msg->SetAccountId(account.c_str());
      const ATPErrorCodeType rc = api_->ReqCashShareQuery(msg, seq);
      ATPReqCashShareQueryMsg::DeleteMessage(msg);
      return rc;
    });
  }

  ATPErrorCodeType send_fund_query(std::int64_t seq, std::int32_t market, const std::string& account) {
    const std::string fund = fund_account_id_.empty() ? config_.fund_account_id : fund_account_id_;
    const std::string branch = branch_id_;
    std::string cust;
    std::string password;
    session_triple(cust, password);
    return vendor_call([&] {
      ATPReqCashFundQueryMsg* msg = ATPReqCashFundQueryMsg::NewMessage();
      msg->SetCustId(cust.c_str());
      msg->SetFundAccountId(fund.c_str());
      msg->SetBranchId(branch.c_str());
      msg->SetPassword(password.c_str());
      msg->SetMarketId(static_cast<std::uint16_t>(market));
      msg->SetAccountId(account.c_str());
      const ATPErrorCodeType rc = api_->ReqCashFundQuery(msg, seq);
      ATPReqCashFundQueryMsg::DeleteMessage(msg);
      return rc;
    });
  }

  ATPErrorCodeType send_secinfo_query(std::int64_t seq, std::int32_t market, const std::string& account, const std::string& code) {
    const std::string fund = fund_account_id_.empty() ? config_.fund_account_id : fund_account_id_;
    std::string cust;
    std::string password;
    session_triple(cust, password);
    return vendor_call([&] {
      ATPReqCashExtQuerySecurityInfoMsg* msg = ATPReqCashExtQuerySecurityInfoMsg::NewMessage();
      msg->SetCustId(cust.c_str());
      msg->SetFundAccountId(fund.c_str());
      msg->SetPassword(password.c_str());
      msg->SetSecurityId(code.c_str());  // empty = full-market sweep (demo)
      msg->SetMarketId(static_cast<std::uint16_t>(market));
      msg->SetAccountId(account.c_str());
      const ATPErrorCodeType rc = api_->ReqCashExtQuerySecurityInfo(msg, seq);
      ATPReqCashExtQuerySecurityInfoMsg::DeleteMessage(msg);
      return rc;
    });
  }

  ATPErrorCodeType send_asset_query(std::int64_t seq, std::int32_t market, const std::string& account) {
    const std::string fund = fund_account_id_.empty() ? config_.fund_account_id : fund_account_id_;
    const std::string branch = branch_id_;
    std::string cust;
    std::string password;
    session_triple(cust, password);
    return vendor_call([&] {
      ATPReqCashAssetQueryMsg* msg = ATPReqCashAssetQueryMsg::NewMessage();
      msg->SetCustId(cust.c_str());
      msg->SetFundAccountId(fund.c_str());
      msg->SetBranchId(branch.c_str());
      msg->SetPassword(password.c_str());
      msg->SetMarketId(static_cast<std::uint16_t>(market));
      msg->SetAccountId(account.c_str());
      const ATPErrorCodeType rc = api_->ReqCashAssetQuery(msg, seq);
      ATPReqCashAssetQueryMsg::DeleteMessage(msg);
      return rc;
    });
  }

  void mark_snapshot_failure(std::int64_t seq) {
    const std::lock_guard<std::mutex> lock(state_);
    snapshot_failed_ = true;
    pending_snapshot_queries_.erase(seq);
  }

  void mark_secinfo_failure(std::int64_t seq) {
    bool finish = false;
    AccountId account;
    {
      const std::lock_guard<std::mutex> lock(state_);
      secinfo_failed_ = true;
      pending_secinfo_queries_.erase(seq);
      if (pending_secinfo_queries_.empty()) {
        // Send-side failure ends the sweep now (empty, complete).
        account = secinfo_account_id_;
        secinfo_rows_.clear();
        secinfo_account_id_.clear();
        finish = true;
      }
    }
    if (finish) enqueue_secinfo(account, {}, true);
  }

  void mark_asset_failure(std::int64_t seq) {
    const std::lock_guard<std::mutex> lock(state_);
    asset_failed_ = true;
    pending_asset_queries_.erase(seq);
  }

  void reject_locally(const ClientOrderId& coi, const std::string& reason) {
    {
      const std::lock_guard<std::mutex> lock(state_);
      ++stats_.local_rejects;
    }
    enqueue_report(make_report("", coi, OrderStatus::rejected, 0, 0, reason, "", 0, "", quantapi_fixtures::kNoRejectCode));
  }

  static ExecutionReport make_report(const std::string& order_id,
                                     const ClientOrderId& coi,
                                     OrderStatus status,
                                     Quantity last_quantity,
                                     Price last_price,
                                     const std::string& reason,
                                     const std::string& execution_id,
                                     Quantity cumulative,
                                     const std::string& broker_order_id,
                                     std::int32_t reject_code) {
    ExecutionReport report;
    report.order_id = order_id;
    report.client_order_id = coi;
    report.status = status;
    report.last_quantity = last_quantity;
    report.last_price = last_price;
    report.reason = reason;
    report.execution_id = execution_id;
    report.cumulative_quantity = cumulative;
    report.version = 0;  // OMS owns versioning
    report.broker_order_id = broker_order_id;
    report.reject_code = reject_code;
    return report;
  }

  void enqueue_report(const ExecutionReport& report) {
    {
      const std::lock_guard<std::mutex> lock(state_);
      ++stats_.reports_enqueued;
    }
    const std::lock_guard<std::mutex> lock(queue_);
    reports_.push_back(report);
  }

  void enqueue_snapshot(const TradeAccountSnapshot& snapshot) {
    const std::lock_guard<std::mutex> lock(queue_);
    snapshots_.push_back(snapshot);
  }

  void enqueue_secinfo(const AccountId& account, std::vector<SecurityInfoRow> rows, bool complete) {
    const std::lock_guard<std::mutex> lock(queue_);
    secinfo_batches_.push_back(SecInfoBatch{account, std::move(rows), complete});
  }

  void enqueue_asset(const TradeAssetSnapshot& snapshot) {
    const std::lock_guard<std::mutex> lock(queue_);
    assets_.push_back(snapshot);
  }

  void enqueue_recovery(const AccountId& account_id, bool success) {
    const std::lock_guard<std::mutex> lock(queue_);
    recoveries_.emplace_back(account_id, success);
  }

  // state_ must be held: monotonic request_id from the persisted sequence.
  std::int64_t alloc_seq_locked() {
    const std::int64_t seq = next_seq_++;
    if (sidecar_out_.is_open()) {
      sidecar_out_ << "seq " << seq << '\n';
      sidecar_out_.flush();
    }
    return seq;
  }

  // state_ must be held.
  void append_map_locked(std::int64_t cl, const ClientOrderId& coi) {
    if (sidecar_out_.is_open()) {
      sidecar_out_ << "map " << cl << ' ' << coi << '\n';
      sidecar_out_.flush();
    }
  }

  // state_ must be held.
  std::string account_for_market_locked(std::int32_t market) const {
    switch (market) {
      case quantapi_fixtures::kMarketSse:
        return account_sse_;
      case quantapi_fixtures::kMarketSzse:
        return account_szse_;
      case quantapi_fixtures::kMarketBse:
        return account_bse_;
      default:
        return {};
    }
  }

  // state_ must be held.
  void collect_markets_locked(std::vector<std::pair<std::int32_t, std::string>>& out) const {
    out.clear();
    if (!account_sse_.empty()) out.emplace_back(quantapi_fixtures::kMarketSse, account_sse_);
    if (!account_szse_.empty()) out.emplace_back(quantapi_fixtures::kMarketSzse, account_szse_);
    if (!account_bse_.empty()) out.emplace_back(quantapi_fixtures::kMarketBse, account_bse_);
  }

  // state_ must be held.
  TradeAccountSnapshot assemble_snapshot_locked() {
    TradeAccountSnapshot snapshot;
    snapshot.account_id = snapshot_account_id_.empty() ? (!fund_account_id_.empty() ? fund_account_id_ : (!cust_id_.empty() ? cust_id_ : config_.fund_account_id)) : snapshot_account_id_;
    const Price cash = quantapi_price_from_vendor(fund_.leaves);
    snapshot.total_asset = cash + snapshot_mv_sum_;  // cash + position market value
    snapshot.available_cash = quantapi_price_from_vendor(fund_.available_t1);
    snapshot.withdrawable_cash = quantapi_price_from_vendor(fund_.available_t0);
    snapshot.frozen_cash = quantapi_price_from_vendor(fund_.frozen);
    snapshot.positions.reserve(snapshot_positions_.size());
    for (const auto& entry : snapshot_positions_) snapshot.positions.push_back(entry.second);
    // Reset the builder; the next request_account_snapshot starts clean.
    fund_valid_ = false;
    fund_ = FundValues{};
    snapshot_positions_.clear();
    snapshot_mv_sum_ = 0;
    return snapshot;
  }

  void open_sidecar() {
    if (config_.sidecar_path.empty()) return;
    // Load attribution first (append-only records, last seq wins).
    std::ifstream in(config_.sidecar_path);
    if (in.is_open()) {
      std::string line;
      while (std::getline(in, line)) {
        std::istringstream fields(line);
        std::string kind;
        fields >> kind;
        if (kind == "seq") {
          std::int64_t seq = 0;
          fields >> seq;
          if (seq >= next_seq_) next_seq_ = seq + 1;
        } else if (kind == "map") {
          std::int64_t cl = 0;
          fields >> cl;
          std::string coi;
          std::getline(fields >> std::ws, coi);
          if (cl > 0 && !coi.empty()) {
            cl_to_coi_[cl] = coi;
            coi_to_cl_[coi] = cl;
          }
        }
      }
    }
    sidecar_out_.open(config_.sidecar_path, std::ios::app);
    sidecar_ok_ = sidecar_out_.is_open();
  }

  QuantapiSessionConfig config_;
  ATPProperties* props_{nullptr};
  ATPQuantAPI* api_{nullptr};
  bool init_done_{false};
  std::atomic<bool> logged_in_{false};

  // sidecar (guarded by state_: all writes happen under it)
  std::ofstream sidecar_out_;
  bool sidecar_ok_{false};
  std::int64_t next_seq_{1};

  mutable std::mutex state_;
  std::unordered_map<std::int64_t, ClientOrderId> inflight_req_;  // request_id -> coi
  std::unordered_set<ClientOrderId> inflight_cois_;
  std::unordered_map<std::int64_t, ClientOrderId> cl_to_coi_;  // attribution (also from sidecar)
  std::unordered_map<ClientOrderId, std::int64_t> coi_to_cl_;
  std::unordered_set<ClientOrderId> cancel_requested_;
  std::unordered_map<std::int64_t, std::vector<QuantapiRtnFixture>> stray_rtns_;
  std::unordered_map<ClientOrderId, OrderStatus> last_status_;
  std::unordered_map<ClientOrderId, Quantity> last_cum_;

  std::string fund_account_id_;
  std::string cust_id_;           // ADR-053: cached from the login reply (both modes)
  std::string session_password_;  // trader-supplied, memory only, zeroed on stop()
  std::string branch_id_;
  std::string account_sse_;
  std::string account_szse_;
  std::string account_bse_;

  std::unordered_set<std::int64_t> pending_order_queries_;
  std::string recovery_account_id_;
  bool order_query_failed_{false};
  std::unordered_set<std::int64_t> pending_snapshot_queries_;
  bool snapshot_failed_{false};
  bool fund_valid_{false};
  FundValues fund_;
  std::unordered_map<InstrumentId, PositionSnapshot> snapshot_positions_;
  Price snapshot_mv_sum_{0};
  std::string snapshot_account_id_;
  // Reference-data accumulation (ADR-054): rows collect per sweep, batches
  // leave through the bounded drain above.
  struct AssetValues {
    double leaves{0.0};
    double init_leaves{0.0};
    double frozen{0.0};
    double available_t0{0.0};
    double available_t1{0.0};
    double total_asset{0.0};
    double market_value{0.0};
  };
  AssetValues asset_{};
  bool asset_valid_{false};
  std::string asset_account_id_;
  std::unordered_set<std::int64_t> pending_asset_queries_;
  bool asset_failed_{false};
  struct SecInfoBatch {
    AccountId account;
    std::vector<SecurityInfoRow> rows;
    bool complete{false};
  };
  std::string secinfo_account_id_;
  std::vector<SecurityInfoRow> secinfo_rows_;
  std::unordered_set<std::int64_t> pending_secinfo_queries_;
  bool secinfo_failed_{false};

  TradeApi::ExecutionHandler execution_handler_;
  TradeApi::AccountSnapshotHandler account_snapshot_handler_;
  TradeApi::RecoveryCompleteHandler recovery_complete_handler_;
  TradeApi::SecurityInfoHandler secinfo_handler_;
  TradeApi::AssetSnapshotHandler asset_snapshot_handler_;
  QuantapiAdapterStats stats_;

  std::recursive_mutex send_;
  mutable std::mutex queue_;
  std::deque<ExecutionReport> reports_;
  std::deque<TradeAccountSnapshot> snapshots_;
  std::deque<SecInfoBatch> secinfo_batches_;
  std::deque<TradeAssetSnapshot> assets_;
  std::deque<std::pair<AccountId, bool>> recoveries_;
};

// ---------------------------------------------------------------------------
// Adapter surface: thin forwarding to Impl.
// ---------------------------------------------------------------------------
QuantapiTradeAdapter::QuantapiTradeAdapter(QuantapiSessionConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

QuantapiTradeAdapter::~QuantapiTradeAdapter() = default;

bool QuantapiTradeAdapter::connected() const noexcept { return impl_->connected(); }

void QuantapiTradeAdapter::submit(const OrderIntent& intent) { impl_->submit(intent); }

void QuantapiTradeAdapter::cancel(const ClientOrderId& client_order_id) { impl_->cancel(client_order_id); }

void QuantapiTradeAdapter::set_execution_handler(ExecutionHandler handler) { impl_->set_execution_handler(std::move(handler)); }

void QuantapiTradeAdapter::request_open_orders(const AccountId& account_id) { impl_->request_open_orders(account_id); }

void QuantapiTradeAdapter::request_account_snapshot(const AccountId& account_id) { impl_->request_account_snapshot(account_id); }

void QuantapiTradeAdapter::request_security_info(const AccountId& account_id, const InstrumentId& instrument) { impl_->request_security_info(account_id, instrument); }

void QuantapiTradeAdapter::set_security_info_handler(SecurityInfoHandler handler) { impl_->set_security_info_handler(std::move(handler)); }

void QuantapiTradeAdapter::request_asset_snapshot(const AccountId& account_id) { impl_->request_asset_snapshot(account_id); }

void QuantapiTradeAdapter::set_asset_snapshot_handler(AssetSnapshotHandler handler) { impl_->set_asset_snapshot_handler(std::move(handler)); }

void QuantapiTradeAdapter::set_account_snapshot_handler(AccountSnapshotHandler handler) { impl_->set_account_snapshot_handler(std::move(handler)); }

void QuantapiTradeAdapter::set_recovery_complete_handler(RecoveryCompleteHandler handler) { impl_->set_recovery_complete_handler(std::move(handler)); }

bool QuantapiTradeAdapter::start() { return impl_->start(); }

bool QuantapiTradeAdapter::connect() { return impl_->connect(); }

bool QuantapiTradeAdapter::login(const std::string& identity, const std::string& password) { return impl_->login(identity, password); }

std::string QuantapiTradeAdapter::session_fund() const { return impl_->session_fund(); }

void QuantapiTradeAdapter::stop() { impl_->stop(); }

bool QuantapiTradeAdapter::started() const noexcept { return impl_->started(); }

std::size_t QuantapiTradeAdapter::drain() { return impl_->drain(); }

QuantapiAdapterStats QuantapiTradeAdapter::stats() const { return impl_->stats(); }

const QuantapiSessionConfig& QuantapiTradeAdapter::config() const { return impl_->config(); }

// ---- fixture entry points (seam + vendor callback bridge) -----------------
void QuantapiTradeAdapter::on_login(const QuantapiLoginFixture& fixture) { impl_->on_login(fixture); }

void QuantapiTradeAdapter::on_ack(const QuantapiAckFixture& fixture) { impl_->on_ack(fixture); }

void QuantapiTradeAdapter::on_rtn(const QuantapiRtnFixture& fixture) { impl_->on_rtn(fixture); }

void QuantapiTradeAdapter::on_cancel_rsp(const QuantapiCancelRspFixture& fixture) { impl_->on_cancel_rsp(fixture); }

void QuantapiTradeAdapter::on_order_query(const QuantapiOrderQueryFixture& fixture) { impl_->on_order_query(fixture); }

void QuantapiTradeAdapter::on_fund(const QuantapiFundFixture& fixture) { impl_->on_fund(fixture); }

void QuantapiTradeAdapter::on_share(const QuantapiShareFixture& fixture) { impl_->on_share(fixture); }

void QuantapiTradeAdapter::on_asset(const QuantapiAssetFixture& fixture) { impl_->on_asset(fixture); }

void QuantapiTradeAdapter::on_secinfo(const QuantapiSecInfoFixture& fixture) { impl_->on_secinfo(fixture); }

void QuantapiTradeAdapter::on_logout(const std::string& desc) { impl_->on_logout(desc); }

void QuantapiTradeAdapter::on_recovering(const std::string& desc) { impl_->on_recovering(desc); }

namespace quantapi_detail {

void inject_login(QuantapiTradeAdapter& adapter, const QuantapiLoginFixture& fixture) { adapter.on_login(fixture); }

void inject_ack(QuantapiTradeAdapter& adapter, const QuantapiAckFixture& fixture) { adapter.on_ack(fixture); }

void inject_rtn(QuantapiTradeAdapter& adapter, const QuantapiRtnFixture& fixture) { adapter.on_rtn(fixture); }

void inject_cancel_rsp(QuantapiTradeAdapter& adapter, const QuantapiCancelRspFixture& fixture) { adapter.on_cancel_rsp(fixture); }

void inject_order_query(QuantapiTradeAdapter& adapter, const QuantapiOrderQueryFixture& fixture) { adapter.on_order_query(fixture); }

void inject_fund(QuantapiTradeAdapter& adapter, const QuantapiFundFixture& fixture) { adapter.on_fund(fixture); }

void inject_share(QuantapiTradeAdapter& adapter, const QuantapiShareFixture& fixture) { adapter.on_share(fixture); }

void inject_asset(QuantapiTradeAdapter& adapter, const QuantapiAssetFixture& fixture) { adapter.on_asset(fixture); }

void inject_secinfo(QuantapiTradeAdapter& adapter, const QuantapiSecInfoFixture& fixture) { adapter.on_secinfo(fixture); }

void inject_inflight(QuantapiTradeAdapter& adapter, std::int64_t request_id, const ClientOrderId& client_order_id) {
  adapter.impl_->inflight_req_[request_id] = client_order_id;
  adapter.impl_->inflight_cois_.insert(client_order_id);
}

}  // namespace quantapi_detail
}  // namespace sts
