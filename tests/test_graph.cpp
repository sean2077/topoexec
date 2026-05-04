#include "topoexec/runtime/graph.hpp"

#include <gtest/gtest.h>

#include <algorithm>
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
  return std::any_of(sets.begin(), sets.end(), [&expected](const auto& value) {
    return sorted(value) == expected;
  });
}

}  // namespace

TEST(Graph, LoadsAndValidatesSchemaVersionOne) {
  const auto graph = minimal_graph();
  const auto result = topoexec::validate_graph_structure(graph);
  ASSERT_TRUE(result.ok) << result.errors.front();
  EXPECT_EQ(result.compiled_plan.region_order.size(), 2u);
  EXPECT_NE(topoexec::graph_plan_json(graph, result.compiled_plan).find("\"schema_version\": 1"), std::string::npos);
  EXPECT_NE(topoexec::graph_mermaid(graph, result.compiled_plan).find("flowchart TD"), std::string::npos);
}

TEST(Graph, RejectsUnknownRootFields) {
  EXPECT_THROW(
      (void)topoexec::load_graph_text(R"(
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
  EXPECT_TRUE(has_error_containing(
      result.errors, "composite_loop partial must exactly match one immediate "
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
  const auto loop_region = std::find_if(
      result.compiled_plan.regions.begin(), result.compiled_plan.regions.end(),
      [](const auto& region) { return region.id == "abc_loop"; });
  ASSERT_NE(loop_region, result.compiled_plan.regions.end());
  EXPECT_EQ(sorted(loop_region->components),
            std::vector<std::string>({"a", "b", "c"}));
  EXPECT_EQ(result.compiled_plan.region_order, std::vector<std::string>({"abc_loop", "d"}));
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
