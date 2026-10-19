// clang-format off
/**
  ******************************************************************************
  * @file           : log.cpp
  * @author         : vivi wu
  * @brief          : Logger implementation: level filter, UTC stamps, unique files, daily rotation.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#include "sts/ops/log.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
  #include <process.h>
#else
  #include <unistd.h>
#endif

namespace sts::log {
namespace {

std::atomic<int>& level_storage() {
  static std::atomic<int> minimum{static_cast<int>(Level::info)};
  return minimum;
}

std::mutex& write_mutex() {
  static std::mutex mutex;
  return mutex;
}

void write_escaped(std::ostream& out, const std::string& value) {
  out << '"';
  for (const char chr : value) {
    switch (chr) {
      case '"':
        out << "\\\"";
        break;
      case '\\':
        out << "\\\\";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        out << chr;
        break;
    }
  }
  out << '"';
}

std::string utc_timestamp() {
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;
  using std::chrono::system_clock;
  const auto now = system_clock::now();
  const auto millis = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
  const std::time_t time_value = system_clock::to_time_t(now);
  std::tm calendar{};
#ifdef _WIN32
  gmtime_s(&calendar, &time_value);
#else
  gmtime_r(&time_value, &calendar);
#endif
  std::ostringstream out;
  out << std::put_time(&calendar, "%Y-%m-%dT%H:%M:%S");
  out << '.' << std::setfill('0') << std::setw(3) << millis.count() << 'Z';
  return out.str();
}

void write_line(std::ostream& out, Level emit_level, const std::string& component, const std::string& message, const std::vector<Field>& fields) {
  out << R"({"time":)";
  write_escaped(out, utc_timestamp());
  out << R"(,"level":")" << level_name(emit_level) << R"(","component":)";
  write_escaped(out, component);
  out << ",\"message\":";
  write_escaped(out, message);
  for (const auto& field : fields) {
    out << ',';
    write_escaped(out, field.first);
    out << ':';
    write_escaped(out, field.second);
  }
  out << "}\n";
}

std::int64_t current_pid() {
#ifdef _WIN32
  return static_cast<std::int64_t>(_getpid());
#else
  return static_cast<std::int64_t>(::getpid());
#endif
}

std::tm local_tm_now() {
  const std::time_t time_value = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &time_value);
#else
  localtime_r(&time_value, &local);
#endif
  return local;
}

std::string day_key(const std::tm& local) {
  char buf[16]{0};
  if (std::strftime(buf, sizeof(buf), "%Y-%m-%d", &local) == 0) return {};
  return std::string{buf};
}

std::string expand_strftime(const std::string& pattern, const std::tm& local) {
  if (pattern.find('%') == std::string::npos) return pattern;
  char buf[1024]{0};
  if (std::strftime(buf, sizeof(buf), pattern.c_str(), &local) == 0) return pattern;
  return std::string{buf};
}

}  // namespace

const char* level_name(Level level) {
  switch (level) {
    case Level::debug:
      return "debug";
    case Level::info:
      return "info";
    case Level::warn:
      return "warn";
    case Level::error:
      return "error";
  }
  return "info";
}

bool parse_level(const std::string& text, Level& out) {
  if (text == "debug") {
    out = Level::debug;
    return true;
  }
  if (text == "info") {
    out = Level::info;
    return true;
  }
  if (text == "warn") {
    out = Level::warn;
    return true;
  }
  if (text == "error") {
    out = Level::error;
    return true;
  }
  return false;
}

void set_level(Level minimum) { level_storage().store(static_cast<int>(minimum), std::memory_order_relaxed); }

Level level() { return static_cast<Level>(level_storage().load(std::memory_order_relaxed)); }

bool should_log(Level candidate) { return static_cast<int>(candidate) >= level_storage().load(std::memory_order_relaxed); }

void emit(std::ostream& out, Level emit_level, const std::string& component, const std::string& message, const std::vector<Field>& fields) {
  if (!should_log(emit_level)) return;
  std::scoped_lock lock{write_mutex()};
  write_line(out, emit_level, component, message, fields);
}

std::string resolve_log_file(const std::string& pattern, const std::tm& local, std::int64_t pid) {
  if (pattern.empty()) return {};
  const std::string expanded = expand_strftime(pattern, local);
  char stamp[64]{0};
  std::snprintf(stamp, sizeof(stamp), "-%04d%02d%02d-%02d%02d%02d-pid%" PRId64, local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec, pid);
  std::string candidate;
  const std::size_t slash = expanded.find_last_of('/');
  const std::size_t dot = expanded.find_last_of('.');
  if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
    candidate = expanded.substr(0, dot) + stamp + expanded.substr(dot);
  } else {
    candidate = expanded + stamp;
  }
  if (!std::filesystem::exists(candidate)) return candidate;
  for (int i = 1; i < 1000; ++i) {
    const std::string retry = candidate + "-" + std::to_string(i);
    if (!std::filesystem::exists(retry)) return retry;
  }
  return candidate;
}

std::string resolve_log_file(const std::string& pattern) {
  if (pattern.empty()) return {};
  return resolve_log_file(pattern, local_tm_now(), current_pid());
}

void prune_log_files(const std::string& pattern, std::size_t keep) {
  if (pattern.empty() || keep == 0) return;
  try {
    // Match the literal family "<stem>-*" (the per-startup stamp always
    // starts with '-'); strftime directives expand per day, so cut the
    // pattern at the first '%' to cover every day's files.
    const std::string head = pattern.substr(0, pattern.find('%'));
    const std::size_t slash = head.find_last_of('/');
    const std::string dir = slash == std::string::npos ? "." : head.substr(0, slash);
    const std::string base = slash == std::string::npos ? head : head.substr(slash + 1);
    const std::size_t dot = base.find_last_of('.');
    const std::string stem = dot == std::string::npos ? base : base.substr(0, dot);
    const std::string ext = dot == std::string::npos ? "" : base.substr(dot);
    std::error_code errc;
    if (!std::filesystem::is_directory(dir, errc)) return;
    std::vector<std::string> matches;
    std::filesystem::directory_iterator end;
    for (std::filesystem::directory_iterator it{dir, errc}; it != end; it.increment(errc)) {
      if (errc) return;
      const std::string name = it->path().filename().string();
      if (name.size() <= stem.size() || name.compare(0, stem.size(), stem) != 0 || name[stem.size()] != '-') continue;
      if (!ext.empty() && (name.size() < ext.size() || name.compare(name.size() - ext.size(), ext.size(), ext) != 0)) continue;
      matches.push_back(it->path().string());
    }
    std::sort(matches.begin(), matches.end());
    while (matches.size() > keep) {
      std::filesystem::remove(matches.front(), errc);
      if (errc) return;
      matches.erase(matches.begin());
    }
  } catch (...) {
    // Retention is best effort: a full or unreadable log dir must never
    // fail startup or trading.
  }
}

Logger::Logger(std::string file_pattern)
    : pattern_(std::move(file_pattern)),
      pid_(current_pid()) {
  if (pattern_.empty()) return;
  std::scoped_lock lock{mutex_};
  open_for_day_locked(local_tm_now());
}

Logger::~Logger() {
  std::scoped_lock lock{mutex_};
  if (file_.is_open()) file_.close();
}

void Logger::open_for_day_locked(const std::tm& local) {
  file_path_ = resolve_log_file(pattern_, local, pid_);
  day_ = day_key(local);
  const std::size_t slash = file_path_.find_last_of('/');
  if (slash != std::string::npos) {
    std::error_code errc;
    std::filesystem::create_directories(file_path_.substr(0, slash), errc);
  }
  file_.open(file_path_, std::ios::app);
  if (!file_) throw std::runtime_error("cannot open log file: " + file_path_);
  has_file_ = true;
}

void Logger::rotate_if_day_changed_locked() {
  const std::tm local = local_tm_now();
  if (day_key(local) == day_) return;
  file_.close();
  open_for_day_locked(local);
}

void Logger::emit(Level emit_level, const std::string& component, const std::string& message, const std::vector<Field>& fields) {
  if (!should_log(emit_level)) return;
  std::scoped_lock stdout_lock{write_mutex()};
  write_line(std::cout, emit_level, component, message, fields);
  std::cout.flush();
  std::scoped_lock lock{mutex_};
  if (!has_file_) return;
  rotate_if_day_changed_locked();
  write_line(file_, emit_level, component, message, fields);
  file_.flush();
}

}  // namespace sts::log
