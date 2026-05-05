#include "topoexec/runtime/graph_builder.hpp"
#include "topoexec/runtime/runtime_runner.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

std::string observed_payload;

class RuntimeSmokeSource : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.smoke.Source";
    descriptor.role = topoexec::ComponentRole::kInputBoundary;
    descriptor.outputs = {{"out", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation&, topoexec::GraphContext& context) override {
    const auto result = context.publish("out", topoexec::make_text_payload("runtime-only"));
    if (!result.accepted) {
      throw std::runtime_error(result.reason);
    }
  }
};

class RuntimeSmokeSink : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.smoke.Sink";
    descriptor.role = topoexec::ComponentRole::kOutputBoundary;
    descriptor.inputs = {{"in", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext&) override {
    if (invocation.payload != nullptr) {
      observed_payload = topoexec::require_text_payload(*invocation.payload, "runtime smoke sink");
    }
  }
};

topoexec::ComponentRegistry registry() {
  topoexec::ComponentRegistry registry;
  registry.register_component({"topoexec.smoke.Source"}, []() { return std::make_unique<RuntimeSmokeSource>(); });
  registry.register_component({"topoexec.smoke.Sink"}, []() { return std::make_unique<RuntimeSmokeSink>(); });
  return registry;
}

topoexec::GraphSpec graph() {
  return topoexec::GraphBuilder("runtime_package_smoke")
      .event_loop_lane("main")
      .component(topoexec::component_node("source", "topoexec.smoke.Source", {topoexec::manual_event_source()},
                                          topoexec::manual_trigger(), topoexec::lane_execution("main")))
      .component(topoexec::component_node("sink", "topoexec.smoke.Sink", {topoexec::message_event_source({"in"})},
                                          topoexec::any_input_trigger({"in"}), topoexec::lane_execution("main")))
      .edge(topoexec::immediate_edge("source_sink", "source.out", "sink.in"))
      .build();
}

} // namespace

int main() {
  observed_payload.clear();
  const auto components = registry();
  topoexec::RuntimeRunner runner(components);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;
  const auto result = runner.run(graph(), options);
  if (!result.ok) {
    for (const auto& error : result.errors) {
      std::cerr << error << "\n";
    }
    return 1;
  }
  if (observed_payload != "runtime-only") {
    std::cerr << "unexpected sink payload: " << observed_payload << "\n";
    return 2;
  }
  if (result.committed_publication_count != 1u || result.channel_delivery_count != 1u) {
    std::cerr << "unexpected runtime counters\n";
    return 3;
  }
  const auto has_metric =
      std::any_of(result.runtime_metrics.begin(), result.runtime_metrics.end(), [](const auto& metric) {
        return metric.name == "runtime.component.execution_count" && metric.component_id == "source" &&
               metric.value == 1.0;
      });
  if (!has_metric || result.metric_samples != result.runtime_metrics.size()) {
    std::cerr << "runtime metrics were not exposed through RuntimeRunnerResult\n";
    return 4;
  }
  const auto has_trace = std::any_of(result.trace.begin(), result.trace.end(), [](const auto& event) {
    return event.name == "channel_publish" && event.attributes.count("edge_kind") != 0u;
  });
  if (!has_trace || result.trace_event_count != result.trace.size()) {
    std::cerr << "runtime trace was not exposed through RuntimeRunnerResult\n";
    return 5;
  }
  std::cout << "runtime_smoke_payload=" << observed_payload << "\n";
  return 0;
}
