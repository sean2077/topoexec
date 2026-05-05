#pragma once

// Public API category: stable 0.x C++ builder helpers over GraphSpec.

#include "topoexec/runtime/graph.hpp"

#include <string>
#include <utility>
#include <vector>

namespace topoexec {

class GraphBuilder {
public:
  explicit GraphBuilder(std::string name, std::string kind = "runnable") {
    graph_.schema_version = 1;
    graph_.name = std::move(name);
    graph_.kind = std::move(kind);
  }

  GraphBuilder& lane(LaneSpec lane) {
    graph_.lanes.push_back(std::move(lane));
    return *this;
  }

  GraphBuilder& event_loop_lane(std::string id) {
    LaneSpec lane;
    lane.id = std::move(id);
    lane.type = "event_loop";
    return this->lane(std::move(lane));
  }

  GraphBuilder& component(ComponentNodeSpec component) {
    graph_.components.push_back(std::move(component));
    return *this;
  }

  GraphBuilder& edge(EdgeSpec edge) {
    graph_.edges.push_back(std::move(edge));
    return *this;
  }

  GraphBuilder& composite_loop(CompositeLoopSpec loop) {
    graph_.composite_loops.push_back(std::move(loop));
    return *this;
  }

  GraphSpec build() const {
    return graph_;
  }

private:
  GraphSpec graph_;
};

inline EventSourceSpec manual_event_source() {
  EventSourceSpec source;
  source.type = "manual";
  return source;
}

inline EventSourceSpec message_event_source(std::vector<std::string> inputs) {
  EventSourceSpec source;
  source.type = "message";
  source.inputs = std::move(inputs);
  return source;
}

inline TriggerPolicySpec manual_trigger() {
  TriggerPolicySpec policy;
  policy.type = "manual";
  return policy;
}

inline TriggerPolicySpec any_input_trigger(std::vector<std::string> inputs) {
  TriggerPolicySpec policy;
  policy.type = "any_input";
  policy.inputs = std::move(inputs);
  return policy;
}

inline ExecutionSpec lane_execution(std::string lane_id) {
  ExecutionSpec execution;
  execution.lane = std::move(lane_id);
  return execution;
}

inline ComponentNodeSpec component_node(std::string id, std::string type, std::vector<EventSourceSpec> event_sources,
                                        TriggerPolicySpec trigger_policy, ExecutionSpec execution,
                                        BoundaryDescriptor boundary = {}) {
  ComponentNodeSpec component;
  component.id = std::move(id);
  component.type = std::move(type);
  component.event_sources = std::move(event_sources);
  component.trigger_policy = std::move(trigger_policy);
  component.execution = std::move(execution);
  component.boundary = std::move(boundary);
  return component;
}

inline EdgePolicySpec latest_shared_policy() {
  EdgePolicySpec policy;
  policy.mode = "latest";
  policy.overflow = "overwrite";
  policy.copy_policy = "shared_view";
  return policy;
}

inline EdgeSpec immediate_edge(std::string id, std::string from, std::string to,
                               EdgePolicySpec policy = latest_shared_policy()) {
  EdgeSpec edge;
  edge.id = std::move(id);
  edge.from = std::move(from);
  edge.to = std::move(to);
  edge.kind = EdgeKind::kImmediate;
  edge.has_kind = true;
  edge.policy = std::move(policy);
  return edge;
}

} // namespace topoexec
