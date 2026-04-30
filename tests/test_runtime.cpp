#include "topoexec/runtime/runtime_runner.hpp"

#include <gtest/gtest.h>

namespace {

class SourceComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.Source";
    descriptor.name = "source";
    descriptor.role = topoexec::ComponentRole::kInputBoundary;
    descriptor.outputs = {{"out", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation&, topoexec::GraphContext& context) override {
    const auto result = context.publish("out", topoexec::make_text_payload("payload"));
    if (!result.accepted) {
      throw std::runtime_error(result.reason);
    }
  }
};

class EchoComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.Echo";
    descriptor.name = "echo";
    descriptor.inputs = {{"in", topoexec::kTextPayloadSchema}};
    descriptor.outputs = {{"out", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation&, topoexec::GraphContext& context) override {
    auto payload = context.inputs().peek_latest("in");
    if (payload == nullptr) {
      return;
    }
    const auto result = context.publish("out", topoexec::make_text_payload(payload->text()));
    if (!result.accepted) {
      throw std::runtime_error(result.reason);
    }
  }
};

class SinkComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.Sink";
    descriptor.name = "sink";
    descriptor.role = topoexec::ComponentRole::kOutputBoundary;
    descriptor.inputs = {{"in", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation&, topoexec::GraphContext& context) override {
    last = context.inputs().peek_latest("in");
  }

  topoexec::RuntimePayloadPtr last;
};

topoexec::ComponentRegistry registry() {
  topoexec::ComponentRegistry registry;
  registry.register_component({"topoexec.test.Source"}, []() { return std::make_unique<SourceComponent>(); });
  registry.register_component({"topoexec.test.Echo"}, []() { return std::make_unique<EchoComponent>(); });
  registry.register_component({"topoexec.test.Sink"}, []() { return std::make_unique<SinkComponent>(); });
  return registry;
}

topoexec::GraphSpec graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: runtime, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - id: source
    type: topoexec.test.Source
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: echo
    type: topoexec.test.Echo
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
  - id: sink
    type: topoexec.test.Sink
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
edges:
  - {id: source_echo, kind: immediate, from: source.out, to: echo.in, policy: {mode: latest, copy_policy: shared_view}}
  - {id: echo_sink, kind: immediate, from: echo.out, to: sink.in, policy: {mode: latest, copy_policy: shared_view}}
)");
}

}  // namespace

TEST(Runtime, StaticRegistryValidationAndDryRunPass) {
  const auto reg = registry();
  const auto spec = graph();
  const auto validation = topoexec::validate_graph(spec, reg);
  ASSERT_TRUE(validation.ok) << validation.errors.front();

  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kDryRun;
  options.tick_iterations = 2;
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_EQ(result.instantiated_components, 3u);
  EXPECT_EQ(result.tick_calls, 6u);
}

TEST(Runtime, RunModeExecutesEventRuntimeAndRoutesChannels) {
  const auto reg = registry();
  const auto spec = graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_GE(result.channel_publish_count, 2u);
  EXPECT_GE(result.channel_delivery_count, 2u);
  EXPECT_NE(std::find(result.ticked_components.begin(), result.ticked_components.end(), "sink"),
            result.ticked_components.end());
}

