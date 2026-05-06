#include "topoexec/runtime/runtime_runner.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Observation {
  std::uint64_t epoch{0};
  std::string component;
  std::string port;
  std::string payload;
};

std::vector<Observation>& observations() {
  static std::vector<Observation> values;
  return values;
}

void record(const topoexec::Invocation& invocation, const std::string& component) {
  for (const auto& [port, payload] : invocation.payloads_by_port) {
    observations().push_back(Observation{invocation.sequence, component, port, payload == nullptr ? "" : payload->text()});
  }
}

void publish_or_throw(topoexec::GraphContext& context, const std::string& port, std::string payload) {
  const auto result = context.publish(port, topoexec::make_text_payload(std::move(payload)));
  if (!result.accepted) {
    throw std::runtime_error(result.reason);
  }
}

class SensorComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.StateSensor";
    descriptor.role = topoexec::ComponentRole::kInputBoundary;
    descriptor.outputs = {{"out", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    publish_or_throw(context, "out", "sensor-" + std::to_string(invocation.sequence));
  }
};

class EstimatorComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.StateEstimator";
    descriptor.inputs = {{"sensor", topoexec::kTextPayloadSchema}, {"correction", topoexec::kTextPayloadSchema}};
    descriptor.outputs = {{"estimate", topoexec::kTextPayloadSchema}, {"state", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    record(invocation, "estimator");
    if (invocation.payload != nullptr) {
      publish_or_throw(context, "estimate", "estimate:" + invocation.payload->text());
      publish_or_throw(context, "state", "state:" + invocation.payload->text());
    }
  }
};

class ControllerComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.StateController";
    descriptor.inputs = {{"estimate", topoexec::kTextPayloadSchema}, {"state", topoexec::kTextPayloadSchema}};
    descriptor.outputs = {{"command", topoexec::kTextPayloadSchema}, {"correction", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    record(invocation, "controller");
    if (invocation.payload != nullptr) {
      publish_or_throw(context, "command", "command:" + invocation.payload->text());
      publish_or_throw(context, "correction", "correction:" + invocation.payload->text());
    }
  }
};

class ActuatorComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.StateActuator";
    descriptor.role = topoexec::ComponentRole::kOutputBoundary;
    descriptor.inputs = {{"command", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext&) override {
    record(invocation, "actuator");
  }
};

topoexec::ComponentRegistry registry() {
  topoexec::ComponentRegistry registry;
  registry.register_component({"topoexec.app.StateSensor"}, []() { return std::make_unique<SensorComponent>(); });
  registry.register_component({"topoexec.app.StateEstimator"}, []() { return std::make_unique<EstimatorComponent>(); });
  registry.register_component({"topoexec.app.StateController"}, []() { return std::make_unique<ControllerComponent>(); });
  registry.register_component({"topoexec.app.StateActuator"}, []() { return std::make_unique<ActuatorComponent>(); });
  return registry;
}

topoexec::GraphSpec graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: app_control_loop_with_state, kind: runnable}
lanes: {control: {type: fixed_rate, period_ms: 10}}
components:
  - {id: sensor, type: topoexec.app.StateSensor, event_sources: [{type: manual}], trigger_policy: {type: manual}, execution: {lane: control}}
  - {id: estimator, type: topoexec.app.StateEstimator, event_sources: [{type: message, inputs: [sensor, correction]}], trigger_policy: {type: any_input, inputs: [sensor, correction]}, execution: {lane: control}}
  - {id: controller, type: topoexec.app.StateController, event_sources: [{type: message, inputs: [estimate, state]}], trigger_policy: {type: any_input, inputs: [estimate, state]}, execution: {lane: control}}
  - {id: actuator, type: topoexec.app.StateActuator, event_sources: [{type: message, inputs: [command]}], trigger_policy: {type: any_input, inputs: [command]}, execution: {lane: control}}
edges:
  - {id: sensor_estimator, kind: immediate, from: sensor.out, to: estimator.sensor, policy: {mode: latest, copy_policy: shared_view}}
  - {id: estimator_controller, kind: immediate, from: estimator.estimate, to: controller.estimate, policy: {mode: latest, copy_policy: shared_view}}
  - {id: estimator_controller_state, kind: state, from: estimator.state, to: controller.state, policy: {mode: latest, copy_policy: shared_view}}
  - {id: controller_actuator, kind: immediate, from: controller.command, to: actuator.command, policy: {mode: latest, copy_policy: shared_view}}
  - {id: controller_estimator_delay, kind: delay, from: controller.correction, to: estimator.correction, policy: {mode: queue, capacity: 4, overflow: drop_oldest, copy_policy: shared_view}}
)");
}

bool saw_state_snapshot_in_second_epoch() {
  for (const auto& observation : observations()) {
    if (observation.epoch == 1 && observation.component == "controller" && observation.port == "state") {
      return false;
    }
    if (observation.epoch == 2 && observation.component == "controller" && observation.port == "state" &&
        observation.payload == "state:sensor-1") {
      return true;
    }
  }
  return false;
}

bool saw_delayed_correction_in_second_epoch() {
  for (const auto& observation : observations()) {
    if (observation.epoch == 1 && observation.component == "estimator" && observation.port == "correction") {
      return false;
    }
    if (observation.epoch == 2 && observation.component == "estimator" && observation.port == "correction" &&
        observation.payload == "correction:estimate:sensor-1") {
      return true;
    }
  }
  return false;
}

bool has_fixed_rate_trace(const topoexec::RuntimeRunnerResult& result) {
  for (const auto& event : result.trace) {
    if (event.name == "fixed_rate_tick_begin") {
      return true;
    }
  }
  return false;
}

} // namespace

int main() {
  observations().clear();
  const auto components = registry();
  topoexec::RuntimeRunner runner(components);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;
  const auto result = runner.run(graph(), options);
  if (!result.ok) {
    for (const auto& error : result.errors) {
      std::cerr << "error: " << error << "\n";
    }
    return 1;
  }
  if (!saw_state_snapshot_in_second_epoch()) {
    std::cerr << "error: state snapshot was not observed in epoch 2 only\n";
    return 2;
  }
  if (!saw_delayed_correction_in_second_epoch()) {
    std::cerr << "error: delayed correction was not observed in epoch 2 only\n";
    return 3;
  }
  if (!has_fixed_rate_trace(result)) {
    std::cerr << "error: fixed-rate lane did not emit trace evidence\n";
    return 4;
  }

  std::cout << "state_snapshot_epoch=2\n";
  std::cout << "delayed_feedback_epoch=2\n";
  std::cout << "fixed_rate_trace=true\n";
  std::cout << "state_publication_count=" << result.state_publication_count << "\n";
  return 0;
}
