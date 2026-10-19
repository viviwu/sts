// clang-format off
/**
  ******************************************************************************
  * @file           : clock.hpp
  * @author         : vivi wu
  * @brief          : Controllable clocks: wall clock plus deterministic manual clock for replay.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <chrono>
#include <mutex>

#include "qts/core/types.hpp"

namespace qts {

// Time source abstraction. Production uses SystemClock; replay, simulation
// and unit tests use ManualClock so identical event streams reproduce
// identical results. Implementations must never block on I/O.
class Clock {
 public:
  Clock() = default;
  virtual ~Clock() = default;

  Clock(const Clock&) = delete;
  Clock& operator=(const Clock&) = delete;
  Clock(Clock&&) = delete;
  Clock& operator=(Clock&&) = delete;

  [[nodiscard]] virtual Timestamp now() const = 0;
};

class SystemClock final : public Clock {
 public:
  [[nodiscard]] Timestamp now() const override { return std::chrono::system_clock::now(); }
};

// Deterministic clock for tests and replay. Single-writer in the replay
// driver; concurrent readers are safe.
class ManualClock final : public Clock {
 public:
  explicit ManualClock(Timestamp start = Timestamp{})
      : now_{start} {}

  Timestamp now() const override {
    std::lock_guard<std::mutex> lock{mutex_};
    return now_;
  }

  void set(Timestamp time) {
    std::lock_guard<std::mutex> lock{mutex_};
    now_ = time;
  }

  void advance(std::chrono::nanoseconds delta) {
    std::lock_guard<std::mutex> lock{mutex_};
    now_ += delta;
  }

 private:
  mutable std::mutex mutex_;
  Timestamp now_;
};

}  // namespace qts
