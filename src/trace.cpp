#include "topoexec/common/trace.hpp"

#include <atomic>
#include <chrono>
#include <sstream>
#include <utility>

namespace topoexec {
namespace {
std::atomic_uint64_t g_trace_counter{1};
}

TraceId::TraceId() = default;
TraceId::TraceId(std::string value) : value_(std::move(value)) {}

TraceId TraceId::generate() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  std::ostringstream out;
  out << "trace-" << now << "-" << g_trace_counter.fetch_add(1);
  return TraceId(out.str());
}

const std::string& TraceId::value() const {
  return value_;
}

bool TraceId::empty() const {
  return value_.empty();
}

std::chrono::nanoseconds SpanRecord::duration() const {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(finished_at - started_at);
}

void TraceCollector::add(const SpanRecord& span) {
  std::lock_guard lock(mutex_);
  spans_.push_back(span);
}

std::vector<SpanRecord> TraceCollector::spans() const {
  std::lock_guard lock(mutex_);
  return spans_;
}

void TraceCollector::clear() {
  std::lock_guard lock(mutex_);
  spans_.clear();
}

ScopedSpan::ScopedSpan(TraceCollector& collector, TraceId trace_id, std::string name)
    : collector_(collector), trace_id_(std::move(trace_id)), name_(std::move(name)),
      started_at_(std::chrono::steady_clock::now()) {}

ScopedSpan::~ScopedSpan() {
  collector_.add(SpanRecord{trace_id_, name_, started_at_, std::chrono::steady_clock::now(), {}});
}

} // namespace topoexec
