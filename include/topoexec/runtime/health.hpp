#pragma once

// API stability: experimental. Health event shapes may change while the runtime observer API is still being designed.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace topoexec {

inline constexpr std::size_t kDefaultHealthEventCapacity = 64;

enum class HealthEventKind {
  kChannelOverflow,
  kChannelStaleDrop,
  kChannelDeadlineMiss,
  kBackpressureHighWatermark,
  kTaskReject,
  kSchedulerReject,
};

inline std::string to_string(HealthEventKind kind) {
  switch (kind) {
  case HealthEventKind::kChannelOverflow:
    return "channel_overflow";
  case HealthEventKind::kChannelStaleDrop:
    return "channel_stale_drop";
  case HealthEventKind::kChannelDeadlineMiss:
    return "channel_deadline_miss";
  case HealthEventKind::kBackpressureHighWatermark:
    return "backpressure_high_watermark";
  case HealthEventKind::kTaskReject:
    return "task_reject";
  case HealthEventKind::kSchedulerReject:
    return "scheduler_reject";
  }
  return "unknown";
}

struct HealthEvent {
  HealthEventKind kind{HealthEventKind::kChannelOverflow};
  std::string source;
  std::string component_id;
  std::string lane;
  std::string channel_id;
  std::string edge_id;
  std::string policy;
  std::string reason;
  std::uint64_t sequence{0};
  std::size_t depth{0};
  std::size_t capacity{0};
  std::size_t occurrence_count{1};
  std::map<std::string, std::string> attributes;
  std::chrono::steady_clock::time_point observed_at{std::chrono::steady_clock::now()};
};

class HealthEventSink {
public:
  explicit HealthEventSink(std::size_t capacity = kDefaultHealthEventCapacity, bool enabled = true)
      : capacity_(capacity), enabled_(enabled) {}

  void set_enabled(bool enabled) {
    enabled_.store(enabled, std::memory_order_relaxed);
  }

  bool enabled() const {
    return enabled_.load(std::memory_order_relaxed);
  }

  std::size_t capacity() const {
    return capacity_;
  }

  void emit(HealthEvent event) {
    if (!enabled()) {
      return;
    }
    if (capacity_ == 0u) {
      dropped_count_.fetch_add(1u, std::memory_order_relaxed);
      return;
    }
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
      dropped_count_.fetch_add(1u, std::memory_order_relaxed);
      return;
    }
    event.observed_at = std::chrono::steady_clock::now();
    event.occurrence_count = event.occurrence_count == 0u ? 1u : event.occurrence_count;
    for (auto& existing : events_) {
      if (same_coalescing_key(existing, event)) {
        existing.occurrence_count += event.occurrence_count;
        existing.sequence = event.sequence;
        existing.depth = event.depth;
        existing.capacity = event.capacity;
        existing.reason = std::move(event.reason);
        existing.attributes = std::move(event.attributes);
        existing.observed_at = event.observed_at;
        coalesced_count_.fetch_add(1u, std::memory_order_relaxed);
        return;
      }
    }
    if (events_.size() >= capacity_) {
      events_.pop_front();
      dropped_count_.fetch_add(1u, std::memory_order_relaxed);
    }
    events_.push_back(std::move(event));
  }

  std::vector<HealthEvent> snapshot() const {
    std::lock_guard lock(mutex_);
    return {events_.begin(), events_.end()};
  }

  std::size_t dropped_count() const {
    return dropped_count_.load(std::memory_order_relaxed);
  }

  std::size_t coalesced_count() const {
    return coalesced_count_.load(std::memory_order_relaxed);
  }

private:
  static bool same_coalescing_key(const HealthEvent& left, const HealthEvent& right) {
    return left.kind == right.kind && left.source == right.source && left.component_id == right.component_id &&
           left.lane == right.lane && left.channel_id == right.channel_id && left.edge_id == right.edge_id &&
           left.policy == right.policy;
  }

  std::size_t capacity_{kDefaultHealthEventCapacity};
  std::atomic_bool enabled_{true};
  mutable std::mutex mutex_;
  std::deque<HealthEvent> events_;
  std::atomic_size_t dropped_count_{0};
  std::atomic_size_t coalesced_count_{0};
};

inline std::map<std::string, std::string> health_event_attributes(const HealthEvent& event) {
  auto attributes = event.attributes;
  attributes["kind"] = to_string(event.kind);
  if (!event.source.empty()) {
    attributes["source"] = event.source;
  }
  if (!event.component_id.empty()) {
    attributes["component_id"] = event.component_id;
  }
  if (!event.lane.empty()) {
    attributes["lane"] = event.lane;
  }
  if (!event.channel_id.empty()) {
    attributes["channel_id"] = event.channel_id;
  }
  if (!event.edge_id.empty()) {
    attributes["edge_id"] = event.edge_id;
  }
  if (!event.policy.empty()) {
    attributes["policy"] = event.policy;
  }
  if (!event.reason.empty()) {
    attributes["reason"] = event.reason;
  }
  if (event.sequence != 0u) {
    attributes["sequence"] = std::to_string(event.sequence);
  }
  attributes["depth"] = std::to_string(event.depth);
  attributes["capacity"] = std::to_string(event.capacity);
  attributes["occurrence_count"] = std::to_string(event.occurrence_count);
  return attributes;
}

} // namespace topoexec
