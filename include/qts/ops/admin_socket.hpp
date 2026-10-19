// clang-format off
/**
  ******************************************************************************
  * @file           : admin_socket.hpp
  * @author         : vivi wu
  * @brief          : Tick-serviced Unix query socket for live server state.
  * @version        : 0.1.0
  * @date           : 05/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace qts {

// Live query socket (observability wrap-up): answers "state" with the same
// JSON payload as the state file, but freshly built on demand instead of
// every-50-ticks. Serviced inside the gateway tick (poll_once), so there
// is no extra thread and no shared-state races: the tick thread is the
// only reader/writer on both ends.
//
// Protocol (one line, one reply, then close):
//   "state\n"  -> "<live-state JSON>\n"
//   anything else -> "ERR unknown command\n"
// Transport is a Unix-domain socket (filesystem permissions are the access
// control); empty path disables the socket entirely. Partial request lines
// accumulate across ticks (bounded); idle/broken peers are dropped.
class AdminSocket {
 public:
  // Binds path (unlinks a stale file first). Throws std::runtime_error on
  // overlong paths or bind/listen failure. Empty path: disabled, poll_once
  // is a no-op.
  explicit AdminSocket(std::string path);
  ~AdminSocket();

  AdminSocket(const AdminSocket&) = delete;
  AdminSocket& operator=(const AdminSocket&) = delete;
  AdminSocket(AdminSocket&&) = delete;
  AdminSocket& operator=(AdminSocket&&) = delete;

  [[nodiscard]] bool enabled() const noexcept { return listen_fd_ >= 0; }

  // Accept pending peers and advance their requests; complete "state"
  // lines are answered with build_state() and closed. Never blocks.
  void poll_once(const std::function<std::string()>& build_state);

 private:
  struct Peer {
    int fd{-1};
    std::string pending;
  };

  std::string path_;
  int listen_fd_{-1};
  std::vector<Peer> peers_;
};

// Query client for qts_admin: connects, asks "state", returns the single
// reply line (JSON — parsed by the caller). Throws std::runtime_error on
// connect/send failure or on a non-JSON reply (server down, wrong path,
// or mid-shutdown race — retry by policy).
[[nodiscard]] std::string query_live_state(const std::string& socket_path);

}  // namespace qts
