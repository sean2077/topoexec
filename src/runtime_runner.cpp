#include "topoexec/runtime/runtime_runner.hpp"

#include "topoexec/common/trace.hpp"
#include "topoexec/runtime/channel.hpp"
#include "topoexec/runtime/event_runtime.hpp"
#include "topoexec/runtime/state.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace topoexec {
namespace {

SchedulerGroupConfig lane_config_from_spec(const LaneSpec& lane) {
  SchedulerGroupConfig config;
  config.id = lane.id;
  config.type = lane.type;
  config.hz = lane.hz;
  config.priority = lane.priority;
  config.max_callback = std::chrono::milliseconds(lane.max_callback_ms);
  config.max_threads = lane.max_threads;
  config.queue_capacity = lane.queue_capacity;
  config.overflow = lane.overflow;
  config.wall_clock_enabled = lane.wall_clock_enabled;
  config.period = std::chrono::milliseconds(lane.period_ms);
  config.tick_budget = std::chrono::milliseconds(lane.tick_budget_ms);
  config.overrun_policy = lane.overrun_policy;
  config.thread_name = lane.thread_name;
  config.cpu_affinity = lane.cpu_affinity;
  config.nice_priority = lane.nice_priority;
  config.rt_policy = lane.rt_policy;
  config.rt_priority = lane.rt_priority;
  config.isolation_intent = lane.isolation_intent;
  return config;
}

std::map<std::string, SchedulerGroupConfig> lane_configs(const GraphSpec& graph) {
  std::map<std::string, SchedulerGroupConfig> values;
  for (const auto& lane : graph.lanes) {
    values[lane.id] = lane_config_from_spec(lane);
  }
  return values;
}

void copy_dry_run_to_runner(const GraphDryRunResult& dry_run, RuntimeRunnerResult& result) {
  result.dry_run = dry_run;
  result.instantiated_components = dry_run.instantiated_components;
  result.configured_components = dry_run.configured_components;
  result.started_components = dry_run.started_components;
  result.stopped_components = dry_run.stopped_components;
  result.tick_calls = dry_run.tick_calls;
  result.metric_samples = dry_run.metric_samples;
  result.channel_publish_count = dry_run.channel_publish_count;
  result.channel_delivery_count = dry_run.channel_delivery_count;
  result.channel_drop_count = dry_run.channel_drop_count;
  result.channel_deadline_miss_count = dry_run.channel_deadline_miss_count;
  result.payload_copy_count = dry_run.payload_copy_count;
  result.ticked_components = dry_run.ticked_components;
  result.runtime_metrics = dry_run.runtime_metrics;
}

void append_runtime_metric(RuntimeRunnerResult& result, std::string name, double value, std::string component_id = {},
                           std::string lane = {}, std::string channel_id = {}) {
  result.runtime_metrics.push_back(
      RuntimeMetricSample{std::move(name), value, std::move(component_id), std::move(lane), std::move(channel_id), {}});
}

void append_runtime_error(RuntimeRunnerResult& result, RuntimeError error, std::string legacy_message = {}) {
  if (legacy_message.empty()) {
    legacy_message = error.message;
    if (!error.component_id.empty()) {
      legacy_message = "component " + error.component_id + " " + error.phase + " failed: " + error.message;
    }
  }
  result.runtime_errors.push_back(std::move(error));
  result.errors.push_back(std::move(legacy_message));
}

RuntimeError make_runtime_error(std::string phase, std::string component_id, std::string lane, std::string message,
                                std::string code = {}, bool fatal = true) {
  RuntimeError error;
  error.phase = std::move(phase);
  error.component_id = std::move(component_id);
  error.lane = std::move(lane);
  error.message = std::move(message);
  error.code = std::move(code);
  error.fatal = fatal;
  return error;
}

std::string component_id_from_legacy_error(const std::string& message) {
  constexpr auto prefix = std::string_view{"component "};
  constexpr auto failed = std::string_view{" failed:"};
  if (message.rfind(prefix, 0) != 0u) {
    return {};
  }
  const auto end = message.find(failed, prefix.size());
  if (end == std::string::npos) {
    return {};
  }
  return message.substr(prefix.size(), end - prefix.size());
}

std::uint64_t non_negative_duration_ns(std::chrono::steady_clock::duration duration) {
  const auto count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
  return count < 0 ? 0u : static_cast<std::uint64_t>(count);
}

std::optional<bool> parse_config_bool(const ConfigView& config, const std::string& key) {
  const auto found = config.values.find(key);
  if (found == config.values.end()) {
    return std::nullopt;
  }
  if (found->second == "true" || found->second == "1" || found->second == "yes" || found->second == "on") {
    return true;
  }
  if (found->second == "false" || found->second == "0" || found->second == "no" || found->second == "off") {
    return false;
  }
  return std::nullopt;
}

std::optional<std::size_t> parse_config_size(const ConfigView& config, const std::string& key) {
  const auto found = config.values.find(key);
  if (found == config.values.end()) {
    return std::nullopt;
  }
  if (found->second.empty() || found->second.front() == '-') {
    return std::nullopt;
  }
  try {
    const auto parsed = std::stoull(found->second);
    return static_cast<std::size_t>(parsed);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

bool resolved_health_event_enabled(const GraphSpec& graph, const RuntimeRunnerOptions& options) {
  return parse_config_bool(graph.config, "emit_health_events").value_or(options.emit_health_events);
}

std::size_t resolved_health_event_capacity(const GraphSpec& graph, const RuntimeRunnerOptions& options) {
  return parse_config_size(graph.config, "health_event_capacity").value_or(options.health_event_capacity);
}

std::chrono::steady_clock::time_point earliest_trace_epoch(const std::vector<SpanRecord>& spans,
                                                           const std::vector<HealthEvent>& health_events) {
  std::chrono::steady_clock::time_point epoch{};
  for (const auto& span : spans) {
    if (epoch == std::chrono::steady_clock::time_point{} || span.started_at < epoch) {
      epoch = span.started_at;
    }
  }
  for (const auto& event : health_events) {
    if (epoch == std::chrono::steady_clock::time_point{} || event.observed_at < epoch) {
      epoch = event.observed_at;
    }
  }
  return epoch;
}

} // namespace

std::string to_string(RuntimeRunMode mode) {
  switch (mode) {
  case RuntimeRunMode::kValidate:
    return "validate";
  case RuntimeRunMode::kDryRun:
    return "dry_run";
  case RuntimeRunMode::kRun:
    return "run";
  }
  return "unknown";
}

RuntimeRunner::RuntimeRunner(const ComponentRegistry& registry) : registry_(registry) {}

RuntimeRunnerResult RuntimeRunner::run(const GraphSpec& graph, RuntimeRunnerOptions options) const {
  RuntimeRunnerResult result;
  result.graph_name = graph.name;
  result.component_count = graph.components.size();
  result.channel_count = graph.edges.size();
  result.validation = validate_graph(graph, registry_);
  if (!result.validation.ok) {
    result.ok = false;
    for (const auto& error : result.validation.errors) {
      append_runtime_error(result, make_runtime_error("validate", {}, {}, error, "validation"));
    }
    return result;
  }
  if (options.mode == RuntimeRunMode::kValidate) {
    result.ok = true;
    return result;
  }
  if (options.mode == RuntimeRunMode::kDryRun) {
    auto dry_run = dry_run_graph(graph, registry_, options.tick_iterations);
    result.ok = dry_run.ok;
    for (const auto& error : dry_run.errors) {
      append_runtime_error(result, make_runtime_error("dry_run", {}, {}, error, "dry_run"));
    }
    copy_dry_run_to_runner(dry_run, result);
    result.scheduler_stop_reason = SchedulerStopReason::kTickBound;
    return result;
  }

  const auto emit_health_events = resolved_health_event_enabled(graph, options);
  HealthEventSink health_events(resolved_health_event_capacity(graph, options), emit_health_events);
  RuntimeChannelBus channels(graph.edges);
  channels.set_health_event_sink(&health_events);
  RuntimePublicationRouter publications(&channels, graph.edges);
  TraceCollector trace;
  publications.set_trace_collector(&trace);
  MetricRegistry metrics;
  RuntimeStateStore state_store;
  ConfigSnapshotStore config_store;
  config_store.set_graph_config(graph.config);
  for (const auto& spec : graph.components) {
    config_store.set_component_config(spec.id, spec.config);
  }
  MemoryLogSink logs;
  StructuredLogger logger(graph.name);
  logger.attach_sink(&logs);
  auto lanes = lane_configs(graph);
  const auto runtime_cancel_token =
      CancellationToken::from_callback([token = options.stop_token]() { return token.stop_requested(); });

  struct Instance {
    std::string id;
    std::unique_ptr<Component> component;
    GraphContext context;
    bool configured{false};
    bool started{false};
  };
  std::vector<Instance> instances;
  instances.reserve(graph.components.size());
  try {
    for (const auto& spec : graph.components) {
      Instance instance;
      instance.id = spec.id;
      instance.component = registry_.create(spec.type);
      instance.context.metrics = &metrics;
      instance.context.logger = &logger;
      instance.context.channels = &channels;
      instance.context.publisher = &publications;
      instance.context.state_store = &state_store;
      instance.context.config_store = &config_store;
      instance.context.cancel_token = runtime_cancel_token;
      instance.context.graph_name = graph.name;
      instance.context.component_id = spec.id;
      const auto configure = instance.component->configure_status(instance.context, spec.config);
      if (!configure.ok()) {
        result.ok = false;
        result.scheduler_stop_reason = SchedulerStopReason::kError;
        append_runtime_error(result, make_runtime_error("configure", spec.id, spec.execution.lane, configure.message(),
                                                        "component_configure"));
        instances.push_back(std::move(instance));
        break;
      }
      instance.configured = true;
      const auto activate = instance.component->activate_status();
      if (!activate.ok()) {
        result.ok = false;
        result.scheduler_stop_reason = SchedulerStopReason::kError;
        append_runtime_error(result, make_runtime_error("activate", spec.id, spec.execution.lane, activate.message(),
                                                        "component_activate"));
        instances.push_back(std::move(instance));
        break;
      }
      instance.started = true;
      instances.push_back(std::move(instance));
    }
    result.instantiated_components = instances.size();
    result.configured_components = static_cast<std::size_t>(
        std::count_if(instances.begin(), instances.end(), [](const auto& instance) { return instance.configured; }));
    result.started_components = static_cast<std::size_t>(
        std::count_if(instances.begin(), instances.end(), [](const auto& instance) { return instance.started; }));
    if (!result.errors.empty()) {
      for (auto it = instances.rbegin(); it != instances.rend(); ++it) {
        if (!it->started) {
          continue;
        }
        const auto deactivate = it->component->deactivate_status();
        ++result.stopped_components;
        if (!deactivate.ok()) {
          append_runtime_error(result, make_runtime_error("deactivate", it->id, {}, deactivate.message(),
                                                          "component_deactivate", false));
        }
      }
      return result;
    }

    EventRuntime runtime(&channels, result.validation.compiled_plan, &publications);
    runtime.set_trace_collector(&trace);
    runtime.set_health_event_sink(&health_events);
    runtime.set_state_store(&state_store);
    runtime.set_config_store(&config_store);
    for (const auto& spec : graph.components) {
      auto instance =
          std::find_if(instances.begin(), instances.end(), [&spec](const auto& item) { return item.id == spec.id; });
      runtime.add_component(EventRuntimeComponent{spec.id, instance->component.get(), &instance->context, spec,
                                                  lanes.at(spec.execution.lane)});
    }
    SchedulerRunOptions run_options;
    run_options.tick_iterations = options.tick_iterations;
    run_options.run_duration_ms = options.run_duration_ms;
    run_options.run_until_idle = options.run_until_idle;
    run_options.stop_token = options.stop_token;
    run_options.cancel_token = runtime_cancel_token;
    run_options.pending_task_cleanup_timeout = options.pending_task_cleanup_timeout;
    run_options.max_recorded_ticked_tasks =
        graph.components.size() * std::max<std::size_t>(1u, options.tick_iterations);
    const auto run_result = runtime.run(run_options);
    result.ok = run_result.ok;
    result.scheduler_stop_reason = run_result.stop_reason;
    result.tick_calls = run_result.tick_calls;
    result.ticked_components = run_result.ticked_tasks;
    for (const auto& error : run_result.errors) {
      const auto component_id = component_id_from_legacy_error(error);
      append_runtime_error(result,
                           make_runtime_error(component_id.empty() ? "runtime" : "execute", component_id, {}, error,
                                              component_id.empty() ? "runtime" : "component_execute"),
                           error);
    }
    for (const auto& [lane_id, metrics] : run_result.group_metrics) {
      append_runtime_metric(result, "runtime.scheduler.tick_count", static_cast<double>(metrics.tick_count), {},
                            lane_id);
      append_runtime_metric(result, "runtime.scheduler.completed_count", static_cast<double>(metrics.completed_count),
                            {}, lane_id);
      append_runtime_metric(result, "runtime.scheduler.tick_overrun_count",
                            static_cast<double>(metrics.tick_overrun_count), {}, lane_id);
      append_runtime_metric(result, "runtime.scheduler.skipped_tick_count",
                            static_cast<double>(metrics.skipped_tick_count), {}, lane_id);
      append_runtime_metric(result, "runtime.scheduler.max_lateness_ms", metrics.max_lateness_ms, {}, lane_id);
      append_runtime_metric(result, "runtime.scheduler.queue_depth", static_cast<double>(metrics.queue_depth), {},
                            lane_id);
      append_runtime_metric(result, "runtime.scheduler.queue_capacity", static_cast<double>(metrics.queue_capacity), {},
                            lane_id);
      append_runtime_metric(result, "runtime.scheduler.worker_count", static_cast<double>(metrics.worker_count), {},
                            lane_id);
      append_runtime_metric(result, "runtime.scheduler.last_callback_duration_ms", metrics.last_callback_duration_ms,
                            {}, lane_id);
      append_runtime_metric(result, "runtime.scheduler.blocked_duration_ms", metrics.blocked_duration_ms, {}, lane_id);
      append_runtime_metric(result, "runtime.scheduler.tick_jitter_ms", metrics.tick_jitter_ms, {}, lane_id);
      append_runtime_metric(result, "runtime.scheduler.active_count", static_cast<double>(metrics.active_count), {},
                            lane_id);
      append_runtime_metric(result, "runtime.scheduler.in_flight_count", static_cast<double>(metrics.in_flight_count),
                            {}, lane_id);
      append_runtime_metric(result, "runtime.scheduler.rejected_count",
                            static_cast<double>(metrics.enqueue_rejected_count), {}, lane_id);
      append_runtime_metric(result, "runtime.scheduler.priority_high_count",
                            static_cast<double>(metrics.priority_high_count), {}, lane_id);
      append_runtime_metric(result, "runtime.scheduler.priority_normal_count",
                            static_cast<double>(metrics.priority_normal_count), {}, lane_id);
      append_runtime_metric(result, "runtime.scheduler.priority_low_count",
                            static_cast<double>(metrics.priority_low_count), {}, lane_id);
      append_runtime_metric(result, "runtime.scheduler.priority_background_count",
                            static_cast<double>(metrics.priority_background_count), {}, lane_id);
      append_runtime_metric(result, "runtime.scheduler.low_priority_rejected_count",
                            static_cast<double>(metrics.low_priority_rejected_count), {}, lane_id);
      append_runtime_metric(result, "runtime.scheduler.starvation_guard_count",
                            static_cast<double>(metrics.starvation_guard_count), {}, lane_id);
    }
    for (const auto& [component_id, metrics] : run_result.component_metrics) {
      append_runtime_metric(result, "runtime.component.execution_count", static_cast<double>(metrics.execution_count),
                            component_id);
      append_runtime_metric(result, "runtime.component.error_count", static_cast<double>(metrics.error_count),
                            component_id);
      append_runtime_metric(result, "runtime.component.last_duration_ns", static_cast<double>(metrics.last_duration_ns),
                            component_id);
      append_runtime_metric(result, "runtime.component.max_duration_ns", static_cast<double>(metrics.max_duration_ns),
                            component_id);
      append_runtime_metric(result, "runtime.component.budget_overrun_count",
                            static_cast<double>(metrics.budget_overrun_count), component_id);
      append_runtime_metric(result, "runtime.component.cancellation_requested_count",
                            static_cast<double>(metrics.cancellation_requested_count), component_id);
      append_runtime_metric(result, "runtime.component.cancellation_observed_count",
                            static_cast<double>(metrics.cancellation_observed_count), component_id);
      append_runtime_metric(result, "runtime.component.timeout_budget_exceeded_count",
                            static_cast<double>(metrics.timeout_budget_exceeded_count), component_id);
      append_runtime_metric(result, "runtime.component.max_in_flight_count",
                            static_cast<double>(metrics.max_in_flight_count), component_id);
    }
    for (const auto& [component_id, metrics] : run_result.trigger_metrics) {
      append_runtime_metric(result, "runtime.trigger.ready_count", static_cast<double>(metrics.ready_count),
                            component_id);
      append_runtime_metric(result, "runtime.trigger.suppressed_count", static_cast<double>(metrics.suppressed_count),
                            component_id);
      append_runtime_metric(result, "runtime.trigger.coalesced_count", static_cast<double>(metrics.coalesced_count),
                            component_id);
      append_runtime_metric(result, "runtime.trigger.timeout_drop_count",
                            static_cast<double>(metrics.timeout_drop_count), component_id);
      append_runtime_metric(result, "runtime.trigger.batch_flush_count", static_cast<double>(metrics.batch_flush_count),
                            component_id);
      append_runtime_metric(result, "runtime.trigger.time_sync_drop_count",
                            static_cast<double>(metrics.time_sync_drop_count), component_id);
    }
    for (const auto& [loop_id, count] : run_result.loop_iteration_count) {
      result.loop_iteration_count += count;
      append_runtime_metric(result, "runtime.loop.iterations", static_cast<double>(count), loop_id);
    }
    for (const auto& [loop_id, count] : run_result.loop_converged_count) {
      result.loop_converged_count += count;
      append_runtime_metric(result, "runtime.loop.converged", static_cast<double>(count), loop_id);
    }
    for (const auto& [loop_id, count] : run_result.loop_budget_overrun_count) {
      result.loop_budget_overrun_count += count;
      append_runtime_metric(result, "runtime.loop.budget_overrun", static_cast<double>(count), loop_id);
    }
    for (const auto& [loop_id, count] : run_result.loop_max_iteration_hit_count) {
      result.loop_max_iteration_hit_count += count;
      append_runtime_metric(result, "runtime.loop.max_iterations_hit", static_cast<double>(count), loop_id);
    }
    for (const auto& [loop_id, count] : run_result.loop_error_count) {
      result.loop_error_count += count;
      append_runtime_metric(result, "runtime.loop.error", static_cast<double>(count), loop_id);
    }
    for (const auto& [loop_id, count] : run_result.loop_cancellation_requested_count) {
      result.loop_cancellation_requested_count += count;
      append_runtime_metric(result, "runtime.loop.cancellation_requested", static_cast<double>(count), loop_id);
    }
    for (const auto& [loop_id, count] : run_result.loop_cancellation_observed_count) {
      result.loop_cancellation_observed_count += count;
      append_runtime_metric(result, "runtime.loop.cancellation_observed", static_cast<double>(count), loop_id);
    }
    for (auto it = instances.rbegin(); it != instances.rend(); ++it) {
      if (!it->started) {
        continue;
      }
      const auto deactivate = it->component->deactivate_status();
      ++result.stopped_components;
      if (!deactivate.ok()) {
        result.ok = false;
        append_runtime_error(
            result, make_runtime_error("deactivate", it->id, {}, deactivate.message(), "component_deactivate", false));
      }
    }
    for (const auto& metric : channels.metrics_snapshot()) {
      result.channel_publish_count += metric.published_count;
      result.channel_delivery_count += metric.delivered_count;
      result.channel_drop_count += metric.drop_count;
      result.channel_deadline_miss_count += metric.deadline_miss_count;
      result.payload_copy_count += metric.payload_copy_count;
      append_runtime_metric(result, "runtime.channel.publish_count", static_cast<double>(metric.published_count), {},
                            {}, metric.channel_id);
      append_runtime_metric(result, "runtime.channel.delivery_count", static_cast<double>(metric.delivered_count), {},
                            {}, metric.channel_id);
      append_runtime_metric(result, "runtime.channel.drop_count", static_cast<double>(metric.drop_count), {}, {},
                            metric.channel_id);
      append_runtime_metric(result, "runtime.channel.deadline_miss_count",
                            static_cast<double>(metric.deadline_miss_count), {}, {}, metric.channel_id);
      append_runtime_metric(result, "runtime.channel.stale_drop_count", static_cast<double>(metric.stale_drop_count),
                            {}, {}, metric.channel_id);
      append_runtime_metric(result, "runtime.channel.reject_count", static_cast<double>(metric.reject_count), {}, {},
                            metric.channel_id);
      append_runtime_metric(result, "runtime.channel.overwrite_count", static_cast<double>(metric.overwrite_count), {},
                            {}, metric.channel_id);
      append_runtime_metric(result, "runtime.channel.health_event_count",
                            static_cast<double>(metric.health_event_count), {}, {}, metric.channel_id);
      append_runtime_metric(result, "runtime.channel.max_depth", static_cast<double>(metric.max_depth), {}, {},
                            metric.channel_id);
      append_runtime_metric(result, "runtime.channel.message_age_ms", metric.message_age_ms, {}, {}, metric.channel_id);
      append_runtime_metric(result, "runtime.channel.payload_copy_count",
                            static_cast<double>(metric.payload_copy_count), {}, {}, metric.channel_id);
    }
    const auto publication_metrics = publications.metrics();
    result.staged_publication_count = publication_metrics.staged_count;
    result.committed_publication_count = publication_metrics.committed_count;
    result.delayed_publication_count = publication_metrics.delayed_staged_count;
    result.state_publication_count = publication_metrics.state_staged_count;
    result.state_commit_count = publication_metrics.state_commit_count;
    result.async_publication_count = publication_metrics.async_staged_count;
    result.failed_publication_commit_count = publication_metrics.failed_commit_count;
    append_runtime_metric(result, "runtime.publication.staged", static_cast<double>(publication_metrics.staged_count));
    append_runtime_metric(result, "runtime.publication.committed",
                          static_cast<double>(publication_metrics.committed_count));
    append_runtime_metric(result, "runtime.publication.delayed",
                          static_cast<double>(publication_metrics.delayed_staged_count));
    append_runtime_metric(result, "runtime.publication.state",
                          static_cast<double>(publication_metrics.state_staged_count));
    append_runtime_metric(result, "runtime.publication.state_committed",
                          static_cast<double>(publication_metrics.state_commit_count));
    append_runtime_metric(result, "runtime.publication.async",
                          static_cast<double>(publication_metrics.async_staged_count));
    append_runtime_metric(result, "runtime.publication.failed_commit",
                          static_cast<double>(publication_metrics.failed_commit_count));
    append_runtime_metric(result, "runtime.async.accepted_count",
                          static_cast<double>(publication_metrics.async_admission_accepted_count));
    append_runtime_metric(result, "runtime.async.rejected_count",
                          static_cast<double>(publication_metrics.async_admission_rejected_count));
    append_runtime_metric(result, "runtime.async.dropped_count",
                          static_cast<double>(publication_metrics.async_admission_dropped_count));
    append_runtime_metric(result, "runtime.async.in_flight_count",
                          static_cast<double>(publication_metrics.async_in_flight_count));
    append_runtime_metric(result, "runtime.async.max_in_flight_count",
                          static_cast<double>(publication_metrics.async_max_in_flight_count));
    append_runtime_metric(result, "runtime.async.completed_count",
                          static_cast<double>(publication_metrics.async_completion_count));
    append_runtime_metric(result, "runtime.async.cancelled_count",
                          static_cast<double>(publication_metrics.async_cancelled_count));
    result.health_events = health_events.snapshot();
    result.health_event_count = result.health_events.size();
    result.health_event_dropped_count = health_events.dropped_count();
    result.health_event_coalesced_count = health_events.coalesced_count();
    append_runtime_metric(result, "runtime.health.event_count", static_cast<double>(result.health_event_count));
    append_runtime_metric(result, "runtime.health.dropped_count",
                          static_cast<double>(result.health_event_dropped_count));
    append_runtime_metric(result, "runtime.health.coalesced_count",
                          static_cast<double>(result.health_event_coalesced_count));
    const auto state_metrics = state_store.metrics();
    if (state_metrics.staged_write_count != 0u || state_metrics.committed_write_count != 0u ||
        state_metrics.rejected_write_count != 0u || state_metrics.snapshot_read_count != 0u) {
      append_runtime_metric(result, "runtime.state.staged_write_count",
                            static_cast<double>(state_metrics.staged_write_count));
      append_runtime_metric(result, "runtime.state.committed_write_count",
                            static_cast<double>(state_metrics.committed_write_count));
      append_runtime_metric(result, "runtime.state.rejected_write_count",
                            static_cast<double>(state_metrics.rejected_write_count));
      append_runtime_metric(result, "runtime.state.snapshot_read_count",
                            static_cast<double>(state_metrics.snapshot_read_count));
      append_runtime_metric(result, "runtime.state.current_value_count",
                            static_cast<double>(state_metrics.current_value_count));
    }
    const auto config_metrics = config_store.metrics();
    if (config_metrics.staged_update_count != 0u || config_metrics.committed_update_count != 0u ||
        config_metrics.immediate_update_count != 0u || config_metrics.rejected_update_count != 0u ||
        config_metrics.snapshot_read_count != 0u) {
      append_runtime_metric(result, "runtime.config.staged_update_count",
                            static_cast<double>(config_metrics.staged_update_count));
      append_runtime_metric(result, "runtime.config.committed_update_count",
                            static_cast<double>(config_metrics.committed_update_count));
      append_runtime_metric(result, "runtime.config.immediate_update_count",
                            static_cast<double>(config_metrics.immediate_update_count));
      append_runtime_metric(result, "runtime.config.rejected_update_count",
                            static_cast<double>(config_metrics.rejected_update_count));
      append_runtime_metric(result, "runtime.config.snapshot_read_count",
                            static_cast<double>(config_metrics.snapshot_read_count));
    }
    for (const auto& sample : metrics.snapshot()) {
      result.runtime_metrics.push_back(RuntimeMetricSample{sample.name, sample.value, {}, {}, {}, {}});
    }
    const auto spans = trace.spans();
    const auto trace_epoch = earliest_trace_epoch(spans, result.health_events);
    for (const auto& span : spans) {
      result.trace_events.push_back(span.name);
      result.trace.push_back(
          RuntimeTraceEvent{span.name, span.trace_id.value(), non_negative_duration_ns(span.started_at - trace_epoch),
                            non_negative_duration_ns(span.finished_at - span.started_at), span.attributes});
    }
    for (const auto& event : result.health_events) {
      result.trace_events.push_back("health_event");
      result.trace.push_back(RuntimeTraceEvent{"health_event", TraceId::generate().value(),
                                               non_negative_duration_ns(event.observed_at - trace_epoch), 0u,
                                               health_event_attributes(event)});
    }
    result.trace_event_count = result.trace_events.size();
    append_runtime_metric(result, "runtime.trace.event_count", static_cast<double>(result.trace_event_count));
    result.metric_samples = result.runtime_metrics.size();
  } catch (const std::exception& error) {
    result.ok = false;
    append_runtime_error(result, make_runtime_error("runtime", {}, {}, error.what(), "exception"));
    for (auto it = instances.rbegin(); it != instances.rend(); ++it) {
      if (!it->started) {
        continue;
      }
      const auto deactivate = it->component->deactivate_status();
      ++result.stopped_components;
      if (!deactivate.ok()) {
        append_runtime_error(
            result, make_runtime_error("deactivate", it->id, {}, deactivate.message(), "component_deactivate", false));
      }
    }
  }
  return result;
}

} // namespace topoexec
