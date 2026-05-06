#pragma once

// API stability: stable-v0.2. RuntimeRunner and RuntimeRunnerResult are intended embedder API.

#include "topoexec/runtime/component_registry.hpp"
#include "topoexec/runtime/graph.hpp"
#include "topoexec/runtime/health.hpp"
#include "topoexec/runtime/scheduler.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace topoexec {

enum class RuntimeRunMode {
  kValidate,
  kDryRun,
  kRun,
};

struct RuntimeRunnerOptions {
  RuntimeRunMode mode{RuntimeRunMode::kDryRun};
  std::size_t tick_iterations{1};
  std::uint64_t run_duration_ms{0};
  bool run_until_idle{false};
  SchedulerStopToken stop_token;
  std::chrono::milliseconds pending_task_cleanup_timeout{1000};
  bool emit_health_events{true};
  std::size_t health_event_capacity{kDefaultHealthEventCapacity};
  std::vector<std::string> reset_component_ids;
  std::map<std::string, ComponentStateSnapshot> restore_component_states;
  bool capture_component_state_snapshots{false};
};

struct RuntimeTraceEvent {
  std::string name;
  std::string trace_id;
  std::uint64_t start_offset_ns{0};
  std::uint64_t duration_ns{0};
  std::map<std::string, std::string> attributes;
};

struct RuntimeError {
  std::string phase;
  std::string component_id;
  std::string lane;
  std::string message;
  std::string code;
  std::string trace_id;
  bool fatal{true};
};

struct RuntimeRunnerResult {
  bool ok{false};
  std::string graph_name;
  std::size_t component_count{0};
  std::size_t channel_count{0};
  GraphValidationResult validation;
  GraphDryRunResult dry_run;
  std::size_t instantiated_components{0};
  std::size_t configured_components{0};
  std::size_t started_components{0};
  std::size_t stopped_components{0};
  std::size_t tick_calls{0};
  std::size_t metric_samples{0};
  std::size_t channel_publish_count{0};
  std::size_t channel_delivery_count{0};
  std::size_t channel_drop_count{0};
  std::size_t channel_deadline_miss_count{0};
  std::size_t payload_copy_count{0};
  std::size_t staged_publication_count{0};
  std::size_t committed_publication_count{0};
  std::size_t delayed_publication_count{0};
  std::size_t state_publication_count{0};
  std::size_t state_commit_count{0};
  std::size_t async_publication_count{0};
  std::size_t failed_publication_commit_count{0};
  std::size_t health_event_count{0};
  std::size_t health_event_dropped_count{0};
  std::size_t health_event_coalesced_count{0};
  std::size_t trace_event_count{0};
  std::size_t loop_iteration_count{0};
  std::size_t loop_converged_count{0};
  std::size_t loop_budget_overrun_count{0};
  std::size_t loop_max_iteration_hit_count{0};
  std::size_t loop_error_count{0};
  std::size_t loop_cancellation_requested_count{0};
  std::size_t loop_cancellation_observed_count{0};
  std::size_t lifecycle_reset_count{0};
  std::size_t lifecycle_reset_failure_count{0};
  std::size_t lifecycle_restore_count{0};
  std::size_t lifecycle_restore_failure_count{0};
  std::size_t lifecycle_snapshot_count{0};
  std::size_t lifecycle_snapshot_failure_count{0};
  std::size_t lifecycle_snapshot_bytes{0};
  SchedulerStopReason scheduler_stop_reason{SchedulerStopReason::kNotStarted};
  std::vector<std::string> ticked_components;
  std::vector<std::string> trace_events;
  std::vector<RuntimeTraceEvent> trace;
  std::vector<HealthEvent> health_events;
  std::vector<RuntimeMetricSample> runtime_metrics;
  std::vector<RuntimeError> runtime_errors;
  std::vector<std::string> errors;
  std::map<std::string, ComponentStateSnapshot> component_state_snapshots;
};

std::string to_string(RuntimeRunMode mode);

class RuntimeRunner {
public:
  explicit RuntimeRunner(const ComponentRegistry& registry);

  RuntimeRunnerResult run(const GraphSpec& graph, RuntimeRunnerOptions options = {}) const;

private:
  const ComponentRegistry& registry_;
};

} // namespace topoexec
