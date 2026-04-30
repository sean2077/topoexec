#include "topoexec/runtime/graph.hpp"

#include <gtest/gtest.h>

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
