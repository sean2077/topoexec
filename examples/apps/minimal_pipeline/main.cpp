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
    descriptor.type = "topoexec.app.Source";
    descriptor.name = "source";
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
    descriptor.type = "topoexec.app.Transform";
    descriptor.name = "transform";
    descriptor.inputs = {{"in", topoexec::kTextPayloadSchema}};
    descriptor.outputs = {{"out", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    if (invocation.payload == nullptr) {
      return;
    }
    const auto published =
        context.publish("out", topoexec::make_text_payload(invocation.payload->text() + ":transformed"));
    if (!published.accepted) {
      throw std::runtime_error(published.reason);
    }
  }
};

class SinkComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.Sink";
    descriptor.name = "sink";
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
  registry.register_component({"topoexec.app.Source"}, []() { return std::make_unique<SourceComponent>(); });
  registry.register_component({"topoexec.app.Transform"}, []() { return std::make_unique<TransformComponent>(); });
  registry.register_component({"topoexec.app.Sink"}, []() { return std::make_unique<SinkComponent>(); });
  return registry;
}

topoexec::GraphSpec graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: app_minimal_pipeline, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - id: source
    type: topoexec.app.Source
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: transform
    type: topoexec.app.Transform
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
  - id: sink
    type: topoexec.app.Sink
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
edges:
  - {id: source_transform, kind: immediate, from: source.out, to: transform.in, policy: {mode: latest, copy_policy: shared_view}}
  - {id: transform_sink, kind: immediate, from: transform.out, to: sink.in, policy: {mode: latest, copy_policy: shared_view}}
)");
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
  const auto reg = registry();
  topoexec::RuntimeRunner runner(reg);
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
  if (state().sink_payload != "hello:transformed") {
    std::cerr << "error: unexpected sink payload: " << state().sink_payload << "\n";
    return 2;
  }
  std::cout << "sink=" << state().sink_payload << "\n";
  std::cout << "order=" << join(result.ticked_components) << "\n";
  std::cout << "channel_publish_count=" << result.channel_publish_count << "\n";
  std::cout << "channel_delivery_count=" << result.channel_delivery_count << "\n";
  std::cout << "runtime_publication_committed=" << result.committed_publication_count << "\n";
  return 0;
}
