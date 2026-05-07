#pragma once

// API stability: stable-v0.2. RuntimeRunner and RuntimeRunnerResult are intended embedder API.

#include "topoexec/runtime/component_registry.hpp"
#include "topoexec/runtime/graph.hpp"
#include "topoexec/runtime/health.hpp"
#include "topoexec/runtime/live_observe.hpp"
#include "topoexec/runtime/scheduler.hpp"
#include "topoexec/runtime/status.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace topoexec {

class RuntimeObserver;

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
  runtime_observe::LiveObserveOptions live_observe;
  std::vector<RuntimeObserver*> observers;
};

struct RuntimeTraceEvent {
  RuntimeTraceEvent() = default;
  RuntimeTraceEvent(std::string name_value, std::string trace_id_value, std::uint64_t start_offset_ns_value,
                    std::uint64_t duration_ns_value, std::map<std::string, std::string> attributes_value)
      : name(std::move(name_value)), trace_id(std::move(trace_id_value)), start_offset_ns(start_offset_ns_value),
        duration_ns(duration_ns_value), attributes(std::move(attributes_value)) {}

  std::string name;
  std::string trace_id;
  std::string phase;
  std::string component_id;
  std::string channel_id;
  std::string lane;
  std::string worker_id;
  std::string epoch_id;
  std::string transaction_id;
  std::string correlation_id;
  std::string causation_id;
  std::uint64_t start_offset_ns{0};
  std::uint64_t duration_ns{0};
  std::map<std::string, std::string> attributes;
};

inline constexpr const char* kRuntimeTraceSchemaVersion = "1";

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
  std::size_t loop_output_discarded_count{0};
  std::size_t lifecycle_reset_count{0};
  std::size_t lifecycle_reset_failure_count{0};
  std::size_t lifecycle_restore_count{0};
  std::size_t lifecycle_restore_failure_count{0};
  std::size_t lifecycle_snapshot_count{0};
  std::size_t lifecycle_snapshot_failure_count{0};
  std::size_t lifecycle_snapshot_bytes{0};
  std::size_t observer_failure_count{0};
  std::size_t observer_dropped_event_count{0};
  std::size_t live_observe_dropped_event_count{0};
  SchedulerStopReason scheduler_stop_reason{SchedulerStopReason::kNotStarted};
  std::vector<std::string> ticked_components;
  std::vector<std::string> trace_events;
  std::vector<RuntimeTraceEvent> trace;
  std::vector<runtime_observe::LiveEvent> live_events;
  std::vector<HealthEvent> health_events;
  std::vector<RuntimeMetricSample> runtime_metrics;
  std::vector<RuntimeError> runtime_errors;
  std::vector<std::string> errors;
  std::map<std::string, double> loop_last_residual;
  std::map<std::string, std::string> loop_stop_reason;
  std::map<std::string, ComponentStateSnapshot> component_state_snapshots;
};

struct RuntimeObserverStatus {
  std::size_t dropped_event_count{0};
  std::size_t failure_count{0};
};

class ResultSink {
public:
  virtual ~ResultSink() = default;
  virtual Status on_result(const RuntimeRunnerResult& result);
};

class MetricSink {
public:
  virtual ~MetricSink() = default;
  virtual Status on_metric(const RuntimeMetricSample& metric);
};

class TraceSink {
public:
  virtual ~TraceSink() = default;
  virtual Status on_trace_event(const RuntimeTraceEvent& event);
};

class RuntimeObserver : public ResultSink, public MetricSink, public TraceSink {
public:
  ~RuntimeObserver() override = default;
  virtual Status on_health_event(const HealthEvent& event);
  virtual Status on_runtime_error(const RuntimeError& error);
  virtual RuntimeObserverStatus status() const;
};

class NoopRuntimeObserver final : public RuntimeObserver {};

class InMemoryRuntimeObserver final : public RuntimeObserver {
public:
  explicit InMemoryRuntimeObserver(std::size_t capacity = 256);

  Status on_result(const RuntimeRunnerResult& result) override;
  Status on_metric(const RuntimeMetricSample& metric) override;
  Status on_trace_event(const RuntimeTraceEvent& event) override;
  Status on_health_event(const HealthEvent& event) override;
  Status on_runtime_error(const RuntimeError& error) override;
  RuntimeObserverStatus status() const override;

  std::vector<RuntimeRunnerResult> results() const;
  std::vector<RuntimeMetricSample> metrics() const;
  std::vector<RuntimeTraceEvent> trace_events() const;
  std::vector<HealthEvent> health_events() const;
  std::vector<RuntimeError> runtime_errors() const;
  void clear();

private:
  template <typename T> Status push_bounded(std::vector<T>& records, T value);

  std::size_t capacity_{256};
  mutable std::mutex mutex_;
  std::vector<RuntimeRunnerResult> results_;
  std::vector<RuntimeMetricSample> metrics_;
  std::vector<RuntimeTraceEvent> trace_events_;
  std::vector<HealthEvent> health_events_;
  std::vector<RuntimeError> runtime_errors_;
  std::atomic_size_t dropped_event_count_{0};
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
