#include "topoexec/runtime/graph.hpp"
#include "topoexec/runtime/runtime_runner.hpp"

#include <CLI/CLI.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct LintFinding {
  std::string severity;
  std::string rule;
  std::string message;
  std::string id;
};

enum class DemoBehavior {
  kSource,
  kForward,
  kLargeSource,
  kEstimator,
  kController,
  kSink,
};

class DemoComponent : public topoexec::Component {
public:
  DemoComponent(topoexec::ComponentDescriptor descriptor, DemoBehavior behavior)
      : descriptor_(std::move(descriptor)), behavior_(behavior) {}

  topoexec::ComponentDescriptor describe() const override {
    return descriptor_;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    switch (behavior_) {
    case DemoBehavior::kSource:
      publish_or_throw(context, "out", "payload-" + std::to_string(invocation.sequence));
      return;
    case DemoBehavior::kForward:
      if (invocation.payload != nullptr) {
        publish_or_throw(context, "out", invocation.payload->text());
      }
      return;
    case DemoBehavior::kLargeSource:
      publish_or_throw(context, "out", "large-payload-placeholder-" + std::to_string(invocation.sequence));
      return;
    case DemoBehavior::kEstimator:
      if (invocation.payload != nullptr) {
        publish_or_throw(context, "estimate", "estimate:" + invocation.payload->text());
      }
      return;
    case DemoBehavior::kController:
      if (invocation.payload != nullptr) {
        publish_or_throw(context, "command", "command:" + invocation.payload->text());
        publish_or_throw(context, "correction", "correction:" + invocation.payload->text());
      }
      return;
    case DemoBehavior::kSink:
      return;
    }
  }

private:
  static void publish_or_throw(topoexec::GraphContext& context, const std::string& port, std::string payload) {
    const auto result = context.publish(port, topoexec::make_text_payload(std::move(payload)));
    if (!result.accepted) {
      throw std::runtime_error(result.reason);
    }
  }

  topoexec::ComponentDescriptor descriptor_;
  DemoBehavior behavior_;
};

topoexec::ComponentDescriptor descriptor(std::string type, topoexec::ComponentRole role,
                                         std::vector<topoexec::PortDescriptor> inputs,
                                         std::vector<topoexec::PortDescriptor> outputs) {
  topoexec::ComponentDescriptor value;
  value.type = std::move(type);
  value.name = value.type;
  value.role = role;
  value.inputs = std::move(inputs);
  value.outputs = std::move(outputs);
  return value;
}

void register_demo_component(topoexec::ComponentRegistry& registry, topoexec::ComponentDescriptor component,
                             DemoBehavior behavior) {
  const auto type = component.type;
  registry.register_component({type}, [component = std::move(component), behavior]() {
    return std::make_unique<DemoComponent>(component, behavior);
  });
}

topoexec::ComponentRegistry demo_registry() {
  topoexec::ComponentRegistry registry;
  const topoexec::PortDescriptor text_in{"in", topoexec::kTextPayloadSchema};
  const topoexec::PortDescriptor text_out{"out", topoexec::kTextPayloadSchema};

  register_demo_component(
      registry, descriptor("topoexec.boundary.Input", topoexec::ComponentRole::kInputBoundary, {}, {text_out}),
      DemoBehavior::kSource);
  register_demo_component(
      registry, descriptor("topoexec.transforms.Identity", topoexec::ComponentRole::kProcessing, {text_in}, {text_out}),
      DemoBehavior::kForward);
  register_demo_component(
      registry, descriptor("topoexec.boundary.Output", topoexec::ComponentRole::kOutputBoundary, {text_in}, {}),
      DemoBehavior::kSink);
  register_demo_component(registry,
                          descriptor("topoexec.boundary.FrameInput", topoexec::ComponentRole::kInputBoundary, {},
                                     {{"out", topoexec::kFrameViewPayloadSchema}}),
                          DemoBehavior::kLargeSource);
  register_demo_component(registry,
                          descriptor("topoexec.boundary.BinaryInput", topoexec::ComponentRole::kInputBoundary, {},
                                     {{"out", topoexec::kBinaryBlobPayloadSchema}}),
                          DemoBehavior::kLargeSource);

  register_demo_component(
      registry, descriptor("topoexec.example.Sensor", topoexec::ComponentRole::kInputBoundary, {}, {text_out}),
      DemoBehavior::kSource);
  register_demo_component(
      registry,
      descriptor("topoexec.example.Estimator", topoexec::ComponentRole::kProcessing,
                 {{"sensor", topoexec::kTextPayloadSchema}, {"correction", topoexec::kTextPayloadSchema}},
                 {{"estimate", topoexec::kTextPayloadSchema}}),
      DemoBehavior::kEstimator);
  register_demo_component(
      registry,
      descriptor("topoexec.example.Controller", topoexec::ComponentRole::kProcessing,
                 {{"estimate", topoexec::kTextPayloadSchema}},
                 {{"command", topoexec::kTextPayloadSchema}, {"correction", topoexec::kTextPayloadSchema}}),
      DemoBehavior::kController);
  register_demo_component(registry,
                          descriptor("topoexec.example.Actuator", topoexec::ComponentRole::kOutputBoundary,
                                     {{"command", topoexec::kTextPayloadSchema}}, {}),
                          DemoBehavior::kSink);
  return registry;
}

int print_validation(const topoexec::GraphValidationResult& result, const std::string& format) {
  if (format == "json") {
    nlohmann::json value;
    value["ok"] = result.ok;
    value["errors"] = result.errors;
    value["region_order"] = result.compiled_plan.region_order;
    std::cout << value.dump(2) << "\n";
  } else {
    std::cout << (result.ok ? "ok" : "error") << "\n";
    for (const auto& error : result.errors) {
      std::cout << "- " << error << "\n";
    }
  }
  return result.ok ? 0 : 1;
}

topoexec::GraphValidationResult load_and_validate(const std::string& path, topoexec::GraphSpec& graph) {
  graph = topoexec::load_graph_file(path);
  return topoexec::validate_graph_structure(graph);
}

std::string component_id_from_endpoint(const std::string& endpoint) {
  const auto dot = endpoint.find('.');
  if (dot == std::string::npos) {
    return endpoint;
  }
  return endpoint.substr(0, dot);
}

std::string port_name_from_endpoint(const std::string& endpoint) {
  const auto dot = endpoint.find('.');
  if (dot == std::string::npos || dot + 1 >= endpoint.size()) {
    return {};
  }
  return endpoint.substr(dot + 1);
}

const topoexec::PortDescriptor* find_port(const std::vector<topoexec::PortDescriptor>& ports, const std::string& name) {
  const auto found = std::find_if(ports.begin(), ports.end(), [&](const auto& port) { return port.name == name; });
  if (found == ports.end()) {
    return nullptr;
  }
  return &*found;
}

bool is_large_payload_schema(const std::string& schema) {
  return schema == topoexec::kFrameViewPayloadSchema || schema == topoexec::kBinaryBlobPayloadSchema;
}

topoexec::RuntimeRunnerResult run_graph_file(const std::string& path, std::size_t steps, std::uint64_t duration_ms,
                                             bool until_idle = false) {
  const auto registry = demo_registry();
  topoexec::RuntimeRunner runner(registry);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = steps;
  options.run_duration_ms = duration_ms;
  options.run_until_idle = until_idle;
  return runner.run(topoexec::load_graph_file(path), options);
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

nlohmann::json runtime_metrics_json(const topoexec::RuntimeRunnerResult& result) {
  nlohmann::json metrics = nlohmann::json::array();
  for (const auto& sample : result.runtime_metrics) {
    metrics.push_back({{"name", sample.name},
                       {"value", sample.value},
                       {"component_id", sample.component_id},
                       {"lane", sample.lane},
                       {"channel_id", sample.channel_id},
                       {"tags", sample.tags}});
  }
  return metrics;
}

nlohmann::json runtime_trace_json(const topoexec::RuntimeRunnerResult& result) {
  nlohmann::json events = nlohmann::json::array();
  for (const auto& event : result.trace) {
    events.push_back({{"name", event.name},
                      {"trace_id", event.trace_id},
                      {"start_offset_ns", event.start_offset_ns},
                      {"duration_ns", event.duration_ns},
                      {"attributes", event.attributes}});
  }
  return events;
}

nlohmann::json chrome_trace_json(const topoexec::RuntimeRunnerResult& result) {
  nlohmann::json events = nlohmann::json::array();
  for (const auto& event : result.trace) {
    nlohmann::json chrome_event;
    chrome_event["name"] = event.name;
    chrome_event["cat"] = "topoexec";
    chrome_event["ph"] = "X";
    chrome_event["ts"] = static_cast<double>(event.start_offset_ns) / 1000.0;
    chrome_event["dur"] = static_cast<double>(event.duration_ns) / 1000.0;
    chrome_event["pid"] = 1;
    chrome_event["tid"] = 0;
    chrome_event["args"] = event.attributes;
    chrome_event["args"]["trace_id"] = event.trace_id;
    events.push_back(std::move(chrome_event));
  }
  return {{"traceEvents", events}, {"displayTimeUnit", "ns"}};
}

nlohmann::json runner_result_json(const topoexec::RuntimeRunnerResult& result) {
  return {{"ok", result.ok},
          {"errors", result.errors},
          {"graph_name", result.graph_name},
          {"component_count", result.component_count},
          {"channel_count", result.channel_count},
          {"tick_calls", result.tick_calls},
          {"stop_reason", topoexec::to_string(result.scheduler_stop_reason)},
          {"ticked_components", result.ticked_components},
          {"channel_publish_count", result.channel_publish_count},
          {"channel_delivery_count", result.channel_delivery_count},
          {"channel_drop_count", result.channel_drop_count},
          {"payload_copy_count", result.payload_copy_count},
          {"staged_publication_count", result.staged_publication_count},
          {"committed_publication_count", result.committed_publication_count},
          {"delayed_publication_count", result.delayed_publication_count},
          {"state_publication_count", result.state_publication_count},
          {"async_publication_count", result.async_publication_count},
          {"failed_publication_commit_count", result.failed_publication_commit_count},
          {"trace_event_count", result.trace_event_count},
          {"trace_events", result.trace_events},
          {"trace", runtime_trace_json(result)},
          {"loop_iteration_count", result.loop_iteration_count},
          {"loop_converged_count", result.loop_converged_count},
          {"loop_budget_overrun_count", result.loop_budget_overrun_count},
          {"loop_max_iteration_hit_count", result.loop_max_iteration_hit_count},
          {"metrics", runtime_metrics_json(result)}};
}

int print_runner_result(const topoexec::RuntimeRunnerResult& result, const std::string& format) {
  if (format == "json") {
    std::cout << runner_result_json(result).dump(2) << "\n";
  } else {
    std::cout << (result.ok ? "ok" : "error") << "\n";
    std::cout << "graph: " << result.graph_name << "\n";
    std::cout << "ticks: " << result.tick_calls << "\n";
    std::cout << "stop_reason: " << topoexec::to_string(result.scheduler_stop_reason) << "\n";
    std::cout << "order: " << join(result.ticked_components) << "\n";
    std::cout << "channel_publish_count: " << result.channel_publish_count << "\n";
    std::cout << "channel_delivery_count: " << result.channel_delivery_count << "\n";
    std::cout << "channel_drop_count: " << result.channel_drop_count << "\n";
    std::cout << "runtime_publication_committed: " << result.committed_publication_count << "\n";
    for (const auto& error : result.errors) {
      std::cout << "- " << error << "\n";
    }
  }
  return result.ok ? 0 : 1;
}

int print_metrics_result(const topoexec::RuntimeRunnerResult& result, const std::string& format) {
  if (format == "json") {
    std::cout << runner_result_json(result).dump(2) << "\n";
  } else {
    std::cout << (result.ok ? "ok" : "error") << "\n";
    for (const auto& sample : result.runtime_metrics) {
      std::cout << sample.name << "=" << sample.value << "\n";
    }
    std::cout << "channel_publish_count=" << result.channel_publish_count << "\n";
    std::cout << "channel_delivery_count=" << result.channel_delivery_count << "\n";
    std::cout << "channel_drop_count=" << result.channel_drop_count << "\n";
  }
  return result.ok ? 0 : 1;
}

int print_trace_result(const topoexec::RuntimeRunnerResult& result, const std::string& format) {
  if (format == "chrome") {
    std::cout << chrome_trace_json(result).dump(2) << "\n";
  } else if (format == "json") {
    nlohmann::json value;
    value["ok"] = result.ok;
    value["errors"] = result.errors;
    value["graph_name"] = result.graph_name;
    value["trace_event_count"] = result.trace_event_count;
    value["trace_events"] = result.trace_events;
    value["trace"] = runtime_trace_json(result);
    std::cout << value.dump(2) << "\n";
  } else {
    std::cout << (result.ok ? "ok" : "error") << "\n";
    for (const auto& event : result.trace_events) {
      std::cout << event << "\n";
    }
    for (const auto& error : result.errors) {
      std::cout << "- " << error << "\n";
    }
  }
  return result.ok ? 0 : 1;
}

std::vector<LintFinding> lint_graph(const topoexec::GraphSpec& graph,
                                    const topoexec::ComponentRegistry* registry = nullptr) {
  std::vector<LintFinding> findings;
  const auto validation = topoexec::validate_graph_structure(graph);
  for (const auto& error : validation.errors) {
    findings.push_back({"error", "validation", error, graph.name});
  }

  std::map<std::string, std::string> component_lane;
  for (const auto& component : graph.components) {
    component_lane[component.id] = component.execution.lane;
  }
  std::map<std::string, topoexec::LaneSpec> lanes;
  for (const auto& lane : graph.lanes) {
    lanes[lane.id] = lane;
  }
  std::map<std::string, topoexec::ComponentDescriptor> descriptors;
  if (registry != nullptr) {
    for (const auto& component : graph.components) {
      if (!registry->contains(component.type)) {
        continue;
      }
      try {
        descriptors[component.id] = registry->create(component.type)->describe();
      } catch (const std::exception&) {
      }
    }
  }

  std::map<std::string, int> state_writers;
  for (const auto& edge : graph.edges) {
    if (edge.policy.capacity > 1024) {
      findings.push_back(
          {"warning", "suspicious_capacity", "edge capacity is unusually high for an in-process runtime", edge.id});
    }
    if (edge.policy.overflow == "block") {
      const auto target_component = component_id_from_endpoint(edge.to);
      const auto lane_id = component_lane[target_component];
      const auto lane = lanes.find(lane_id);
      if (lane != lanes.end() && lane->second.type == "event_loop") {
        findings.push_back({"warning", "blocking_overflow_event_loop",
                            "blocking overflow can stall a single-thread event_loop", edge.id});
      }
    }
    if (edge.kind == topoexec::EdgeKind::kAsync && edge.policy.max_inflight <= 0 && edge.policy.capacity <= 1) {
      findings.push_back(
          {"info", "async_capacity", "async edge uses capacity <= 1; excess completions may be dropped", edge.id});
    }
    if (edge.kind == topoexec::EdgeKind::kDelay) {
      findings.push_back(
          {"info", "delay_epoch_boundary", "delay edge becomes visible at the next epoch boundary", edge.id});
    }
    if (edge.kind == topoexec::EdgeKind::kState) {
      ++state_writers[edge.to];
    }
    if (edge.policy.copy_policy == "copy") {
      const auto source_component = component_id_from_endpoint(edge.from);
      const auto source_port = port_name_from_endpoint(edge.from);
      const auto descriptor = descriptors.find(source_component);
      if (descriptor != descriptors.end()) {
        const auto* port = find_port(descriptor->second.outputs, source_port);
        if (port != nullptr && is_large_payload_schema(port->schema)) {
          findings.push_back({"warning", "large_payload_copy",
                              "large payload output uses copy policy; use shared_view or loaned_view", edge.id});
        }
      }
    }
  }
  for (const auto& [target, count] : state_writers) {
    if (count > 1) {
      findings.push_back({"warning", "state_multiple_writers", "state target has multiple writers: " + target, target});
    }
  }
  for (const auto& component : graph.components) {
    const auto lane = lanes.find(component.execution.lane);
    if (lane == lanes.end() || lane->second.hz <= 0.0 || component.execution.budget_ms <= 0) {
      continue;
    }
    const auto period_ms = 1000.0 / lane->second.hz;
    if (static_cast<double>(component.execution.budget_ms) > period_ms) {
      findings.push_back({"warning", "budget_exceeds_lane_period",
                          "component budget exceeds lane period: " + component.id, component.id});
    }
  }
  return findings;
}

int print_lint_findings(const std::vector<LintFinding>& findings, const std::string& format) {
  if (format == "json") {
    nlohmann::json value;
    value["ok"] =
        std::none_of(findings.begin(), findings.end(), [](const auto& finding) { return finding.severity == "error"; });
    value["findings"] = nlohmann::json::array();
    for (const auto& finding : findings) {
      value["findings"].push_back(
          {{"severity", finding.severity}, {"rule", finding.rule}, {"message", finding.message}, {"id", finding.id}});
    }
    std::cout << value.dump(2) << "\n";
  } else {
    if (findings.empty()) {
      std::cout << "ok\n";
    }
    for (const auto& finding : findings) {
      std::cout << finding.severity << " " << finding.rule << " " << finding.id << ": " << finding.message << "\n";
    }
  }
  return std::any_of(findings.begin(), findings.end(), [](const auto& finding) { return finding.severity == "error"; })
             ? 1
             : 0;
}

int print_explain(const topoexec::GraphSpec& graph, const topoexec::GraphValidationResult& validation,
                  const std::string& format) {
  if (format == "json") {
    nlohmann::json value;
    value["ok"] = validation.ok;
    value["errors"] = validation.errors;
    value["graph_name"] = graph.name;
    value["semantics"] = {
        {"publish", "GraphContext::publish stages output; scheduler/runtime commit decides visibility"},
        {"immediate", "visible during the current epoch according to compiled region order"},
        {"delay", "visible at the next epoch boundary"},
        {"state", "staged during the current epoch and committed at the next epoch boundary"},
        {"async", "deferred to a later epoch; no recursive downstream execution"},
    };
    value["region_order"] = validation.compiled_plan.region_order;
    std::cout << value.dump(2) << "\n";
  } else {
    std::cout << "graph: " << graph.name << "\n";
    std::cout << "publish: staged by runtime; never directly executes downstream components\n";
    std::cout << "immediate: current epoch by compiled region order\n";
    std::cout << "delay/state/async: deferred to next epoch boundary\n";
    std::cout << topoexec::graph_plan_text(graph, validation.compiled_plan);
  }
  return validation.ok ? 0 : 1;
}

nlohmann::json region_json(const topoexec::GraphCompiledPlan& plan) {
  nlohmann::json regions = nlohmann::json::array();
  for (const auto& region : plan.regions) {
    regions.push_back({{"id", region.id},
                       {"kind", topoexec::to_string(region.kind)},
                       {"components", region.components},
                       {"incoming_regions", region.incoming_regions},
                       {"outgoing_regions", region.outgoing_regions}});
  }
  return regions;
}

int print_diff_plan(const std::string& left_path, const std::string& right_path, const std::string& format) {
  topoexec::GraphSpec left_graph;
  topoexec::GraphSpec right_graph;
  const auto left = load_and_validate(left_path, left_graph);
  const auto right = load_and_validate(right_path, right_graph);
  const bool same_region_order = left.compiled_plan.region_order == right.compiled_plan.region_order;
  const bool same_component_regions = left.compiled_plan.component_region == right.compiled_plan.component_region;
  const bool ok = left.ok && right.ok;
  if (format == "json") {
    nlohmann::json value;
    value["ok"] = ok;
    value["left_errors"] = left.errors;
    value["right_errors"] = right.errors;
    value["same_region_order"] = same_region_order;
    value["same_component_regions"] = same_component_regions;
    value["left_region_order"] = left.compiled_plan.region_order;
    value["right_region_order"] = right.compiled_plan.region_order;
    value["left_regions"] = region_json(left.compiled_plan);
    value["right_regions"] = region_json(right.compiled_plan);
    std::cout << value.dump(2) << "\n";
  } else {
    std::cout << (ok ? "ok" : "error") << "\n";
    std::cout << "same_region_order: " << (same_region_order ? "true" : "false") << "\n";
    std::cout << "same_component_regions: " << (same_component_regions ? "true" : "false") << "\n";
    std::cout << "left_order: " << join(left.compiled_plan.region_order) << "\n";
    std::cout << "right_order: " << join(right.compiled_plan.region_order) << "\n";
  }
  return ok ? 0 : 1;
}

int print_bench_result(const std::string& path, std::size_t steps, std::size_t runs, const std::string& format) {
  const auto started = std::chrono::steady_clock::now();
  std::size_t ok_runs = 0;
  std::size_t tick_calls = 0;
  std::vector<std::string> errors;
  for (std::size_t run_index = 0; run_index < runs; ++run_index) {
    const auto result = run_graph_file(path, steps, 0);
    if (result.ok) {
      ++ok_runs;
      tick_calls += result.tick_calls;
    } else {
      errors.insert(errors.end(), result.errors.begin(), result.errors.end());
    }
  }
  const auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
  if (format == "json") {
    nlohmann::json value;
    value["ok"] = errors.empty();
    value["runs"] = runs;
    value["ok_runs"] = ok_runs;
    value["steps"] = steps;
    value["tick_calls"] = tick_calls;
    value["elapsed_ms"] = elapsed_ms;
    value["errors"] = errors;
    std::cout << value.dump(2) << "\n";
  } else {
    std::cout << (errors.empty() ? "ok" : "error") << "\n";
    std::cout << "runs: " << runs << "\n";
    std::cout << "ok_runs: " << ok_runs << "\n";
    std::cout << "steps: " << steps << "\n";
    std::cout << "tick_calls: " << tick_calls << "\n";
    std::cout << "elapsed_ms: " << elapsed_ms << "\n";
    for (const auto& error : errors) {
      std::cout << "- " << error << "\n";
    }
  }
  return errors.empty() ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
  CLI::App app{"TopoExec graph tooling"};
  app.require_subcommand(1);

  auto* graph_cmd = app.add_subcommand("graph", "Graph inspection commands");
  graph_cmd->require_subcommand(1);

  std::string validate_path;
  std::string validate_format{"text"};
  auto* validate = graph_cmd->add_subcommand("validate", "Validate a TopoExec graph");
  validate->add_option("file", validate_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  validate->add_option("--format", validate_format, "Output format")->check(CLI::IsMember({"text", "json"}));

  std::string plan_path;
  std::string plan_format{"text"};
  auto* plan = graph_cmd->add_subcommand("plan", "Print compiled graph plan");
  plan->add_option("file", plan_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  plan->add_option("--format", plan_format, "Output format")->check(CLI::IsMember({"text", "json"}));

  std::string render_path;
  std::string render_format{"mermaid"};
  auto* render = graph_cmd->add_subcommand("render", "Render a graph");
  render->add_option("file", render_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  render->add_option("--format", render_format, "Output format")->check(CLI::IsMember({"mermaid", "text", "json"}));

  std::string run_path;
  std::string run_format{"text"};
  std::size_t run_steps{1};
  std::uint64_t run_duration_ms{0};
  bool run_until_idle{false};
  auto* run = graph_cmd->add_subcommand("run", "Run a graph with built-in demo components");
  run->add_option("file", run_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  run->add_option("--steps", run_steps, "Bounded event-loop steps");
  run->add_option("--duration-ms", run_duration_ms, "Optional duration bound in milliseconds");
  run->add_flag("--until-idle", run_until_idle, "Stop early after an event-loop iteration executes no components");
  run->add_option("--format", run_format, "Output format")->check(CLI::IsMember({"text", "json"}));

  std::string metrics_path;
  std::string metrics_format{"text"};
  std::size_t metrics_steps{1};
  std::uint64_t metrics_duration_ms{0};
  bool metrics_until_idle{false};
  auto* metrics = graph_cmd->add_subcommand("metrics", "Run a graph and print runtime metrics");
  metrics->add_option("file", metrics_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  metrics->add_option("--steps", metrics_steps, "Bounded event-loop steps");
  metrics->add_option("--duration-ms", metrics_duration_ms, "Optional duration bound in milliseconds");
  metrics->add_flag("--until-idle", metrics_until_idle,
                    "Stop early after an event-loop iteration executes no components");
  metrics->add_option("--format", metrics_format, "Output format")->check(CLI::IsMember({"text", "json"}));

  std::string trace_path;
  std::string trace_format{"text"};
  std::size_t trace_steps{1};
  std::uint64_t trace_duration_ms{0};
  bool trace_until_idle{false};
  auto* trace = graph_cmd->add_subcommand("trace", "Run a graph and print runtime trace events");
  trace->add_option("file", trace_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  trace->add_option("--steps", trace_steps, "Bounded event-loop steps");
  trace->add_option("--duration-ms", trace_duration_ms, "Optional duration bound in milliseconds");
  trace->add_flag("--until-idle", trace_until_idle, "Stop early after an event-loop iteration executes no components");
  trace->add_option("--format", trace_format, "Output format")->check(CLI::IsMember({"text", "json", "chrome"}));

  std::string lint_path;
  std::string lint_format{"text"};
  auto* lint = graph_cmd->add_subcommand("lint", "Lint a graph for suspicious runtime contracts");
  lint->add_option("file", lint_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  lint->add_option("--format", lint_format, "Output format")->check(CLI::IsMember({"text", "json"}));

  std::string explain_path;
  std::string explain_format{"text"};
  auto* explain = graph_cmd->add_subcommand("explain", "Explain graph runtime semantics and compiled plan");
  explain->add_option("file", explain_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  explain->add_option("--format", explain_format, "Output format")->check(CLI::IsMember({"text", "json"}));

  std::string diff_left_path;
  std::string diff_right_path;
  std::string diff_format{"text"};
  auto* diff_plan = graph_cmd->add_subcommand("diff-plan", "Compare two compiled graph plans");
  diff_plan->add_option("left", diff_left_path, "Left graph YAML file")->required()->check(CLI::ExistingFile);
  diff_plan->add_option("right", diff_right_path, "Right graph YAML file")->required()->check(CLI::ExistingFile);
  diff_plan->add_option("--format", diff_format, "Output format")->check(CLI::IsMember({"text", "json"}));

  std::string bench_path;
  std::string bench_format{"text"};
  std::size_t bench_steps{1};
  std::size_t bench_runs{3};
  auto* bench = graph_cmd->add_subcommand("bench", "Run a small local RuntimeRunner benchmark");
  bench->add_option("file", bench_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  bench->add_option("--steps", bench_steps, "Bounded event-loop steps per run");
  bench->add_option("--runs", bench_runs, "Number of repeated runs");
  bench->add_option("--format", bench_format, "Output format")->check(CLI::IsMember({"text", "json"}));

  try {
    app.parse(argc, argv);
    if (*validate) {
      topoexec::GraphSpec graph;
      const auto result = load_and_validate(validate_path, graph);
      return print_validation(result, validate_format);
    }
    if (*plan) {
      topoexec::GraphSpec graph;
      const auto result = load_and_validate(plan_path, graph);
      if (!result.ok) {
        return print_validation(result, plan_format == "json" ? "json" : "text");
      }
      if (plan_format == "json") {
        std::cout << topoexec::graph_plan_json(graph, result.compiled_plan) << "\n";
      } else {
        std::cout << topoexec::graph_plan_text(graph, result.compiled_plan);
      }
      return 0;
    }
    if (*render) {
      topoexec::GraphSpec graph;
      const auto result = load_and_validate(render_path, graph);
      if (!result.ok) {
        return print_validation(result, render_format == "json" ? "json" : "text");
      }
      if (render_format == "json") {
        std::cout << topoexec::graph_plan_json(graph, result.compiled_plan) << "\n";
      } else if (render_format == "text") {
        std::cout << topoexec::graph_plan_text(graph, result.compiled_plan);
      } else {
        std::cout << topoexec::graph_mermaid(graph, result.compiled_plan);
      }
      return 0;
    }
    if (*run) {
      return print_runner_result(run_graph_file(run_path, run_steps, run_duration_ms, run_until_idle), run_format);
    }
    if (*metrics) {
      return print_metrics_result(run_graph_file(metrics_path, metrics_steps, metrics_duration_ms, metrics_until_idle),
                                  metrics_format);
    }
    if (*trace) {
      return print_trace_result(run_graph_file(trace_path, trace_steps, trace_duration_ms, trace_until_idle),
                                trace_format);
    }
    if (*lint) {
      const auto registry = demo_registry();
      return print_lint_findings(lint_graph(topoexec::load_graph_file(lint_path), &registry), lint_format);
    }
    if (*explain) {
      topoexec::GraphSpec graph;
      const auto validation = load_and_validate(explain_path, graph);
      return print_explain(graph, validation, explain_format);
    }
    if (*diff_plan) {
      return print_diff_plan(diff_left_path, diff_right_path, diff_format);
    }
    if (*bench) {
      return print_bench_result(bench_path, bench_steps, bench_runs, bench_format);
    }
  } catch (const CLI::ParseError& error) {
    return app.exit(error);
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << "\n";
    return 1;
  }
  return 0;
}
