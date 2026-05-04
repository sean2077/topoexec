#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace topoexec {

enum class LogLevel {
  kDebug,
  kInfo,
  kWarn,
  kError,
};

struct LogRecord {
  std::chrono::system_clock::time_point timestamp;
  LogLevel level{LogLevel::kInfo};
  std::string component;
  std::string event;
  std::string message;
  std::string trace_id;
  std::map<std::string, std::string> fields;
};

std::string to_string(LogLevel level);
std::string to_json_line(const LogRecord& record);

class MemoryLogSink {
public:
  void write(const LogRecord& record);
  std::vector<LogRecord> records() const;
  void clear();

private:
  mutable std::mutex mutex_;
  std::vector<LogRecord> records_;
};

class StructuredLogger {
public:
  explicit StructuredLogger(std::string component);

  LogRecord log(LogLevel level, std::string event, std::string message, std::map<std::string, std::string> fields = {},
                std::string trace_id = {});
  bool log_once(LogLevel level, const std::string& key, std::string event, std::string message,
                std::map<std::string, std::string> fields = {}, std::string trace_id = {});
  bool log_throttle(LogLevel level, const std::string& key, std::chrono::steady_clock::duration interval,
                    std::string event, std::string message, std::map<std::string, std::string> fields = {},
                    std::string trace_id = {});
  void attach_sink(MemoryLogSink* sink);
  const std::string& component() const;

private:
  std::string component_;
  MemoryLogSink* sink_{nullptr};
  std::set<std::string> once_keys_;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point> throttle_last_seen_;
};

class ScopeTimer {
public:
  ScopeTimer(StructuredLogger& logger, std::string event);
  ~ScopeTimer();

  ScopeTimer(const ScopeTimer&) = delete;
  ScopeTimer& operator=(const ScopeTimer&) = delete;

private:
  StructuredLogger& logger_;
  std::string event_;
  std::chrono::steady_clock::time_point started_at_;
};

} // namespace topoexec
