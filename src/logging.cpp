#include "topoexec/common/logging.hpp"

#include <chrono>
#include <nlohmann/json.hpp>
#include <utility>

namespace topoexec {

std::string to_string(LogLevel level) {
  switch (level) {
  case LogLevel::kDebug:
    return "debug";
  case LogLevel::kInfo:
    return "info";
  case LogLevel::kWarn:
    return "warn";
  case LogLevel::kError:
    return "error";
  }
  return "unknown";
}

std::string to_json_line(const LogRecord& record) {
  const auto millis =
      std::chrono::duration_cast<std::chrono::milliseconds>(record.timestamp.time_since_epoch()).count();
  nlohmann::json value;
  value["timestamp_ms"] = millis;
  value["level"] = to_string(record.level);
  value["component"] = record.component;
  value["event"] = record.event;
  value["message"] = record.message;
  if (!record.trace_id.empty()) {
    value["trace_id"] = record.trace_id;
  }
  value["fields"] = record.fields;
  return value.dump();
}

void MemoryLogSink::write(const LogRecord& record) {
  std::lock_guard lock(mutex_);
  records_.push_back(record);
}

std::vector<LogRecord> MemoryLogSink::records() const {
  std::lock_guard lock(mutex_);
  return records_;
}

void MemoryLogSink::clear() {
  std::lock_guard lock(mutex_);
  records_.clear();
}

StructuredLogger::StructuredLogger(std::string component) : component_(std::move(component)) {}

LogRecord StructuredLogger::log(LogLevel level, std::string event, std::string message,
                                std::map<std::string, std::string> fields, std::string trace_id) {
  LogRecord record;
  record.timestamp = std::chrono::system_clock::now();
  record.level = level;
  record.component = component_;
  record.event = std::move(event);
  record.message = std::move(message);
  record.fields = std::move(fields);
  record.trace_id = std::move(trace_id);
  if (sink_ != nullptr) {
    sink_->write(record);
  }
  return record;
}

bool StructuredLogger::log_once(LogLevel level, const std::string& key, std::string event, std::string message,
                                std::map<std::string, std::string> fields, std::string trace_id) {
  if (!once_keys_.insert(key).second) {
    return false;
  }
  (void)log(level, std::move(event), std::move(message), std::move(fields), std::move(trace_id));
  return true;
}

bool StructuredLogger::log_throttle(LogLevel level, const std::string& key,
                                    std::chrono::steady_clock::duration interval, std::string event,
                                    std::string message, std::map<std::string, std::string> fields,
                                    std::string trace_id) {
  const auto now = std::chrono::steady_clock::now();
  const auto found = throttle_last_seen_.find(key);
  if (found != throttle_last_seen_.end() && now - found->second < interval) {
    return false;
  }
  throttle_last_seen_[key] = now;
  (void)log(level, std::move(event), std::move(message), std::move(fields), std::move(trace_id));
  return true;
}

void StructuredLogger::attach_sink(MemoryLogSink* sink) {
  sink_ = sink;
}

const std::string& StructuredLogger::component() const {
  return component_;
}

ScopeTimer::ScopeTimer(StructuredLogger& logger, std::string event)
    : logger_(logger), event_(std::move(event)), started_at_(std::chrono::steady_clock::now()) {}

ScopeTimer::~ScopeTimer() {
  const auto elapsed = std::chrono::steady_clock::now() - started_at_;
  const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
  logger_.log(LogLevel::kDebug, event_, "scope finished", {{"duration_us", std::to_string(micros)}});
}

} // namespace topoexec
