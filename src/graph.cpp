#include "topoexec/runtime/graph.hpp"

#include "topoexec/runtime/channel.hpp"
#include "topoexec/runtime/diagnostics.hpp"
#include "topoexec/runtime/trigger_policy.hpp"

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace topoexec {
namespace {

std::string diagnostic_code_for(const std::string& error) {
  if (error.find("unknown component") != std::string::npos ||
      error.find("unknown target component") != std::string::npos ||
      error.find("unknown source component") != std::string::npos ||
      error.find("references missing component") != std::string::npos) {
    return "unknown_component";
  }
  if (error.find("unknown output port") != std::string::npos || error.find("unknown input port") != std::string::npos) {
    return "unknown_port";
  }
  if (error.find("duplicate component") != std::string::npos || error.find("duplicate edge") != std::string::npos ||
      error.find("duplicate lane") != std::string::npos ||
      error.find("duplicate composite_loop") != std::string::npos) {
    return "duplicate_id";
  }
  if (error.find("immediate cycle") != std::string::npos) {
    return "immediate_cycle_without_loop";
  }
  if (error.find("does not exactly match") != std::string::npos ||
      error.find("must match an immediate SCC") != std::string::npos) {
    return "partial_composite_loop";
  }
  if (error.find("does not own a cycle") != std::string::npos) {
    return "decorative_composite_loop";
  }
  if (error.find("multiple writers") != std::string::npos) {
    return "multi_state_writer";
  }
  if (error.find("move_only") != std::string::npos) {
    return "invalid_move_only_multireader";
  }
  if (error.find("policy.") != std::string::npos || error.find("capacity") != std::string::npos ||
      error.find("overflow") != std::string::npos) {
    return "invalid_channel_policy";
  }
  if (error.find("unsupported type") != std::string::npos && error.find("lane ") != std::string::npos) {
    return "unsupported_lane_type";
  }
  if (error.find("trigger_policy") != std::string::npos && error.find("input") != std::string::npos) {
    return "trigger_missing_input";
  }
  if (error.find("trigger_policy") != std::string::npos || error.find("event_source") != std::string::npos) {
    return "incompatible_trigger_edge_mode";
  }
  if (error.find("execution.on_error") != std::string::npos) {
    return "unsupported_error_policy";
  }
  return "graph_validation_error";
}

std::string suggested_fix_for(const std::string& code) {
  if (const auto descriptor = graph_diagnostic_descriptor(code); descriptor.has_value()) {
    return descriptor->suggested_fix;
  }
  return "Inspect the graph path and schema reference for the invalid contract.";
}

GraphDiagnostic make_diagnostic(std::string error) {
  GraphDiagnostic diagnostic;
  diagnostic.message = std::move(error);
  diagnostic.code = diagnostic_code_for(diagnostic.message);
  diagnostic.suggested_fix = suggested_fix_for(diagnostic.code);
  auto token_after = [&](std::string_view prefix) -> std::string {
    if (diagnostic.message.rfind(prefix, 0) != 0u) {
      return {};
    }
    const auto begin = prefix.size();
    const auto end = diagnostic.message.find(' ', begin);
    return diagnostic.message.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
  };
  if (auto component = token_after("component "); !component.empty()) {
    diagnostic.graph_path = "components." + component;
    diagnostic.involved_components.push_back(std::move(component));
  } else if (auto edge = token_after("edge "); !edge.empty()) {
    diagnostic.graph_path = "edges." + edge;
    diagnostic.involved_edges.push_back(std::move(edge));
  } else if (auto lane = token_after("lane "); !lane.empty()) {
    diagnostic.graph_path = "lanes." + lane;
  } else if (auto loop = token_after("composite_loop "); !loop.empty()) {
    diagnostic.graph_path = "composite_loops." + loop;
  } else if (diagnostic.message.find("graph.clock") == 0u) {
    diagnostic.graph_path = "graph.clock";
  } else if (diagnostic.message.find("runnable graph") == 0u) {
    diagnostic.graph_path = "graph";
  }
  return diagnostic;
}

void add_error(GraphValidationResult& result, std::string error) {
  result.ok = false;
  result.diagnostics.push_back(make_diagnostic(error));
  result.errors.push_back(result.diagnostics.back().message);
}

void add_advisory(GraphValidationResult& result, std::string code, std::string message, std::string graph_path) {
  GraphDiagnostic diagnostic;
  diagnostic.code = std::move(code);
  diagnostic.severity = "advisory";
  diagnostic.message = std::move(message);
  diagnostic.graph_path = std::move(graph_path);
  diagnostic.suggested_fix = suggested_fix_for(diagnostic.code);
  result.diagnostics.push_back(std::move(diagnostic));
}

void add_error(GraphCompileResult& result, std::string error) {
  result.ok = false;
  result.diagnostics.push_back(make_diagnostic(error));
  result.errors.push_back(result.diagnostics.back().message);
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
         policy == "fail_fast" || policy == "reject";
}

bool is_allowed_lane_overflow(const std::string& policy) {
  return is_allowed_drop_policy(policy) || policy == "reject_new";
}

bool is_allowed_overrun_policy(const std::string& policy) {
  return policy == "drop_tick" || policy == "skip_next" || policy == "catch_up_once";
}

bool is_non_default_advisory_lane_field(const LaneSpec& lane, const std::string& field) {
  if (field == "priority") {
    return !lane.priority.empty();
  }
  if (field == "thread_name") {
#ifdef __linux__
    if (lane.type == "thread_pool") {
      return false;
    }
#endif
    return !lane.thread_name.empty();
  }
  if (field == "cpu_affinity") {
    return !lane.cpu_affinity.empty();
  }
  if (field == "nice_priority") {
    return lane.nice_priority != 0;
  }
  if (field == "rt_policy") {
    return !lane.rt_policy.empty() && lane.rt_policy != "none";
  }
  if (field == "rt_priority") {
    return lane.rt_priority != 0;
  }
  if (field == "isolation_intent") {
    return !lane.isolation_intent.empty() && lane.isolation_intent != "none";
  }
  if (field == "wall_clock_enabled") {
    if (lane.type == "fixed_rate") {
      return false;
    }
    return lane.wall_clock_enabled;
  }
  return false;
}

bool is_allowed_copy_policy(const std::string& policy) {
  return policy == "copy" || policy == "shared_view" || policy == "loaned_view" || policy == "move_only";
}

bool is_allowed_owner(const std::string& owner) {
  return owner == "producer" || owner == "runtime" || owner == "consumer";
}

bool is_allowed_readers(const std::string& readers) {
  return readers == "single" || readers == "multi" || readers == "multiple";
}

bool is_allowed_event_source_type(const std::string& type) {
  return type == "message" || type == "timer" || type == "request" || type == "action_goal" ||
         type == "action_cancel" || type == "task_ready" || type == "future_ready" || type == "manual";
}

bool is_allowed_trigger_policy_type(const std::string& type) {
  return type == "on_event" || type == "any_input" || type == "all_inputs" || type == "time_sync" || type == "batch" ||
         type == "request" || type == "task_ready" || type == "manual";
}

bool is_allowed_loop_policy_type(const std::string& type) {
  return type == "fixed_point" || type == "transaction" || type == "coalesced_event" || type == "async_task";
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
        add_error(result,
                  "component " + graph.components[index].id + " has duplicate lifecycle dependency " + dependency);
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
  if (graph.schema_version != kTopoExecSchemaVersion) {
    add_error(result, "schema_version must be " + std::to_string(kTopoExecSchemaVersion));
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
    if (lane.queue_capacity < 0) {
      add_error(result, "lane " + lane.id + " queue_capacity must be non-negative");
    }
    if (lane.period_ms < 0) {
      add_error(result, "lane " + lane.id + " period_ms must be non-negative");
    }
    if (lane.tick_budget_ms < 0) {
      add_error(result, "lane " + lane.id + " tick_budget_ms must be non-negative");
    }
    if (!is_allowed_lane_overflow(lane.overflow)) {
      add_error(result, "lane " + lane.id + " has unsupported overflow " + lane.overflow);
    }
    if (!is_allowed_overrun_policy(lane.overrun_policy)) {
      add_error(result, "lane " + lane.id + " has unsupported overrun_policy " + lane.overrun_policy);
    }
    for (const auto& field : {"priority", "thread_name", "cpu_affinity", "nice_priority", "rt_policy", "rt_priority",
                              "isolation_intent", "wall_clock_enabled"}) {
      if (is_non_default_advisory_lane_field(lane, field)) {
        add_advisory(result, "advisory_lane_field_ignored",
                     "lane " + lane.id + " advisory field " + field +
                         " is parsed and preserved but not enforced by the current runtime",
                     "lanes." + lane.id + "." + field);
      }
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
    if (!component.execution.priority.empty() && component.execution.priority != "normal") {
      add_advisory(result, "advisory_execution_field_ignored",
                   "component " + component.id +
                       " execution.priority is parsed and preserved but not used for runtime admission yet",
                   "components." + component.id + ".execution.priority");
    }
    if (component.execution.on_error != "fail_fast") {
      add_error(result, "component " + component.id + " has unsupported execution.on_error " +
                            component.execution.on_error + " (only fail_fast is implemented)");
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
    if (component.trigger_policy.batch_size < 0 || component.trigger_policy.batch_window_ms < 0 ||
        component.trigger_policy.sync_slop_ms < 0 || component.trigger_policy.min_interval_ms < 0 ||
        component.trigger_policy.max_latency_ms < 0) {
      add_error(result, "component " + component.id + " trigger_policy numeric fields must be non-negative");
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
  std::map<std::string, std::vector<std::string>> state_writers_by_target;
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
    if (edge.policy.max_inflight < 0) {
      add_error(result, "edge " + edge.id + " policy.max_inflight must be non-negative");
    }
    if (edge.policy.max_inflight > 0 && edge.kind != EdgeKind::kAsync) {
      add_error(result, "edge " + edge.id + " policy.max_inflight applies only to async edges");
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
    if (edge.policy.copy_policy == "move_only" && edge.policy.readers != "single") {
      add_error(result, "edge " + edge.id + " move_only copy_policy requires readers: single");
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
    if (edge.kind == EdgeKind::kState) {
      state_writers_by_target[edge.to].push_back(edge.id);
    }

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

  for (const auto& [target, writers] : state_writers_by_target) {
    if (writers.size() > 1u) {
      add_error(result, "state edge target has multiple writers: " + target + " via " + join_ids(writers));
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

void append_metric(std::vector<RuntimeMetricSample>& metrics, std::string name, double value,
                   std::string component_id = {}, std::string lane = {}, std::string channel_id = {}) {
  metrics.push_back(
      RuntimeMetricSample{std::move(name), value, std::move(component_id), std::move(lane), std::move(channel_id), {}});
}

} // namespace

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

GraphDryRunResult dry_run_graph(const GraphSpec& graph, const ComponentRegistry& registry,
                                std::size_t tick_iterations) {
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
    out << "- region " << region.id << " kind=" << to_string(region.kind)
        << " components=" << join_ids(region.components);
    if (region.kind == CompiledRegionKind::kCompositeLoop) {
      out << " loop_policy=" << region.loop_policy.type;
    }
    out << "\n";
  }
  return out.str();
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
    out << "  " << component_id_from_endpoint(edge.from) << " -->|\"" << edge.id << ":" << to_string(edge.kind) << "/"
        << edge.policy.mode << "\"| " << component_id_from_endpoint(edge.to) << "\n";
  }
  out << "  %% region_order: " << join_ids(plan.region_order) << "\n";
  return out.str();
}

} // namespace topoexec
