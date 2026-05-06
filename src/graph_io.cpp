#include "topoexec/runtime/graph.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <yaml-cpp/yaml.h>

namespace topoexec {
namespace {

constexpr std::size_t kMaxGraphInputBytes = 1024u * 1024u;
constexpr std::size_t kMaxLanes = 256u;
constexpr std::size_t kMaxComponents = 4096u;
constexpr std::size_t kMaxEdges = 8192u;
constexpr std::size_t kMaxCompositeLoops = 1024u;
constexpr std::size_t kMaxIdentifierLength = 128u;
constexpr std::size_t kMaxConfigDepth = 8u;
constexpr std::size_t kMaxConfigValueBytes = 4096u;

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

bool is_multi_reader_value(const std::string& readers) {
  return readers == "multi" || readers == "multiple";
}

bool slow_reader_drop_risk(const EdgePolicySpec& policy) {
  return is_multi_reader_value(policy.readers) && (policy.overflow == "drop_oldest" || policy.overflow == "overwrite");
}

void enforce_limit(std::size_t value, std::size_t limit, const std::string& context) {
  if (value > limit) {
    throw std::invalid_argument(context + " exceeds limit " + std::to_string(limit));
  }
}

std::string checked_identifier(std::string value, const std::string& context) {
  if (value.empty()) {
    throw std::invalid_argument(context + " must not be empty");
  }
  enforce_limit(value.size(), kMaxIdentifierLength, context + " length");
  return value;
}

void enforce_config_limits(const YAML::Node& node, const std::string& context, std::size_t depth = 0u) {
  if (!node || node.IsNull()) {
    return;
  }
  enforce_limit(depth, kMaxConfigDepth, context + " depth");
  if (node.IsMap()) {
    for (const auto& item : node) {
      const auto key = item.first.as<std::string>();
      enforce_limit(key.size(), kMaxIdentifierLength, context + " key length");
      enforce_config_limits(item.second, context + "." + key, depth + 1u);
    }
    return;
  }
  if (node.IsSequence()) {
    for (std::size_t index = 0; index < node.size(); ++index) {
      enforce_config_limits(node[index], context + "[" + std::to_string(index) + "]", depth + 1u);
    }
    return;
  }
  enforce_limit(node.as<std::string>().size(), kMaxConfigValueBytes, context + " value size");
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

ConfigView read_config_node(const YAML::Node& config, const std::string& context) {
  ConfigView view;
  if (!config || config.IsNull()) {
    return view;
  }
  require_map(config, context);
  enforce_config_limits(config, context);
  for (const auto& item : config) {
    const auto key = item.first.as<std::string>();
    if (item.second.IsMap() || item.second.IsSequence()) {
      std::ostringstream out;
      out << item.second;
      enforce_limit(out.str().size(), kMaxConfigValueBytes, context + "." + key + " serialized value size");
      view.values[key] = out.str();
      view.nested_values.insert(key);
    } else {
      view.values[key] = item.second.as<std::string>();
    }
  }
  return view;
}

ConfigView read_config(const YAML::Node& component_node, const std::string& component_id) {
  return read_config_node(component_node["config"], "components." + component_id + ".config");
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
                        {"lane", "reentrant", "priority", "budget_ms", "on_error"});
  ExecutionSpec spec;
  spec.lane = require_string(execution_node, "lane", "components." + component_id + ".execution");
  spec.reentrant = optional_bool(execution_node, "reentrant");
  spec.priority = optional_string(execution_node, "priority", spec.priority);
  spec.budget_ms = optional_int(execution_node, "budget_ms");
  spec.on_error = optional_string(execution_node, "on_error", spec.on_error);
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
                        {"mode", "capacity", "overflow", "lifespan_ms", "deadline_ms", "max_inflight", "preserve_order",
                         "allow_drop", "emit_health_events", "timestamp_domain", "copy_policy", "owner", "readers"});
  EdgePolicySpec policy;
  policy.mode = optional_string(policy_node, "mode", policy.mode);
  policy.capacity = optional_int(policy_node, "capacity", policy.capacity);
  policy.overflow = optional_string(policy_node, "overflow", policy.overflow);
  policy.lifespan_ms = optional_int(policy_node, "lifespan_ms");
  policy.deadline_ms = optional_int(policy_node, "deadline_ms");
  policy.max_inflight = optional_int(policy_node, "max_inflight");
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
  loop.id = checked_identifier(require_string(loop_node, "id", context), context + ".id");
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
  reject_unknown_fields(graph_node, "runtime graph.graph", {"name", "kind", "clock", "config"});
  graph.name =
      checked_identifier(require_string(graph_node, "name", "runtime graph.graph"), "runtime graph.graph.name");
  graph.kind = optional_string(graph_node, "kind", "runnable");
  graph.config = read_config_node(graph_node["config"], "runtime graph.graph.config");
  const auto clock_node = graph_node["clock"];
  if (clock_node && !clock_node.IsNull()) {
    require_map(clock_node, "runtime graph.graph.clock");
    reject_unknown_fields(clock_node, "runtime graph.graph.clock", {"runtime_domain", "event_domain"});
    graph.clock.runtime_domain = optional_string(clock_node, "runtime_domain", graph.clock.runtime_domain);
    graph.clock.event_domain = optional_string(clock_node, "event_domain", graph.clock.event_domain);
  }

  const auto lanes_node = require_node(root, "lanes", "runtime graph");
  require_map(lanes_node, "runtime graph.lanes");
  enforce_limit(lanes_node.size(), kMaxLanes, "runtime graph.lanes count");
  for (const auto& item : lanes_node) {
    LaneSpec lane;
    lane.id = checked_identifier(item.first.as<std::string>(), "lanes id");
    require_map(item.second, "lanes." + lane.id);
    reject_unknown_fields(item.second, "lanes." + lane.id,
                          {"type", "hz", "priority", "max_callback_ms", "max_threads", "queue_capacity", "overflow",
                           "wall_clock_enabled", "period_ms", "tick_budget_ms", "overrun_policy", "thread_name",
                           "cpu_affinity", "nice_priority", "rt_policy", "rt_priority", "isolation_intent"});
    lane.type = require_string(item.second, "type", "lanes." + lane.id);
    lane.hz = optional_double(item.second, "hz");
    lane.priority = optional_string(item.second, "priority");
    lane.max_callback_ms = optional_int(item.second, "max_callback_ms");
    lane.max_threads = optional_int(item.second, "max_threads");
    lane.queue_capacity = optional_int(item.second, "queue_capacity");
    lane.overflow = optional_string(item.second, "overflow", lane.overflow);
    lane.wall_clock_enabled = optional_bool(item.second, "wall_clock_enabled", lane.wall_clock_enabled);
    lane.period_ms = optional_int(item.second, "period_ms");
    lane.tick_budget_ms = optional_int(item.second, "tick_budget_ms");
    lane.overrun_policy = optional_string(item.second, "overrun_policy", lane.overrun_policy);
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
  enforce_limit(components_node.size(), kMaxComponents, "runtime graph.components count");
  for (std::size_t index = 0; index < components_node.size(); ++index) {
    const auto component_node = components_node[index];
    require_map(component_node, "components[" + std::to_string(index) + "]");
    ComponentNodeSpec component;
    component.id = checked_identifier(require_string(component_node, "id", "components[" + std::to_string(index) + "]"),
                                      "components[" + std::to_string(index) + "].id");
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
  enforce_limit(edges_node.size(), kMaxEdges, "runtime graph.edges count");
  for (std::size_t index = 0; index < edges_node.size(); ++index) {
    const auto edge_node = edges_node[index];
    require_map(edge_node, "edges[" + std::to_string(index) + "]");
    EdgeSpec edge;
    edge.id = checked_identifier(require_string(edge_node, "id", "edges[" + std::to_string(index) + "]"),
                                 "edges[" + std::to_string(index) + "].id");
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
    enforce_limit(loops_node.size(), kMaxCompositeLoops, "runtime graph.composite_loops count");
    for (std::size_t index = 0; index < loops_node.size(); ++index) {
      graph.composite_loops.push_back(
          read_composite_loop(loops_node[index], "composite_loops[" + std::to_string(index) + "]"));
    }
  }
  return graph;
}

} // namespace

GraphSpec load_graph_text(const std::string& text) {
  enforce_limit(text.size(), kMaxGraphInputBytes, "graph input size");
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

nlohmann::json lane_capability_summary(const LaneSpec& lane) {
  nlohmann::json summary;
  summary["id"] = lane.id;
  summary["type"] = lane.type;
  summary["advisory_fields"] = {"priority",  "thread_name", "cpu_affinity",    "nice_priority",
                                "rt_policy", "rt_priority", "isolation_intent"};
  summary["unsupported_claims"] = {"hard_preemption", "hard_real_time", "implicit_os_scheduler_tuning"};
  if (lane.type == "event_loop") {
    summary["implemented"] = {"deterministic_region_order",       "bounded_runner_stop_checks",
                              "runtime_owned_publication_commit", "runtime_priority_ordering",
                              "cooperative_cancellation",         "timeout_budget_observation"};
    summary["future_extensions"] = {"manual_step_lane"};
  } else if (lane.type == "fixed_rate") {
    summary["implemented"] = {"bounded_simulated_ticks",   "opt_in_wall_clock_cadence",
                              "overrun_metrics",           "jitter_metrics",
                              "max_lateness_metrics",      "overrun_policy",
                              "runtime_priority_ordering", "cooperative_cancellation",
                              "timeout_budget_observation"};
    summary["future_extensions"] = {"independent_lane_threads", "hard_realtime_jitter_control"};
  } else if (lane.type == "thread_pool") {
    summary["implemented"] = {"persistent_worker_lifecycle",
                              "bounded_priority_queue",
                              "queue_admission",
                              "overflow_policy",
                              "non_reentrant_serialization",
                              "priority_queue",
                              "cooperative_cancellation",
                              "timeout_budget_observation",
                              "worker_id_trace",
                              "batch_trace_span",
                              "best_effort_worker_thread_naming"};
    summary["future_extensions"] = nlohmann::json::array();
  } else {
    summary["implemented"] = nlohmann::json::array();
    summary["future_extensions"] = {"isolated_thread", "manual_step"};
  }
  summary["max_threads"] = lane.max_threads;
  summary["queue_capacity"] = lane.queue_capacity;
  summary["overflow"] = lane.overflow;
  if (lane.type == "fixed_rate") {
    summary["overrun_policy"] = lane.overrun_policy;
  }
  return summary;
}

std::string graph_plan_json(const GraphSpec& graph, const GraphCompiledPlan& plan) {
  nlohmann::json root;
  root["graph"] = graph.name;
  root["kind"] = graph.kind;
  root["schema_version"] = graph.schema_version;
  root["scheduler_contract_version"] = topoexec::kTopoExecSemanticContractVersion;
  root["lane_capabilities"] = nlohmann::json::array();
  for (const auto& lane : graph.lanes) {
    root["lane_capabilities"].push_back(lane_capability_summary(lane));
  }
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
                             {"capacity", edge.policy.capacity},
                             {"overflow", edge.policy.overflow},
                             {"max_inflight", edge.policy.max_inflight},
                             {"copy_policy", edge.policy.copy_policy},
                             {"readers", edge.policy.readers},
                             {"slow_reader_drop_risk", slow_reader_drop_risk(edge.policy)}});
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
