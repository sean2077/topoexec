#include "topoexec/runtime/runtime_runner.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
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
    descriptor.type = "topoexec.app.SolverSource";
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
    descriptor.type = "topoexec.app.SolverEstimator";
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

class SlowEstimatorComponent : public EstimatorComponent {
public:
  topoexec::ComponentDescriptor describe() const override {
    auto descriptor = EstimatorComponent::describe();
    descriptor.type = "topoexec.app.SlowSolverEstimator";
    return descriptor;
  }

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EstimatorComponent::execute(invocation, context);
  }
};

class ControllerComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.SolverController";
    descriptor.inputs = {{"state", topoexec::kTextPayloadSchema}};
    descriptor.outputs = {{"command", topoexec::kTextPayloadSchema}, {"correction", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    const auto residual = context.loop_iteration.iteration_number >= 2u ? 0.05 : 0.5;
    context.report_loop_convergence(topoexec::LoopConvergenceReport{false, residual, "solver_residual"});
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
    descriptor.type = "topoexec.app.SolverSink";
    descriptor.role = topoexec::ComponentRole::kOutputBoundary;
    descriptor.inputs = {{"in", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}
  void execute(const topoexec::Invocation&, topoexec::GraphContext&) override {}
};

topoexec::ComponentRegistry registry() {
  topoexec::ComponentRegistry registry;
  registry.register_component({"topoexec.app.SolverSource"}, []() { return std::make_unique<SourceComponent>(); });
  registry.register_component({"topoexec.app.SolverEstimator"},
                              []() { return std::make_unique<EstimatorComponent>(); });
  registry.register_component({"topoexec.app.SlowSolverEstimator"},
                              []() { return std::make_unique<SlowEstimatorComponent>(); });
  registry.register_component({"topoexec.app.SolverController"},
                              []() { return std::make_unique<ControllerComponent>(); });
  registry.register_component({"topoexec.app.SolverSink"}, []() { return std::make_unique<SinkComponent>(); });
  return registry;
}

topoexec::GraphSpec solver_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: app_composite_solver, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - {id: source, type: topoexec.app.SolverSource, event_sources: [{type: manual}], trigger_policy: {type: manual}, execution: {lane: main}}
  - {id: estimator, type: topoexec.app.SolverEstimator, event_sources: [{type: message, inputs: [measurement, correction]}], trigger_policy: {type: any_input, inputs: [measurement, correction]}, execution: {lane: main}}
  - {id: controller, type: topoexec.app.SolverController, event_sources: [{type: message, inputs: [state]}], trigger_policy: {type: any_input, inputs: [state]}, execution: {lane: main}}
  - {id: sink, type: topoexec.app.SolverSink, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
edges:
  - {id: source_estimator, kind: immediate, from: source.out, to: estimator.measurement, policy: {mode: latest, copy_policy: shared_view}}
  - {id: estimator_controller, kind: immediate, from: estimator.state, to: controller.state, policy: {mode: latest, copy_policy: shared_view}}
  - {id: controller_estimator, kind: immediate, from: controller.correction, to: estimator.correction, policy: {mode: latest, copy_policy: shared_view}}
  - {id: controller_sink, kind: immediate, from: controller.command, to: sink.in, policy: {mode: latest, copy_policy: shared_view}}
composite_loops:
  - id: solver_loop
    components: [estimator, controller]
    loop_policy: {type: solver_iteration, max_iterations: 5, residual_threshold: 0.1}
)");
}

topoexec::GraphSpec budget_graph() {
  auto graph = solver_graph();
  graph.name = "app_composite_solver_budget";
  graph.components[1].type = "topoexec.app.SlowSolverEstimator";
  graph.composite_loops.front().loop_policy.convergence.clear();
  graph.composite_loops.front().loop_policy.budget_ms = 1;
  return graph;
}

topoexec::RuntimeRunnerResult run_graph(topoexec::RuntimeRunner& runner, const topoexec::GraphSpec& graph) {
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;
  return runner.run(graph, options);
}

} // namespace

int main() {
  const auto components = registry();
  topoexec::RuntimeRunner runner(components);

  const auto converged = run_graph(runner, solver_graph());
  if (!converged.ok) {
    for (const auto& error : converged.errors) {
      std::cerr << "error: " << error << "\n";
    }
    return 1;
  }
  if (converged.loop_iteration_count != 2u || converged.loop_converged_count != 1u) {
    std::cerr << "error: solver did not converge by residual threshold\n";
    return 2;
  }
  const auto residual = converged.loop_last_residual.find("solver_loop");
  if (residual == converged.loop_last_residual.end() || residual->second > 0.1) {
    std::cerr << "error: solver residual evidence was not reported\n";
    return 2;
  }

  const auto budget = run_graph(runner, budget_graph());
  if (!budget.ok) {
    for (const auto& error : budget.errors) {
      std::cerr << "error: " << error << "\n";
    }
    return 3;
  }
  if (budget.loop_budget_overrun_count != 1u || budget.loop_max_iteration_hit_count != 0u) {
    std::cerr << "error: solver budget overrun metrics were not reported\n";
    return 4;
  }

  std::cout << "converged_iteration_count=" << converged.loop_iteration_count << "\n";
  std::cout << "loop_converged_count=" << converged.loop_converged_count << "\n";
  std::cout << "solver_residual=" << residual->second << "\n";
  std::cout << "budget_overrun_count=" << budget.loop_budget_overrun_count << "\n";
  return 0;
}
