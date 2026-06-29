#pragma once

// API stability: experimental. Prometheus text exporter preview is dependency-free adapter boundary evidence.

#include "topoexec/adapters/sdk.hpp"
#include "topoexec/runtime/metric_schema.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace topoexec::adapters::prometheus {

inline constexpr std::string_view kPrometheusPreviewContractVersion{"0"};

struct TextExporterPreviewOptions {
  std::size_t capacity{1024};
  bool fail_on_unknown_metric{true};
};

class TextExporterPreview final : public RuntimeObserver {
public:
  explicit TextExporterPreview(TextExporterPreviewOptions options = {}) : options_(options) {}

  Status on_metric(const RuntimeMetricSample& metric) override {
    const auto* descriptor = find_runtime_metric_descriptor(metric.name);
    HistogramSummaryKey histogram_key;
    if (descriptor == nullptr && !parse_histogram_summary(metric.name, histogram_key)) {
      failure_count_.fetch_add(1u, std::memory_order_relaxed);
      if (options_.fail_on_unknown_metric) {
        return Status::error("unknown runtime metric descriptor: " + metric.name);
      }
    }
    if (descriptor != nullptr) {
      const auto label_status = validate_descriptor_labels(*descriptor, metric);
      if (!label_status.ok()) {
        failure_count_.fetch_add(1u, std::memory_order_relaxed);
        return label_status;
      }
    }
    return push_bounded(metric);
  }

  Status on_result(const RuntimeRunnerResult&) override {
    return Status::success();
  }

  Status export_result(const RuntimeRunnerResult& result) {
    for (const auto& metric : result.runtime_metrics) {
      const auto status = on_metric(metric);
      if (!status.ok()) {
        return status;
      }
    }
    return Status::success();
  }

  std::string render_text() const {
    const auto metrics = metric_samples();
    std::ostringstream out;
    std::set<std::string> emitted_headers;
    std::map<std::string, HistogramSummary> histograms;
    for (const auto& metric : metrics) {
      const auto* descriptor = find_runtime_metric_descriptor(metric.name);
      if (descriptor != nullptr) {
        render_descriptor_sample(out, emitted_headers, *descriptor, metric);
        continue;
      }
      HistogramSummaryKey key;
      if (parse_histogram_summary(metric.name, key)) {
        histograms[key.base_name].observe(key.part, metric.value);
      }
    }
    for (const auto& [base_name, histogram] : histograms) {
      render_histogram_summary(out, emitted_headers, base_name, histogram);
    }
    return out.str();
  }

  RuntimeObserverStatus status() const override {
    return RuntimeObserverStatus{dropped_event_count_.load(std::memory_order_relaxed),
                                 failure_count_.load(std::memory_order_relaxed)};
  }

  std::vector<RuntimeMetricSample> metric_samples() const {
    std::lock_guard lock(mutex_);
    return metrics_;
  }

  void clear() {
    std::lock_guard lock(mutex_);
    metrics_.clear();
    dropped_event_count_.store(0u, std::memory_order_relaxed);
    failure_count_.store(0u, std::memory_order_relaxed);
  }

private:
  struct HistogramSummaryKey {
    std::string base_name;
    std::string part;
  };

  struct HistogramSummary {
    std::optional<double> count;
    std::optional<double> min;
    std::optional<double> max;
    std::optional<double> avg;
    std::optional<double> p50;
    std::optional<double> p95;
    std::optional<double> p99;

    void observe(const std::string& part, double value) {
      if (part == "count") {
        count = value;
      } else if (part == "min") {
        min = value;
      } else if (part == "max") {
        max = value;
      } else if (part == "avg") {
        avg = value;
      } else if (part == "p50") {
        p50 = value;
      } else if (part == "p95") {
        p95 = value;
      } else if (part == "p99") {
        p99 = value;
      }
    }
  };

  Status push_bounded(RuntimeMetricSample metric) {
    std::lock_guard lock(mutex_);
    if (metrics_.size() >= options_.capacity) {
      dropped_event_count_.fetch_add(1u, std::memory_order_relaxed);
      return Status::success();
    }
    metrics_.push_back(std::move(metric));
    return Status::success();
  }

  static Status validate_descriptor_labels(const RuntimeMetricDescriptor& descriptor,
                                           const RuntimeMetricSample& metric) {
    if (!metric.component_id.empty() && !allows_label(descriptor, "component_id")) {
      return Status::error("metric " + metric.name + " has unexpected component_id label");
    }
    if (!metric.lane.empty() && !allows_label(descriptor, "lane")) {
      return Status::error("metric " + metric.name + " has unexpected lane label");
    }
    if (!metric.channel_id.empty() && !allows_label(descriptor, "channel_id")) {
      return Status::error("metric " + metric.name + " has unexpected channel_id label");
    }
    return Status::success();
  }

  static bool allows_label(const RuntimeMetricDescriptor& descriptor, std::string_view label) {
    return std::find(descriptor.labels.begin(), descriptor.labels.end(), label) != descriptor.labels.end();
  }

  static std::string sanitize_name(std::string_view name) {
    std::string result;
    result.reserve(name.size() + 1u);
    for (const auto character : name) {
      const auto value = static_cast<unsigned char>(character);
      if (std::isalnum(value) != 0 || character == '_' || character == ':') {
        result.push_back(character);
      } else {
        result.push_back('_');
      }
    }
    if (result.empty() || std::isdigit(static_cast<unsigned char>(result.front())) != 0) {
      result.insert(result.begin(), '_');
    }
    return result;
  }

  static bool ends_with(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
  }

  static std::string prometheus_sample_name(const RuntimeMetricDescriptor& descriptor) {
    auto name = sanitize_name(descriptor.name);
    if (descriptor.kind == "counter" && !ends_with(name, "_total")) {
      name += "_total";
    }
    return name;
  }

  static std::string prometheus_type(const RuntimeMetricDescriptor& descriptor) {
    if (descriptor.kind == "counter") {
      return "counter";
    }
    if (descriptor.kind == "histogram") {
      return "histogram";
    }
    return "gauge";
  }

  static std::string escape_label_value(std::string_view value) {
    std::string result;
    for (const auto character : value) {
      if (character == '\\' || character == '"') {
        result.push_back('\\');
        result.push_back(character);
      } else if (character == '\n') {
        result += "\\n";
      } else if (character == '\r') {
        result += "\\r";
      } else {
        result.push_back(character);
      }
    }
    return result;
  }

  // Format a metric value for the Prometheus text exposition format. Non-finite values have specific spellings
  // (NaN, +Inf, -Inf); the default ostream output (nan/inf) is rejected by scrapers, so map them explicitly.
  static std::string format_value(double value) {
    if (std::isnan(value)) {
      return "NaN";
    }
    if (std::isinf(value)) {
      return value > 0.0 ? "+Inf" : "-Inf";
    }
    std::ostringstream out;
    out << value;
    return out.str();
  }

  static std::map<std::string, std::string> descriptor_labels(const RuntimeMetricDescriptor& descriptor,
                                                              const RuntimeMetricSample& metric) {
    std::map<std::string, std::string> labels;
    if (allows_label(descriptor, "component_id") && !metric.component_id.empty()) {
      labels["component_id"] = metric.component_id;
    }
    if (allows_label(descriptor, "lane") && !metric.lane.empty()) {
      labels["lane"] = metric.lane;
    }
    if (allows_label(descriptor, "channel_id") && !metric.channel_id.empty()) {
      labels["channel_id"] = metric.channel_id;
    }
    return labels;
  }

  static std::string render_labels(const std::map<std::string, std::string>& labels) {
    if (labels.empty()) {
      return {};
    }
    std::ostringstream out;
    out << "{";
    bool first = true;
    for (const auto& [key, value] : labels) {
      if (!first) {
        out << ",";
      }
      first = false;
      out << key << "=\"" << escape_label_value(value) << "\"";
    }
    out << "}";
    return out.str();
  }

  static void render_header(std::ostringstream& out, std::set<std::string>& emitted_headers, const std::string& name,
                            const std::string& type, const std::string& help) {
    if (!emitted_headers.insert(name).second) {
      return;
    }
    out << "# HELP " << name << " " << help << "\n";
    out << "# TYPE " << name << " " << type << "\n";
  }

  static void render_descriptor_sample(std::ostringstream& out, std::set<std::string>& emitted_headers,
                                       const RuntimeMetricDescriptor& descriptor, const RuntimeMetricSample& metric) {
    const auto name = prometheus_sample_name(descriptor);
    render_header(out, emitted_headers, name, prometheus_type(descriptor),
                  "TopoExec runtime metric " + descriptor.name);
    out << name << render_labels(descriptor_labels(descriptor, metric)) << " " << format_value(metric.value) << "\n";
  }

  static bool parse_histogram_summary(std::string_view name, HistogramSummaryKey& key) {
    static constexpr std::string_view suffixes[] = {".count", ".min", ".max", ".avg", ".p50", ".p95", ".p99"};
    for (const auto suffix : suffixes) {
      if (!ends_with(name, suffix)) {
        continue;
      }
      key.base_name = std::string(name.substr(0u, name.size() - suffix.size()));
      key.part = std::string(suffix.substr(1u));
      return !key.base_name.empty();
    }
    return false;
  }

  static void render_optional_gauge(std::ostringstream& out, std::set<std::string>& emitted_headers,
                                    const std::string& base_name, std::string_view suffix,
                                    const std::optional<double>& value) {
    if (!value.has_value()) {
      return;
    }
    const auto name = base_name + "_" + std::string(suffix);
    render_header(out, emitted_headers, name, "gauge", "TopoExec custom histogram " + std::string(suffix));
    out << name << " " << format_value(*value) << "\n";
  }

  static void render_quantile(std::ostringstream& out, const std::string& name, std::string_view quantile,
                              const std::optional<double>& value) {
    if (value.has_value()) {
      out << name << "{quantile=\"" << quantile << "\"} " << format_value(*value) << "\n";
    }
  }

  static void render_histogram_summary(std::ostringstream& out, std::set<std::string>& emitted_headers,
                                       const std::string& raw_base_name, const HistogramSummary& histogram) {
    const auto name = sanitize_name(raw_base_name);
    render_header(out, emitted_headers, name, "summary", "TopoExec custom histogram summary " + raw_base_name);
    if (histogram.count.has_value()) {
      out << name << "_count " << format_value(*histogram.count) << "\n";
      if (histogram.avg.has_value()) {
        out << name << "_sum " << format_value(*histogram.avg * *histogram.count) << "\n";
      }
    }
    render_quantile(out, name, "0.5", histogram.p50);
    render_quantile(out, name, "0.95", histogram.p95);
    render_quantile(out, name, "0.99", histogram.p99);
    render_optional_gauge(out, emitted_headers, name, "min", histogram.min);
    render_optional_gauge(out, emitted_headers, name, "max", histogram.max);
    render_optional_gauge(out, emitted_headers, name, "avg", histogram.avg);
  }

  TextExporterPreviewOptions options_;
  mutable std::mutex mutex_;
  std::vector<RuntimeMetricSample> metrics_;
  std::atomic_size_t dropped_event_count_{0};
  std::atomic_size_t failure_count_{0};
};

} // namespace topoexec::adapters::prometheus
