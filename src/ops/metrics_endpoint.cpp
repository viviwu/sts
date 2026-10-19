// clang-format off
/**
  ******************************************************************************
  * @file           : metrics_endpoint.cpp
  * @author         : vivi wu
  * @brief          : Single-threaded Prometheus scrape endpoint implementation.
  * @version        : 0.1.0
  * @date           : 05/10/26
  ******************************************************************************
  */
// clang-format on
#include "qts/ops/metrics_endpoint.hpp"

#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <sstream>
#include <stdexcept>

namespace qts {
namespace {

int open_listener(const std::string& host, const std::string& service, std::uint16_t& bound_port) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  addrinfo* list = nullptr;
  if (getaddrinfo(host.empty() ? nullptr : host.c_str(), service.c_str(), &hints, &list) != 0) {
    throw std::runtime_error("metrics endpoint: cannot resolve listen address");
  }
  int sock = -1;
  for (addrinfo* item = list; item != nullptr; item = item->ai_next) {
    sock = ::socket(item->ai_family, item->ai_socktype, item->ai_protocol);
    if (sock < 0) continue;
    int reuse = 1;
    ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (::bind(sock, item->ai_addr, item->ai_addrlen) == 0 && ::listen(sock, 4) == 0) break;
    ::close(sock);
    sock = -1;
  }
  if (sock >= 0) {
    sockaddr_storage actual{};
    socklen_t length = sizeof(actual);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): sockaddr_* downcasts are the POSIX API shape.
    if (::getsockname(sock, reinterpret_cast<sockaddr*>(&actual), &length) == 0) {
      if (actual.ss_family == AF_INET) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): see above.
        bound_port = ntohs(reinterpret_cast<sockaddr_in*>(&actual)->sin_port);
      } else if (actual.ss_family == AF_INET6) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): see above.
        bound_port = ntohs(reinterpret_cast<sockaddr_in6*>(&actual)->sin6_port);
      }
    }
  }
  freeaddrinfo(list);
  if (sock < 0) throw std::runtime_error("metrics endpoint: bind/listen failed");
  return sock;
}

void send_all(int sock, const std::string& text) {
  std::size_t sent = 0;
  while (sent < text.size()) {
    const ssize_t done = ::send(sock, text.data() + sent, text.size() - sent, MSG_NOSIGNAL);
    if (done <= 0) return;  // peer gone: nothing to report to
    sent += static_cast<std::size_t>(done);
  }
}

}  // namespace

MetricsEndpoint::MetricsEndpoint(std::string listen, MetricsRegistry& registry)
    : listen_(std::move(listen)),
      registry_(registry) {
  if (listen_.empty()) return;
  const std::size_t colon = listen_.find_last_of(':');
  if (colon == std::string::npos) throw std::runtime_error("metrics endpoint: listen must be host:port");
  listen_fd_ = open_listener(listen_.substr(0, colon), listen_.substr(colon + 1), port_);
  worker_ = std::thread(&MetricsEndpoint::serve, this);
}

MetricsEndpoint::~MetricsEndpoint() {
  stop_ = true;
  if (worker_.joinable()) worker_.join();
  if (listen_fd_ >= 0) ::close(listen_fd_);
}

void MetricsEndpoint::serve() {
  while (!stop_) {
    fd_set readable{};
    FD_ZERO(&readable);
    FD_SET(listen_fd_, &readable);
    timeval window{};
    window.tv_sec = 0;
    window.tv_usec = 50'000;
    const int ready = ::select(listen_fd_ + 1, &readable, nullptr, nullptr, &window);
    if (ready <= 0) continue;
    const int conn = ::accept(listen_fd_, nullptr, nullptr);
    if (conn < 0) continue;
    serve_one(conn);
    ::close(conn);
  }
}

void MetricsEndpoint::serve_one(int conn) {
  std::string request;
  char chunk[512]{0};
  while (request.size() < 4096) {
    const ssize_t done = ::recv(conn, chunk, sizeof(chunk) - 1, 0);
    if (done <= 0) return;
    request.append(chunk, static_cast<std::size_t>(done));
    if (request.find("\r\n\r\n") != std::string::npos || request.find("\n\n") != std::string::npos) break;
  }
  std::istringstream first{request};
  std::string method;
  std::string target;
  first >> method >> target;
  if (method == "GET" && target == "/metrics") {
    std::ostringstream body;
    registry_.write_prometheus(body);
    const std::string payload = body.str();
    std::ostringstream head;
    head << "HTTP/1.0 200 OK\r\nContent-Type: text/plain; version=0.0.4\r\nContent-Length: " << payload.size() << "\r\nConnection: close\r\n\r\n";
    send_all(conn, head.str());
    send_all(conn, payload);
    return;
  }
  static constexpr const char* kNotFound = "HTTP/1.0 404 Not Found\r\nContent-Length: 9\r\nConnection: close\r\n\r\nnot found";
  send_all(conn, kNotFound);
}

}  // namespace qts
