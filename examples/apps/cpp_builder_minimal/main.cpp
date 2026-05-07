#include "topoexec/runtime/graph_builder.hpp"
#include "topoexec/runtime/runtime_runner.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct PipelineState {
  std::string sink_payload;
};

PipelineState& state() {
  static PipelineState value;
  return value;
}

class SourceComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.BuilderSource";
    descriptor.name = "builder_source";
    descriptor.role = topoexec::ComponentRole::kInputBoundary;
    descriptor.outputs = {{"out", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation&, topoexec::GraphContext& context) override {
    const auto published = context.publish("out", topoexec::make_text_payload("hello"));
    if (!published.accepted) {
      throw std::runtime_error(published.reason);
    }
  }
};

class TransformComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.BuilderTransform";
    descriptor.name = "builder_transform";
    descriptor.inputs = {{"in", topoexec::kTextPayloadSchema}};
    descriptor.outputs = {{"out", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    if (invocation.payload == nullptr) {
      return;
    }
    const auto published = context.publish("out", topoexec::make_text_payload(invocation.payload->text() + ":built"));
    if (!published.accepted) {
      throw std::runtime_error(published.reason);
    }
  }
};

class SinkComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.BuilderSink";
    descriptor.name = "builder_sink";
    descriptor.role = topoexec::ComponentRole::kOutputBoundary;
    descriptor.inputs = {{"in", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext&) override {
    if (invocation.payload != nullptr) {
      state().sink_payload = invocation.payload->text();
    }
  }
};

topoexec::ComponentRegistry registry() {
  topoexec::ComponentRegistry registry;
  registry.register_component({"topoexec.app.BuilderSource"}, []() { return std::make_unique<SourceComponent>(); });
  registry.register_component({"topoexec.app.BuilderTransform"},
                              []() { return std::make_unique<TransformComponent>(); });
  registry.register_component({"topoexec.app.BuilderSink"}, []() { return std::make_unique<SinkComponent>(); });
  return registry;
}

topoexec::BoundaryDescriptor boundary(topoexec::ComponentRole role) {
  topoexec::BoundaryDescriptor boundary;
  boundary.role = role;
  boundary.descriptor = "cpp-builder";
  return boundary;
}

topoexec::GraphSpec graph() {
  return topoexec::GraphBuilder("app_cpp_builder_minimal")
      .event_loop_lane("main")
      .component(topoexec::component_node("source", "topoexec.app.BuilderSource", {topoexec::manual_event_source()},
                                          topoexec::manual_trigger(), topoexec::lane_execution("main"),
                                          boundary(topoexec::ComponentRole::kInputBoundary)))
      .component(topoexec::component_node("transform", "topoexec.app.BuilderTransform",
                                          {topoexec::message_event_source({"in"})}, topoexec::any_input_trigger({"in"}),
                                          topoexec::lane_execution("main")))
      .component(topoexec::component_node("sink", "topoexec.app.BuilderSink", {topoexec::message_event_source({"in"})},
                                          topoexec::any_input_trigger({"in"}), topoexec::lane_execution("main"),
                                          boundary(topoexec::ComponentRole::kOutputBoundary)))
      .edge(topoexec::immediate_edge("source_transform", "source.out", "transform.in"))
      .edge(topoexec::immediate_edge("transform_sink", "transform.out", "sink.in"))
      .build();
}

std::string join(const std::vector<std::string>& values) {
  std::string output;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index > 0u) {
      output += ",";
    }
    output += values[index];
  }
  return output;
}

} // namespace

int main() {
  state() = {};
  const auto components = registry();
  topoexec::RuntimeRunner runner(components);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;
  const auto result = runner.run(graph(), options);
  if (!result.ok) {
    for (const auto& error : result.runtime_errors) {
      std::cerr << "error: " << error.message << "\n";
    }
    return 1;
  }
  if (state().sink_payload != "hello:built") {
    std::cerr << "error: unexpected sink payload: " << state().sink_payload << "\n";
    return 2;
  }
  std::cout << "builder_sink=" << state().sink_payload << "\n";
  std::cout << "builder_order=" << join(result.ticked_components) << "\n";
  std::cout << "builder_committed=" << result.committed_publication_count << "\n";
  return 0;
}
