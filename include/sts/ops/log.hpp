// clang-format off
/**
  ******************************************************************************
  * @file           : log.hpp
  * @author         : vivi wu
  * @brief          : Structured JSON logging: levels, filtering, unique-file Logger with daily rotation.
  * @version        : 0.1.0
  * @date           : 02/10/26
  ******************************************************************************
  */
// clang-format on
#pragma once

#include <cstdint>
#include <ctime>
#include <fstream>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace sts::log {

// Minimal structured logging (M3-3, extended M9.1/M9.2): one JSON object per
// line on the given stream (stdout in gateway). Values are string-escaped;
// keep the field set small and never log secrets (see config/credentials.hpp).
// Not for the trading hot path (see dev_scheme.md section 2): control-plane
// use only, synchronous writes guarded by an internal mutex.
enum class Level : std::uint8_t { debug, info, warn, error };

using Field = std::pair<std::string, std::string>;

const char* level_name(Level level);
// Parses "debug"/"info"/"warn"/"error" (exact, lowercase). Returns false
// when the text is not a known level, leaving out untouched.
bool parse_level(const std::string& text, Level& out);
// Process-wide minimum level (default info). Messages below it are dropped
// by emit() before touching the stream.
void set_level(Level minimum);
Level level();
bool should_log(Level candidate);

void emit(std::ostream& out, Level emit_level, const std::string& component, const std::string& message, const std::vector<Field>& fields = {});

inline void debug(std::ostream& out, const std::string& component, const std::string& message, const std::vector<Field>& fields = {}) { emit(out, Level::debug, component, message, fields); }

inline void info(std::ostream& out, const std::string& component, const std::string& message, const std::vector<Field>& fields = {}) { emit(out, Level::info, component, message, fields); }

inline void warn(std::ostream& out, const std::string& component, const std::string& message, const std::vector<Field>& fields = {}) { emit(out, Level::warn, component, message, fields); }

inline void error(std::ostream& out, const std::string& component, const std::string& message, const std::vector<Field>& fields = {}) { emit(out, Level::error, component, message, fields); }

// Resolves a log file pattern into a unique path (M9.2): strftime
// % directives are expanded with local time, then a
// "-YYYYMMDD-HHMMSS-pid<PID>" stamp is inserted before the extension (or
// appended when there is none). Empty pattern resolves to empty. Parent
// directories are created. On collision a "-N" counter is appended, so two
// startups in the same second never share a file.
std::string resolve_log_file(const std::string& pattern);
std::string resolve_log_file(const std::string& pattern, const std::tm& local, std::int64_t pid);

// Retention for the per-startup unique files above: removes
// "<stem>-*" matches in the pattern's directory beyond the newest `keep`
// (names embed YYYYMMDD-HHMMSS, so lexical order is chronological).
// keep == 0 keeps everything. Call before opening the current file; best
// effort, never throws.
void prune_log_files(const std::string& pattern, std::size_t keep);

// Dual sink for apps (M9.2): every message goes to stdout and, when a file
// pattern was given, to a daily-rotating file. The file name differs per
// day and per startup; a long run crossing midnight rotates to a new file
// automatically. Each line is flushed so a second terminal can tail it.
class Logger {
 public:
  Logger() = default;
  explicit Logger(std::string file_pattern);
  ~Logger();
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
  Logger(Logger&&) = delete;
  Logger& operator=(Logger&&) = delete;

  void emit(Level emit_level, const std::string& component, const std::string& message, const std::vector<Field>& fields = {});
  void debug(const std::string& component, const std::string& message, const std::vector<Field>& fields = {}) { emit(Level::debug, component, message, fields); }
  void info(const std::string& component, const std::string& message, const std::vector<Field>& fields = {}) { emit(Level::info, component, message, fields); }
  void warn(const std::string& component, const std::string& message, const std::vector<Field>& fields = {}) { emit(Level::warn, component, message, fields); }
  void error(const std::string& component, const std::string& message, const std::vector<Field>& fields = {}) { emit(Level::error, component, message, fields); }

  bool has_file() const { return has_file_; }
  const std::string& file_path() const { return file_path_; }

 private:
  void open_for_day_locked(const std::tm& local);
  void rotate_if_day_changed_locked();

  mutable std::mutex mutex_;
  std::ofstream file_;
  std::string pattern_;
  std::string file_path_;
  std::string day_;
  bool has_file_{false};
  std::int64_t pid_{0};
};

}  // namespace sts::log
