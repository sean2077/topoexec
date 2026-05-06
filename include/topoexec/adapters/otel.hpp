#pragma once

// API stability: experimental. OTel exporter preview records are dependency-free adapter boundary evidence.

#include "topoexec/adapters/sdk.hpp"
#include "topoexec/runtime/metric_schema.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace topoexec::adapters::otel {

inline constexpr std::string_view kOtelPreviewContractVersion{"0"};

struct ExporterPreviewOptions {
  std::size_t capacity{512};
  bool fail_on_unknown_metric{true};
};

struct MetricRecord {
  std::string name;
  std::string instrument_kind;
  std::string unit;
  std::string stability;
  double value{0.0};
  std::map<std::string, std::string> attributes;
};

struct SpanRecord {
  std::string name;
  std::string trace_id;
  std::string span_kind{"internal"};
  std::string phase;
  std::uint64_t start_offset_ns{0};
  std::uint64_t duration_ns{0};
  std::map<std::string, std::string> attributes;
};

struct LogRecord {
  std::string severity;
  std::string body;
  std::map<std::string, std::string> attributes;
};

struct ExporterPreviewSnapshot {
  std::vector<MetricRecord> metrics;
  std::vector<SpanRecord> spans;
  std::vector<LogRecord> logs;
  std::size_t dropped_event_count{0};
  std::size_t failure_count{0};
};

class ExporterPreview final : public RuntimeObserver {
public:
  explicit ExporterPreview(ExporterPreviewOptions options = {}) : options_(options) {}

  Status on_metric(const RuntimeMetricSample& metric) override {
    MetricRecord record;
    const auto status = map_metric(metric, record);
    if (!status.ok()) {
      return status;
    }
    return push_bounded(metrics_, std::move(record));
  }

  Status on_trace_event(const RuntimeTraceEvent& event) override {
    SpanRecord record;
    record.name = event.name;
    record.trace_id = event.trace_id;
    record.phase = event.phase;
    record.start_offset_ns = event.start_offset_ns;
    record.duration_ns = event.duration_ns;
    record.attributes = event.attributes;
    add_if_present(record.attributes, "topoexec.trace_schema_version", std::string_view{kRuntimeTraceSchemaVersion});
    add_if_present(record.attributes, "phase", event.phase);
    add_if_present(record.attributes, "component_id", event.component_id);
    add_if_present(record.attributes, "channel_id", event.channel_id);
    add_if_present(record.attributes, "lane", event.lane);
    add_if_present(record.attributes, "worker_id", event.worker_id);
    add_if_present(record.attributes, "epoch_id", event.epoch_id);
    add_if_present(record.attributes, "transaction_id", event.transaction_id);
    add_if_present(record.attributes, "correlation_id", event.correlation_id);
    add_if_present(record.attributes, "causation_id", event.causation_id);
    return push_bounded(spans_, std::move(record));
  }

  Status on_health_event(const HealthEvent& event) override {
    LogRecord record;
    record.severity = "warning";
    record.body = "topoexec.health." + to_string(event.kind);
    add_if_present(record.attributes, "source", event.source);
    add_if_present(record.attributes, "component_id", event.component_id);
    add_if_present(record.attributes, "lane", event.lane);
    add_if_present(record.attributes, "channel_id", event.channel_id);
    add_if_present(record.attributes, "edge_id", event.edge_id);
    add_if_present(record.attributes, "policy", event.policy);
    add_if_present(record.attributes, "reason", event.reason);
    record.attributes["sequence"] = std::to_string(event.sequence);
    record.attributes["occurrence_count"] = std::to_string(event.occurrence_count);
    if (event.depth != 0u) {
      record.attributes["depth"] = std::to_string(event.depth);
    }
    if (event.capacity != 0u) {
      record.attributes["capacity"] = std::to_string(event.capacity);
    }
    for (const auto& [key, value] : event.attributes) {
      add_if_present(record.attributes, key, value);
    }
    return push_bounded(logs_, std::move(record));
  }

  Status on_runtime_error(const RuntimeError& error) override {
    LogRecord record;
    record.severity = error.fatal ? "error" : "warning";
    record.body = error.message;
    add_if_present(record.attributes, "phase", error.phase);
    add_if_present(record.attributes, "component_id", error.component_id);
    add_if_present(record.attributes, "lane", error.lane);
    add_if_present(record.attributes, "code", error.code);
    add_if_present(record.attributes, "trace_id", error.trace_id);
    record.attributes["fatal"] = error.fatal ? "true" : "false";
    return push_bounded(logs_, std::move(record));
  }

  Status on_result(const RuntimeRunnerResult& result) override {
    LogRecord record;
    record.severity = result.ok ? "info" : "error";
    record.body = "topoexec.runtime.result";
    add_if_present(record.attributes, "graph_name", result.graph_name);
    record.attributes["ok"] = result.ok ? "true" : "false";
    record.attributes["tick_calls"] = std::to_string(result.tick_calls);
    record.attributes["metric_samples"] = std::to_string(result.runtime_metrics.size());
    record.attributes["trace_events"] = std::to_string(result.trace.size());
    record.attributes["runtime_errors"] = std::to_string(result.runtime_errors.size());
    return push_bounded(logs_, std::move(record));
  }

  Status export_result(const RuntimeRunnerResult& result) {
    for (const auto& metric : result.runtime_metrics) {
      const auto status = on_metric(metric);
      if (!status.ok()) {
        return status;
      }
    }
    for (const auto& event : result.trace) {
      const auto status = on_trace_event(event);
      if (!status.ok()) {
        return status;
      }
    }
    for (const auto& event : result.health_events) {
      const auto status = on_health_event(event);
      if (!status.ok()) {
        return status;
      }
    }
    for (const auto& error : result.runtime_errors) {
      const auto status = on_runtime_error(error);
      if (!status.ok()) {
        return status;
      }
    }
    return on_result(result);
  }

  RuntimeObserverStatus status() const override {
    return RuntimeObserverStatus{dropped_event_count_.load(std::memory_order_relaxed),
                                 failure_count_.load(std::memory_order_relaxed)};
  }

  std::vector<MetricRecord> metric_records() const {
    std::lock_guard lock(mutex_);
    return metrics_;
  }

  std::vector<SpanRecord> span_records() const {
    std::lock_guard lock(mutex_);
    return spans_;
  }

  std::vector<LogRecord> log_records() const {
    std::lock_guard lock(mutex_);
    return logs_;
  }

  ExporterPreviewSnapshot snapshot() const {
    std::lock_guard lock(mutex_);
    return ExporterPreviewSnapshot{metrics_, spans_, logs_, dropped_event_count_.load(std::memory_order_relaxed),
                                   failure_count_.load(std::memory_order_relaxed)};
  }

  void clear() {
    std::lock_guard lock(mutex_);
    metrics_.clear();
    spans_.clear();
    logs_.clear();
    dropped_event_count_.store(0u, std::memory_order_relaxed);
    failure_count_.store(0u, std::memory_order_relaxed);
  }

private:
  template <typename T> Status push_bounded(std::vector<T>& records, T record) {
    std::lock_guard lock(mutex_);
    if (records.size() >= options_.capacity) {
      dropped_event_count_.fetch_add(1u, std::memory_order_relaxed);
      return Status::success();
    }
    records.push_back(std::move(record));
    return Status::success();
  }

  Status map_metric(const RuntimeMetricSample& metric, MetricRecord& record) {
    const auto* descriptor = find_runtime_metric_descriptor(metric.name);
    if (descriptor == nullptr) {
      failure_count_.fetch_add(1u, std::memory_order_relaxed);
      if (options_.fail_on_unknown_metric) {
        return Status::error("unknown runtime metric descriptor: " + metric.name);
      }
      record.name = metric.name;
      record.instrument_kind = "observable_gauge";
      record.unit = "1";
      record.stability = "unknown";
    } else {
      record.name = descriptor->name;
      record.instrument_kind = otel_instrument_kind(descriptor->kind);
      record.unit = descriptor->unit;
      record.stability = descriptor->stability;
      record.attributes["topoexec.metric_schema_version"] = std::string(kRuntimeMetricSchemaVersion);
      if (allows_label(*descriptor, "component_id")) {
        add_if_present(record.attributes, "component_id", metric.component_id);
      }
      if (allows_label(*descriptor, "lane")) {
        add_if_present(record.attributes, "lane", metric.lane);
      }
      if (allows_label(*descriptor, "channel_id")) {
        add_if_present(record.attributes, "channel_id", metric.channel_id);
      }
    }
    record.value = metric.value;
    if (!metric.tags.empty()) {
      record.attributes["topoexec.ignored_tag_count"] = std::to_string(metric.tags.size());
    }
    return Status::success();
  }

  static bool allows_label(const RuntimeMetricDescriptor& descriptor, std::string_view label) {
    for (const auto& item : descriptor.labels) {
      if (item == label) {
        return true;
      }
    }
    return false;
  }

  static std::string otel_instrument_kind(std::string_view kind) {
    if (kind == "counter") {
      return "counter";
    }
    if (kind == "histogram") {
      return "histogram";
    }
    return "observable_gauge";
  }

  static void add_if_present(std::map<std::string, std::string>& attributes, std::string key,
                             const std::string& value) {
    if (!value.empty()) {
      attributes[std::move(key)] = value;
    }
  }

  static void add_if_present(std::map<std::string, std::string>& attributes, std::string key, std::string_view value) {
    if (!value.empty()) {
      attributes[std::move(key)] = std::string(value);
    }
  }

  ExporterPreviewOptions options_;
  mutable std::mutex mutex_;
  std::vector<MetricRecord> metrics_;
  std::vector<SpanRecord> spans_;
  std::vector<LogRecord> logs_;
  std::atomic_size_t dropped_event_count_{0};
  std::atomic_size_t failure_count_{0};
};

} // namespace topoexec::adapters::otel
