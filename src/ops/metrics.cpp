// clang-format off
/**
  ******************************************************************************
  * @file           : metrics.cpp
  * @author         : vivi wu
  * @brief          : Metrics registry implementation with Prometheus text export.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/ops/metrics.hpp"

namespace sts {

void MetricsRegistry::inc(const std::string& name, std::int64_t delta) {
  std::scoped_lock lock{mutex_};
  Sample& sample = samples_[name];
  sample.name = name;
  sample.kind = Kind::counter;
  sample.value += delta;
}

void MetricsRegistry::set_gauge(const std::string& name, std::int64_t value) {
  std::scoped_lock lock{mutex_};
  Sample& sample = samples_[name];
  sample.name = name;
  sample.kind = Kind::gauge;
  sample.value = value;
}

std::int64_t MetricsRegistry::get(const std::string& name) const {
  std::scoped_lock lock{mutex_};
  const auto iter = samples_.find(name);
  return iter == samples_.end() ? 0 : iter->second.value;
}

std::vector<MetricsRegistry::Sample> MetricsRegistry::snapshot() const {
  std::scoped_lock lock{mutex_};
  std::vector<Sample> out;
  out.reserve(samples_.size());
  for (const auto& entry : samples_) out.push_back(entry.second);
  return out;
}

void MetricsRegistry::write_prometheus(std::ostream& out) const {
  for (const auto& sample : snapshot()) {
    out << "# TYPE " << sample.name << (sample.kind == Kind::counter ? " counter\n" : " gauge\n");
    out << sample.name << ' ' << sample.value << '\n';
  }
}

}  // namespace sts
