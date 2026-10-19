// clang-format off
/**
  ******************************************************************************
  * @file           : metrics.hpp
  * @author         : vivi wu
  * @brief          : Prometheus-text metrics registry (counters, gauges) for OMS and apps.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <mutex>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace sts {

// Minimal in-process metrics (M3-3). Counters and gauges behind a
// mutex-guarded registry; export is plain Prometheus exposition text so
// no exporter dependency is needed until real infra lands (M3+).
// Hot-path cost is one mutex lock per event; keep reporting, not
// per-tick sampling, on this path.
class MetricsRegistry {
 public:
  enum class Kind : std::uint8_t { counter, gauge };

  struct Sample {
    std::string name;
    Kind kind{Kind::counter};
    std::int64_t value{0};
  };

  void inc(const std::string& name, std::int64_t delta = 1);
  void set_gauge(const std::string& name, std::int64_t value);
  [[nodiscard]] std::int64_t get(const std::string& name) const;
  [[nodiscard]] std::vector<Sample> snapshot() const;
  void write_prometheus(std::ostream& out) const;

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, Sample> samples_;
};

}  // namespace sts
