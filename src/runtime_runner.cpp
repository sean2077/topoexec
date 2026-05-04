#include "topoexec/runtime/runtime_runner.hpp"

#include "topoexec/common/trace.hpp"
#include "topoexec/runtime/channel.hpp"
#include "topoexec/runtime/event_runtime.hpp"

#include <algorithm>
#include <exception>
#include <map>
#include <memory>
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
    result.errors = result.validation.errors;
    return result;
  }
  if (options.mode == RuntimeRunMode::kValidate) {
    result.ok = true;
    return result;
  }
  if (options.mode == RuntimeRunMode::kDryRun) {
    auto dry_run = dry_run_graph(graph, registry_, options.tick_iterations);
    result.ok = dry_run.ok;
    result.errors = dry_run.errors;
    copy_dry_run_to_runner(dry_run, result);
    result.scheduler_stop_reason = SchedulerStopReason::kTickBound;
    return result;
  }

  RuntimeChannelBus channels(graph.edges);
  RuntimePublicationRouter publications(&channels, graph.edges);
  TraceCollector trace;
  publications.set_trace_collector(&trace);
  MetricRegistry metrics;
  MemoryLogSink logs;
  StructuredLogger logger(graph.name);
  logger.attach_sink(&logs);
  auto lanes = lane_configs(graph);

  struct Instance {
    std::string id;
    std::unique_ptr<Component> component;
    GraphContext context;
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
      instance.context.graph_name = graph.name;
      instance.context.component_id = spec.id;
      instance.component->configure(instance.context, spec.config);
      instance.component->activate();
      instances.push_back(std::move(instance));
    }

    EventRuntime runtime(&channels, result.validation.compiled_plan, &publications);
    runtime.set_trace_collector(&trace);
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
    run_options.pending_task_cleanup_timeout = options.pending_task_cleanup_timeout;
    run_options.max_recorded_ticked_tasks =
        graph.components.size() * std::max<std::size_t>(1u, options.tick_iterations);
    const auto run_result = runtime.run(run_options);
    result.ok = run_result.ok;
    result.scheduler_stop_reason = run_result.stop_reason;
    result.tick_calls = run_result.tick_calls;
    result.ticked_components = run_result.ticked_tasks;
    result.errors = run_result.errors;
    result.instantiated_components = instances.size();
    result.configured_components = instances.size();
    result.started_components = instances.size();
    for (const auto& [lane_id, metrics] : run_result.group_metrics) {
      append_runtime_metric(result, "runtime.scheduler.completed_count", static_cast<double>(metrics.completed_count),
                            {}, lane_id);
      append_runtime_metric(result, "runtime.scheduler.tick_overrun_count",
                            static_cast<double>(metrics.tick_overrun_count), {}, lane_id);
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
    for (auto it = instances.rbegin(); it != instances.rend(); ++it) {
      it->component->deactivate();
      ++result.stopped_components;
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
      append_runtime_metric(result, "runtime.channel.max_depth", static_cast<double>(metric.max_depth), {}, {},
                            metric.channel_id);
      append_runtime_metric(result, "runtime.channel.payload_copy_count",
                            static_cast<double>(metric.payload_copy_count), {}, {}, metric.channel_id);
    }
    const auto publication_metrics = publications.metrics();
    result.staged_publication_count = publication_metrics.staged_count;
    result.committed_publication_count = publication_metrics.committed_count;
    result.delayed_publication_count = publication_metrics.delayed_staged_count;
    result.state_publication_count = publication_metrics.state_staged_count;
    result.async_publication_count = publication_metrics.async_staged_count;
    result.failed_publication_commit_count = publication_metrics.failed_commit_count;
    append_runtime_metric(result, "runtime.publication.staged", static_cast<double>(publication_metrics.staged_count));
    append_runtime_metric(result, "runtime.publication.committed",
                          static_cast<double>(publication_metrics.committed_count));
    append_runtime_metric(result, "runtime.publication.delayed",
                          static_cast<double>(publication_metrics.delayed_staged_count));
    append_runtime_metric(result, "runtime.publication.state",
                          static_cast<double>(publication_metrics.state_staged_count));
    append_runtime_metric(result, "runtime.publication.async",
                          static_cast<double>(publication_metrics.async_staged_count));
    append_runtime_metric(result, "runtime.publication.failed_commit",
                          static_cast<double>(publication_metrics.failed_commit_count));
    for (const auto& sample : metrics.snapshot()) {
      result.runtime_metrics.push_back(RuntimeMetricSample{sample.name, sample.value, {}, {}, {}, {}});
    }
    for (const auto& span : trace.spans()) {
      result.trace_events.push_back(span.name);
    }
    result.trace_event_count = result.trace_events.size();
    append_runtime_metric(result, "runtime.trace.event_count", static_cast<double>(result.trace_event_count));
    result.metric_samples = result.runtime_metrics.size();
  } catch (const std::exception& error) {
    result.ok = false;
    result.errors.push_back(error.what());
  }
  return result;
}

} // namespace topoexec
