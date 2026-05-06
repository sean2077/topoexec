#include "topoexec/adapters/prometheus.hpp"
#include "topoexec/runtime/graph_builder.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace {

class PrometheusProbeComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.PrometheusProbe";
    descriptor.role = topoexec::ComponentRole::kInputOutputBoundary;
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}
  void execute(const topoexec::Invocation&, topoexec::GraphContext&) override {}
};

topoexec::ComponentRegistry prometheus_registry() {
  topoexec::ComponentRegistry registry;
  registry.register_component({"topoexec.test.PrometheusProbe"},
                              []() { return std::make_unique<PrometheusProbeComponent>(); });
  return registry;
}

topoexec::GraphSpec prometheus_probe_graph() {
  return topoexec::GraphBuilder("prometheus_probe")
      .event_loop_lane("main")
      .component(topoexec::component_node("probe", "topoexec.test.PrometheusProbe", {topoexec::manual_event_source()},
                                          topoexec::manual_trigger(), topoexec::lane_execution("main")))
      .build();
}

bool contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

} // namespace

TEST(PrometheusAdapter, MapsCountersAndGaugesWithBoundedLabels) {
  topoexec::adapters::prometheus::TextExporterPreview exporter;
  topoexec::RuntimeMetricSample counter;
  counter.name = "runtime.component.execution_count";
  counter.value = 3.0;
  counter.component_id = "probe";
  counter.tags = {"correlation_id=not-a-default-label"};
  ASSERT_TRUE(exporter.on_metric(counter).ok());

  topoexec::RuntimeMetricSample gauge;
  gauge.name = "runtime.scheduler.queue_depth";
  gauge.value = 4.0;
  gauge.lane = "main";
  ASSERT_TRUE(exporter.on_metric(gauge).ok());

  const auto text = exporter.render_text();
  EXPECT_TRUE(contains(text, "# TYPE runtime_component_execution_count_total counter"));
  EXPECT_TRUE(contains(text, "runtime_component_execution_count_total{component_id=\"probe\"} 3"));
  EXPECT_TRUE(contains(text, "# TYPE runtime_scheduler_queue_depth gauge"));
  EXPECT_TRUE(contains(text, "runtime_scheduler_queue_depth{lane=\"main\"} 4"));
  EXPECT_FALSE(contains(text, "correlation_id"));
}

TEST(PrometheusAdapter, MapsCustomHistogramSummariesToSummaryText) {
  topoexec::adapters::prometheus::TextExporterPreview exporter;
  ASSERT_TRUE(exporter.on_metric({"latency_ms.count", 2.0, {}, {}, {}, {}}).ok());
  ASSERT_TRUE(exporter.on_metric({"latency_ms.avg", 20.0, {}, {}, {}, {}}).ok());
  ASSERT_TRUE(exporter.on_metric({"latency_ms.p50", 20.0, {}, {}, {}, {}}).ok());
  ASSERT_TRUE(exporter.on_metric({"latency_ms.p95", 29.0, {}, {}, {}, {}}).ok());
  ASSERT_TRUE(exporter.on_metric({"latency_ms.p99", 30.0, {}, {}, {}, {}}).ok());
  ASSERT_TRUE(exporter.on_metric({"latency_ms.min", 10.0, {}, {}, {}, {}}).ok());
  ASSERT_TRUE(exporter.on_metric({"latency_ms.max", 30.0, {}, {}, {}, {}}).ok());

  const auto text = exporter.render_text();
  EXPECT_TRUE(contains(text, "# TYPE latency_ms summary"));
  EXPECT_TRUE(contains(text, "latency_ms_count 2"));
  EXPECT_TRUE(contains(text, "latency_ms_sum 40"));
  EXPECT_TRUE(contains(text, "latency_ms{quantile=\"0.95\"} 29"));
  EXPECT_TRUE(contains(text, "# TYPE latency_ms_min gauge"));
  EXPECT_TRUE(contains(text, "latency_ms_max 30"));
}

TEST(PrometheusAdapter, RejectsUnexpectedDescriptorLabels) {
  topoexec::adapters::prometheus::TextExporterPreview exporter;
  topoexec::RuntimeMetricSample metric;
  metric.name = "runtime.trace.event_count";
  metric.value = 1.0;
  metric.component_id = "unexpected";

  const auto status = exporter.on_metric(metric);

  EXPECT_FALSE(status.ok());
  EXPECT_NE(status.message().find("unexpected component_id"), std::string::npos);
  EXPECT_EQ(exporter.status().failure_count, 1u);
}

TEST(PrometheusAdapter, ObservesRuntimeRunnerWithoutChangingSemantics) {
  const auto registry = prometheus_registry();
  topoexec::RuntimeRunner runner(registry);
  topoexec::adapters::prometheus::TextExporterPreview exporter;
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;
  options.observers.push_back(&exporter);

  const auto result = runner.run(prometheus_probe_graph(), options);

  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_EQ(result.observer_failure_count, 0u);
  EXPECT_GT(exporter.metric_samples().size(), 0u);
  EXPECT_EQ(exporter.status().failure_count, 0u);
  const auto text = exporter.render_text();
  EXPECT_TRUE(contains(text, "runtime_component_execution_count_total"));
}

TEST(PrometheusAdapter, ExportResultMapsExistingRunnerMetrics) {
  topoexec::RuntimeRunnerResult result;
  result.ok = true;
  result.graph_name = "manual_export";
  result.runtime_metrics.push_back({"runtime.trace.event_count", 2.0, {}, {}, {}, {}});

  topoexec::adapters::prometheus::TextExporterPreview exporter;
  const auto status = exporter.export_result(result);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(exporter.metric_samples().size(), 1u);
  EXPECT_TRUE(contains(exporter.render_text(), "runtime_trace_event_count 2"));
}
