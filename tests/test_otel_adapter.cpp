#include "topoexec/adapters/otel.hpp"
#include "topoexec/runtime/graph_builder.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace {

class OTelProbeComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.OTelProbe";
    descriptor.role = topoexec::ComponentRole::kInputOutputBoundary;
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}
  void execute(const topoexec::Invocation&, topoexec::GraphContext&) override {}
};

topoexec::ComponentRegistry otel_registry() {
  topoexec::ComponentRegistry registry;
  registry.register_component({"topoexec.test.OTelProbe"}, []() { return std::make_unique<OTelProbeComponent>(); });
  return registry;
}

topoexec::GraphSpec otel_probe_graph() {
  return topoexec::GraphBuilder("otel_probe")
      .event_loop_lane("main")
      .component(topoexec::component_node("probe", "topoexec.test.OTelProbe", {topoexec::manual_event_source()},
                                          topoexec::manual_trigger(), topoexec::lane_execution("main")))
      .build();
}

bool has_attribute(const std::map<std::string, std::string>& attributes, const std::string& key,
                   const std::string& value) {
  const auto found = attributes.find(key);
  return found != attributes.end() && found->second == value;
}

} // namespace

TEST(OtelAdapter, MapsMetricDescriptorsToPreviewMetricRecords) {
  topoexec::adapters::otel::ExporterPreview exporter;
  topoexec::RuntimeMetricSample metric;
  metric.name = "runtime.component.execution_count";
  metric.value = 3.0;
  metric.component_id = "probe";
  metric.lane = "main";
  metric.tags = {"correlation_id=not-a-default-label"};

  const auto status = exporter.on_metric(metric);

  ASSERT_TRUE(status.ok()) << status.message();
  const auto records = exporter.metric_records();
  ASSERT_EQ(records.size(), 1u);
  EXPECT_EQ(records.front().name, "runtime.component.execution_count");
  EXPECT_EQ(records.front().instrument_kind, "counter");
  EXPECT_EQ(records.front().unit, "count");
  EXPECT_DOUBLE_EQ(records.front().value, 3.0);
  EXPECT_TRUE(has_attribute(records.front().attributes, "component_id", "probe"));
  EXPECT_TRUE(has_attribute(records.front().attributes, "topoexec.metric_schema_version", "1"));
  EXPECT_TRUE(has_attribute(records.front().attributes, "topoexec.ignored_tag_count", "1"));
  EXPECT_FALSE(records.front().attributes.contains("correlation_id"));
}

TEST(OtelAdapter, MapsTraceErrorsAndHealthToPreviewRecords) {
  topoexec::adapters::otel::ExporterPreview exporter;

  topoexec::RuntimeTraceEvent event;
  event.name = "component_execute";
  event.trace_id = "trace-1";
  event.phase = "component";
  event.component_id = "probe";
  event.lane = "main";
  event.start_offset_ns = 10u;
  event.duration_ns = 20u;
  event.attributes["trigger_kind"] = "manual";
  ASSERT_TRUE(exporter.on_trace_event(event).ok());

  topoexec::RuntimeError error;
  error.phase = "component";
  error.component_id = "probe";
  error.code = "probe_failed";
  error.message = "probe failed";
  error.fatal = false;
  ASSERT_TRUE(exporter.on_runtime_error(error).ok());

  topoexec::HealthEvent health;
  health.kind = topoexec::HealthEventKind::kSchedulerReject;
  health.source = "scheduler";
  health.reason = "queue_full";
  health.occurrence_count = 2u;
  ASSERT_TRUE(exporter.on_health_event(health).ok());

  const auto spans = exporter.span_records();
  ASSERT_EQ(spans.size(), 1u);
  EXPECT_EQ(spans.front().name, "component_execute");
  EXPECT_EQ(spans.front().span_kind, "internal");
  EXPECT_EQ(spans.front().duration_ns, 20u);
  EXPECT_TRUE(has_attribute(spans.front().attributes, "component_id", "probe"));
  EXPECT_TRUE(has_attribute(spans.front().attributes, "topoexec.trace_schema_version", "1"));

  const auto logs = exporter.log_records();
  ASSERT_EQ(logs.size(), 2u);
  EXPECT_EQ(logs[0].severity, "warning");
  EXPECT_EQ(logs[0].body, "probe failed");
  EXPECT_TRUE(has_attribute(logs[0].attributes, "code", "probe_failed"));
  EXPECT_EQ(logs[1].body, "topoexec.health.scheduler_reject");
  EXPECT_TRUE(has_attribute(logs[1].attributes, "reason", "queue_full"));
  EXPECT_TRUE(has_attribute(logs[1].attributes, "occurrence_count", "2"));
}

TEST(OtelAdapter, ObservesRuntimeRunnerWithoutChangingSemantics) {
  const auto registry = otel_registry();
  topoexec::RuntimeRunner runner(registry);
  topoexec::adapters::otel::ExporterPreview exporter;
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;
  options.observers.push_back(&exporter);

  const auto result = runner.run(otel_probe_graph(), options);

  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_EQ(result.observer_failure_count, 0u);
  EXPECT_GT(exporter.metric_records().size(), 0u);
  EXPECT_GT(exporter.span_records().size(), 0u);
  EXPECT_GT(exporter.log_records().size(), 0u);
  EXPECT_EQ(exporter.status().failure_count, 0u);
}

TEST(OtelAdapter, ExportResultMapsExistingRunnerRecords) {
  topoexec::RuntimeRunnerResult result;
  result.ok = true;
  result.graph_name = "manual_export";
  result.runtime_metrics.push_back({"runtime.trace.event_count", 2.0, {}, {}, {}, {}});
  result.trace.push_back({"runtime_begin", "trace-2", 0u, 0u, {{"phase", "runtime"}}});
  result.trace.front().phase = "runtime";

  topoexec::adapters::otel::ExporterPreview exporter;
  const auto status = exporter.export_result(result);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(exporter.metric_records().size(), 1u);
  EXPECT_EQ(exporter.span_records().size(), 1u);
  EXPECT_EQ(exporter.log_records().size(), 1u);
  EXPECT_TRUE(has_attribute(exporter.log_records().front().attributes, "graph_name", "manual_export"));
}
