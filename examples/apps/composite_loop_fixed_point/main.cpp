#include "topoexec/runtime/runtime_runner.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void publish_or_throw(topoexec::GraphContext& context, const std::string& port, std::string payload) {
  const auto result = context.publish(port, topoexec::make_text_payload(std::move(payload)));
  if (!result.accepted) {
    throw std::runtime_error(result.reason);
  }
}

class SourceComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.LoopSource";
    descriptor.role = topoexec::ComponentRole::kInputBoundary;
    descriptor.outputs = {{"out", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    publish_or_throw(context, "out", "measurement-" + std::to_string(invocation.sequence));
  }
};

class EstimatorComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.LoopEstimator";
    descriptor.inputs = {{"measurement", topoexec::kTextPayloadSchema}, {"correction", topoexec::kTextPayloadSchema}};
    descriptor.outputs = {{"state", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    if (invocation.payload != nullptr) {
      publish_or_throw(context, "state", "state:" + invocation.payload->text());
    }
  }
};

class ControllerComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.LoopController";
    descriptor.inputs = {{"state", topoexec::kTextPayloadSchema}};
    descriptor.outputs = {{"command", topoexec::kTextPayloadSchema}, {"correction", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    if (invocation.payload != nullptr) {
      publish_or_throw(context, "command", "command:" + invocation.payload->text());
      publish_or_throw(context, "correction", "correction:" + invocation.payload->text());
    }
  }
};

class SinkComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.LoopSink";
    descriptor.role = topoexec::ComponentRole::kOutputBoundary;
    descriptor.inputs = {{"in", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}
  void execute(const topoexec::Invocation&, topoexec::GraphContext&) override {}
};

topoexec::ComponentRegistry registry() {
  topoexec::ComponentRegistry registry;
  registry.register_component({"topoexec.app.LoopSource"}, []() { return std::make_unique<SourceComponent>(); });
  registry.register_component({"topoexec.app.LoopEstimator"}, []() { return std::make_unique<EstimatorComponent>(); });
  registry.register_component({"topoexec.app.LoopController"}, []() { return std::make_unique<ControllerComponent>(); });
  registry.register_component({"topoexec.app.LoopSink"}, []() { return std::make_unique<SinkComponent>(); });
  return registry;
}

topoexec::GraphSpec graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: app_composite_loop_fixed_point, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - {id: source, type: topoexec.app.LoopSource, event_sources: [{type: manual}], trigger_policy: {type: manual}, execution: {lane: main}}
  - {id: estimator, type: topoexec.app.LoopEstimator, event_sources: [{type: message, inputs: [measurement, correction]}], trigger_policy: {type: any_input, inputs: [measurement, correction]}, execution: {lane: main}}
  - {id: controller, type: topoexec.app.LoopController, event_sources: [{type: message, inputs: [state]}], trigger_policy: {type: any_input, inputs: [state]}, execution: {lane: main}}
  - {id: sink, type: topoexec.app.LoopSink, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
edges:
  - {id: source_estimator, kind: immediate, from: source.out, to: estimator.measurement, policy: {mode: latest, copy_policy: shared_view}}
  - {id: estimator_controller, kind: immediate, from: estimator.state, to: controller.state, policy: {mode: latest, copy_policy: shared_view}}
  - {id: controller_estimator, kind: immediate, from: controller.correction, to: estimator.correction, policy: {mode: latest, copy_policy: shared_view}}
  - {id: controller_sink, kind: immediate, from: controller.command, to: sink.in, policy: {mode: latest, copy_policy: shared_view}}
composite_loops:
  - id: estimator_controller_loop
    components: [estimator, controller]
    loop_policy: {type: fixed_point, max_iterations: 2}
)");
}

}  // namespace

int main() {
  const auto components = registry();
  auto valid_graph = graph();
  auto invalid_graph = valid_graph;
  invalid_graph.composite_loops.clear();
  const auto invalid = topoexec::validate_graph(invalid_graph, components);
  if (invalid.ok) {
    std::cerr << "error: undeclared immediate feedback loop was accepted\n";
    return 1;
  }

  topoexec::RuntimeRunner runner(components);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;
  const auto result = runner.run(valid_graph, options);
  if (!result.ok) {
    for (const auto& error : result.errors) {
      std::cerr << "error: " << error << "\n";
    }
    return 2;
  }
  if (result.loop_iteration_count != 2u || result.loop_max_iteration_hit_count != 1u) {
    std::cerr << "error: unexpected loop metrics\n";
    return 3;
  }
  std::cout << "undeclared_loop_rejected=true\n";
  std::cout << "loop_iteration_count=" << result.loop_iteration_count << "\n";
  std::cout << "loop_max_iteration_hit_count=" << result.loop_max_iteration_hit_count << "\n";
  return 0;
}
