// clang-format off
/**
  ******************************************************************************
  * @file           : core_test.cpp
  * @author         : vivi wu
  * @brief          : Unit tests for clocks, envelopes and the risk-chain interface freeze.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include <cassert>
#include <chrono>
#include <unordered_set>

#include "qts/core/clock.hpp"
#include "qts/core/event.hpp"
#include "qts/core/event_bus.hpp"

namespace {

void manual_clock_set_and_advance() {
  qts::ManualClock clock{};
  assert(clock.now() == qts::Timestamp{});  // epoch by default

  const qts::Timestamp t1{std::chrono::seconds{1'700'000'000}};
  clock.set(t1);
  assert(clock.now() == t1);
  clock.advance(std::chrono::milliseconds{500});
  assert(clock.now() == t1 + std::chrono::milliseconds{500});
}

void system_clock_is_sane() {
  qts::SystemClock clock{};
  const qts::Timestamp t0 = clock.now();
  assert(t0 > qts::Timestamp{});  // past epoch
  assert(clock.now() >= t0);      // non-decreasing
}

void clock_is_polymorphic() {
  qts::ManualClock manual{};
  const qts::Clock& as_base = manual;
  const qts::Timestamp t{std::chrono::seconds{42}};
  manual.set(t);
  assert(as_base.now() == t);
}

void event_id_generator_is_monotonic() {
  qts::EventIdGenerator gen{};
  assert(gen.next() == 1);
  assert(gen.next() == 2);

  qts::EventIdGenerator replay{1'000};  // replay reproduces ids from a start
  assert(replay.next() == 1'000);

  std::unordered_set<std::uint64_t> seen;
  for (int i = 0; i < 1'000; ++i) seen.insert(gen.next());
  assert(seen.size() == 1'000);
}

void envelope_defaults() {
  const qts::EventEnvelope env{};
  assert(env.event_id == 0);
  assert(env.trace_id.empty());
  assert(env.source_time == qts::Timestamp{});
  assert(env.receive_time == qts::Timestamp{});
  assert(env.config_version.empty());
  assert(env.rules_version.empty());
  assert(env.source_message_id.empty());
}

// M11-7 regression: a released one-shot collector must never see a later
// publish (the restart segfault was a dangling query subscription).
void unsubscribed_collector_never_fires() {
  qts::EventBus events;
  int permanent = 0;
  events.subscribe_execution([&](const qts::ExecutionReport&) { ++permanent; });
  int collected = 0;
  const qts::EventBus::Subscription token = events.subscribe_execution([&](const qts::ExecutionReport&) { ++collected; });
  qts::ExecutionReport report{};
  events.publish(report);
  assert(collected == 1 && permanent == 1);
  events.unsubscribe_execution(token);
  events.publish(report);
  assert(collected == 1 && permanent == 2);
  events.unsubscribe_execution(999'999);  // unknown token: silent no-op, never throws
  events.publish(report);
  assert(collected == 1 && permanent == 3);
}

}  // namespace

int main() {
  manual_clock_set_and_advance();
  system_clock_is_sane();
  clock_is_polymorphic();
  event_id_generator_is_monotonic();
  envelope_defaults();
  unsubscribed_collector_never_fires();
  return 0;
}
