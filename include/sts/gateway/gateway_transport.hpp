// clang-format off
/**
  ******************************************************************************
  * @file           : gateway_transport.hpp
  * @author         : vivi wu
  * @brief          : Async socket shell over the synchronous GatewayService core.
  * @version        : 0.1.0
  * @date           : 10/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "sts/gateway/gateway_service.hpp"

namespace sts::gateway {

// Async shell over the synchronous GatewayService core (ADR-041, M12-1).
//
// The core stays synchronous and deterministic for tests/demo/server; this
// transport gives remote operator apps the API/SPI shape: requests carry a
// caller req id and get exactly one reply echoing it, while quotes/reports
// arrive as unprompted push frames. Request ids live only in this layer —
// the core never sees them.
//
// Wire protocol (one JSON object per line, '\n' terminated):
//   request: {"req": 7, "op": "login", "token": "sess-1", ...params}
//   reply:   {"req": 7, "ok": true, "message": "...", "data": {...}}
//   push:    {"push": "quote", "quote": {...}} / {"push": "report", ...} /
//            {"push": "strategy", "strategy": {...}} (lifecycle started|stopped
//            through the service; inbox-driven changes stay list-only).
// Frames are FIFO per peer; a request's triggered pushes precede its reply
// (strategy start/stop emit synchronously during dispatch). Clients
// correlate replies by req, never by arrival order.
// Malformed lines reply {"req": 0, "ok": false, ...} (fail closed, loudly).
//
// Ops: ping | login | logout | whoami | submit | cancel | strategy_start |
// strategy_stop | strategy_list | strategy_progress | fills | settle_confirm |
// cash | open_orders | find_order | positions_for | settle_report |
// subscribe_quotes | subscribe_reports | subscribe_strategies | unsubscribe.
// Auth rule (ADR-050): paper-only servers stay open (no passwords, socket
// permissions are the access control); live-broker accounts need their
// password (accessing a counter naturally needs login, sim and live alike).
// A bound session holds full permission on its account, no scopes.
// Unknown fields are ignored, never refused.
//
// Per-account nuance: paper accounts stay open while live-broker accounts
// need their password on the same server. Anonymous requests naming a
// login-required account refuse; anonymous global views refuse whenever the
// gate is on (they would mix live data in); bound sessions default to their
// own account and refuse explicit strangers.
struct GatewayAuth {
  // Accounts requiring password login (server: live-broker accounts).
  // Empty = paper-open mode (socket permissions are the access control).
  std::vector<AccountId> login_accounts;
  // Password check against the broker credential the server logged in with
  // (constant-time, memory only, never logged). Empty = nobody can log in.
  std::function<bool(const AccountId&, const std::string&)> verify;
  // Counter login hook (ADR-053 lazy login): when set, gateway login calls
  // it with what the trader typed; on true it outputs the canonical
  // account to bind (e.g. the counter fund id). It performs the counter
  // login on first use and verifies against the live session afterwards
  // (no counter traffic). Unset keeps the verify-only path (paper/tests).
  std::function<bool(const AccountId&, const std::string&, std::string&)> counter_login;
};
//
// Threading: the server tick thread owns the transport. poll_once accepts,
// dispatches and flushes without blocking; subscriber handlers queue pushes
// into per-peer outboxes for the next flush (same drain-pump discipline as
// the vendor adapters). Slow peers (outbox over 1MiB) are dropped with
// their subscriptions; at most 8 peers wait, the 9th is refused.
class GatewayTransport {
 public:
  // Binds path (unlinks a stale file first). Throws std::runtime_error on
  // overlong paths or bind/listen failure. Empty path disables the socket.
  // bind adds a TCP listen endpoint "host:port" alongside the Unix socket
  // (P1, ADR-049; empty disables, port 0 selects an ephemeral port). auth
  // carries the login tables (empty = paper-open mode, see above).
  GatewayTransport(GatewayService& service, std::string socket_path, std::string bind = {}, GatewayAuth auth = {});
  ~GatewayTransport();

  GatewayTransport(const GatewayTransport&) = delete;
  GatewayTransport& operator=(const GatewayTransport&) = delete;
  GatewayTransport(GatewayTransport&&) = delete;
  GatewayTransport& operator=(GatewayTransport&&) = delete;

  [[nodiscard]] bool enabled() const noexcept { return listen_fd_ >= 0 || tcp_fd_ >= 0; }
  // Bound TCP endpoint "host:port" (actual port when 0 was requested).
  // Empty when TCP is disabled.
  [[nodiscard]] const std::string& tcp_endpoint() const noexcept { return tcp_endpoint_; }

  // Accept peers, dispatch complete lines, flush outboxes. Never blocks.
  void poll_once();

  // Session gate for the file-local dispatch (same tick thread only).
  [[nodiscard]] bool auth_enabled() const noexcept { return gate_on(); }
  [[nodiscard]] bool gate_on() const noexcept { return !auth_.login_accounts.empty(); }
  // Account-password login (ADR-050): verifies against the broker
  // credential via auth_.verify (single failure shape, no enumeration) and
  // binds full permission on that account. Returns false with error on
  // mismatch, unknown accounts, or disabled auth.
  bool login_peer(int peer_fd, const std::string& account, const std::string& credential, std::string& bound_account_out, std::string& error);
  // Idempotent: clears the session (account binding included) and its
  // subscriptions.
  void logout_peer(int peer_fd);
  // Gate: ping/login/logout/whoami always pass; paper-account requests
  // pass open; live-account requests and global views need a session when
  // the gate is on. Bound sessions pass everything on their own account.
  // Non-const: session lookup walks the live peer set (same tick thread).
  bool authorize(int peer_fd, const std::string& operation, const std::string& account, std::string& error);
  // Bound account of an authed account-session (false otherwise): the
  // transport-level account gate defaults/filters on it.
  bool bound_session_account(int peer_fd, std::string& account_out);
  // Identity snapshot for whoami (open mode reports the default admin).
  void identity(int peer_fd, std::string& name_out, std::string& scope_out, std::string& account_out, bool& authed_out);

 private:
  struct Peer {
    int fd{-1};
    std::string pending;
    std::string outbox;
    std::vector<SubscriptionToken> subscriptions;
    std::string peer_operator;
    std::string scope;
    std::string bound_account;  // ADR-050: account session (full permission)
    bool authed{false};
  };

  void drop_peer(std::size_t index);
  void queue_push(int peer_fd, const std::string& frame);
  [[nodiscard]] Peer* find_peer(int peer_fd);
  void accept_peers();
  void accept_tcp_peers();
  // Reads one peer's pending bytes and dispatches complete lines (replies
  // queued to its outbox). Returns false when the peer must be dropped.
  bool pump_peer(std::size_t index);
  // Flushes one peer's outbox without blocking. Returns false on a dead
  // peer or an overgrown outbox (slow consumer shed).
  bool flush_peer(std::size_t index);

  // Non-owning borrowed view (same pattern as control::ControlContext):
  // the server owns the service lifetime.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  GatewayService& service_;
  std::string path_;
  GatewayAuth auth_;
  std::string tcp_endpoint_;
  int listen_fd_{-1};
  int tcp_fd_{-1};
  std::vector<Peer> peers_;
};

// Blocking client for tests and sts_admin live mode (never the hot path):
// connects on construction, throws std::runtime_error when the server is
// unreachable. One send per request line; one recv_line per reply/push.
class GatewayClient {
 public:
  explicit GatewayClient(std::string socket_path);
  // TCP peer (P1): connects to host:port on construction, same framing.
  GatewayClient(std::string host, std::uint16_t port);
  ~GatewayClient();

  GatewayClient(const GatewayClient&) = delete;
  GatewayClient& operator=(const GatewayClient&) = delete;
  GatewayClient(GatewayClient&&) = delete;
  GatewayClient& operator=(GatewayClient&&) = delete;

  // Sends one JSON line (appends '\n'). Throws on failure.
  void send(const std::string& line) const;
  // Reads one '\n'-terminated frame (blocking, default 2s). Throws on
  // timeout, closed peer or overlong frames. Surplus bytes past the first
  // newline are stashed for the next call, so pipelined replies survive.
  [[nodiscard]] std::string recv_line(int timeout_ms = 2000);
  // Poll one frame: waits up to timeout_ms, returns true with the frame in
  // out, or false on a quiet timeout. Peer loss and overlong frames throw
  // (a resident reader must tell "nothing yet" from "server gone").
  bool poll_line(std::string& out, int timeout_ms);

 private:
  int sock_{-1};
  std::string stash_;
};

}  // namespace sts::gateway
