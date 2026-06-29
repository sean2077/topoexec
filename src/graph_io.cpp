#include "topoexec/runtime/graph.hpp"

#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <yaml-cpp/eventhandler.h>
#include <yaml-cpp/parser.h>
#include <yaml-cpp/yaml.h>

namespace topoexec {
namespace {

thread_local const GraphInputLimits* g_active_limits = nullptr;

const GraphInputLimits& active_limits() {
  return g_active_limits == nullptr ? default_graph_input_limits() : *g_active_limits;
}

class ScopedGraphInputLimits {
public:
  explicit ScopedGraphInputLimits(const GraphInputLimits& limits) : previous_(g_active_limits) {
    g_active_limits = &limits;
  }

  ~ScopedGraphInputLimits() {
    g_active_limits = previous_;
  }

  ScopedGraphInputLimits(const ScopedGraphInputLimits&) = delete;
  ScopedGraphInputLimits& operator=(const ScopedGraphInputLimits&) = delete;

private:
  const GraphInputLimits* previous_;
};

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

bool is_valid_utf8(const std::string& text) {
  for (std::size_t index = 0; index < text.size();) {
    const auto byte = static_cast<unsigned char>(text[index]);
    if (byte <= 0x7Fu) {
      ++index;
      continue;
    }

    std::size_t needed = 0u;
    unsigned int codepoint = 0u;
    if ((byte & 0xE0u) == 0xC0u) {
      needed = 1u;
      codepoint = byte & 0x1Fu;
      if (codepoint == 0u) {
        return false;
      }
    } else if ((byte & 0xF0u) == 0xE0u) {
      needed = 2u;
      codepoint = byte & 0x0Fu;
    } else if ((byte & 0xF8u) == 0xF0u) {
      needed = 3u;
      codepoint = byte & 0x07u;
    } else {
      return false;
    }

    if (index + needed >= text.size()) {
      return false;
    }
    for (std::size_t offset = 1u; offset <= needed; ++offset) {
      const auto continuation = static_cast<unsigned char>(text[index + offset]);
      if ((continuation & 0xC0u) != 0x80u) {
        return false;
      }
      codepoint = (codepoint << 6u) | (continuation & 0x3Fu);
    }
    if ((needed == 1u && codepoint < 0x80u) || (needed == 2u && codepoint < 0x800u) ||
        (needed == 3u && codepoint < 0x10000u) || (codepoint >= 0xD800u && codepoint <= 0xDFFFu) ||
        codepoint > 0x10FFFFu) {
      return false;
    }
    index += needed + 1u;
  }
  return true;
}

void enforce_valid_utf8(const std::string& text) {
  if (!is_valid_utf8(text)) {
    throw std::invalid_argument("graph input must be valid UTF-8 text");
  }
}

void enforce_string_limit(const std::string& value, const std::string& context, const GraphInputLimits& limits) {
  enforce_limit(value.size(), limits.max_string_bytes, context + " length");
}

void enforce_identifier_limit(const std::string& value, const std::string& context, const GraphInputLimits& limits) {
  if (value.empty()) {
    throw std::invalid_argument(context + " must not be empty");
  }
  enforce_limit(value.size(), limits.max_identifier_bytes, context + " length");
}

std::string checked_identifier(std::string value, const std::string& context) {
  enforce_identifier_limit(value, context, active_limits());
  return value;
}

void enforce_config_limits(const YAML::Node& node, const std::string& context, std::size_t depth = 0u) {
  if (!node || node.IsNull()) {
    return;
  }
  enforce_limit(depth, active_limits().max_config_depth, context + " depth");
  if (node.IsMap()) {
    for (const auto& item : node) {
      const auto key = item.first.as<std::string>();
      enforce_limit(key.size(), active_limits().max_identifier_bytes, context + " key length");
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
  enforce_limit(node.as<std::string>().size(), active_limits().max_config_value_bytes, context + " value size");
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
  try {
    return child.as<int>();
  } catch (const std::exception&) {
    throw std::invalid_argument(std::string("field ") + key + " must be an integer");
  }
}

double optional_double(const YAML::Node& node, const char* key, double fallback = 0.0) {
  const auto child = node[key];
  if (!child || child.IsNull()) {
    return fallback;
  }
  try {
    return child.as<double>();
  } catch (const std::exception&) {
    throw std::invalid_argument(std::string("field ") + key + " must be a number");
  }
}

bool optional_bool(const YAML::Node& node, const char* key, bool fallback = false) {
  const auto child = node[key];
  if (!child || child.IsNull()) {
    return fallback;
  }
  try {
    return child.as<bool>();
  } catch (const std::exception&) {
    throw std::invalid_argument(std::string("field ") + key + " must be a boolean (true/false)");
  }
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

std::string component_id_from_endpoint(const std::string& endpoint) {
  const auto dot = endpoint.rfind('.');
  if (dot == std::string::npos) {
    return endpoint;
  }
  return endpoint.substr(0, dot);
}

std::string prefixed_id(const std::string& namespace_id, const std::string& local_id, const std::string& context) {
  return checked_identifier(namespace_id + "." + local_id, context);
}

std::string namespace_endpoint(const std::string& namespace_id, const std::string& endpoint,
                               const std::string& context) {
  const auto local_component = component_id_from_endpoint(endpoint);
  if (local_component.empty()) {
    throw std::invalid_argument(context + " must name a local component");
  }
  std::string prefixed_component = namespace_id + "." + local_component;
  enforce_identifier_limit(prefixed_component, context + " component", active_limits());
  if (local_component.size() == endpoint.size()) {
    return prefixed_component;
  }
  return prefixed_component + endpoint.substr(local_component.size());
}

std::vector<std::string> namespace_component_refs(const std::string& namespace_id,
                                                  const std::vector<std::string>& local_ids,
                                                  const std::string& context) {
  std::vector<std::string> values;
  values.reserve(local_ids.size());
  for (const auto& local_id : local_ids) {
    enforce_identifier_limit(local_id, context, active_limits());
    values.push_back(namespace_id + "." + local_id);
    enforce_identifier_limit(values.back(), context, active_limits());
  }
  return values;
}

struct GraphTemplateDefinition {
  std::string id;
  std::vector<std::string> parameters;
  YAML::Node body;
};

std::string substitute_placeholders(const std::string& value, const std::map<std::string, std::string>& parameters,
                                    const std::string& context) {
  std::string output;
  std::size_t cursor = 0u;
  while (cursor < value.size()) {
    const auto open = value.find("{{", cursor);
    if (open == std::string::npos) {
      output.append(value.substr(cursor));
      break;
    }
    output.append(value.substr(cursor, open - cursor));
    const auto close = value.find("}}", open + 2u);
    if (close == std::string::npos) {
      throw std::invalid_argument(context + " has an unterminated template parameter");
    }
    const auto name = value.substr(open + 2u, close - open - 2u);
    checked_identifier(name, context + " parameter");
    const auto found = parameters.find(name);
    if (found == parameters.end()) {
      throw std::invalid_argument(context + " references missing template parameter " + name);
    }
    output.append(found->second);
    cursor = close + 2u;
  }
  enforce_string_limit(output, context, active_limits());
  return output;
}

YAML::Node substitute_template_node(const YAML::Node& node, const std::map<std::string, std::string>& parameters,
                                    const std::string& context) {
  if (!node || node.IsNull()) {
    return {};
  }
  if (node.IsScalar()) {
    return YAML::Node(substitute_placeholders(node.as<std::string>(), parameters, context));
  }
  if (node.IsSequence()) {
    YAML::Node output(YAML::NodeType::Sequence);
    for (std::size_t index = 0; index < node.size(); ++index) {
      output.push_back(substitute_template_node(node[index], parameters, context + "[" + std::to_string(index) + "]"));
    }
    return output;
  }
  if (node.IsMap()) {
    YAML::Node output(YAML::NodeType::Map);
    for (const auto& item : node) {
      const auto key = substitute_placeholders(item.first.as<std::string>(), parameters, context + ".key");
      output[key] = substitute_template_node(item.second, parameters, context + "." + key);
    }
    return output;
  }
  throw std::invalid_argument(context + " has unsupported template node type");
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
      enforce_limit(out.str().size(), active_limits().max_config_value_bytes,
                    context + "." + key + " serialized value size");
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
                         "max_latency_ms", "watermark_lateness_ms", "debounce_window_ms", "condition", "coalesce"});
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
  policy.watermark_lateness_ms = optional_int(policy_node, "watermark_lateness_ms");
  policy.debounce_window_ms = optional_int(policy_node, "debounce_window_ms");
  policy.condition = optional_string(policy_node, "condition", "all_inputs_ready");
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

ComponentNodeSpec read_component_node(const YAML::Node& component_node, const std::string& component_id,
                                      const std::string& context) {
  require_map(component_node, context);
  ComponentNodeSpec component;
  component.id = checked_identifier(component_id, context + ".id");
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
  return component;
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

EdgeSpec read_edge_node(const YAML::Node& edge_node, const std::string& edge_id, const std::string& context) {
  require_map(edge_node, context);
  EdgeSpec edge;
  edge.id = checked_identifier(edge_id, context + ".id");
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
  return edge;
}

LoopPolicySpec read_loop_policy(const YAML::Node& policy_node, const std::string& context) {
  require_map(policy_node, context);
  reject_unknown_fields(policy_node, context,
                        {"type", "budget_ms", "max_iterations", "max_inflight", "drop_policy", "min_interval_ms",
                         "convergence", "residual_threshold", "partial_success"});
  LoopPolicySpec policy;
  policy.type = require_string(policy_node, "type", context);
  policy.budget_ms = optional_int(policy_node, "budget_ms");
  policy.max_iterations = optional_int(policy_node, "max_iterations");
  policy.max_inflight = optional_int(policy_node, "max_inflight");
  policy.drop_policy = optional_string(policy_node, "drop_policy");
  policy.min_interval_ms = optional_int(policy_node, "min_interval_ms");
  policy.convergence = optional_string(policy_node, "convergence");
  const auto residual_threshold_node = policy_node["residual_threshold"];
  if (residual_threshold_node && !residual_threshold_node.IsNull()) {
    policy.residual_threshold = residual_threshold_node.as<double>();
  }
  policy.partial_success = optional_string(policy_node, "partial_success");
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

void append_subgraph_expansion(const YAML::Node& subgraph_node, GraphSpec& graph, const std::string& context) {
  require_map(subgraph_node, context);
  reject_unknown_fields(subgraph_node, context, {"id", "components", "edges", "composite_loops"});

  GraphHierarchyEntry hierarchy;
  hierarchy.id = checked_identifier(require_string(subgraph_node, "id", context), context + ".id");

  const auto components_node = require_node(subgraph_node, "components", context);
  require_sequence(components_node, context + ".components");
  if (components_node.size() == 0u) {
    throw std::invalid_argument(context + ".components must contain at least one component");
  }
  for (std::size_t index = 0; index < components_node.size(); ++index) {
    const auto component_node = components_node[index];
    const auto local_id =
        checked_identifier(require_string(component_node, "id", context + ".components[" + std::to_string(index) + "]"),
                           context + ".components[" + std::to_string(index) + "].id");
    auto component = read_component_node(
        component_node, prefixed_id(hierarchy.id, local_id, context + ".components[" + std::to_string(index) + "].id"),
        context + ".components[" + std::to_string(index) + "]");
    component.depends_on =
        namespace_component_refs(hierarchy.id, component.depends_on, "components." + component.id + ".depends_on");
    hierarchy.components.push_back(component.id);
    graph.components.push_back(std::move(component));
  }

  const auto edges_node = require_node(subgraph_node, "edges", context);
  require_sequence(edges_node, context + ".edges");
  for (std::size_t index = 0; index < edges_node.size(); ++index) {
    const auto edge_node = edges_node[index];
    const auto local_id =
        checked_identifier(require_string(edge_node, "id", context + ".edges[" + std::to_string(index) + "]"),
                           context + ".edges[" + std::to_string(index) + "].id");
    auto edge = read_edge_node(
        edge_node, prefixed_id(hierarchy.id, local_id, context + ".edges[" + std::to_string(index) + "].id"),
        context + ".edges[" + std::to_string(index) + "]");
    edge.from = namespace_endpoint(hierarchy.id, edge.from, "edges." + edge.id + ".from");
    edge.to = namespace_endpoint(hierarchy.id, edge.to, "edges." + edge.id + ".to");
    hierarchy.edges.push_back(edge.id);
    graph.edges.push_back(std::move(edge));
  }

  const auto loops_node = subgraph_node["composite_loops"];
  if (loops_node && !loops_node.IsNull()) {
    require_sequence(loops_node, context + ".composite_loops");
    for (std::size_t index = 0; index < loops_node.size(); ++index) {
      auto loop = read_composite_loop(loops_node[index], context + ".composite_loops[" + std::to_string(index) + "]");
      loop.id = prefixed_id(hierarchy.id, loop.id, context + ".composite_loops[" + std::to_string(index) + "].id");
      loop.components =
          namespace_component_refs(hierarchy.id, loop.components, "composite_loops." + loop.id + ".components");
      hierarchy.composite_loops.push_back(loop.id);
      graph.composite_loops.push_back(std::move(loop));
    }
  }

  graph.hierarchy.push_back(std::move(hierarchy));
}

GraphTemplateDefinition read_graph_template(const YAML::Node& template_node, const std::string& context) {
  require_map(template_node, context);
  reject_unknown_fields(template_node, context, {"id", "parameters", "components", "edges", "composite_loops"});
  GraphTemplateDefinition definition;
  definition.id = checked_identifier(require_string(template_node, "id", context), context + ".id");
  definition.parameters = optional_string_vector(template_node, "parameters", context);
  std::set<std::string> parameter_names;
  for (const auto& parameter : definition.parameters) {
    checked_identifier(parameter, context + ".parameters");
    if (!parameter_names.insert(parameter).second) {
      throw std::invalid_argument(context + " lists duplicate template parameter " + parameter);
    }
  }
  require_node(template_node, "components", context);
  require_node(template_node, "edges", context);
  definition.body = template_node;
  return definition;
}

std::map<std::string, std::string> read_template_instance_parameters(const YAML::Node& instance_node,
                                                                     const GraphTemplateDefinition& definition,
                                                                     const std::string& context) {
  std::set<std::string> required(definition.parameters.begin(), definition.parameters.end());
  std::map<std::string, std::string> values;
  const auto parameters_node = instance_node["parameters"];
  if (parameters_node && !parameters_node.IsNull()) {
    require_map(parameters_node, context + ".parameters");
    for (const auto& item : parameters_node) {
      const auto name = checked_identifier(item.first.as<std::string>(), context + ".parameters key");
      if (required.count(name) == 0u) {
        throw std::invalid_argument(context + " provides unknown template parameter " + name);
      }
      if (!item.second.IsScalar()) {
        throw std::invalid_argument(context + ".parameters." + name + " must be a scalar string");
      }
      const auto value = item.second.as<std::string>();
      enforce_string_limit(value, context + ".parameters." + name, active_limits());
      values[name] = value;
    }
  }
  for (const auto& required_name : definition.parameters) {
    if (values.count(required_name) == 0u) {
      throw std::invalid_argument(context + " is missing template parameter " + required_name);
    }
  }
  return values;
}

void append_template_instance_expansion(const YAML::Node& instance_node,
                                        const std::map<std::string, GraphTemplateDefinition>& templates,
                                        GraphSpec& graph, const std::string& context) {
  require_map(instance_node, context);
  reject_unknown_fields(instance_node, context, {"id", "template", "parameters"});
  const auto instance_id = checked_identifier(require_string(instance_node, "id", context), context + ".id");
  const auto template_id =
      checked_identifier(require_string(instance_node, "template", context), context + ".template");
  const auto found = templates.find(template_id);
  if (found == templates.end()) {
    throw std::invalid_argument(context + " references unknown template " + template_id);
  }

  const auto parameter_values = read_template_instance_parameters(instance_node, found->second, context);
  YAML::Node expansion(YAML::NodeType::Map);
  expansion["id"] = instance_id;
  expansion["components"] =
      substitute_template_node(found->second.body["components"], parameter_values, context + ".components");
  expansion["edges"] = substitute_template_node(found->second.body["edges"], parameter_values, context + ".edges");
  const auto loops_node = found->second.body["composite_loops"];
  if (loops_node && !loops_node.IsNull()) {
    expansion["composite_loops"] = substitute_template_node(loops_node, parameter_values, context + ".composite_loops");
  }
  append_subgraph_expansion(expansion, graph, context);
}

GraphSpec load_graph_node(const YAML::Node& root) {
  require_map(root, "runtime graph");
  reject_unknown_fields(root, "runtime graph",
                        {"schema_version", "graph", "components", "lanes", "edges", "composite_loops", "subgraphs",
                         "templates", "template_instances"});
  GraphSpec graph;
  graph.schema_version = require_node(root, "schema_version", "runtime graph").as<int>();
  if (graph.schema_version != kTopoExecSchemaVersion) {
    throw std::invalid_argument("runtime graph.schema_version must be " + std::to_string(kTopoExecSchemaVersion));
  }

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
  enforce_limit(lanes_node.size(), active_limits().max_lanes, "runtime graph.lanes count");
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
  enforce_limit(components_node.size(), active_limits().max_components, "runtime graph.components count");
  for (std::size_t index = 0; index < components_node.size(); ++index) {
    const auto component_node = components_node[index];
    const auto component_id =
        checked_identifier(require_string(component_node, "id", "components[" + std::to_string(index) + "]"),
                           "components[" + std::to_string(index) + "].id");
    graph.components.push_back(
        read_component_node(component_node, component_id, "components[" + std::to_string(index) + "]"));
  }

  const auto edges_node = require_node(root, "edges", "runtime graph");
  require_sequence(edges_node, "runtime graph.edges");
  enforce_limit(edges_node.size(), active_limits().max_edges, "runtime graph.edges count");
  for (std::size_t index = 0; index < edges_node.size(); ++index) {
    const auto edge_node = edges_node[index];
    const auto edge_id = checked_identifier(require_string(edge_node, "id", "edges[" + std::to_string(index) + "]"),
                                            "edges[" + std::to_string(index) + "].id");
    graph.edges.push_back(read_edge_node(edge_node, edge_id, "edges[" + std::to_string(index) + "]"));
  }

  const auto loops_node = root["composite_loops"];
  if (loops_node && !loops_node.IsNull()) {
    require_sequence(loops_node, "runtime graph.composite_loops");
    enforce_limit(loops_node.size(), active_limits().max_composite_loops, "runtime graph.composite_loops count");
    for (std::size_t index = 0; index < loops_node.size(); ++index) {
      graph.composite_loops.push_back(
          read_composite_loop(loops_node[index], "composite_loops[" + std::to_string(index) + "]"));
    }
  }

  const auto subgraphs_node = root["subgraphs"];
  if (subgraphs_node && !subgraphs_node.IsNull()) {
    require_sequence(subgraphs_node, "runtime graph.subgraphs");
    enforce_limit(subgraphs_node.size(), active_limits().max_components, "runtime graph.subgraphs count");
    std::set<std::string> subgraph_ids;
    for (std::size_t index = 0; index < subgraphs_node.size(); ++index) {
      const auto context = "subgraphs[" + std::to_string(index) + "]";
      const auto subgraph_id =
          checked_identifier(require_string(subgraphs_node[index], "id", context), context + ".id");
      if (!subgraph_ids.insert(subgraph_id).second) {
        throw std::invalid_argument("duplicate subgraph: " + subgraph_id);
      }
      append_subgraph_expansion(subgraphs_node[index], graph, context);
    }
  }

  std::map<std::string, GraphTemplateDefinition> templates;
  const auto templates_node = root["templates"];
  if (templates_node && !templates_node.IsNull()) {
    require_sequence(templates_node, "runtime graph.templates");
    enforce_limit(templates_node.size(), active_limits().max_components, "runtime graph.templates count");
    for (std::size_t index = 0; index < templates_node.size(); ++index) {
      const auto definition = read_graph_template(templates_node[index], "templates[" + std::to_string(index) + "]");
      if (!templates.emplace(definition.id, definition).second) {
        throw std::invalid_argument("duplicate template: " + definition.id);
      }
    }
  }

  const auto instances_node = root["template_instances"];
  if (instances_node && !instances_node.IsNull()) {
    require_sequence(instances_node, "runtime graph.template_instances");
    enforce_limit(instances_node.size(), active_limits().max_components, "runtime graph.template_instances count");
    for (std::size_t index = 0; index < instances_node.size(); ++index) {
      append_template_instance_expansion(instances_node[index], templates, graph,
                                         "template_instances[" + std::to_string(index) + "]");
    }
  }

  enforce_limit(graph.components.size(), active_limits().max_components, "runtime graph expanded components count");
  enforce_limit(graph.edges.size(), active_limits().max_edges, "runtime graph expanded edges count");
  enforce_limit(graph.composite_loops.size(), active_limits().max_composite_loops,
                "runtime graph expanded composite_loops count");
  return graph;
}

void enforce_config_view_string_limits(const ConfigView& config, const std::string& context,
                                       const GraphInputLimits& limits) {
  for (const auto& [key, value] : config.values) {
    enforce_limit(key.size(), limits.max_identifier_bytes, context + "." + key + " key length");
    enforce_limit(value.size(), limits.max_config_value_bytes, context + "." + key + " value size");
  }
}

void enforce_graph_string_limits(const GraphSpec& graph, const GraphInputLimits& limits) {
  enforce_identifier_limit(graph.name, "runtime graph.graph.name", limits);
  enforce_string_limit(graph.kind, "runtime graph.graph.kind", limits);
  enforce_string_limit(graph.clock.runtime_domain, "runtime graph.graph.clock.runtime_domain", limits);
  enforce_string_limit(graph.clock.event_domain, "runtime graph.graph.clock.event_domain", limits);
  enforce_config_view_string_limits(graph.config, "runtime graph.graph.config", limits);

  for (const auto& lane : graph.lanes) {
    enforce_identifier_limit(lane.id, "lanes id", limits);
    enforce_string_limit(lane.type, "lanes." + lane.id + ".type", limits);
    enforce_string_limit(lane.priority, "lanes." + lane.id + ".priority", limits);
    enforce_string_limit(lane.overflow, "lanes." + lane.id + ".overflow", limits);
    enforce_string_limit(lane.overrun_policy, "lanes." + lane.id + ".overrun_policy", limits);
    enforce_string_limit(lane.thread_name, "lanes." + lane.id + ".thread_name", limits);
    enforce_string_limit(lane.rt_policy, "lanes." + lane.id + ".rt_policy", limits);
    enforce_string_limit(lane.isolation_intent, "lanes." + lane.id + ".isolation_intent", limits);
  }

  for (const auto& component : graph.components) {
    enforce_identifier_limit(component.id, "components." + component.id + ".id", limits);
    enforce_string_limit(component.type, "components." + component.id + ".type", limits);
    enforce_string_limit(component.boundary.descriptor, "components." + component.id + ".boundary.descriptor", limits);
    enforce_string_limit(to_string(component.boundary.role), "components." + component.id + ".boundary.role", limits);
    enforce_config_view_string_limits(component.config, "components." + component.id + ".config", limits);
    for (const auto& dependency : component.depends_on) {
      enforce_identifier_limit(dependency, "components." + component.id + ".depends_on", limits);
    }
    for (const auto& source : component.event_sources) {
      enforce_string_limit(source.id, "components." + component.id + ".event_sources.id", limits);
      enforce_string_limit(source.type, "components." + component.id + ".event_sources.type", limits);
      enforce_string_limit(source.input, "components." + component.id + ".event_sources.input", limits);
      for (const auto& input : source.inputs) {
        enforce_string_limit(input, "components." + component.id + ".event_sources.inputs", limits);
      }
    }
    enforce_string_limit(component.trigger_policy.type, "components." + component.id + ".trigger_policy.type", limits);
    enforce_string_limit(component.trigger_policy.input, "components." + component.id + ".trigger_policy.input",
                         limits);
    enforce_string_limit(component.trigger_policy.condition, "components." + component.id + ".trigger_policy.condition",
                         limits);
    for (const auto& input : component.trigger_policy.inputs) {
      enforce_string_limit(input, "components." + component.id + ".trigger_policy.inputs", limits);
    }
    enforce_identifier_limit(component.execution.lane, "components." + component.id + ".execution.lane", limits);
    enforce_string_limit(component.execution.priority, "components." + component.id + ".execution.priority", limits);
    enforce_string_limit(component.execution.on_error, "components." + component.id + ".execution.on_error", limits);
  }

  for (const auto& edge : graph.edges) {
    enforce_identifier_limit(edge.id, "edges." + edge.id + ".id", limits);
    enforce_string_limit(edge.from, "edges." + edge.id + ".from", limits);
    enforce_string_limit(edge.to, "edges." + edge.id + ".to", limits);
    enforce_string_limit(edge.invalid_kind, "edges." + edge.id + ".kind", limits);
    enforce_string_limit(edge.policy.mode, "edges." + edge.id + ".policy.mode", limits);
    enforce_string_limit(edge.policy.overflow, "edges." + edge.id + ".policy.overflow", limits);
    enforce_string_limit(edge.policy.timestamp_domain, "edges." + edge.id + ".policy.timestamp_domain", limits);
    enforce_string_limit(edge.policy.copy_policy, "edges." + edge.id + ".policy.copy_policy", limits);
    enforce_string_limit(edge.policy.owner, "edges." + edge.id + ".policy.owner", limits);
    enforce_string_limit(edge.policy.readers, "edges." + edge.id + ".policy.readers", limits);
  }

  for (const auto& loop : graph.composite_loops) {
    enforce_identifier_limit(loop.id, "composite_loops." + loop.id + ".id", limits);
    for (const auto& component : loop.components) {
      enforce_identifier_limit(component, "composite_loops." + loop.id + ".components", limits);
    }
    enforce_string_limit(loop.loop_policy.type, "composite_loops." + loop.id + ".loop_policy.type", limits);
    enforce_string_limit(loop.loop_policy.drop_policy, "composite_loops." + loop.id + ".loop_policy.drop_policy",
                         limits);
    enforce_string_limit(loop.loop_policy.convergence, "composite_loops." + loop.id + ".loop_policy.convergence",
                         limits);
    enforce_string_limit(loop.loop_policy.partial_success,
                         "composite_loops." + loop.id + ".loop_policy.partial_success", limits);
  }

  for (const auto& hierarchy : graph.hierarchy) {
    enforce_identifier_limit(hierarchy.id, "subgraphs." + hierarchy.id + ".id", limits);
    for (const auto& component : hierarchy.components) {
      enforce_identifier_limit(component, "subgraphs." + hierarchy.id + ".components", limits);
    }
    for (const auto& edge : hierarchy.edges) {
      enforce_identifier_limit(edge, "subgraphs." + hierarchy.id + ".edges", limits);
    }
    for (const auto& loop : hierarchy.composite_loops) {
      enforce_identifier_limit(loop, "subgraphs." + hierarchy.id + ".composite_loops", limits);
    }
  }
}

std::string read_bounded_graph_file(const std::string& path, const GraphInputLimits& limits) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::invalid_argument("failed to open graph file: " + path);
  }

  std::string text;
  char buffer[4096];
  while (input) {
    input.read(buffer, sizeof(buffer));
    const auto count = input.gcount();
    if (count <= 0) {
      break;
    }
    const auto chunk_size = static_cast<std::size_t>(count);
    if (text.size() > limits.max_graph_input_bytes || chunk_size > limits.max_graph_input_bytes - text.size()) {
      throw std::invalid_argument("graph input size exceeds limit " + std::to_string(limits.max_graph_input_bytes));
    }
    text.append(buffer, chunk_size);
  }
  if (input.bad()) {
    throw std::invalid_argument("failed to read graph file: " + path);
  }
  return text;
}

// Counts YAML alias references with yaml-cpp's event parser instead of a raw text scan. Every `*anchor`
// reference emits exactly one OnAlias event regardless of the anchor name's characters (so digit-named
// aliases like `*1` are counted), and `*`-containing block-scalar text is reported as scalar content, not an
// alias (so literal text is never miscounted). The event stream is linear in the source tokens — aliases are
// NOT expanded into subtrees — so this stays cheap even for nested-alias ("billion laughs") inputs and runs
// before the potentially expensive node-tree build in YAML::Load.
class AliasCountingEventHandler : public YAML::EventHandler {
public:
  void OnDocumentStart(const YAML::Mark& /*mark*/) override {}
  void OnDocumentEnd() override {}
  void OnNull(const YAML::Mark& /*mark*/, YAML::anchor_t /*anchor*/) override {}
  void OnAlias(const YAML::Mark& /*mark*/, YAML::anchor_t /*anchor*/) override {
    ++count_;
  }
  void OnScalar(const YAML::Mark& /*mark*/, const std::string& /*tag*/, YAML::anchor_t /*anchor*/,
                const std::string& /*value*/) override {}
  void OnSequenceStart(const YAML::Mark& /*mark*/, const std::string& /*tag*/, YAML::anchor_t /*anchor*/,
                       YAML::EmitterStyle::value /*style*/) override {}
  void OnSequenceEnd() override {}
  void OnMapStart(const YAML::Mark& /*mark*/, const std::string& /*tag*/, YAML::anchor_t /*anchor*/,
                  YAML::EmitterStyle::value /*style*/) override {}
  void OnMapEnd() override {}

  std::size_t count() const {
    return count_;
  }

private:
  std::size_t count_{0u};
};

std::size_t count_yaml_alias_references(const std::string& text) {
  std::istringstream stream(text);
  YAML::Parser parser(stream);
  AliasCountingEventHandler handler;
  try {
    while (parser.HandleNextDocument(handler)) {
      // Drive the event stream across every document in the input.
    }
  } catch (const YAML::Exception&) {
    // Malformed YAML: let the normal YAML::Load path surface a precise parse error rather than reporting an
    // alias overflow here. A genuine alias bomb must parse to expand, so it is still counted before that throw.
    return handler.count();
  }
  return handler.count();
}

} // namespace

GraphSpec load_graph_text(const std::string& text) {
  return load_graph_text(text, default_graph_input_limits());
}

GraphSpec load_graph_text(const std::string& text, const GraphInputLimits& limits) {
  enforce_limit(text.size(), limits.max_graph_input_bytes, "graph input size");
  enforce_valid_utf8(text);
  // Bound YAML alias expansion before YAML::Load: nested aliases can blow up traversal cost exponentially.
  const auto alias_count = count_yaml_alias_references(text);
  if (alias_count > limits.max_yaml_alias_count) {
    throw std::invalid_argument("graph input uses too many YAML aliases (" + std::to_string(alias_count) +
                                " exceeds limit " + std::to_string(limits.max_yaml_alias_count) + ")");
  }
  const ScopedGraphInputLimits scope(limits);
  try {
    auto graph = load_graph_node(YAML::Load(text));
    enforce_graph_string_limits(graph, limits);
    return graph;
  } catch (const YAML::Exception& error) {
    throw std::invalid_argument(std::string("failed to parse graph input: ") + error.what());
  }
}

GraphSpec load_graph_file(const std::string& path) {
  return load_graph_file(path, default_graph_input_limits());
}

GraphSpec load_graph_file(const std::string& path, const GraphInputLimits& limits) {
  return load_graph_text(read_bounded_graph_file(path, limits), limits);
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
  root["hierarchy"] = nlohmann::json::array();
  for (const auto& hierarchy : graph.hierarchy) {
    root["hierarchy"].push_back({{"id", hierarchy.id},
                                 {"components", hierarchy.components},
                                 {"edges", hierarchy.edges},
                                 {"composite_loops", hierarchy.composite_loops},
                                 {"expansion", "compile_time_namespace"}});
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
