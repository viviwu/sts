// clang-format off
/**
  ******************************************************************************
  * @file           : admin_socket.cpp
  * @author         : vivi wu
  * @brief          : Non-blocking Unix query socket serviced per gateway tick.
  * @version        : 0.1.0
  * @date           : 05/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/ops/admin_socket.hpp"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <stdexcept>

#include "nlohmann/json.hpp"

namespace sts {
namespace {

void set_nonblocking(int sock) {
  const int flags = ::fcntl(sock, F_GETFL, 0);
  if (flags >= 0) ::fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

void send_all(int sock, const std::string& text) {
  std::size_t sent = 0;
  while (sent < text.size()) {
    const ssize_t done = ::send(sock, text.data() + sent, text.size() - sent, MSG_NOSIGNAL);
    if (done <= 0) return;  // peer gone mid-reply: drop it
    sent += static_cast<std::size_t>(done);
  }
}

}  // namespace

AdminSocket::AdminSocket(std::string path)
    : path_(std::move(path)) {
  if (path_.empty()) return;
  if (path_.size() >= sizeof(sockaddr_un::sun_path)) throw std::runtime_error("admin socket path too long: " + path_);
  std::error_code ignored;
  std::filesystem::remove(path_, ignored);  // stale file from an unclean stop
  const int sock = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0) throw std::runtime_error("admin socket: socket() failed");
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  std::strncpy(address.sun_path, path_.c_str(), sizeof(address.sun_path) - 1);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): sockaddr_un downcast is the POSIX API shape.
  if (::bind(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || ::listen(sock, 4) != 0) {
    ::close(sock);
    throw std::runtime_error("admin socket: bind/listen failed: " + path_);
  }
  set_nonblocking(sock);
  listen_fd_ = sock;
}

AdminSocket::~AdminSocket() {
  for (const auto& peer : peers_) {
    if (peer.fd >= 0) ::close(peer.fd);
  }
  if (listen_fd_ >= 0) ::close(listen_fd_);
  if (!path_.empty()) {
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
  }
}

void AdminSocket::poll_once(const std::function<std::string()>& build_state) {
  if (listen_fd_ < 0) return;
  for (;;) {
    const int conn = ::accept(listen_fd_, nullptr, nullptr);
    if (conn < 0) break;
    if (peers_.size() >= 8) {
      ::close(conn);  // busy: shed load instead of queueing
      continue;
    }
    set_nonblocking(conn);
    peers_.push_back({conn, {}});
  }
  for (auto iter = peers_.begin(); iter != peers_.end();) {
    char chunk[256]{0};
    const ssize_t done = ::recv(iter->fd, chunk, sizeof(chunk) - 1, 0);
    if (done > 0) iter->pending.append(chunk, static_cast<std::size_t>(done));
    const std::size_t end = iter->pending.find('\n');
    if (end == std::string::npos) {
      if (done == 0 || iter->pending.size() > 1024) {
        ::close(iter->fd);
        iter = peers_.erase(iter);
      } else {
        ++iter;
      }
      continue;
    }
    const std::string line = iter->pending.substr(0, end);
    if (line == "state") {
      send_all(iter->fd, build_state() + "\n");
    } else {
      send_all(iter->fd, "ERR unknown command\n");
    }
    ::close(iter->fd);
    iter = peers_.erase(iter);
  }
}

std::string query_live_state(const std::string& socket_path) {
  if (socket_path.empty()) throw std::runtime_error("query socket needs a non-empty path");
  if (socket_path.size() >= sizeof(sockaddr_un::sun_path)) throw std::runtime_error("query socket path too long: " + socket_path);
  const int sock = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0) throw std::runtime_error("query socket: socket() failed");
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  std::strncpy(address.sun_path, socket_path.c_str(), sizeof(address.sun_path) - 1);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): sockaddr_un downcast is the POSIX API shape.
  const bool connected = ::connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
  if (!connected) {
    ::close(sock);
    throw std::runtime_error("query socket: cannot connect (server down?): " + socket_path);
  }
  static constexpr const char* kAsk = "state\n";
  send_all(sock, kAsk);
  std::string reply;
  char chunk[512]{0};
  for (;;) {
    const ssize_t done = ::recv(sock, chunk, sizeof(chunk) - 1, 0);
    if (done <= 0) break;
    reply.append(chunk, static_cast<std::size_t>(done));
  }
  ::close(sock);
  if (reply.empty() || reply[0] != '{') throw std::runtime_error("query socket: bad reply from " + socket_path);
  return reply;
}

}  // namespace sts
