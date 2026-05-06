#include "topoexec/runtime/metric_schema.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

namespace topoexec {
namespace {

RuntimeMetricDescriptor make_descriptor(std::string name, std::string kind, std::string unit,
                                        std::vector<std::string> labels, std::string cardinality, std::string stability,
                                        std::string description = {}) {
  return RuntimeMetricDescriptor{std::move(name),        std::move(kind),      std::move(unit),       std::move(labels),
                                 std::move(cardinality), std::move(stability), std::move(description)};
}

void add_descriptor(std::vector<RuntimeMetricDescriptor>& descriptors, std::string name, std::string kind,
                    std::string unit, std::vector<std::string> labels, std::string cardinality,
                    std::string stability = "stable-v0.2", std::string description = {}) {
  descriptors.push_back(make_descriptor(std::move(name), std::move(kind), std::move(unit), std::move(labels),
                                        std::move(cardinality), std::move(stability), std::move(description)));
}

bool has_label(const RuntimeMetricDescriptor& descriptor, const std::string& label) {
  return std::find(descriptor.labels.begin(), descriptor.labels.end(), label) != descriptor.labels.end();
}

bool starts_with(std::string_view value, std::string_view prefix) {
  return value.rfind(prefix, 0u) == 0u;
}

bool is_forbidden_default_label(std::string_view label) {
  return label == "correlation_id" || label == "causation_id" || label == "transaction_id" || label == "trace_id" ||
         label == "request_id";
}

} // namespace

const std::vector<RuntimeMetricDescriptor>& runtime_metric_descriptors() {
  static const auto descriptors = [] {
    std::vector<RuntimeMetricDescriptor> values;
    const auto no_labels = std::vector<std::string>{};
    const auto lane = std::vector<std::string>{"lane"};
    const auto component = std::vector<std::string>{"component_id"};
    const auto channel = std::vector<std::string>{"channel_id"};

    for (const auto& name :
         {"runtime.scheduler.tick_count", "runtime.scheduler.completed_count", "runtime.scheduler.tick_overrun_count",
          "runtime.scheduler.skipped_tick_count", "runtime.scheduler.rejected_count",
          "runtime.scheduler.priority_high_count", "runtime.scheduler.priority_normal_count",
          "runtime.scheduler.priority_low_count", "runtime.scheduler.priority_background_count",
          "runtime.scheduler.low_priority_rejected_count", "runtime.scheduler.starvation_guard_count"}) {
      add_descriptor(values, name, "counter", "count", lane, "bounded: lane id");
    }
    for (const auto& name :
         {"runtime.scheduler.queue_depth", "runtime.scheduler.queue_capacity", "runtime.scheduler.worker_count",
          "runtime.scheduler.active_count", "runtime.scheduler.in_flight_count"}) {
      add_descriptor(values, name, "gauge", "count", lane, "bounded: lane id");
    }
    for (const auto& name : {"runtime.scheduler.max_lateness_ms", "runtime.scheduler.last_callback_duration_ms",
                             "runtime.scheduler.blocked_duration_ms", "runtime.scheduler.tick_jitter_ms"}) {
      add_descriptor(values, name, "gauge", "ms", lane, "bounded: lane id");
    }

    for (const auto& name :
         {"runtime.channel.configured", "runtime.channel.publish_count", "runtime.channel.delivery_count",
          "runtime.channel.drop_count", "runtime.channel.deadline_miss_count", "runtime.channel.stale_drop_count",
          "runtime.channel.reject_count", "runtime.channel.overwrite_count", "runtime.channel.health_event_count",
          "runtime.channel.payload_copy_count"}) {
      add_descriptor(values, name, "counter", "count", channel, "bounded: graph channel/edge id");
    }
    add_descriptor(values, "runtime.channel.max_depth", "gauge", "count", channel, "bounded: graph channel/edge id");
    add_descriptor(values, "runtime.channel.message_age_ms", "gauge", "ms", channel, "bounded: graph channel/edge id");

    for (const auto& name :
         {"runtime.component.execution_count", "runtime.component.error_count",
          "runtime.component.budget_overrun_count", "runtime.component.cancellation_requested_count",
          "runtime.component.cancellation_observed_count", "runtime.component.timeout_budget_exceeded_count"}) {
      add_descriptor(values, name, "counter", "count", component, "bounded: graph component id");
    }
    add_descriptor(values, "runtime.component.last_duration_ns", "gauge", "ns", component,
                   "bounded: graph component id");
    add_descriptor(values, "runtime.component.max_duration_ns", "gauge", "ns", component,
                   "bounded: graph component id");
    add_descriptor(values, "runtime.component.max_in_flight_count", "gauge", "count", component,
                   "bounded: graph component id");

    for (const auto& name :
         {"runtime.trigger.ready_count", "runtime.trigger.suppressed_count", "runtime.trigger.coalesced_count",
          "runtime.trigger.timeout_drop_count", "runtime.trigger.batch_flush_count",
          "runtime.trigger.time_sync_drop_count", "runtime.trigger.late_drop_count",
          "runtime.trigger.condition_suppressed_count", "runtime.trigger.rate_limit_suppressed_count"}) {
      add_descriptor(values, name, "counter", "count", component, "bounded: graph component id");
    }

    for (const auto& name : {"runtime.loop.iterations", "runtime.loop.converged", "runtime.loop.budget_overrun",
                             "runtime.loop.max_iterations_hit", "runtime.loop.error",
                             "runtime.loop.cancellation_requested", "runtime.loop.cancellation_observed"}) {
      add_descriptor(values, name, "counter", "count", component, "bounded: composite loop id in component_id");
    }

    for (const auto& name :
         {"runtime.publication.staged", "runtime.publication.committed", "runtime.publication.delayed",
          "runtime.publication.state", "runtime.publication.state_committed", "runtime.publication.async",
          "runtime.publication.failed_commit", "runtime.async.accepted_count", "runtime.async.rejected_count",
          "runtime.async.dropped_count", "runtime.async.completed_count", "runtime.async.cancelled_count"}) {
      add_descriptor(values, name, "counter", "count", no_labels, "none");
    }
    for (const auto& name : {"runtime.async.in_flight_count", "runtime.async.max_in_flight_count"}) {
      add_descriptor(values, name, "gauge", "count", no_labels, "none");
    }

    for (const auto& name :
         {"runtime.health.event_count", "runtime.health.dropped_count", "runtime.health.coalesced_count"}) {
      add_descriptor(values, name, "counter", "count", no_labels, "none");
    }

    for (const auto& name : {"runtime.state.staged_write_count", "runtime.state.committed_write_count",
                             "runtime.state.rejected_write_count", "runtime.state.snapshot_read_count"}) {
      add_descriptor(values, name, "counter", "count", no_labels, "none", "experimental");
    }
    add_descriptor(values, "runtime.state.current_value_count", "gauge", "count", no_labels, "none", "experimental");

    add_descriptor(values, "runtime.config.version", "gauge", "version", no_labels, "none", "experimental");
    add_descriptor(values, "runtime.config.last_transaction_id", "gauge", "id", no_labels, "none", "experimental");
    for (const auto& name : {"runtime.config.staged_update_count", "runtime.config.committed_update_count",
                             "runtime.config.immediate_update_count", "runtime.config.rejected_update_count",
                             "runtime.config.rolled_back_update_count", "runtime.config.snapshot_read_count"}) {
      add_descriptor(values, name, "counter", "count", no_labels, "none", "experimental");
    }

    for (const auto& name : {"runtime.lifecycle.reset_count", "runtime.lifecycle.reset_failure_count",
                             "runtime.lifecycle.restore_count", "runtime.lifecycle.restore_failure_count",
                             "runtime.lifecycle.snapshot_count", "runtime.lifecycle.snapshot_failure_count"}) {
      add_descriptor(values, name, "counter", "count", no_labels, "none", "experimental");
    }
    add_descriptor(values, "runtime.lifecycle.snapshot_size_bytes", "counter", "bytes", no_labels, "none",
                   "experimental");

    add_descriptor(values, "runtime.trace.event_count", "gauge", "count", no_labels, "none");
    add_descriptor(values, "runtime.observer.failure_count", "counter", "count", no_labels, "none");
    add_descriptor(values, "runtime.observer.dropped_event_count", "counter", "count", no_labels, "none");

    return values;
  }();
  return descriptors;
}

const RuntimeMetricDescriptor* find_runtime_metric_descriptor(std::string_view name) {
  const auto& descriptors = runtime_metric_descriptors();
  const auto found = std::find_if(descriptors.begin(), descriptors.end(),
                                  [&](const auto& descriptor) { return descriptor.name == name; });
  return found == descriptors.end() ? nullptr : &*found;
}

RuntimeMetricSchemaValidationResult validate_runtime_metric_samples(const std::vector<RuntimeMetricSample>& samples) {
  RuntimeMetricSchemaValidationResult result;
  std::set<std::string> descriptor_names;
  for (const auto& descriptor : runtime_metric_descriptors()) {
    if (!descriptor_names.insert(descriptor.name).second) {
      result.ok = false;
      result.errors.push_back("duplicate metric descriptor: " + descriptor.name);
    }
    for (const auto& label : descriptor.labels) {
      if (is_forbidden_default_label(label)) {
        result.ok = false;
        result.errors.push_back("metric descriptor uses forbidden default label " + label + ": " + descriptor.name);
      }
    }
  }

  for (const auto& sample : samples) {
    const auto* descriptor = find_runtime_metric_descriptor(sample.name);
    if (starts_with(sample.name, "runtime.") && descriptor == nullptr) {
      result.ok = false;
      result.errors.push_back("missing metric descriptor: " + sample.name);
      continue;
    }
    if (descriptor == nullptr) {
      continue;
    }
    if (!sample.component_id.empty() && !has_label(*descriptor, "component_id")) {
      result.ok = false;
      result.errors.push_back("metric " + sample.name + " has unexpected component_id label");
    }
    if (!sample.lane.empty() && !has_label(*descriptor, "lane")) {
      result.ok = false;
      result.errors.push_back("metric " + sample.name + " has unexpected lane label");
    }
    if (!sample.channel_id.empty() && !has_label(*descriptor, "channel_id")) {
      result.ok = false;
      result.errors.push_back("metric " + sample.name + " has unexpected channel_id label");
    }
    for (const auto& tag : sample.tags) {
      if (is_forbidden_default_label(tag)) {
        result.ok = false;
        result.errors.push_back("metric " + sample.name + " uses forbidden default tag " + tag);
        continue;
      }
      if (!has_label(*descriptor, tag)) {
        result.ok = false;
        result.errors.push_back("metric " + sample.name + " uses undeclared tag " + tag);
      }
    }
  }
  return result;
}

} // namespace topoexec
