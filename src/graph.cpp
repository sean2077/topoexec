#include "topoexec/runtime/graph.hpp"

#include "topoexec/runtime/channel.hpp"
#include "topoexec/runtime/trigger_policy.hpp"

#include <algorithm>
#include <deque>
#include <fstream>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <yaml-cpp/yaml.h>

namespace topoexec {
namespace {

constexpr int kSupportedSchemaVersion = 1;

void add_error(GraphValidationResult& result, std::string error) {
  result.ok = false;
  result.errors.push_back(std::move(error));
}

void add_error(GraphCompileResult& result, std::string error) {
  result.ok = false;
  result.errors.push_back(std::move(error));
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

bool is_allowed_scheduler_type(const std::string& type) {
  return type == "fixed_rate" || type == "event_loop" || type == "thread_pool";
}

bool is_allowed_channel_type(const std::string& type) {
  return type == "latest" || type == "queue" || type == "ring_buffer" || type == "latched" || type == "barrier" ||
         type == "previous_tick";
}

bool is_latest_style_channel_type(const std::string& type) {
  return type == "latest" || type == "latched" || type == "previous_tick";
}

bool is_backpressure_drop_policy(const std::string& policy) {
  return policy == "drop_newest" || policy == "block";
}

bool is_allowed_drop_policy(const std::string& policy) {
  return policy == "overwrite" || policy == "drop_oldest" || policy == "drop_newest" || policy == "block" ||
         policy == "reject";
}

bool is_allowed_copy_policy(const std::string& policy) {
  return policy == "copy" || policy == "shared_view" || policy == "loaned_view" || policy == "move_only";
}

bool is_allowed_owner(const std::string& owner) {
  return owner == "producer" || owner == "runtime" || owner == "consumer";
}

bool is_allowed_readers(const std::string& readers) {
  return readers == "single" || readers == "multi";
}

bool is_allowed_event_source_type(const std::string& type) {
  return type == "message" || type == "timer" || type == "request" || type == "action_goal" ||
         type == "action_cancel" || type == "task_ready" || type == "future_ready" || type == "manual";
}

bool is_allowed_trigger_policy_type(const std::string& type) {
  return type == "on_event" || type == "any_input" || type == "all_inputs" || type == "time_sync" ||
         type == "batch" || type == "request" || type == "task_ready" || type == "manual";
}

bool is_allowed_loop_policy_type(const std::string& type) {
  return type == "fixed_point" || type == "transaction" || type == "coalesced_event" || type == "async_task";
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
                        {"type", "inputs", "input", "batch_size", "batch_window_ms", "sync_slop_ms",
                         "min_interval_ms", "max_latency_ms", "coalesce"});
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
                        {"mode", "capacity", "overflow", "lifespan_ms", "deadline_ms", "preserve_order",
                         "allow_drop", "emit_health_events", "timestamp_domain", "copy_policy", "owner", "readers"});
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
    reject_unknown_fields(component_node, "components." + component.id,
                          {"id", "type", "event_sources", "trigger_policy", "execution", "depends_on", "boundary",
                           "config"});
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

struct LifecycleOrderResult {
  bool ok{true};
  std::vector<std::string> errors;
  std::vector<std::size_t> order;
};

void add_error(LifecycleOrderResult& result, std::string error) {
  result.ok = false;
  result.errors.push_back(std::move(error));
}

LifecycleOrderResult compute_component_lifecycle_order(const GraphSpec& graph) {
  LifecycleOrderResult result;
  const auto count = graph.components.size();
  result.order.reserve(count);
  std::map<std::string, std::size_t> indexes;
  for (std::size_t index = 0; index < count; ++index) {
    indexes[graph.components[index].id] = index;
  }
  std::vector<std::vector<std::size_t>> dependents(count);
  std::vector<std::size_t> indegrees(count, 0u);
  for (std::size_t index = 0; index < count; ++index) {
    std::set<std::string> seen;
    for (const auto& dependency : graph.components[index].depends_on) {
      if (!seen.insert(dependency).second) {
        add_error(result, "component " + graph.components[index].id + " has duplicate lifecycle dependency " + dependency);
        continue;
      }
      const auto found = indexes.find(dependency);
      if (found == indexes.end()) {
        add_error(result, "component " + graph.components[index].id + " depends_on missing component " + dependency);
        continue;
      }
      dependents[found->second].push_back(index);
      ++indegrees[index];
    }
  }
  std::deque<std::size_t> ready;
  for (std::size_t index = 0; index < indegrees.size(); ++index) {
    if (indegrees[index] == 0u) {
      ready.push_back(index);
    }
  }
  while (!ready.empty()) {
    const auto current = ready.front();
    ready.pop_front();
    result.order.push_back(current);
    for (const auto dependent : dependents[current]) {
      if (indegrees[dependent] > 0u) {
        --indegrees[dependent];
      }
      if (indegrees[dependent] == 0u) {
        ready.push_back(dependent);
      }
    }
  }
  if (result.order.size() != count) {
    add_error(result, "component depends_on graph contains a cycle");
  }
  return result;
}

std::vector<std::vector<std::string>> tarjan_scc(const std::vector<std::string>& nodes,
                                                 const std::map<std::string, std::vector<std::string>>& edges) {
  std::vector<std::vector<std::string>> components;
  std::map<std::string, int> index_of;
  std::map<std::string, int> lowlink;
  std::set<std::string> on_stack;
  std::vector<std::string> stack;
  int next_index = 0;

  std::function<void(const std::string&)> strong_connect = [&](const std::string& node) {
    index_of[node] = next_index;
    lowlink[node] = next_index;
    ++next_index;
    stack.push_back(node);
    on_stack.insert(node);
    const auto found_edges = edges.find(node);
    if (found_edges != edges.end()) {
      for (const auto& next : found_edges->second) {
        if (index_of.count(next) == 0u) {
          strong_connect(next);
          lowlink[node] = std::min(lowlink[node], lowlink[next]);
        } else if (on_stack.count(next) != 0u) {
          lowlink[node] = std::min(lowlink[node], index_of[next]);
        }
      }
    }
    if (lowlink[node] == index_of[node]) {
      std::vector<std::string> component;
      while (!stack.empty()) {
        auto value = stack.back();
        stack.pop_back();
        on_stack.erase(value);
        component.push_back(value);
        if (value == node) {
          break;
        }
      }
      std::sort(component.begin(), component.end());
      components.push_back(std::move(component));
    }
  };

  for (const auto& node : nodes) {
    if (index_of.count(node) == 0u) {
      strong_connect(node);
    }
  }
  return components;
}

bool same_component_set(std::vector<std::string> left, std::vector<std::string> right) {
  std::sort(left.begin(), left.end());
  std::sort(right.begin(), right.end());
  return left == right;
}

std::string join_ids(const std::vector<std::string>& ids) {
  std::ostringstream out;
  for (std::size_t index = 0; index < ids.size(); ++index) {
    if (index > 0u) {
      out << ",";
    }
    out << ids[index];
  }
  return out.str();
}

bool has_immediate_self_loop(const GraphSpec& graph, const std::string& component) {
  for (const auto& edge : graph.edges) {
    if (!edge.has_kind || !edge.invalid_kind.empty() || edge.kind != EdgeKind::kImmediate) {
      continue;
    }
    if (component_id_from_endpoint(edge.from) == component && component_id_from_endpoint(edge.to) == component) {
      return true;
    }
  }
  return false;
}

GraphCompileResult compile_graph_impl(const GraphSpec& graph) {
  GraphCompileResult result;
  std::vector<std::string> component_ids;
  std::set<std::string> known_components;
  for (const auto& component : graph.components) {
    if (!component.id.empty() && known_components.insert(component.id).second) {
      component_ids.push_back(component.id);
    }
  }

  std::map<std::string, std::vector<std::string>> immediate_edges;
  for (const auto& component_id : component_ids) {
    immediate_edges[component_id] = {};
  }
  for (const auto& edge : graph.edges) {
    if (!edge.has_kind || !edge.invalid_kind.empty() || edge.kind != EdgeKind::kImmediate) {
      continue;
    }
    const auto from = component_id_from_endpoint(edge.from);
    const auto to = component_id_from_endpoint(edge.to);
    if (known_components.count(from) != 0u && known_components.count(to) != 0u) {
      immediate_edges[from].push_back(to);
    }
  }

  result.plan.immediate_sccs = tarjan_scc(component_ids, immediate_edges);

  std::map<std::string, const CompositeLoopSpec*> loops_by_component;
  std::set<std::string> loop_ids;
  for (const auto& loop : graph.composite_loops) {
    if (loop.id.empty()) {
      add_error(result, "composite_loop id must not be empty");
      continue;
    }
    if (!loop_ids.insert(loop.id).second) {
      add_error(result, "duplicate composite_loop: " + loop.id);
    }
    std::set<std::string> loop_components;
    for (const auto& component : loop.components) {
      if (!loop_components.insert(component).second) {
        add_error(result, "composite_loop " + loop.id + " lists duplicate component " + component);
      }
      if (known_components.count(component) == 0u) {
        add_error(result, "composite_loop " + loop.id + " references missing component " + component);
        continue;
      }
      const auto owner = loops_by_component.find(component);
      if (owner != loops_by_component.end() && owner->second->id != loop.id) {
        add_error(result, "component " + component + " is owned by multiple composite_loops: " + owner->second->id +
                              "," + loop.id);
      } else {
        loops_by_component[component] = &loop;
      }
    }
  }

  std::map<std::string, std::size_t> component_scc;
  for (std::size_t scc_index = 0; scc_index < result.plan.immediate_sccs.size(); ++scc_index) {
    for (const auto& component : result.plan.immediate_sccs[scc_index]) {
      component_scc[component] = scc_index;
    }
  }

  std::set<std::string> matched_loop_ids;
  std::vector<std::string> region_ids(result.plan.immediate_sccs.size());
  for (std::size_t scc_index = 0; scc_index < result.plan.immediate_sccs.size(); ++scc_index) {
    const auto& scc = result.plan.immediate_sccs[scc_index];
    const bool cyclic = scc.size() > 1u || (!scc.empty() && has_immediate_self_loop(graph, scc.front()));
    const CompositeLoopSpec* matching_loop = nullptr;
    for (const auto& loop : graph.composite_loops) {
      if (same_component_set(loop.components, scc)) {
        matching_loop = &loop;
        matched_loop_ids.insert(loop.id);
        break;
      }
    }
    if (cyclic && matching_loop == nullptr) {
      add_error(result, "immediate cycle detected among components " + join_ids(scc) +
                            "; declare a matching composite_loop or use delay/state/async edges");
    }
    if (!cyclic && matching_loop != nullptr) {
      add_error(result, "composite_loop " + matching_loop->id +
                            " does not match an immediate cyclic strongly connected component");
    }

    CompiledGraphRegion region;
    region.components = scc;
    if (matching_loop != nullptr) {
      region.id = matching_loop->id;
      region.kind = CompiledRegionKind::kCompositeLoop;
      region.composite_loop_id = matching_loop->id;
      region.loop_policy = matching_loop->loop_policy;
    } else {
      region.id = scc.size() == 1u ? scc.front() : "region_" + std::to_string(scc_index);
    }
    region_ids[scc_index] = region.id;
    for (const auto& component : scc) {
      result.plan.component_region[component] = region.id;
    }
    result.plan.regions.push_back(std::move(region));
  }

  for (const auto& loop : graph.composite_loops) {
    if (matched_loop_ids.count(loop.id) == 0u) {
      add_error(result,
                "composite_loop " + loop.id + " must exactly match one immediate cyclic strongly connected component");
    }
  }

  std::map<std::string, std::set<std::string>> outgoing_regions;
  std::map<std::string, std::set<std::string>> incoming_regions;
  std::map<std::string, std::size_t> indegree;
  for (const auto& region_id : region_ids) {
    indegree[region_id] = 0u;
  }
  for (const auto& edge : graph.edges) {
    if (!edge.has_kind || !edge.invalid_kind.empty() || edge.kind != EdgeKind::kImmediate) {
      continue;
    }
    const auto from = component_id_from_endpoint(edge.from);
    const auto to = component_id_from_endpoint(edge.to);
    const auto from_scc = component_scc.find(from);
    const auto to_scc = component_scc.find(to);
    if (from_scc == component_scc.end() || to_scc == component_scc.end() || from_scc->second == to_scc->second) {
      continue;
    }
    const auto& from_region = region_ids[from_scc->second];
    const auto& to_region = region_ids[to_scc->second];
    if (outgoing_regions[from_region].insert(to_region).second) {
      incoming_regions[to_region].insert(from_region);
      ++indegree[to_region];
    }
  }

  for (auto& region : result.plan.regions) {
    const auto outgoing = outgoing_regions.find(region.id);
    if (outgoing != outgoing_regions.end()) {
      region.outgoing_regions.assign(outgoing->second.begin(), outgoing->second.end());
    }
    const auto incoming = incoming_regions.find(region.id);
    if (incoming != incoming_regions.end()) {
      region.incoming_regions.assign(incoming->second.begin(), incoming->second.end());
    }
  }

  std::deque<std::string> ready;
  for (const auto& region_id : region_ids) {
    if (indegree[region_id] == 0u) {
      ready.push_back(region_id);
    }
  }
  while (!ready.empty()) {
    const auto region_id = ready.front();
    ready.pop_front();
    result.plan.region_order.push_back(region_id);
    for (const auto& next : outgoing_regions[region_id]) {
      auto& next_indegree = indegree[next];
      if (next_indegree > 0u) {
        --next_indegree;
      }
      if (next_indegree == 0u) {
        ready.push_back(next);
      }
    }
  }
  if (result.plan.region_order.size() != region_ids.size()) {
    add_error(result, "compiled immediate region graph is cyclic after SCC condensation");
  }
  return result;
}

std::string config_kind_name(ConfigValueKind kind) {
  switch (kind) {
  case ConfigValueKind::kString:
    return "string";
  case ConfigValueKind::kInt:
    return "int";
  case ConfigValueKind::kDouble:
    return "double";
  case ConfigValueKind::kBool:
    return "bool";
  }
  return "unknown";
}

bool parse_double(const std::string& value, double& parsed) {
  try {
    std::size_t pos = 0;
    parsed = std::stod(value, &pos);
    return pos == value.size();
  } catch (const std::exception&) {
    return false;
  }
}

void validate_component_config(const ComponentNodeSpec& component, const ComponentDescriptor& descriptor,
                               GraphValidationResult& result) {
  for (const auto& field : descriptor.config_fields) {
    const auto found = component.config.values.find(field.name);
    if (found == component.config.values.end()) {
      if (field.required) {
        add_error(result, "component " + component.id + " missing required config field " + field.name);
      }
      continue;
    }
    if (component.config.is_nested(field.name) && !field.allow_nested) {
      add_error(result, "component " + component.id + " config field " + field.name + " does not allow nested values");
      continue;
    }
    const auto& value = found->second;
    if (field.kind == ConfigValueKind::kString) {
      if (!field.enum_values.empty() &&
          std::find(field.enum_values.begin(), field.enum_values.end(), value) == field.enum_values.end()) {
        add_error(result,
                  "component " + component.id + " config field " + field.name + " has unsupported enum value " + value);
      }
      continue;
    }
    if (field.kind == ConfigValueKind::kBool) {
      if (value != "true" && value != "false") {
        add_error(result, "component " + component.id + " config field " + field.name + " must be bool true/false");
      }
      continue;
    }
    double numeric = 0.0;
    if (!parse_double(value, numeric)) {
      add_error(result, "component " + component.id + " config field " + field.name + " must be " +
                            config_kind_name(field.kind));
      continue;
    }
    if (field.kind == ConfigValueKind::kInt && numeric != static_cast<int>(numeric)) {
      add_error(result, "component " + component.id + " config field " + field.name + " must be int");
    }
    if (field.min_value.has_value() && numeric < *field.min_value) {
      add_error(result, "component " + component.id + " config field " + field.name + " is below minimum");
    }
    if (field.max_value.has_value() && numeric > *field.max_value) {
      add_error(result, "component " + component.id + " config field " + field.name + " is above maximum");
    }
  }
}

const PortDescriptor* find_port(const std::vector<PortDescriptor>& ports, const std::string& name) {
  const auto found =
      std::find_if(ports.begin(), ports.end(), [&name](const PortDescriptor& port) { return port.name == name; });
  if (found == ports.end()) {
    return nullptr;
  }
  return &*found;
}

GraphValidationResult validate_graph_impl(const GraphSpec& graph, const ComponentRegistry* registry) {
  GraphValidationResult result;
  if (graph.schema_version != kSupportedSchemaVersion) {
    add_error(result, "schema_version must be 1");
  }
  if (graph.name.empty()) {
    add_error(result, "graph.name must not be empty");
  }
  if (graph.kind.empty()) {
    add_error(result, "graph.kind must not be empty");
  } else if (graph.kind != "runnable" && graph.kind != "internal_test") {
    add_error(result, "graph.kind must be runnable or internal_test");
  }
  if (graph.clock.runtime_domain != "steady") {
    add_error(result, "graph.clock.runtime_domain must be steady");
  }
  if (!is_timestamp_domain_value(graph.clock.event_domain)) {
    add_error(result, "graph.clock.event_domain has unsupported domain " + graph.clock.event_domain);
  }

  std::set<std::string> lanes;
  for (const auto& lane : graph.lanes) {
    if (lane.id.empty()) {
      add_error(result, "lane id must not be empty");
      continue;
    }
    if (!lanes.insert(lane.id).second) {
      add_error(result, "duplicate lane: " + lane.id);
    }
    if (!is_allowed_scheduler_type(lane.type)) {
      add_error(result, "lane " + lane.id + " has unsupported type " + lane.type);
    }
    if (lane.max_threads < 0) {
      add_error(result, "lane " + lane.id + " max_threads must be non-negative");
    }
  }

  std::set<std::string> component_ids;
  std::map<std::string, ComponentDescriptor> descriptors;
  bool has_input_boundary = false;
  bool has_output_boundary = false;
  for (const auto& component : graph.components) {
    if (component.id.empty()) {
      add_error(result, "component id must not be empty");
      continue;
    }
    if (!component_ids.insert(component.id).second) {
      add_error(result, "duplicate component: " + component.id);
    }
    if (component.type.empty()) {
      add_error(result, "component " + component.id + " type must not be empty");
    }
    if (lanes.count(component.execution.lane) == 0u) {
      add_error(result, "component " + component.id + " references missing lane " + component.execution.lane);
    }
    if (component.event_sources.empty()) {
      add_error(result, "component " + component.id + " requires at least one event_source");
    }
    for (const auto& source : component.event_sources) {
      if (!is_allowed_event_source_type(source.type)) {
        add_error(result, "component " + component.id + " has unsupported event_source.type " + source.type);
      }
      if (source.type == "timer" && source.period_ms <= 0) {
        add_error(result, "component " + component.id + " timer event_source requires positive period_ms");
      }
      if (source.type == "message" && source.inputs.empty()) {
        add_error(result, "component " + component.id + " message event_source requires at least one input");
      }
    }
    if (!is_allowed_trigger_policy_type(component.trigger_policy.type)) {
      add_error(result,
                "component " + component.id + " has unsupported trigger_policy.type " + component.trigger_policy.type);
    }
    if ((component.trigger_policy.type == "on_event" || component.trigger_policy.type == "all_inputs" ||
         component.trigger_policy.type == "any_input" || component.trigger_policy.type == "time_sync" ||
         component.trigger_policy.type == "batch") &&
        has_message_event_source(component) && trigger_policy_inputs_for(component).empty()) {
      add_error(result, "component " + component.id + " trigger_policy requires at least one input");
    }
    if (component.trigger_policy.type == "batch" && component.trigger_policy.batch_size <= 0 &&
        component.trigger_policy.batch_window_ms <= 0) {
      add_error(result, "component " + component.id + " batch trigger_policy requires batch_size or batch_window_ms");
    }
    if (component.boundary.role != ComponentRole::kProcessing) {
      has_input_boundary = has_input_boundary || component_role_has_input(component.boundary.role);
      has_output_boundary = has_output_boundary || component_role_has_output(component.boundary.role);
    }

    if (registry == nullptr) {
      continue;
    }
    const auto metadata = registry->metadata(component.type);
    if (!metadata.has_value()) {
      add_error(result, "component " + component.id + " type is not registered: " + component.type);
      continue;
    }
    try {
      auto instance = registry->create(component.type);
      auto descriptor = instance->describe();
      if (descriptor.type != component.type) {
        add_error(result, "component " + component.id + " descriptor type mismatch: " + descriptor.type +
                              " != " + component.type);
      }
      if (!descriptor.scheduler.lane.empty() && descriptor.scheduler.lane != component.execution.lane) {
        add_error(result, "component " + component.id + " lane mismatch: descriptor " + descriptor.scheduler.lane +
                              " != graph " + component.execution.lane);
      }
      if (descriptor.role != ComponentRole::kProcessing) {
        has_input_boundary = has_input_boundary || component_role_has_input(descriptor.role);
        has_output_boundary = has_output_boundary || component_role_has_output(descriptor.role);
      }
      validate_component_config(component, descriptor, result);
      descriptors[component.id] = std::move(descriptor);
    } catch (const std::exception& error) {
      add_error(result, "component " + component.id + " descriptor probe failed: " + error.what());
    }
  }

  const auto lifecycle = compute_component_lifecycle_order(graph);
  for (const auto& error : lifecycle.errors) {
    add_error(result, error);
  }

  if (registry != nullptr && graph.kind == "runnable") {
    if (!has_input_boundary) {
      add_error(result, "runnable graph must declare at least one input boundary component");
    }
    if (!has_output_boundary) {
      add_error(result, "runnable graph must declare at least one output boundary component");
    }
  }

  std::set<std::string> edge_ids;
  std::set<std::string> source_endpoints;
  std::set<std::string> target_endpoints;
  for (const auto& edge : graph.edges) {
    if (edge.id.empty()) {
      add_error(result, "edge id must not be empty");
      continue;
    }
    if (!edge_ids.insert(edge.id).second) {
      add_error(result, "duplicate edge: " + edge.id);
    }
    if (!edge.has_kind) {
      add_error(result, "edge " + edge.id + " kind is required");
    } else if (!edge.invalid_kind.empty()) {
      add_error(result, "edge " + edge.id + " has unsupported kind " + edge.invalid_kind);
    }
    if (!is_allowed_channel_type(edge.policy.mode)) {
      add_error(result, "edge " + edge.id + " has unsupported policy.mode " + edge.policy.mode);
    }
    if (edge.policy.capacity <= 0) {
      add_error(result, "edge " + edge.id + " policy.capacity must be positive");
    }
    if (!is_allowed_drop_policy(edge.policy.overflow)) {
      add_error(result, "edge " + edge.id + " has unsupported policy.overflow " + edge.policy.overflow);
    }
    if (is_latest_style_channel_type(edge.policy.mode) && is_backpressure_drop_policy(edge.policy.overflow)) {
      add_error(result, "edge " + edge.id + " latest-style policy cannot use overflow " + edge.policy.overflow);
    }
    if (!is_timestamp_domain_value(edge.policy.timestamp_domain)) {
      add_error(result, "edge " + edge.id + " has unsupported policy.timestamp_domain " + edge.policy.timestamp_domain);
    }
    if (!is_allowed_copy_policy(edge.policy.copy_policy)) {
      add_error(result, "edge " + edge.id + " has unsupported policy.copy_policy " + edge.policy.copy_policy);
    }
    if (!is_allowed_owner(edge.policy.owner)) {
      add_error(result, "edge " + edge.id + " has unsupported policy.owner " + edge.policy.owner);
    }
    if (!is_allowed_readers(edge.policy.readers)) {
      add_error(result, "edge " + edge.id + " has unsupported policy.readers " + edge.policy.readers);
    }

    const auto from_component = component_id_from_endpoint(edge.from);
    const auto to_component = component_id_from_endpoint(edge.to);
    if (component_ids.count(from_component) == 0u) {
      add_error(result, "edge " + edge.id + " references missing source component " + from_component);
    }
    if (component_ids.count(to_component) == 0u) {
      add_error(result, "edge " + edge.id + " references missing target component " + to_component);
    }
    if (!source_endpoints.insert(edge.from).second && edge.policy.owner == "producer") {
      add_error(result, "source endpoint has multiple producer-owned edges: " + edge.from);
    }
    target_endpoints.insert(edge.to);

    if (registry != nullptr && descriptors.count(from_component) != 0u) {
      const auto port = port_name_from_endpoint(edge.from);
      if (!port.empty() && find_port(descriptors.at(from_component).outputs, port) == nullptr) {
        add_error(result, "edge " + edge.id + " references missing output port " + edge.from);
      }
    }
    if (registry != nullptr && descriptors.count(to_component) != 0u) {
      const auto port = port_name_from_endpoint(edge.to);
      if (!port.empty() && find_port(descriptors.at(to_component).inputs, port) == nullptr) {
        add_error(result, "edge " + edge.id + " references missing input port " + edge.to);
      }
    }
  }

  for (const auto& component : graph.components) {
    for (const auto& input : trigger_policy_inputs_for(component)) {
      if (target_endpoints.count(component.id + "." + input) == 0u) {
        add_error(result, "component " + component.id + " trigger input has no incoming edge: " + input);
      }
    }
  }

  for (const auto& loop : graph.composite_loops) {
    if (!is_allowed_loop_policy_type(loop.loop_policy.type)) {
      add_error(result, "composite_loop " + loop.id + " has unsupported loop_policy.type " + loop.loop_policy.type);
    }
    if (loop.loop_policy.budget_ms < 0 || loop.loop_policy.max_iterations < 0 || loop.loop_policy.max_inflight < 0 ||
        loop.loop_policy.min_interval_ms < 0) {
      add_error(result, "composite_loop " + loop.id + " numeric policy fields must be non-negative");
    }
  }

  const auto compile = compile_graph_impl(graph);
  result.compiled_plan = compile.plan;
  for (const auto& error : compile.errors) {
    add_error(result, error);
  }
  return result;
}

void append_metric(std::vector<RuntimeMetricSample>& metrics, std::string name, double value, std::string component_id = {},
                   std::string lane = {}, std::string channel_id = {}) {
  metrics.push_back(RuntimeMetricSample{std::move(name), value, std::move(component_id), std::move(lane),
                                        std::move(channel_id), {}});
}

}  // namespace

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

std::string to_string(EdgeKind kind) {
  switch (kind) {
  case EdgeKind::kImmediate:
    return "immediate";
  case EdgeKind::kDelay:
    return "delay";
  case EdgeKind::kState:
    return "state";
  case EdgeKind::kAsync:
    return "async";
  }
  return "unknown";
}

std::string to_string(CompiledRegionKind kind) {
  switch (kind) {
  case CompiledRegionKind::kComponent:
    return "component";
  case CompiledRegionKind::kCompositeLoop:
    return "composite_loop";
  }
  return "unknown";
}

GraphCompileResult compile_graph(const GraphSpec& graph) {
  return compile_graph_impl(graph);
}

GraphValidationResult validate_graph_structure(const GraphSpec& graph) {
  return validate_graph_impl(graph, nullptr);
}

GraphValidationResult validate_graph(const GraphSpec& graph, const ComponentRegistry& registry) {
  return validate_graph_impl(graph, &registry);
}

std::vector<std::size_t> component_lifecycle_order(const GraphSpec& graph) {
  const auto result = compute_component_lifecycle_order(graph);
  if (!result.ok) {
    throw std::invalid_argument(result.errors.front());
  }
  return result.order;
}

GraphDryRunResult dry_run_graph(const GraphSpec& graph, const ComponentRegistry& registry, std::size_t tick_iterations) {
  GraphDryRunResult result;
  const auto validation = validate_graph(graph, registry);
  if (!validation.ok) {
    result.ok = false;
    result.errors = validation.errors;
    return result;
  }
  result.instantiated_components = graph.components.size();
  result.configured_components = graph.components.size();
  result.started_components = graph.components.size();
  result.stopped_components = graph.components.size();
  result.tick_calls = graph.components.size() * tick_iterations;
  for (std::size_t iteration = 0; iteration < tick_iterations; ++iteration) {
    for (const auto& component : graph.components) {
      result.ticked_components.push_back(component.id);
    }
  }
  for (const auto& edge : graph.edges) {
    append_metric(result.runtime_metrics, "runtime.channel.configured", 1.0, {}, {}, edge.id);
  }
  result.metric_samples = result.runtime_metrics.size();
  return result;
}

std::string graph_plan_text(const GraphSpec& graph, const GraphCompiledPlan& plan) {
  std::ostringstream out;
  out << "graph: " << graph.name << "\n";
  out << "schema_version: " << graph.schema_version << "\n";
  out << "components: " << graph.components.size() << "\n";
  out << "edges: " << graph.edges.size() << "\n";
  out << "composite_loops: " << graph.composite_loops.size() << "\n";
  out << "region_order:";
  for (const auto& region : plan.region_order) {
    out << " " << region;
  }
  out << "\n";
  for (const auto& region : plan.regions) {
    out << "- region " << region.id << " kind=" << to_string(region.kind) << " components=" << join_ids(region.components);
    if (region.kind == CompiledRegionKind::kCompositeLoop) {
      out << " loop_policy=" << region.loop_policy.type;
    }
    out << "\n";
  }
  return out.str();
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

std::string graph_mermaid(const GraphSpec& graph, const GraphCompiledPlan& plan) {
  std::ostringstream out;
  out << "flowchart TD\n";
  out << "  %% graph: " << graph.name << "\n";
  for (const auto& component : graph.components) {
    out << "  " << component.id << "[\"" << component.id << "\\n" << component.type << "\"]\n";
  }
  for (const auto& loop : graph.composite_loops) {
    out << "  subgraph " << loop.id << "[CompositeLoop: " << loop.loop_policy.type << "]\n";
    for (const auto& component : loop.components) {
      out << "    " << component << "\n";
    }
    out << "  end\n";
  }
  for (const auto& edge : graph.edges) {
    out << "  " << component_id_from_endpoint(edge.from) << " -->|\"" << edge.id << ":" << to_string(edge.kind)
        << "/" << edge.policy.mode << "\"| " << component_id_from_endpoint(edge.to) << "\n";
  }
  out << "  %% region_order: " << join_ids(plan.region_order) << "\n";
  return out.str();
}

}  // namespace topoexec
