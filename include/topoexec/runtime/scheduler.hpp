#pragma once

// API stability: mixed. SchedulerStopToken/Reason are stable-v0.2 through RuntimeRunner; direct scheduler machinery is
// experimental.

#include "topoexec/runtime/component.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace topoexec {

struct SchedulerGroupConfig {
  std::string id;
  std::string type;
  double hz{0.0};
  std::string priority;
  std::chrono::milliseconds max_callback{0};
  int max_threads{0};
  int queue_capacity{0};
  std::string overflow{"reject"};
  bool wall_clock_enabled{false};
  std::chrono::milliseconds period{0};
  std::chrono::milliseconds tick_budget{0};
  std::string overrun_policy{"drop_tick"};
  std::string thread_name;
  std::vector<int> cpu_affinity;
  int nice_priority{0};
  std::string rt_policy{"none"};
  int rt_priority{0};
  std::string isolation_intent{"none"};
};

struct SchedulerMetrics {
  std::size_t tick_count{0};
  double tick_jitter_ms{0.0};
  std::size_t tick_overrun_count{0};
  std::size_t skipped_tick_count{0};
  double max_lateness_ms{0.0};
  double last_callback_duration_ms{0.0};
  double blocked_duration_ms{0.0};
  std::size_t queue_depth{0};
  std::size_t queue_capacity{0};
  std::size_t worker_count{0};
  std::size_t active_count{0};
  std::size_t in_flight_count{0};
  std::size_t enqueue_rejected_count{0};
  std::size_t completed_count{0};
};

struct ComponentExecutionMetrics {
  std::size_t execution_count{0};
  std::size_t error_count{0};
  std::uint64_t last_duration_ns{0};
  std::uint64_t max_duration_ns{0};
  std::size_t budget_overrun_count{0};
  std::size_t max_in_flight_count{0};
};

struct TriggerRuntimeMetrics {
  std::size_t ready_count{0};
  std::size_t suppressed_count{0};
  std::size_t coalesced_count{0};
  std::size_t timeout_drop_count{0};
  std::size_t batch_flush_count{0};
  std::size_t time_sync_drop_count{0};
};

class SchedulerStopToken {
public:
  SchedulerStopToken() = default;

  bool valid() const;
  bool stop_requested() const;

private:
  friend class SchedulerStopSource;
  explicit SchedulerStopToken(std::shared_ptr<std::atomic_bool> stop_requested);

  std::shared_ptr<std::atomic_bool> stop_requested_;
};

class SchedulerStopSource {
public:
  SchedulerStopSource();

  SchedulerStopToken token() const;
  void request_stop();
  bool stop_requested() const;

private:
  std::shared_ptr<std::atomic_bool> stop_requested_;
};

enum class SchedulerStopReason {
  kNotStarted,
  kTickBound,
  kDurationBound,
  kIdle,
  kStopRequested,
  kError,
};

struct SchedulerRunResult;

struct SchedulerRunOptions {
  std::size_t tick_iterations{0};
  std::uint64_t run_duration_ms{0};
  bool run_until_idle{false};
  std::function<void(std::uint64_t)> after_iteration;
  SchedulerStopToken stop_token;
  std::chrono::milliseconds pending_task_cleanup_timeout{1000};
  std::function<void(const SchedulerRunResult&)> progress_callback;
  std::size_t max_recorded_ticked_tasks{0};
};

struct SchedulerRunResult {
  bool ok{true};
  std::vector<std::string> errors;
  SchedulerStopReason stop_reason{SchedulerStopReason::kNotStarted};
  std::uint64_t iterations{0};
  std::size_t tick_calls{0};
  std::map<std::string, SchedulerMetrics> group_metrics;
  std::map<std::string, ComponentExecutionMetrics> component_metrics;
  std::map<std::string, TriggerRuntimeMetrics> trigger_metrics;
  std::map<std::string, std::size_t> loop_iteration_count;
  std::map<std::string, std::size_t> loop_converged_count;
  std::map<std::string, std::size_t> loop_budget_overrun_count;
  std::map<std::string, std::size_t> loop_max_iteration_hit_count;
  std::map<std::string, std::size_t> loop_error_count;
  std::vector<std::string> ticked_tasks;
};

std::string to_string(SchedulerStopReason reason);

class SchedulerRegistry {
public:
  void add_group(SchedulerGroupConfig group);
  std::optional<SchedulerGroupConfig> get_group(const std::string& id) const;
  bool has_group(const std::string& id) const;
  std::size_t size() const;

private:
  std::map<std::string, SchedulerGroupConfig> groups_;
};

class SchedulerMetricsTracker {
public:
  void observe_tick(const SchedulerGroupConfig& group, std::chrono::steady_clock::time_point scheduled_at,
                    std::chrono::steady_clock::time_point started_at,
                    std::chrono::steady_clock::duration callback_duration);
  void observe_skipped_tick(const SchedulerGroupConfig& group, std::chrono::steady_clock::duration blocked_duration);
  void observe_worker_pool(std::size_t queue_depth, std::size_t queue_capacity, std::size_t worker_count,
                           std::size_t active_count, std::size_t in_flight_count);
  void observe_enqueue_rejected();
  void observe_completed();
  const SchedulerMetrics& metrics() const;

private:
  SchedulerMetrics metrics_;
};

} // namespace topoexec
