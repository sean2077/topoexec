#include "topoexec/runtime/graph.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <yaml-cpp/yaml.h>

namespace topoexec {
namespace {

std::optional<EdgeKind> parse_edge_kind(const std::string& kind) {
  if (kind == "immediate") {
    return EdgeKind::kImmediate;
  }
  if (kind == "delay") {
    return EdgeKind::kDelay;
  }
  if (kind == "state") {
    return EdgeKind::kState;
  }
  if (kind == "async") {
    return EdgeKind::kAsync;
  }
  return std::nullopt;
}
void require_map(const YAML::Node& node, const std::string& context) {
  if (!node || node.IsNull() || !node.IsMap()) {
    throw std::invalid_argument(context + " must be a mapping");
  }
}

void require_sequence(const YAML::Node& node, const std::string& context) {
  if (!node || node.IsNull() || !node.IsSequence()) {
    throw std::invalid_argument(context + " must be a sequence");
  }
}

YAML::Node require_node(const YAML::Node& node, const char* key, const std::string& context) {
  const auto child = node[key];
  if (!child || child.IsNull()) {
    throw std::invalid_argument(context + "." + key + " is required");
  }
  return child;
}

std::string require_string(const YAML::Node& node, const char* key, const std::string& context) {
  return require_node(node, key, context).as<std::string>();
}

std::string optional_string(const YAML::Node& node, const char* key, std::string fallback = {}) {
  const auto child = node[key];
  if (!child || child.IsNull()) {
    return fallback;
  }
  return child.as<std::string>();
}

int optional_int(const YAML::Node& node, const char* key, int fallback = 0) {
  const auto child = node[key];
  if (!child || child.IsNull()) {
    return fallback;
  }
  return child.as<int>();
}

double optional_double(const YAML::Node& node, const char* key, double fallback = 0.0) {
  const auto child = node[key];
  if (!child || child.IsNull()) {
    return fallback;
  }
  return child.as<double>();
}

bool optional_bool(const YAML::Node& node, const char* key, bool fallback = false) {
  const auto child = node[key];
  if (!child || child.IsNull()) {
    return fallback;
  }
  return child.as<bool>();
}

std::vector<std::string> optional_string_vector(const YAML::Node& node, const char* key, const std::string& context) {
  const auto child = node[key];
  if (!child || child.IsNull()) {
    return {};
  }
  require_sequence(child, context + "." + key);
  std::vector<std::string> values;
  values.reserve(child.size());
  for (const auto& item : child) {
    values.push_back(item.as<std::string>());
  }
  return values;
}

std::vector<int> optional_int_vector(const YAML::Node& node, const char* key, const std::string& context) {
  const auto child = node[key];
  if (!child || child.IsNull()) {
    return {};
  }
  require_sequence(child, context + "." + key);
  std::vector<int> values;
  values.reserve(child.size());
  for (const auto& item : child) {
    values.push_back(item.as<int>());
  }
  return values;
}

void reject_unknown_fields(const YAML::Node& node, const std::string& context, const std::set<std::string>& allowed) {
  if (!node || node.IsNull()) {
    return;
  }
  require_map(node, context);
  for (const auto& item : node) {
    const auto key = item.first.as<std::string>();
    if (allowed.count(key) == 0u) {
      throw std::invalid_argument("unexpected field " + context + "." + key);
    }
  }
}

ConfigView read_config(const YAML::Node& component_node, const std::string& component_id) {
  ConfigView view;
  const auto config = component_node["config"];
  if (!config || config.IsNull()) {
    return view;
  }
  require_map(config, "components." + component_id + ".config");
  for (const auto& item : config) {
    const auto key = item.first.as<std::string>();
    if (item.second.IsMap() || item.second.IsSequence()) {
      std::ostringstream out;
      out << item.second;
      view.values[key] = out.str();
      view.nested_values.insert(key);
    } else {
      view.values[key] = item.second.as<std::string>();
    }
  }
  return view;
}

EventSourceSpec read_event_source(const YAML::Node& source_node, const std::string& context) {
  require_map(source_node, context);
  reject_unknown_fields(source_node, context, {"id", "type", "inputs", "input", "period_ms"});
  EventSourceSpec source;
  source.id = optional_string(source_node, "id");
  source.type = optional_string(source_node, "type", "manual");
  source.inputs = optional_string_vector(source_node, "inputs", context);
  source.input = optional_string(source_node, "input");
  source.period_ms = optional_int(source_node, "period_ms");
  if (!source.input.empty() && source.inputs.empty()) {
    source.inputs = {source.input};
  }
  return source;
}

std::vector<EventSourceSpec> read_event_sources(const YAML::Node& component_node, const std::string& component_id) {
  const auto event_sources = component_node["event_sources"];
  if (!event_sources || event_sources.IsNull()) {
    return {EventSourceSpec{}};
  }
  require_sequence(event_sources, "components." + component_id + ".event_sources");
  std::vector<EventSourceSpec> values;
  values.reserve(event_sources.size());
  for (std::size_t index = 0; index < event_sources.size(); ++index) {
    values.push_back(read_event_source(event_sources[index],
                                       "components." + component_id + ".event_sources[" + std::to_string(index) + "]"));
  }
  return values;
}

TriggerPolicySpec read_trigger_policy(const YAML::Node& component_node, const std::string& component_id) {
  const auto policy_node = component_node["trigger_policy"];
  if (!policy_node || policy_node.IsNull()) {
    return {};
  }
  require_map(policy_node, "components." + component_id + ".trigger_policy");
  reject_unknown_fields(policy_node, "components." + component_id + ".trigger_policy",
                        {"type", "inputs", "input", "batch_size", "batch_window_ms", "sync_slop_ms", "min_interval_ms",
                         "max_latency_ms", "coalesce"});
  TriggerPolicySpec policy;
  policy.type = optional_string(policy_node, "type", "manual");
  policy.inputs = optional_string_vector(policy_node, "inputs", "components." + component_id + ".trigger_policy");
  policy.input = optional_string(policy_node, "input");
  if (!policy.input.empty() && policy.inputs.empty()) {
    policy.inputs = {policy.input};
  }
  policy.batch_size = optional_int(policy_node, "batch_size");
  policy.batch_window_ms = optional_int(policy_node, "batch_window_ms");
  policy.sync_slop_ms = optional_int(policy_node, "sync_slop_ms");
  policy.min_interval_ms = optional_int(policy_node, "min_interval_ms");
  policy.max_latency_ms = optional_int(policy_node, "max_latency_ms");
  policy.coalesce = optional_bool(policy_node, "coalesce");
  return policy;
}

ExecutionSpec read_execution_spec(const YAML::Node& component_node, const std::string& component_id) {
  const auto execution_node = require_node(component_node, "execution", "components." + component_id);
  require_map(execution_node, "components." + component_id + ".execution");
  reject_unknown_fields(execution_node, "components." + component_id + ".execution",
                        {"lane", "reentrant", "priority", "budget_ms"});
  ExecutionSpec spec;
  spec.lane = require_string(execution_node, "lane", "components." + component_id + ".execution");
  spec.reentrant = optional_bool(execution_node, "reentrant");
  spec.priority = optional_string(execution_node, "priority", spec.priority);
  spec.budget_ms = optional_int(execution_node, "budget_ms");
  return spec;
}

BoundaryDescriptor read_boundary_descriptor(const YAML::Node& component_node, const std::string& component_id) {
  BoundaryDescriptor boundary;
  const auto boundary_node = component_node["boundary"];
  if (!boundary_node || boundary_node.IsNull()) {
    return boundary;
  }
  require_map(boundary_node, "components." + component_id + ".boundary");
  reject_unknown_fields(boundary_node, "components." + component_id + ".boundary", {"role", "descriptor"});
  const auto role = parse_component_role(optional_string(boundary_node, "role", "processing"));
  if (!role.has_value()) {
    throw std::invalid_argument("components." + component_id + ".boundary.role has unsupported value");
  }
  boundary.role = *role;
  boundary.descriptor = optional_string(boundary_node, "descriptor");
  return boundary;
}

EdgePolicySpec read_edge_policy(const YAML::Node& edge_node, const std::string& edge_id) {
  const auto policy_node = edge_node["policy"];
  if (!policy_node || policy_node.IsNull()) {
    return {};
  }
  require_map(policy_node, "edges." + edge_id + ".policy");
  reject_unknown_fields(policy_node, "edges." + edge_id + ".policy",
                        {"mode", "capacity", "overflow", "lifespan_ms", "deadline_ms", "preserve_order", "allow_drop",
                         "emit_health_events", "timestamp_domain", "copy_policy", "owner", "readers"});
  EdgePolicySpec policy;
  policy.mode = optional_string(policy_node, "mode", policy.mode);
  policy.capacity = optional_int(policy_node, "capacity", policy.capacity);
  policy.overflow = optional_string(policy_node, "overflow", policy.overflow);
  policy.lifespan_ms = optional_int(policy_node, "lifespan_ms");
  policy.deadline_ms = optional_int(policy_node, "deadline_ms");
  policy.preserve_order = optional_bool(policy_node, "preserve_order", policy.preserve_order);
  policy.allow_drop = optional_bool(policy_node, "allow_drop", policy.allow_drop);
  policy.emit_health_events = optional_bool(policy_node, "emit_health_events", policy.emit_health_events);
  policy.timestamp_domain = optional_string(policy_node, "timestamp_domain", policy.timestamp_domain);
  policy.copy_policy = optional_string(policy_node, "copy_policy", policy.copy_policy);
  policy.owner = optional_string(policy_node, "owner", policy.owner);
  policy.readers = optional_string(policy_node, "readers", policy.readers);
  return policy;
}

LoopPolicySpec read_loop_policy(const YAML::Node& policy_node, const std::string& context) {
  require_map(policy_node, context);
  reject_unknown_fields(
      policy_node, context,
      {"type", "budget_ms", "max_iterations", "max_inflight", "drop_policy", "min_interval_ms", "convergence"});
  LoopPolicySpec policy;
  policy.type = require_string(policy_node, "type", context);
  policy.budget_ms = optional_int(policy_node, "budget_ms");
  policy.max_iterations = optional_int(policy_node, "max_iterations");
  policy.max_inflight = optional_int(policy_node, "max_inflight");
  policy.drop_policy = optional_string(policy_node, "drop_policy");
  policy.min_interval_ms = optional_int(policy_node, "min_interval_ms");
  policy.convergence = optional_string(policy_node, "convergence");
  return policy;
}

CompositeLoopSpec read_composite_loop(const YAML::Node& loop_node, const std::string& context) {
  require_map(loop_node, context);
  reject_unknown_fields(loop_node, context, {"id", "components", "loop_policy"});
  CompositeLoopSpec loop;
  loop.id = require_string(loop_node, "id", context);
  loop.components = optional_string_vector(loop_node, "components", context);
  if (loop.components.empty()) {
    throw std::invalid_argument(context + ".components must contain at least one component");
  }
  loop.loop_policy = read_loop_policy(require_node(loop_node, "loop_policy", context), context + ".loop_policy");
  return loop;
}

GraphSpec load_graph_node(const YAML::Node& root) {
  require_map(root, "runtime graph");
  reject_unknown_fields(root, "runtime graph",
                        {"schema_version", "graph", "components", "lanes", "edges", "composite_loops"});
  GraphSpec graph;
  graph.schema_version = require_node(root, "schema_version", "runtime graph").as<int>();

  const auto graph_node = require_node(root, "graph", "runtime graph");
  require_map(graph_node, "runtime graph.graph");
  reject_unknown_fields(graph_node, "runtime graph.graph", {"name", "kind", "clock"});
  graph.name = require_string(graph_node, "name", "runtime graph.graph");
  graph.kind = optional_string(graph_node, "kind", "runnable");
  const auto clock_node = graph_node["clock"];
  if (clock_node && !clock_node.IsNull()) {
    require_map(clock_node, "runtime graph.graph.clock");
    reject_unknown_fields(clock_node, "runtime graph.graph.clock", {"runtime_domain", "event_domain"});
    graph.clock.runtime_domain = optional_string(clock_node, "runtime_domain", graph.clock.runtime_domain);
    graph.clock.event_domain = optional_string(clock_node, "event_domain", graph.clock.event_domain);
  }

  const auto lanes_node = require_node(root, "lanes", "runtime graph");
  require_map(lanes_node, "runtime graph.lanes");
  for (const auto& item : lanes_node) {
    LaneSpec lane;
    lane.id = item.first.as<std::string>();
    require_map(item.second, "lanes." + lane.id);
    reject_unknown_fields(item.second, "lanes." + lane.id,
                          {"type", "hz", "priority", "max_callback_ms", "max_threads", "thread_name", "cpu_affinity",
                           "nice_priority", "rt_policy", "rt_priority", "isolation_intent"});
    lane.type = require_string(item.second, "type", "lanes." + lane.id);
    lane.hz = optional_double(item.second, "hz");
    lane.priority = optional_string(item.second, "priority");
    lane.max_callback_ms = optional_int(item.second, "max_callback_ms");
    lane.max_threads = optional_int(item.second, "max_threads");
    lane.thread_name = optional_string(item.second, "thread_name");
    lane.cpu_affinity = optional_int_vector(item.second, "cpu_affinity", "lanes." + lane.id);
    lane.nice_priority = optional_int(item.second, "nice_priority");
    lane.rt_policy = optional_string(item.second, "rt_policy", lane.rt_policy);
    lane.rt_priority = optional_int(item.second, "rt_priority");
    lane.isolation_intent = optional_string(item.second, "isolation_intent", lane.isolation_intent);
    graph.lanes.push_back(std::move(lane));
  }

  const auto components_node = require_node(root, "components", "runtime graph");
  require_sequence(components_node, "runtime graph.components");
  for (std::size_t index = 0; index < components_node.size(); ++index) {
    const auto component_node = components_node[index];
    require_map(component_node, "components[" + std::to_string(index) + "]");
    ComponentNodeSpec component;
    component.id = require_string(component_node, "id", "components[" + std::to_string(index) + "]");
    reject_unknown_fields(
        component_node, "components." + component.id,
        {"id", "type", "event_sources", "trigger_policy", "execution", "depends_on", "boundary", "config"});
    component.type = require_string(component_node, "type", "components." + component.id);
    component.event_sources = read_event_sources(component_node, component.id);
    component.trigger_policy = read_trigger_policy(component_node, component.id);
    component.execution = read_execution_spec(component_node, component.id);
    component.depends_on = optional_string_vector(component_node, "depends_on", "components." + component.id);
    component.boundary = read_boundary_descriptor(component_node, component.id);
    component.config = read_config(component_node, component.id);
    graph.components.push_back(std::move(component));
  }

  const auto edges_node = require_node(root, "edges", "runtime graph");
  require_sequence(edges_node, "runtime graph.edges");
  for (std::size_t index = 0; index < edges_node.size(); ++index) {
    const auto edge_node = edges_node[index];
    require_map(edge_node, "edges[" + std::to_string(index) + "]");
    EdgeSpec edge;
    edge.id = require_string(edge_node, "id", "edges[" + std::to_string(index) + "]");
    reject_unknown_fields(edge_node, "edges." + edge.id, {"id", "from", "to", "kind", "policy"});
    edge.from = require_string(edge_node, "from", "edges." + edge.id);
    edge.to = require_string(edge_node, "to", "edges." + edge.id);
    const auto kind_node = edge_node["kind"];
    if (kind_node && !kind_node.IsNull()) {
      edge.has_kind = true;
      const auto kind = kind_node.as<std::string>();
      const auto parsed = parse_edge_kind(kind);
      if (parsed.has_value()) {
        edge.kind = *parsed;
      } else {
        edge.invalid_kind = kind;
      }
    }
    edge.policy = read_edge_policy(edge_node, edge.id);
    graph.edges.push_back(std::move(edge));
  }

  const auto loops_node = root["composite_loops"];
  if (loops_node && !loops_node.IsNull()) {
    require_sequence(loops_node, "runtime graph.composite_loops");
    for (std::size_t index = 0; index < loops_node.size(); ++index) {
      graph.composite_loops.push_back(
          read_composite_loop(loops_node[index], "composite_loops[" + std::to_string(index) + "]"));
    }
  }
  return graph;
}

} // namespace

GraphSpec load_graph_text(const std::string& text) {
  return load_graph_node(YAML::Load(text));
}

GraphSpec load_graph_file(const std::string& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::invalid_argument("failed to open graph file: " + path);
  }
  std::ostringstream text;
  text << input.rdbuf();
  return load_graph_text(text.str());
}
std::string graph_plan_json(const GraphSpec& graph, const GraphCompiledPlan& plan) {
  nlohmann::json root;
  root["graph"] = graph.name;
  root["kind"] = graph.kind;
  root["schema_version"] = graph.schema_version;
  root["components"] = nlohmann::json::array();
  for (const auto& component : graph.components) {
    root["components"].push_back({{"id", component.id},
                                  {"type", component.type},
                                  {"lane", component.execution.lane},
                                  {"boundary_role", to_string(component.boundary.role)},
                                  {"boundary_descriptor", component.boundary.descriptor}});
  }
  root["edges"] = nlohmann::json::array();
  for (const auto& edge : graph.edges) {
    root["edges"].push_back({{"id", edge.id},
                             {"from", edge.from},
                             {"to", edge.to},
                             {"kind", to_string(edge.kind)},
                             {"mode", edge.policy.mode},
                             {"copy_policy", edge.policy.copy_policy}});
  }
  root["compiled_regions"] = nlohmann::json::array();
  for (const auto& region : plan.regions) {
    root["compiled_regions"].push_back({{"id", region.id},
                                        {"kind", to_string(region.kind)},
                                        {"components", region.components},
                                        {"incoming_regions", region.incoming_regions},
                                        {"outgoing_regions", region.outgoing_regions},
                                        {"loop_policy", region.loop_policy.type}});
  }
  root["region_order"] = plan.region_order;
  root["component_region"] = plan.component_region;
  return root.dump(2);
}

} // namespace topoexec
