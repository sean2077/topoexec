#pragma once

// API stability: experimental. Trace helpers are public but timeline/exporter contracts are not stable yet.

#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace topoexec {

class TraceId {
public:
  TraceId();
  explicit TraceId(std::string value);

  static TraceId generate();

  const std::string& value() const;
  bool empty() const;

private:
  std::string value_;
};

struct SpanRecord {
  TraceId trace_id;
  std::string name;
  std::chrono::steady_clock::time_point started_at;
  std::chrono::steady_clock::time_point finished_at;
  std::map<std::string, std::string> attributes;

  std::chrono::nanoseconds duration() const;
};

class TraceCollector {
public:
  void add(const SpanRecord& span);
  std::vector<SpanRecord> spans() const;
  void clear();

private:
  mutable std::mutex mutex_;
  std::vector<SpanRecord> spans_;
};

class ScopedSpan {
public:
  ScopedSpan(TraceCollector& collector, TraceId trace_id, std::string name);
  ~ScopedSpan();

  ScopedSpan(const ScopedSpan&) = delete;
  ScopedSpan& operator=(const ScopedSpan&) = delete;

private:
  TraceCollector& collector_;
  TraceId trace_id_;
  std::string name_;
  std::chrono::steady_clock::time_point started_at_;
};

} // namespace topoexec
