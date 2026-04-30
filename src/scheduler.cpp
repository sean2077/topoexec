#include "topoexec/runtime/scheduler.hpp"

#include <chrono>
#include <utility>

namespace topoexec {

SchedulerStopToken::SchedulerStopToken(std::shared_ptr<std::atomic_bool> stop_requested)
    : stop_requested_(std::move(stop_requested)) {}

bool SchedulerStopToken::valid() const {
  return stop_requested_ != nullptr;
}

bool SchedulerStopToken::stop_requested() const {
  return stop_requested_ != nullptr && stop_requested_->load();
}

SchedulerStopSource::SchedulerStopSource() : stop_requested_(std::make_shared<std::atomic_bool>(false)) {}

SchedulerStopToken SchedulerStopSource::token() const {
  return SchedulerStopToken(stop_requested_);
}

void SchedulerStopSource::request_stop() {
  stop_requested_->store(true);
}

bool SchedulerStopSource::stop_requested() const {
  return stop_requested_->load();
}

std::string to_string(SchedulerStopReason reason) {
  switch (reason) {
  case SchedulerStopReason::kNotStarted:
    return "not_started";
  case SchedulerStopReason::kTickBound:
    return "tick_bound";
  case SchedulerStopReason::kDurationBound:
    return "duration_bound";
  case SchedulerStopReason::kStopRequested:
    return "stop_requested";
  case SchedulerStopReason::kError:
    return "error";
  }
  return "unknown";
}

void SchedulerRegistry::add_group(SchedulerGroupConfig group) {
  groups_[group.id] = std::move(group);
}

std::optional<SchedulerGroupConfig> SchedulerRegistry::get_group(const std::string& id) const {
  const auto found = groups_.find(id);
  if (found == groups_.end()) {
    return std::nullopt;
  }
  return found->second;
}

bool SchedulerRegistry::has_group(const std::string& id) const {
  return groups_.count(id) != 0u;
}

std::size_t SchedulerRegistry::size() const {
  return groups_.size();
}

void SchedulerMetricsTracker::observe_tick(const SchedulerGroupConfig&, std::chrono::steady_clock::time_point scheduled_at,
                                           std::chrono::steady_clock::time_point started_at,
                                           std::chrono::steady_clock::duration callback_duration) {
  metrics_.tick_jitter_ms = std::chrono::duration<double, std::milli>(started_at - scheduled_at).count();
  metrics_.last_callback_duration_ms = std::chrono::duration<double, std::milli>(callback_duration).count();
  if (metrics_.last_callback_duration_ms > 0.0) {
    ++metrics_.completed_count;
  }
}

void SchedulerMetricsTracker::observe_skipped_tick(const SchedulerGroupConfig&,
                                                   std::chrono::steady_clock::duration blocked_duration) {
  ++metrics_.skipped_tick_count;
  metrics_.blocked_duration_ms = std::chrono::duration<double, std::milli>(blocked_duration).count();
}

void SchedulerMetricsTracker::observe_worker_pool(std::size_t queue_depth, std::size_t queue_capacity,
                                                  std::size_t worker_count, std::size_t active_count,
                                                  std::size_t in_flight_count) {
  metrics_.queue_depth = queue_depth;
  metrics_.queue_capacity = queue_capacity;
  metrics_.worker_count = worker_count;
  metrics_.active_count = active_count;
  metrics_.in_flight_count = in_flight_count;
}

void SchedulerMetricsTracker::observe_enqueue_rejected() {
  ++metrics_.enqueue_rejected_count;
}

void SchedulerMetricsTracker::observe_completed() {
  ++metrics_.completed_count;
}

const SchedulerMetrics& SchedulerMetricsTracker::metrics() const {
  return metrics_;
}

}  // namespace topoexec

