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
#include <set>
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

void notify_runtime_observers(const RuntimeRunnerOptions& options, RuntimeRunnerResult& result) {
  if (options.observers.empty()) {
    return;
  }
  std::size_t callback_failure_count = 0;
  std::string first_failure;
  auto record_delivery = [&](const Status& status, const std::string& callback) {
    if (status.ok()) {
      return;
    }
    ++callback_failure_count;
    if (first_failure.empty()) {
      first_failure = callback + ": " + status.message();
    }
  };

  for (auto* observer : options.observers) {
    if (observer == nullptr) {
      continue;
    }
    for (const auto& metric : result.runtime_metrics) {
      record_delivery(observer->on_metric(metric), "on_metric");
    }
    for (const auto& event : result.trace) {
      record_delivery(observer->on_trace_event(event), "on_trace_event");
    }
    for (const auto& event : result.health_events) {
      record_delivery(observer->on_health_event(event), "on_health_event");
    }
    for (const auto& error : result.runtime_errors) {
      record_delivery(observer->on_runtime_error(error), "on_runtime_error");
    }
    record_delivery(observer->on_result(result), "on_result");
    const auto observer_status = observer->status();
    result.observer_dropped_event_count += observer_status.dropped_event_count;
    result.observer_failure_count += observer_status.failure_count;
  }

  result.observer_failure_count += callback_failure_count;
  if (callback_failure_count != 0u) {
    result.runtime_errors.push_back(
        make_runtime_error("observer", {}, {},
                           "observer callback failure count: " + std::to_string(callback_failure_count) +
                               (first_failure.empty() ? std::string{} : " (" + first_failure + ")"),
                           "observer_failure", false));
  }
  if (result.observer_failure_count != 0u) {
    append_runtime_metric(result, "runtime.observer.failure_count", static_cast<double>(result.observer_failure_count));
  }
  if (result.observer_dropped_event_count != 0u) {
    append_runtime_metric(result, "runtime.observer.dropped_event_count",
                          static_cast<double>(result.observer_dropped_event_count));
  }
  result.metric_samples = result.runtime_metrics.size();
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

std::size_t snapshot_size_bytes(const ComponentStateSnapshot& snapshot) {
  if (snapshot.size_bytes != 0u) {
    return snapshot.size_bytes;
  }
  if (snapshot.payload == nullptr) {
    return 0u;
  }
  return describe_payload_schema(*snapshot.payload).size_estimate;
}

void record_runner_trace_event(TraceCollector& trace, const std::string& name,
                               std::map<std::string, std::string> attributes) {
  const auto now = std::chrono::steady_clock::now();
  trace.add(SpanRecord{TraceId::generate(), name, now, now, std::move(attributes)});
}

void append_lifecycle_metrics(RuntimeRunnerResult& result) {
  if (result.lifecycle_reset_count != 0u) {
    append_runtime_metric(result, "runtime.lifecycle.reset_count", static_cast<double>(result.lifecycle_reset_count));
  }
  if (result.lifecycle_reset_failure_count != 0u) {
    append_runtime_metric(result, "runtime.lifecycle.reset_failure_count",
                          static_cast<double>(result.lifecycle_reset_failure_count));
  }
  if (result.lifecycle_restore_count != 0u) {
    append_runtime_metric(result, "runtime.lifecycle.restore_count",
                          static_cast<double>(result.lifecycle_restore_count));
  }
  if (result.lifecycle_restore_failure_count != 0u) {
    append_runtime_metric(result, "runtime.lifecycle.restore_failure_count",
                          static_cast<double>(result.lifecycle_restore_failure_count));
  }
  if (result.lifecycle_snapshot_count != 0u) {
    append_runtime_metric(result, "runtime.lifecycle.snapshot_count",
                          static_cast<double>(result.lifecycle_snapshot_count));
  }
  if (result.lifecycle_snapshot_failure_count != 0u) {
    append_runtime_metric(result, "runtime.lifecycle.snapshot_failure_count",
                          static_cast<double>(result.lifecycle_snapshot_failure_count));
  }
  if (result.lifecycle_snapshot_bytes != 0u) {
    append_runtime_metric(result, "runtime.lifecycle.snapshot_size_bytes",
                          static_cast<double>(result.lifecycle_snapshot_bytes));
  }
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

std::string trace_phase_for_name(const std::string& name) {
  if (name.rfind("component_", 0u) == 0u) {
    return "component";
  }
  if (name.rfind("channel_", 0u) == 0u || name == "state_commit" || name == "async_admission") {
    return "channel";
  }
  if (name.rfind("scheduler_", 0u) == 0u || name.rfind("fixed_rate_", 0u) == 0u || name == "thread_pool_batch") {
    return "scheduler";
  }
  if (name.rfind("loop_", 0u) == 0u) {
    return "loop";
  }
  if (name.rfind("config_transaction_", 0u) == 0u) {
    return "config";
  }
  if (name == "health_event") {
    return "health";
  }
  return "runtime";
}

std::string attribute_value(const std::map<std::string, std::string>& attributes, const std::string& key) {
  const auto found = attributes.find(key);
  return found == attributes.end() ? std::string{} : found->second;
}

RuntimeTraceEvent make_runtime_trace_event(std::string name, std::string trace_id, std::uint64_t start_offset_ns,
                                           std::uint64_t duration_ns, std::map<std::string, std::string> attributes) {
  RuntimeTraceEvent event;
  event.name = std::move(name);
  event.trace_id = std::move(trace_id);
  event.phase = trace_phase_for_name(event.name);
  event.component_id = attribute_value(attributes, "component_id");
  if (event.component_id.empty()) {
    event.component_id = attribute_value(attributes, "source_component");
  }
  event.channel_id = attribute_value(attributes, "channel_id");
  event.lane = attribute_value(attributes, "lane");
  event.worker_id = attribute_value(attributes, "worker_id");
  event.epoch_id = attribute_value(attributes, "epoch_id");
  event.transaction_id = attribute_value(attributes, "transaction_id");
  event.correlation_id = attribute_value(attributes, "correlation_id");
  event.causation_id = attribute_value(attributes, "causation_id");
  event.start_offset_ns = start_offset_ns;
  event.duration_ns = duration_ns;
  event.attributes = std::move(attributes);
  return event;
}

void copy_trace_to_result(const TraceCollector& trace, const std::vector<HealthEvent>& health_events,
                          RuntimeRunnerResult& result) {
  const auto spans = trace.spans();
  const auto trace_epoch = earliest_trace_epoch(spans, health_events);
  std::vector<std::pair<std::size_t, RuntimeTraceEvent>> ordered_events;
  ordered_events.reserve(spans.size() + health_events.size());
  for (const auto& span : spans) {
    ordered_events.emplace_back(ordered_events.size(),
                                make_runtime_trace_event(span.name, span.trace_id.value(),
                                                         non_negative_duration_ns(span.started_at - trace_epoch),
                                                         non_negative_duration_ns(span.finished_at - span.started_at),
                                                         span.attributes));
  }
  for (const auto& event : health_events) {
    ordered_events.emplace_back(ordered_events.size(),
                                make_runtime_trace_event("health_event", TraceId::generate().value(),
                                                         non_negative_duration_ns(event.observed_at - trace_epoch), 0u,
                                                         health_event_attributes(event)));
  }
  std::stable_sort(ordered_events.begin(), ordered_events.end(), [](const auto& left, const auto& right) {
    if (left.second.start_offset_ns != right.second.start_offset_ns) {
      return left.second.start_offset_ns < right.second.start_offset_ns;
    }
    return left.first < right.first;
  });
  for (auto& [order, event] : ordered_events) {
    (void)order;
    result.trace_events.push_back(event.name);
    result.trace.push_back(std::move(event));
  }
  result.trace_event_count = result.trace_events.size();
}

} // namespace

Status ResultSink::on_result(const RuntimeRunnerResult&) {
  return Status::success();
}

Status MetricSink::on_metric(const RuntimeMetricSample&) {
  return Status::success();
}

Status TraceSink::on_trace_event(const RuntimeTraceEvent&) {
  return Status::success();
}

Status RuntimeObserver::on_health_event(const HealthEvent&) {
  return Status::success();
}

Status RuntimeObserver::on_runtime_error(const RuntimeError&) {
  return Status::success();
}

RuntimeObserverStatus RuntimeObserver::status() const {
  return {};
}

InMemoryRuntimeObserver::InMemoryRuntimeObserver(std::size_t capacity) : capacity_(capacity) {}

template <typename T> Status InMemoryRuntimeObserver::push_bounded(std::vector<T>& records, T value) {
  if (capacity_ == 0u) {
    dropped_event_count_.fetch_add(1u, std::memory_order_relaxed);
    return Status::success();
  }
  std::unique_lock lock(mutex_, std::try_to_lock);
  if (!lock.owns_lock()) {
    dropped_event_count_.fetch_add(1u, std::memory_order_relaxed);
    return Status::success();
  }
  if (records.size() >= capacity_) {
    records.erase(records.begin());
    dropped_event_count_.fetch_add(1u, std::memory_order_relaxed);
  }
  records.push_back(std::move(value));
  return Status::success();
}

Status InMemoryRuntimeObserver::on_result(const RuntimeRunnerResult& result) {
  return push_bounded(results_, result);
}

Status InMemoryRuntimeObserver::on_metric(const RuntimeMetricSample& metric) {
  return push_bounded(metrics_, metric);
}

Status InMemoryRuntimeObserver::on_trace_event(const RuntimeTraceEvent& event) {
  return push_bounded(trace_events_, event);
}

Status InMemoryRuntimeObserver::on_health_event(const HealthEvent& event) {
  return push_bounded(health_events_, event);
}

Status InMemoryRuntimeObserver::on_runtime_error(const RuntimeError& error) {
  return push_bounded(runtime_errors_, error);
}

RuntimeObserverStatus InMemoryRuntimeObserver::status() const {
  return RuntimeObserverStatus{dropped_event_count_.load(std::memory_order_relaxed), 0u};
}

std::vector<RuntimeRunnerResult> InMemoryRuntimeObserver::results() const {
  std::lock_guard lock(mutex_);
  return results_;
}

std::vector<RuntimeMetricSample> InMemoryRuntimeObserver::metrics() const {
  std::lock_guard lock(mutex_);
  return metrics_;
}

std::vector<RuntimeTraceEvent> InMemoryRuntimeObserver::trace_events() const {
  std::lock_guard lock(mutex_);
  return trace_events_;
}

std::vector<HealthEvent> InMemoryRuntimeObserver::health_events() const {
  std::lock_guard lock(mutex_);
  return health_events_;
}

std::vector<RuntimeError> InMemoryRuntimeObserver::runtime_errors() const {
  std::lock_guard lock(mutex_);
  return runtime_errors_;
}

void InMemoryRuntimeObserver::clear() {
  std::lock_guard lock(mutex_);
  results_.clear();
  metrics_.clear();
  trace_events_.clear();
  health_events_.clear();
  runtime_errors_.clear();
  dropped_event_count_.store(0u, std::memory_order_relaxed);
}

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
  auto finish_result = [&]() {
    notify_runtime_observers(options, result);
    return result;
  };
  result.validation = validate_graph(graph, registry_);
  if (!result.validation.ok) {
    result.ok = false;
    for (const auto& error : result.validation.errors) {
      append_runtime_error(result, make_runtime_error("validate", {}, {}, error, "validation"));
    }
    return finish_result();
  }
  if (options.mode == RuntimeRunMode::kValidate) {
    result.ok = true;
    return finish_result();
  }
  if (options.mode == RuntimeRunMode::kDryRun) {
    auto dry_run = dry_run_graph(graph, registry_, options.tick_iterations);
    result.ok = dry_run.ok;
    for (const auto& error : dry_run.errors) {
      append_runtime_error(result, make_runtime_error("dry_run", {}, {}, error, "dry_run"));
    }
    copy_dry_run_to_runner(dry_run, result);
    result.scheduler_stop_reason = SchedulerStopReason::kTickBound;
    return finish_result();
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
    auto deactivate_started_components = [&]() {
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
    };
    auto instance_for_id = [&](const std::string& component_id) -> Instance* {
      auto found =
          std::find_if(instances.begin(), instances.end(), [&](const auto& item) { return item.id == component_id; });
      if (found == instances.end()) {
        return nullptr;
      }
      return &*found;
    };
    auto spec_for_id = [&](const std::string& component_id) -> const ComponentNodeSpec* {
      const auto found = std::find_if(graph.components.begin(), graph.components.end(),
                                      [&](const auto& item) { return item.id == component_id; });
      if (found == graph.components.end()) {
        return nullptr;
      }
      return &*found;
    };
    auto finish_early_after_lifecycle_error = [&]() {
      deactivate_started_components();
      append_lifecycle_metrics(result);
      result.health_events = health_events.snapshot();
      result.health_event_count = result.health_events.size();
      result.health_event_dropped_count = health_events.dropped_count();
      result.health_event_coalesced_count = health_events.coalesced_count();
      copy_trace_to_result(trace, result.health_events, result);
      if (result.trace_event_count != 0u) {
        append_runtime_metric(result, "runtime.trace.event_count", static_cast<double>(result.trace_event_count));
      }
      result.metric_samples = result.runtime_metrics.size();
    };
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
      deactivate_started_components();
      return finish_result();
    }

    for (const auto& [component_id, snapshot] : options.restore_component_states) {
      auto* instance = instance_for_id(component_id);
      const auto* spec = spec_for_id(component_id);
      if (instance == nullptr || spec == nullptr || !instance->started) {
        result.ok = false;
        result.scheduler_stop_reason = SchedulerStopReason::kError;
        ++result.lifecycle_restore_failure_count;
        append_runtime_error(result,
                             make_runtime_error("restore", component_id, {},
                                                "component state restore target is not started", "component_restore"));
        continue;
      }
      if (!snapshot.component_type.empty() && snapshot.component_type != spec->type) {
        result.ok = false;
        result.scheduler_stop_reason = SchedulerStopReason::kError;
        ++result.lifecycle_restore_failure_count;
        append_runtime_error(result, make_runtime_error("restore", component_id, spec->execution.lane,
                                                        "component state type " + snapshot.component_type +
                                                            " does not match graph type " + spec->type,
                                                        "component_restore"));
        record_runner_trace_event(trace, "component_restore",
                                  {{"component_id", component_id}, {"status", "type_mismatch"}});
        continue;
      }
      record_runner_trace_event(trace, "component_restore", {{"component_id", component_id}, {"status", "begin"}});
      const auto restore = instance->component->restore_state(snapshot);
      if (!restore.ok()) {
        result.ok = false;
        result.scheduler_stop_reason = SchedulerStopReason::kError;
        ++result.lifecycle_restore_failure_count;
        append_runtime_error(result, make_runtime_error("restore", component_id, spec->execution.lane,
                                                        restore.message(), "component_restore"));
        record_runner_trace_event(trace, "component_restore", {{"component_id", component_id}, {"status", "error"}});
        continue;
      }
      ++result.lifecycle_restore_count;
      record_runner_trace_event(trace, "component_restore", {{"component_id", component_id}, {"status", "ok"}});
    }

    std::set<std::string> reset_seen;
    for (const auto& component_id : options.reset_component_ids) {
      if (!reset_seen.insert(component_id).second) {
        continue;
      }
      auto* instance = instance_for_id(component_id);
      const auto* spec = spec_for_id(component_id);
      if (instance == nullptr || spec == nullptr || !instance->started) {
        result.ok = false;
        result.scheduler_stop_reason = SchedulerStopReason::kError;
        ++result.lifecycle_reset_failure_count;
        append_runtime_error(result, make_runtime_error("reset", component_id, {},
                                                        "component reset target is not started", "component_reset"));
        continue;
      }
      record_runner_trace_event(trace, "component_reset", {{"component_id", component_id}, {"status", "begin"}});
      const auto reset = instance->component->reset_status(instance->context);
      if (!reset.ok()) {
        result.ok = false;
        result.scheduler_stop_reason = SchedulerStopReason::kError;
        ++result.lifecycle_reset_failure_count;
        append_runtime_error(result, make_runtime_error("reset", component_id, spec->execution.lane, reset.message(),
                                                        "component_reset"));
        record_runner_trace_event(trace, "component_reset", {{"component_id", component_id}, {"status", "error"}});
        continue;
      }
      ++result.lifecycle_reset_count;
      record_runner_trace_event(trace, "component_reset", {{"component_id", component_id}, {"status", "ok"}});
    }

    if (!result.errors.empty()) {
      finish_early_after_lifecycle_error();
      return finish_result();
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
      append_runtime_metric(result, "runtime.trigger.late_drop_count", static_cast<double>(metrics.late_drop_count),
                            component_id);
      append_runtime_metric(result, "runtime.trigger.condition_suppressed_count",
                            static_cast<double>(metrics.condition_suppressed_count), component_id);
      append_runtime_metric(result, "runtime.trigger.rate_limit_suppressed_count",
                            static_cast<double>(metrics.rate_limit_suppressed_count), component_id);
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
    if (options.capture_component_state_snapshots) {
      for (const auto& instance : instances) {
        if (!instance.started) {
          continue;
        }
        const auto* spec = spec_for_id(instance.id);
        record_runner_trace_event(trace, "component_snapshot", {{"component_id", instance.id}, {"status", "begin"}});
        const auto snapshot = instance.component->snapshot_state();
        if (!snapshot.ok()) {
          result.ok = false;
          ++result.lifecycle_snapshot_failure_count;
          append_runtime_error(result,
                               make_runtime_error("snapshot", instance.id, spec == nullptr ? "" : spec->execution.lane,
                                                  snapshot.status().message(), "component_snapshot", false));
          record_runner_trace_event(trace, "component_snapshot", {{"component_id", instance.id}, {"status", "error"}});
          continue;
        }
        auto value = snapshot.value();
        if (value.component_type.empty() && spec != nullptr) {
          value.component_type = spec->type;
        }
        value.size_bytes = snapshot_size_bytes(value);
        result.lifecycle_snapshot_bytes += value.size_bytes;
        result.component_state_snapshots[instance.id] = std::move(value);
        ++result.lifecycle_snapshot_count;
        record_runner_trace_event(trace, "component_snapshot", {{"component_id", instance.id}, {"status", "ok"}});
      }
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
        config_metrics.rolled_back_update_count != 0u || config_metrics.snapshot_read_count != 0u ||
        config_metrics.version != 0u || config_metrics.last_transaction_id != 0u) {
      append_runtime_metric(result, "runtime.config.version", static_cast<double>(config_metrics.version));
      append_runtime_metric(result, "runtime.config.last_transaction_id",
                            static_cast<double>(config_metrics.last_transaction_id));
      append_runtime_metric(result, "runtime.config.staged_update_count",
                            static_cast<double>(config_metrics.staged_update_count));
      append_runtime_metric(result, "runtime.config.committed_update_count",
                            static_cast<double>(config_metrics.committed_update_count));
      append_runtime_metric(result, "runtime.config.immediate_update_count",
                            static_cast<double>(config_metrics.immediate_update_count));
      append_runtime_metric(result, "runtime.config.rejected_update_count",
                            static_cast<double>(config_metrics.rejected_update_count));
      append_runtime_metric(result, "runtime.config.rolled_back_update_count",
                            static_cast<double>(config_metrics.rolled_back_update_count));
      append_runtime_metric(result, "runtime.config.snapshot_read_count",
                            static_cast<double>(config_metrics.snapshot_read_count));
    }
    append_lifecycle_metrics(result);
    for (const auto& sample : metrics.snapshot()) {
      result.runtime_metrics.push_back(RuntimeMetricSample{sample.name, sample.value, {}, {}, {}, {}});
    }
    copy_trace_to_result(trace, result.health_events, result);
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
  return finish_result();
}

} // namespace topoexec
