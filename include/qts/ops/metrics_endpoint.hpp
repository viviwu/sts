// clang-format off
/**
  ******************************************************************************
  * @file           : metrics_endpoint.hpp
  * @author         : vivi wu
  * @brief          : Minimal Prometheus scrape endpoint over POSIX TCP loopback.
  * @version        : 0.1.0
  * @date           : 05/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include "qts/ops/metrics.hpp"

namespace qts {

// Minimal Prometheus scrape endpoint (observability wrap-up). Serves the
// registry's exposition text on GET /metrics (anything else 404s) from a
// single background thread; the gateway hot path never touches it (the
// registry snapshot is mutex-guarded inside MetricsRegistry already).
//
// Zero new dependencies by design: plain POSIX sockets, HTTP/1.0 framing,
// one connection at a time, loopback only unless the listen host says
// otherwise (ops takes responsibility for binding beyond localhost).
// Empty listen string disables the endpoint entirely (no thread, no
// socket): the default, so drills and tests stay silent.
class MetricsEndpoint {
 public:
  // listen is "host:port" ("127.0.0.1:9090"); host may be empty (=
  // 127.0.0.1), port may be 0 (ephemeral, for tests — see port()).
  // Throws std::runtime_error on resolution/bind/listen failure.
  explicit MetricsEndpoint(std::string listen, MetricsRegistry& registry);
  ~MetricsEndpoint();

  MetricsEndpoint(const MetricsEndpoint&) = delete;
  MetricsEndpoint& operator=(const MetricsEndpoint&) = delete;
  MetricsEndpoint(MetricsEndpoint&&) = delete;
  MetricsEndpoint& operator=(MetricsEndpoint&&) = delete;

  [[nodiscard]] bool enabled() const noexcept { return listen_fd_ >= 0; }
  // Actual bound port (== requested unless 0 was given); 0 when disabled.
  [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

 private:
  void serve();
  void serve_one(int conn);

  std::string listen_;
  MetricsRegistry& registry_;
  int listen_fd_{-1};
  std::uint16_t port_{0};
  std::atomic<bool> stop_{false};
  std::thread worker_;
};

}  // namespace qts
