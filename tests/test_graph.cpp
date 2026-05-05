#include "topoexec/runtime/graph.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <random>
#include <string>
#include <utility>

namespace {

topoexec::GraphSpec minimal_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: minimal, kind: internal_test}
lanes: {main: {type: event_loop}}
components:
  - id: a
    type: topoexec.test.Source
    boundary: {role: input, descriptor: test}
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: b
    type: topoexec.test.Sink
    boundary: {role: output, descriptor: test}
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
edges:
  - id: e
    kind: immediate
    from: a.out
    to: b.in
    policy: {mode: latest, copy_policy: shared_view}
)");
}

bool has_error_containing(const std::vector<std::string>& errors, const std::string& text) {
  for (const auto& error : errors) {
    if (error.find(text) != std::string::npos) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> sorted(std::vector<std::string> values) {
  std::sort(values.begin(), values.end());
  return values;
}

bool contains_set(const std::vector<std::vector<std::string>>& sets, std::vector<std::string> expected) {
  expected = sorted(std::move(expected));
  return std::any_of(sets.begin(), sets.end(), [&expected](const auto& value) { return sorted(value) == expected; });
}

std::string component_id_from_endpoint(const std::string& endpoint) {
  const auto dot = endpoint.find('.');
  if (dot == std::string::npos) {
    return endpoint;
  }
  return endpoint.substr(0, dot);
}

topoexec::ComponentNodeSpec random_dag_component(std::string id) {
  topoexec::ComponentNodeSpec component;
  component.id = std::move(id);
  component.type = "topoexec.test.Node";
  component.event_sources = {topoexec::EventSourceSpec{}};
  component.event_sources.front().type = "manual";
  component.trigger_policy.type = "manual";
  component.execution.lane = "main";
  return component;
}

topoexec::EdgeSpec random_dag_edge(std::string id, const std::string& from, const std::string& to) {
  topoexec::EdgeSpec edge;
  edge.id = std::move(id);
  edge.from = from + ".out";
  edge.to = to + ".in";
  edge.has_kind = true;
  edge.kind = topoexec::EdgeKind::kImmediate;
  edge.policy.mode = "latest";
  edge.policy.copy_policy = "shared_view";
  return edge;
}

topoexec::GraphSpec fixed_seed_random_dag(std::mt19937& rng, int graph_index) {
  topoexec::GraphSpec graph;
  graph.schema_version = 1;
  graph.name = "random_dag_" + std::to_string(graph_index);
  graph.kind = "internal_test";
  graph.lanes = {topoexec::LaneSpec{}};
  graph.lanes.front().id = "main";
  graph.lanes.front().type = "event_loop";

  const int component_count = 6 + (graph_index % 4);
  for (int index = 0; index < component_count; ++index) {
    graph.components.push_back(random_dag_component("c" + std::to_string(index)));
  }

  int edge_index = 0;
  for (int index = 0; index + 1 < component_count; ++index) {
    graph.edges.push_back(random_dag_edge("chain_" + std::to_string(edge_index++), graph.components[index].id,
                                          graph.components[index + 1].id));
  }

  std::bernoulli_distribution include_edge(0.35);
  for (int from = 0; from < component_count; ++from) {
    for (int to = from + 2; to < component_count; ++to) {
      if (!include_edge(rng)) {
        continue;
      }
      graph.edges.push_back(random_dag_edge("random_" + std::to_string(edge_index++), graph.components[from].id,
                                            graph.components[to].id));
    }
  }
  return graph;
}

topoexec::GraphSpec fixed_seed_random_cycle(std::mt19937& rng, int graph_index) {
  topoexec::GraphSpec graph;
  graph.schema_version = 1;
  graph.name = "random_cycle_" + std::to_string(graph_index);
  graph.kind = "internal_test";
  graph.lanes = {topoexec::LaneSpec{}};
  graph.lanes.front().id = "main";
  graph.lanes.front().type = "event_loop";

  const int component_count = 3 + static_cast<int>(rng() % 4u);
  for (int index = 0; index < component_count; ++index) {
    graph.components.push_back(random_dag_component("c" + std::to_string(index)));
  }

  int edge_index = 0;
  for (int index = 0; index < component_count; ++index) {
    const auto& from = graph.components[index].id;
    const auto& to = graph.components[(index + 1) % component_count].id;
    graph.edges.push_back(random_dag_edge("cycle_" + std::to_string(edge_index++), from, to));
  }

  std::bernoulli_distribution include_chord(0.25);
  for (int from = 0; from < component_count; ++from) {
    for (int to = 0; to < component_count; ++to) {
      if (from == to || to == (from + 1) % component_count || !include_chord(rng)) {
        continue;
      }
      graph.edges.push_back(
          random_dag_edge("chord_" + std::to_string(edge_index++), graph.components[from].id, graph.components[to].id));
    }
  }
  return graph;
}

} // namespace

TEST(Graph, LoadsAndValidatesSchemaVersionOne) {
  const auto graph = minimal_graph();
  const auto result = topoexec::validate_graph_structure(graph);
  ASSERT_TRUE(result.ok) << result.errors.front();
  EXPECT_EQ(result.compiled_plan.region_order.size(), 2u);
  EXPECT_NE(topoexec::graph_plan_json(graph, result.compiled_plan).find("\"schema_version\": 1"), std::string::npos);
  EXPECT_NE(topoexec::graph_mermaid(graph, result.compiled_plan).find("flowchart TD"), std::string::npos);
}

TEST(Graph, ParsesAndValidatesSchedulerLaneAdmissionFields) {
  const auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: lane_fields, kind: internal_test}
lanes:
  pool:
    type: thread_pool
    max_threads: 2
    queue_capacity: 4
    overflow: reject_new
    wall_clock_enabled: false
    period_ms: 10
    tick_budget_ms: 8
components:
  - id: a
    type: topoexec.test.Source
    boundary: {role: input, descriptor: test}
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: pool}
edges: []
)");

  ASSERT_EQ(graph.lanes.size(), 1u);
  EXPECT_EQ(graph.lanes.front().queue_capacity, 4);
  EXPECT_EQ(graph.lanes.front().overflow, "reject_new");
  EXPECT_FALSE(graph.lanes.front().wall_clock_enabled);
  EXPECT_EQ(graph.lanes.front().period_ms, 10);
  EXPECT_EQ(graph.lanes.front().tick_budget_ms, 8);
  const auto result = topoexec::validate_graph_structure(graph);
  ASSERT_TRUE(result.ok) << result.errors.front();
}

TEST(Graph, RejectsInvalidSchedulerLaneAdmissionFields) {
  auto graph = minimal_graph();
  graph.lanes.front().queue_capacity = -1;
  graph.lanes.front().overflow = "mystery";

  const auto result = topoexec::validate_graph_structure(graph);

  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors, "lane main queue_capacity must be non-negative"));
  EXPECT_TRUE(has_error_containing(result.errors, "lane main has unsupported overflow mystery"));
}

TEST(Graph, NonFailFastExecutionPolicyIsParsedButRejected) {
  auto graph = minimal_graph();
  graph.components.front().execution.on_error = "continue";

  const auto result = topoexec::validate_graph_structure(graph);

  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors, "unsupported execution.on_error continue"));
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().code, "unsupported_error_policy");
  EXPECT_EQ(result.diagnostics.front().severity, "error");
  EXPECT_EQ(result.diagnostics.front().graph_path, "components.a");
  EXPECT_EQ(result.diagnostics.front().involved_components, std::vector<std::string>({"a"}));
}

TEST(Graph, RejectsUnknownRootFields) {
  EXPECT_THROW((void)topoexec::load_graph_text(R"(
graph_version: 2
graph: {name: invalid, kind: runnable}
lanes: {main: {type: event_loop}}
components: []
edges: []
)"),
               std::invalid_argument);
}

TEST(Graph, ImmediateCycleRequiresCompositeLoop) {
  auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: loop, kind: internal_test}
lanes: {main: {type: event_loop}}
components:
  - {id: a, type: topoexec.test.A, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
  - {id: b, type: topoexec.test.B, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
edges:
  - {id: ab, kind: immediate, from: a.out, to: b.in, policy: {mode: latest, copy_policy: shared_view}}
  - {id: ba, kind: immediate, from: b.out, to: a.in, policy: {mode: latest, copy_policy: shared_view}}
)");
  auto result = topoexec::validate_graph_structure(graph);
  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors, "immediate cycle detected"));

  topoexec::CompositeLoopSpec loop;
  loop.id = "ab_loop";
  loop.components = {"a", "b"};
  loop.loop_policy.type = "fixed_point";
  loop.loop_policy.max_iterations = 3;
  graph.composite_loops = {loop};
  result = topoexec::validate_graph_structure(graph);
  ASSERT_TRUE(result.ok) << result.errors.front();
  ASSERT_EQ(result.compiled_plan.regions.size(), 1u);
  EXPECT_EQ(result.compiled_plan.regions.front().kind, topoexec::CompiledRegionKind::kCompositeLoop);
}

TEST(Graph, DelayEdgeBreaksImmediateCycle) {
  auto graph = minimal_graph();
  topoexec::EventSourceSpec source;
  source.type = "message";
  source.inputs = {"in"};
  graph.components.front().event_sources = {source};
  graph.components.front().trigger_policy.type = "any_input";
  graph.components.front().trigger_policy.inputs = {"in"};
  topoexec::EdgeSpec edge;
  edge.id = "feedback";
  edge.from = "b.out";
  edge.to = "a.in";
  edge.has_kind = true;
  edge.kind = topoexec::EdgeKind::kDelay;
  edge.policy.mode = "latest";
  edge.policy.copy_policy = "shared_view";
  graph.edges.push_back(edge);

  const auto result = topoexec::validate_graph_structure(graph);
  ASSERT_TRUE(result.ok) << result.errors.front();
  EXPECT_EQ(result.compiled_plan.region_order.size(), 2u);
}

TEST(Graph, AcceptsFailFastOverflowPolicyName) {
  auto graph = minimal_graph();
  graph.edges.front().policy.mode = "queue";
  graph.edges.front().policy.capacity = 1;
  graph.edges.front().policy.overflow = "fail_fast";

  const auto result = topoexec::validate_graph_structure(graph);
  ASSERT_TRUE(result.ok) << result.errors.front();
}

TEST(Graph, MoveOnlyPolicyRequiresSingleReader) {
  auto graph = minimal_graph();
  graph.edges.front().policy.copy_policy = "move_only";
  graph.edges.front().policy.readers = "multi";

  const auto result = topoexec::validate_graph_structure(graph);
  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors, "move_only copy_policy requires readers: single"));
}

TEST(Graph, MaxInflightPolicyAppliesOnlyToAsyncEdges) {
  auto graph = minimal_graph();
  graph.edges.front().policy.max_inflight = 2;

  auto result = topoexec::validate_graph_structure(graph);
  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors, "policy.max_inflight applies only to async edges"));

  graph.edges.front().kind = topoexec::EdgeKind::kAsync;
  result = topoexec::validate_graph_structure(graph);
  ASSERT_TRUE(result.ok) << result.errors.front();
}

TEST(Graph, TriggerPolicyNumericFieldsMustBeNonNegative) {
  auto graph = minimal_graph();
  graph.components.back().trigger_policy.min_interval_ms = -1;

  const auto result = topoexec::validate_graph_structure(graph);

  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors, "trigger_policy numeric fields must be non-negative"));
}

TEST(Graph, PartialCompositeLoopDeclarationIsRejected) {
  auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: partial_loop, kind: internal_test}
lanes: {main: {type: event_loop}}
components:
  - {id: a, type: topoexec.test.A, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
  - {id: b, type: topoexec.test.B, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
  - {id: c, type: topoexec.test.C, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
edges:
  - {id: ab, kind: immediate, from: a.out, to: b.in, policy: {mode: latest, copy_policy: shared_view}}
  - {id: bc, kind: immediate, from: b.out, to: c.in, policy: {mode: latest, copy_policy: shared_view}}
  - {id: ca, kind: immediate, from: c.out, to: a.in, policy: {mode: latest, copy_policy: shared_view}}
composite_loops:
  - id: partial
    components: [a, b]
    loop_policy: {type: fixed_point, max_iterations: 3}
)");
  const auto result = topoexec::validate_graph_structure(graph);
  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors, "immediate cycle detected among components a,b,c"));
  EXPECT_TRUE(has_error_containing(result.errors, "composite_loop partial must exactly match one immediate "
                                                  "cyclic strongly connected component"));
}

TEST(Graph, OverlappingImmediateCyclesCollapseIntoOneCompositeLoopRegion) {
  auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: overlapping_loop, kind: internal_test}
lanes: {main: {type: event_loop}}
components:
  - {id: a, type: topoexec.test.A, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
  - {id: b, type: topoexec.test.B, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
  - {id: c, type: topoexec.test.C, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
  - {id: d, type: topoexec.test.D, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
edges:
  - {id: ab, kind: immediate, from: a.out, to: b.in, policy: {mode: latest, copy_policy: shared_view}}
  - {id: ba, kind: immediate, from: b.out, to: a.in, policy: {mode: latest, copy_policy: shared_view}}
  - {id: bc, kind: immediate, from: b.out, to: c.in, policy: {mode: latest, copy_policy: shared_view}}
  - {id: cb, kind: immediate, from: c.out, to: b.in, policy: {mode: latest, copy_policy: shared_view}}
  - {id: cd, kind: immediate, from: c.out, to: d.in, policy: {mode: latest, copy_policy: shared_view}}
composite_loops:
  - id: abc_loop
    components: [a, b, c]
    loop_policy: {type: fixed_point, max_iterations: 5}
)");
  const auto result = topoexec::validate_graph_structure(graph);
  ASSERT_TRUE(result.ok) << result.errors.front();
  ASSERT_EQ(result.compiled_plan.regions.size(), 2u);
  EXPECT_TRUE(contains_set(result.compiled_plan.immediate_sccs, {"a", "b", "c"}));
  EXPECT_TRUE(contains_set(result.compiled_plan.immediate_sccs, {"d"}));
  EXPECT_EQ(result.compiled_plan.component_region.at("a"), "abc_loop");
  EXPECT_EQ(result.compiled_plan.component_region.at("b"), "abc_loop");
  EXPECT_EQ(result.compiled_plan.component_region.at("c"), "abc_loop");
  const auto loop_region = std::find_if(result.compiled_plan.regions.begin(), result.compiled_plan.regions.end(),
                                        [](const auto& region) { return region.id == "abc_loop"; });
  ASSERT_NE(loop_region, result.compiled_plan.regions.end());
  EXPECT_EQ(sorted(loop_region->components), std::vector<std::string>({"a", "b", "c"}));
  EXPECT_EQ(result.compiled_plan.region_order, std::vector<std::string>({"abc_loop", "d"}));
}

TEST(Graph, ComponentCannotHaveMultipleCompositeLoopOwners) {
  const auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: duplicate_loop_owner, kind: internal_test}
lanes: {main: {type: event_loop}}
components:
  - {id: a, type: topoexec.test.A, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
  - {id: b, type: topoexec.test.B, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
edges:
  - {id: ab, kind: immediate, from: a.out, to: b.in, policy: {mode: latest, copy_policy: shared_view}}
  - {id: ba, kind: immediate, from: b.out, to: a.in, policy: {mode: latest, copy_policy: shared_view}}
composite_loops:
  - id: first
    components: [a, b]
    loop_policy: {type: fixed_point, max_iterations: 3}
  - id: second
    components: [a, b]
    loop_policy: {type: fixed_point, max_iterations: 3}
)");
  const auto result = topoexec::validate_graph_structure(graph);

  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors, "component a is owned by multiple composite_loops"));
  EXPECT_TRUE(has_error_containing(result.errors, "component b is owned by multiple composite_loops"));
}

TEST(Graph, NonImmediateFeedbackEdgesDoNotCreateImmediateSccs) {
  for (const auto kind : {topoexec::EdgeKind::kDelay, topoexec::EdgeKind::kState, topoexec::EdgeKind::kAsync}) {
    auto graph = minimal_graph();
    topoexec::EventSourceSpec source;
    source.type = "message";
    source.inputs = {"in"};
    graph.components.front().event_sources = {source};
    graph.components.front().trigger_policy.type = "any_input";
    graph.components.front().trigger_policy.inputs = {"in"};
    topoexec::EdgeSpec edge;
    edge.id = "feedback_" + topoexec::to_string(kind);
    edge.from = "b.out";
    edge.to = "a.in";
    edge.has_kind = true;
    edge.kind = kind;
    edge.policy.mode = "latest";
    edge.policy.copy_policy = "shared_view";
    graph.edges.push_back(edge);

    const auto result = topoexec::validate_graph_structure(graph);
    ASSERT_TRUE(result.ok) << topoexec::to_string(kind) << ": " << result.errors.front();
    EXPECT_EQ(result.compiled_plan.region_order, std::vector<std::string>({"a", "b"}));
  }
}

TEST(Graph, StateEdgesRejectMultipleWritersToSameTarget) {
  const auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: state_conflict, kind: internal_test}
lanes: {main: {type: event_loop}}
components:
  - {id: left, type: topoexec.test.Left, event_sources: [{type: manual}], trigger_policy: {type: manual}, execution: {lane: main}}
  - {id: right, type: topoexec.test.Right, event_sources: [{type: manual}], trigger_policy: {type: manual}, execution: {lane: main}}
  - {id: sink, type: topoexec.test.Sink, event_sources: [{type: message, inputs: [state]}], trigger_policy: {type: any_input, inputs: [state]}, execution: {lane: main}}
edges:
  - {id: left_state, kind: state, from: left.out, to: sink.state, policy: {mode: latest, copy_policy: shared_view}}
  - {id: right_state, kind: state, from: right.out, to: sink.state, policy: {mode: latest, copy_policy: shared_view}}
)");
  const auto result = topoexec::validate_graph_structure(graph);

  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors, "state edge target has multiple writers: sink.state"));
}

TEST(Graph, BranchingImmediateDagRegionOrderIsDeterministic) {
  const auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: branching, kind: internal_test}
lanes: {main: {type: event_loop}}
components:
  - {id: source, type: topoexec.test.Source, event_sources: [{type: manual}], trigger_policy: {type: manual}, execution: {lane: main}}
  - {id: left, type: topoexec.test.Left, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
  - {id: right, type: topoexec.test.Right, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
  - {id: sink, type: topoexec.test.Sink, event_sources: [{type: message, inputs: [left, right]}], trigger_policy: {type: all_inputs, inputs: [left, right]}, execution: {lane: main}}
edges:
  - {id: source_left, kind: immediate, from: source.out, to: left.in, policy: {mode: latest, copy_policy: shared_view}}
  - {id: source_right, kind: immediate, from: source.out, to: right.in, policy: {mode: latest, copy_policy: shared_view}}
  - {id: left_sink, kind: immediate, from: left.out, to: sink.left, policy: {mode: latest, copy_policy: shared_view}}
  - {id: right_sink, kind: immediate, from: right.out, to: sink.right, policy: {mode: latest, copy_policy: shared_view}}
)");
  const auto first = topoexec::validate_graph_structure(graph);
  const auto second = topoexec::validate_graph_structure(graph);
  ASSERT_TRUE(first.ok) << first.errors.front();
  ASSERT_TRUE(second.ok) << second.errors.front();
  EXPECT_EQ(first.compiled_plan.region_order, std::vector<std::string>({"source", "left", "right", "sink"}));
  EXPECT_EQ(second.compiled_plan.region_order, first.compiled_plan.region_order);
}

TEST(Graph, FixedSeedImmediateDagsCompileWithDeterministicRegionOrder) {
  constexpr std::uint32_t kSeed = 0xC0FFEEu;
  std::mt19937 rng(kSeed);
  for (int graph_index = 0; graph_index < 8; ++graph_index) {
    SCOPED_TRACE("seed=" + std::to_string(kSeed) + " graph_index=" + std::to_string(graph_index));
    const auto graph = fixed_seed_random_dag(rng, graph_index);

    const auto first = topoexec::validate_graph_structure(graph);
    const auto second = topoexec::validate_graph_structure(graph);

    ASSERT_TRUE(first.ok) << (first.errors.empty() ? "" : first.errors.front());
    ASSERT_TRUE(second.ok) << (second.errors.empty() ? "" : second.errors.front());
    EXPECT_EQ(second.compiled_plan.region_order, first.compiled_plan.region_order);
    EXPECT_EQ(first.compiled_plan.region_order.size(), graph.components.size());
    ASSERT_EQ(first.compiled_plan.immediate_sccs.size(), graph.components.size());
    for (const auto& scc : first.compiled_plan.immediate_sccs) {
      EXPECT_EQ(scc.size(), 1u);
    }

    std::map<std::string, std::size_t> region_position;
    for (std::size_t index = 0; index < first.compiled_plan.region_order.size(); ++index) {
      region_position[first.compiled_plan.region_order[index]] = index;
    }
    for (const auto& edge : graph.edges) {
      ASSERT_EQ(edge.kind, topoexec::EdgeKind::kImmediate);
      const auto from = component_id_from_endpoint(edge.from);
      const auto to = component_id_from_endpoint(edge.to);
      ASSERT_NE(region_position.find(from), region_position.end());
      ASSERT_NE(region_position.find(to), region_position.end());
      EXPECT_LT(region_position.at(from), region_position.at(to));
    }
  }
}

TEST(Graph, FixedSeedImmediateCyclesRejectAndAcceptExactCompositeLoop) {
  constexpr std::uint32_t kSeed = 0xC1C1E5u;
  std::mt19937 rng(kSeed);
  for (int graph_index = 0; graph_index < 8; ++graph_index) {
    SCOPED_TRACE("seed=" + std::to_string(kSeed) + " graph_index=" + std::to_string(graph_index));
    auto graph = fixed_seed_random_cycle(rng, graph_index);

    auto rejected = topoexec::validate_graph_structure(graph);
    EXPECT_FALSE(rejected.ok);
    EXPECT_TRUE(has_error_containing(rejected.errors, "immediate cycle detected among components"));
    for (const auto& component : graph.components) {
      EXPECT_TRUE(has_error_containing(rejected.errors, component.id));
    }

    topoexec::CompositeLoopSpec loop;
    loop.id = "cycle_loop_" + std::to_string(graph_index);
    loop.loop_policy.type = "fixed_point";
    loop.loop_policy.max_iterations = 3;
    for (const auto& component : graph.components) {
      loop.components.push_back(component.id);
    }
    graph.composite_loops = {loop};

    const auto accepted = topoexec::validate_graph_structure(graph);
    ASSERT_TRUE(accepted.ok) << (accepted.errors.empty() ? "" : accepted.errors.front());
    ASSERT_EQ(accepted.compiled_plan.regions.size(), 1u);
    EXPECT_EQ(accepted.compiled_plan.regions.front().id, loop.id);
    EXPECT_EQ(accepted.compiled_plan.regions.front().kind, topoexec::CompiledRegionKind::kCompositeLoop);
    EXPECT_TRUE(contains_set(accepted.compiled_plan.immediate_sccs, loop.components));
    for (const auto& component : graph.components) {
      EXPECT_EQ(accepted.compiled_plan.component_region.at(component.id), loop.id);
    }
  }
}
