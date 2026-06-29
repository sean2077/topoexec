#include "topoexec/runtime/diagnostics.hpp"
#include "topoexec/runtime/graph.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
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

bool has_diagnostic(const std::vector<topoexec::GraphDiagnostic>& diagnostics, const std::string& code,
                    const std::string& severity, const std::string& path) {
  return std::any_of(diagnostics.begin(), diagnostics.end(), [&](const auto& diagnostic) {
    return diagnostic.code == code && diagnostic.severity == severity && diagnostic.graph_path == path;
  });
}

const topoexec::GraphDiagnostic* find_diagnostic(const std::vector<topoexec::GraphDiagnostic>& diagnostics,
                                                 const std::string& code) {
  const auto found = std::find_if(diagnostics.begin(), diagnostics.end(),
                                  [&](const auto& diagnostic) { return diagnostic.code == code; });
  return found == diagnostics.end() ? nullptr : &*found;
}

std::vector<std::string> sorted(std::vector<std::string> values) {
  std::sort(values.begin(), values.end());
  return values;
}

bool contains_set(const std::vector<std::vector<std::string>>& sets, std::vector<std::string> expected) {
  expected = sorted(std::move(expected));
  return std::any_of(sets.begin(), sets.end(), [&expected](const auto& value) { return sorted(value) == expected; });
}

class DescriptorComponent : public topoexec::Component {
public:
  explicit DescriptorComponent(topoexec::ComponentDescriptor descriptor) : descriptor_(std::move(descriptor)) {}

  topoexec::ComponentDescriptor describe() const override {
    return descriptor_;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

private:
  topoexec::ComponentDescriptor descriptor_;
};

topoexec::PortDescriptor port(std::string name, std::string schema = topoexec::kTextPayloadSchema,
                              std::string payload_type = {},
                              topoexec::PortMultiplicity multiplicity = topoexec::PortMultiplicity::kSingle,
                              bool required = false) {
  topoexec::PortDescriptor descriptor;
  descriptor.name = std::move(name);
  descriptor.schema = std::move(schema);
  descriptor.payload_type = std::move(payload_type);
  descriptor.multiplicity = multiplicity;
  descriptor.required = required;
  return descriptor;
}

topoexec::ComponentDescriptor component_descriptor(std::string type, topoexec::ComponentRole role,
                                                   std::vector<topoexec::PortDescriptor> inputs,
                                                   std::vector<topoexec::PortDescriptor> outputs) {
  topoexec::ComponentDescriptor descriptor;
  descriptor.type = std::move(type);
  descriptor.name = descriptor.type;
  descriptor.role = role;
  descriptor.inputs = std::move(inputs);
  descriptor.outputs = std::move(outputs);
  return descriptor;
}

topoexec::ComponentRegistry registry_for(std::vector<topoexec::ComponentDescriptor> descriptors) {
  topoexec::ComponentRegistry registry;
  for (auto descriptor : descriptors) {
    const auto type = descriptor.type;
    registry.register_component({type}, [descriptor]() { return std::make_unique<DescriptorComponent>(descriptor); });
  }
  return registry;
}

std::string graph_with_component_count(std::size_t count) {
  std::ostringstream out;
  out << "schema_version: 1\n";
  out << "graph: {name: component_limit, kind: internal_test}\n";
  out << "lanes: {main: {type: event_loop}}\n";
  out << "components:\n";
  for (std::size_t index = 0; index < count; ++index) {
    out << "  - id: c" << index << "\n";
    out << "    type: topoexec.test.Node\n";
    out << "    event_sources: [{type: manual}]\n";
    out << "    trigger_policy: {type: manual}\n";
    out << "    execution: {lane: main}\n";
  }
  out << "edges: []\n";
  return out.str();
}

std::string graph_with_edge_count(std::size_t count) {
  std::ostringstream out;
  out << "schema_version: 1\n";
  out << "graph: {name: edge_limit, kind: internal_test}\n";
  out << "lanes: {main: {type: event_loop}}\n";
  out << "components:\n";
  out << "  - {id: a, type: topoexec.test.A, event_sources: [{type: manual}], trigger_policy: {type: manual}, "
         "execution: {lane: main}}\n";
  out << "  - {id: b, type: topoexec.test.B, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: "
         "any_input, inputs: [in]}, execution: {lane: main}}\n";
  out << "edges:\n";
  for (std::size_t index = 0; index < count; ++index) {
    out << "  - {id: e" << index << ", kind: immediate, from: a.out, to: b.in}\n";
  }
  return out.str();
}

std::filesystem::path write_temp_graph_file(const std::string& name, const std::string& content) {
  const auto path = std::filesystem::temp_directory_path() / name;
  std::ofstream output(path, std::ios::binary);
  output << content;
  return path;
}

std::string component_id_from_endpoint(const std::string& endpoint) {
  const auto dot = endpoint.rfind('.');
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

TEST(GraphInputLimits, RejectsOversizedGraphTextBeforeYamlParse) {
  std::string text(1024u * 1024u + 1u, 'x');
  EXPECT_THROW(
      {
        try {
          (void)topoexec::load_graph_text(text);
        } catch (const std::invalid_argument& error) {
          EXPECT_NE(std::string(error.what()).find("graph input size exceeds limit"), std::string::npos);
          throw;
        }
      },
      std::invalid_argument);
}

TEST(GraphInputLimits, ExposesDefaultLimitContract) {
  const auto& limits = topoexec::default_graph_input_limits();
  EXPECT_EQ(limits.max_graph_input_bytes, 1024u * 1024u);
  EXPECT_EQ(limits.max_lanes, 256u);
  EXPECT_EQ(limits.max_components, 4096u);
  EXPECT_EQ(limits.max_edges, 8192u);
  EXPECT_EQ(limits.max_composite_loops, 1024u);
  EXPECT_EQ(limits.max_identifier_bytes, 128u);
  EXPECT_EQ(limits.max_config_depth, 8u);
  EXPECT_EQ(limits.max_config_value_bytes, 4096u);
  EXPECT_EQ(limits.max_string_bytes, 4096u);
  EXPECT_EQ(limits.max_yaml_alias_count, 32u);
}

TEST(GraphInputLimits, CustomLimitsApplyToGraphText) {
  auto limits = topoexec::default_graph_input_limits();
  limits.max_components = 1u;

  EXPECT_THROW(
      {
        try {
          (void)topoexec::load_graph_text(graph_with_component_count(2u), limits);
        } catch (const std::invalid_argument& error) {
          EXPECT_NE(std::string(error.what()).find("runtime graph.components count exceeds limit 1"),
                    std::string::npos);
          throw;
        }
      },
      std::invalid_argument);
}

TEST(GraphInputLimits, CustomFileLimitRejectsBeforeUnboundedRead) {
  auto limits = topoexec::default_graph_input_limits();
  limits.max_graph_input_bytes = 32u;
  const auto path = write_temp_graph_file("topoexec_graph_limit.yaml", graph_with_component_count(1u));

  EXPECT_THROW(
      {
        try {
          (void)topoexec::load_graph_file(path.string(), limits);
        } catch (const std::invalid_argument& error) {
          EXPECT_NE(std::string(error.what()).find("graph input size exceeds limit 32"), std::string::npos);
          std::filesystem::remove(path);
          throw;
        }
      },
      std::invalid_argument);
  std::filesystem::remove(path);
}

TEST(GraphInputLimits, RejectsTooManyComponents) {
  EXPECT_THROW(
      {
        try {
          (void)topoexec::load_graph_text(graph_with_component_count(4097u));
        } catch (const std::invalid_argument& error) {
          EXPECT_NE(std::string(error.what()).find("runtime graph.components count exceeds limit 4096"),
                    std::string::npos);
          throw;
        }
      },
      std::invalid_argument);
}

TEST(GraphInputLimits, RejectsTooManyEdges) {
  EXPECT_THROW(
      {
        try {
          (void)topoexec::load_graph_text(graph_with_edge_count(8193u));
        } catch (const std::invalid_argument& error) {
          EXPECT_NE(std::string(error.what()).find("runtime graph.edges count exceeds limit 8192"), std::string::npos);
          throw;
        }
      },
      std::invalid_argument);
}

TEST(GraphInputLimits, RejectsOverlongIdentifiers) {
  const std::string long_id(129u, 'x');
  EXPECT_THROW(
      {
        try {
          (void)topoexec::load_graph_text("schema_version: 1\n"
                                          "graph: {name: id_limit, kind: internal_test}\n"
                                          "lanes: {main: {type: event_loop}}\n"
                                          "components:\n"
                                          "  - id: " +
                                          long_id +
                                          "\n"
                                          "    type: topoexec.test.Node\n"
                                          "    event_sources: [{type: manual}]\n"
                                          "    trigger_policy: {type: manual}\n"
                                          "    execution: {lane: main}\n"
                                          "edges: []\n");
        } catch (const std::invalid_argument& error) {
          EXPECT_NE(std::string(error.what()).find("id length exceeds limit 128"), std::string::npos);
          throw;
        }
      },
      std::invalid_argument);
}

TEST(GraphInputLimits, RejectsOverlongNonConfigStrings) {
  const std::string long_type(4097u, 'x');
  EXPECT_THROW(
      {
        try {
          (void)topoexec::load_graph_text("schema_version: 1\n"
                                          "graph: {name: string_limit, kind: internal_test}\n"
                                          "lanes: {main: {type: event_loop}}\n"
                                          "components:\n"
                                          "  - id: a\n"
                                          "    type: " +
                                          long_type +
                                          "\n"
                                          "    event_sources: [{type: manual}]\n"
                                          "    trigger_policy: {type: manual}\n"
                                          "    execution: {lane: main}\n"
                                          "edges: []\n");
        } catch (const std::invalid_argument& error) {
          EXPECT_NE(std::string(error.what()).find("components.a.type length exceeds limit 4096"), std::string::npos);
          throw;
        }
      },
      std::invalid_argument);
}

TEST(GraphInputLimits, RejectsDeepConfigSnapshots) {
  EXPECT_THROW(
      {
        try {
          (void)topoexec::load_graph_text(R"(
schema_version: 1
graph:
  name: deep_config
  kind: internal_test
  config: {a: {b: {c: {d: {e: {f: {g: {h: {i: too_deep}}}}}}}}}
lanes: {main: {type: event_loop}}
components: []
edges: []
)");
        } catch (const std::invalid_argument& error) {
          EXPECT_NE(std::string(error.what()).find("config"), std::string::npos);
          EXPECT_NE(std::string(error.what()).find("depth exceeds limit 8"), std::string::npos);
          throw;
        }
      },
      std::invalid_argument);
}

TEST(GraphInputLimits, RejectsInvalidUtf8BeforeYamlParse) {
  std::string text = "schema_version: 1\n";
  text.push_back(static_cast<char>(0xC3));
  text.push_back(static_cast<char>(0x28));

  EXPECT_THROW(
      {
        try {
          (void)topoexec::load_graph_text(text);
        } catch (const std::invalid_argument& error) {
          EXPECT_NE(std::string(error.what()).find("valid UTF-8"), std::string::npos);
          throw;
        }
      },
      std::invalid_argument);
}

TEST(Graph, LoadsAndValidatesSchemaVersionOne) {
  const auto graph = minimal_graph();
  const auto result = topoexec::validate_graph_structure(graph);
  ASSERT_TRUE(result.ok) << result.errors.front();
  EXPECT_EQ(result.compiled_plan.region_order.size(), 2u);
  EXPECT_NE(topoexec::graph_plan_json(graph, result.compiled_plan).find("\"schema_version\": 1"), std::string::npos);
  EXPECT_NE(topoexec::graph_mermaid(graph, result.compiled_plan).find("flowchart TD"), std::string::npos);
}

TEST(Graph, RejectsSchemaVersionTwoUntilV2LoaderExists) {
  EXPECT_THROW(
      {
        try {
          (void)topoexec::load_graph_text(R"(
schema_version: 2
graph: {name: future_v2_sketch, kind: runnable}
lanes: {main: {type: event_loop}}
components: []
edges: []
)");
        } catch (const std::invalid_argument& error) {
          EXPECT_NE(std::string(error.what()).find("schema_version must be 1"), std::string::npos);
          throw;
        }
      },
      std::invalid_argument);
}

TEST(Graph, SubgraphExpandsToNamespacedComponentsEdgesAndPlanHierarchy) {
  const auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: hierarchical_preview, kind: internal_test}
lanes: {main: {type: event_loop}}
components: []
edges: []
subgraphs:
  - id: cell
    components:
      - {id: source, type: topoexec.test.Source, event_sources: [{type: manual}], trigger_policy: {type: manual}, execution: {lane: main}}
      - {id: sink, type: topoexec.test.Sink, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}, depends_on: [source]}
    edges:
      - {id: source_sink, kind: immediate, from: source.out, to: sink.in, policy: {mode: latest, copy_policy: shared_view}}
)");

  ASSERT_EQ(graph.components.size(), 2u);
  ASSERT_EQ(graph.edges.size(), 1u);
  ASSERT_EQ(graph.hierarchy.size(), 1u);
  EXPECT_EQ(graph.components.front().id, "cell.source");
  EXPECT_EQ(graph.components.back().id, "cell.sink");
  EXPECT_EQ(graph.components.back().depends_on, std::vector<std::string>({"cell.source"}));
  EXPECT_EQ(graph.edges.front().id, "cell.source_sink");
  EXPECT_EQ(graph.edges.front().from, "cell.source.out");
  EXPECT_EQ(graph.edges.front().to, "cell.sink.in");
  EXPECT_EQ(graph.hierarchy.front().components, std::vector<std::string>({"cell.source", "cell.sink"}));
  EXPECT_EQ(graph.hierarchy.front().edges, std::vector<std::string>({"cell.source_sink"}));

  const auto result = topoexec::validate_graph_structure(graph);
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_EQ(result.compiled_plan.region_order, std::vector<std::string>({"cell.source", "cell.sink"}));
  const auto plan_json = topoexec::graph_plan_json(graph, result.compiled_plan);
  EXPECT_NE(plan_json.find("\"hierarchy\""), std::string::npos);
  EXPECT_NE(plan_json.find("\"id\": \"cell\""), std::string::npos);
  EXPECT_NE(plan_json.find("\"cell.source\""), std::string::npos);
  EXPECT_NE(plan_json.find("\"expansion\": \"compile_time_namespace\""), std::string::npos);
  const auto mermaid = topoexec::graph_mermaid(graph, result.compiled_plan);
  EXPECT_NE(mermaid.find("Subgraph: cell"), std::string::npos);
  EXPECT_NE(mermaid.find("cell.source"), std::string::npos);
}

TEST(Graph, SubgraphExpansionDoesNotHideImmediateCycles) {
  const auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: hierarchical_cycle, kind: internal_test}
lanes: {main: {type: event_loop}}
components: []
edges: []
subgraphs:
  - id: cell
    components:
      - {id: controller, type: topoexec.test.Controller, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
      - {id: estimator, type: topoexec.test.Estimator, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
    edges:
      - {id: controller_estimator, kind: immediate, from: controller.out, to: estimator.in, policy: {mode: latest, copy_policy: shared_view}}
      - {id: estimator_controller, kind: immediate, from: estimator.out, to: controller.in, policy: {mode: latest, copy_policy: shared_view}}
)");

  const auto result = topoexec::validate_graph_structure(graph);

  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors, "immediate cycle detected among components"));
  EXPECT_TRUE(has_error_containing(result.errors, "cell.controller"));
  EXPECT_TRUE(has_error_containing(result.errors, "cell.estimator"));
}

TEST(Graph, SubgraphCompositeLoopOwnsExpandedCycle) {
  const auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: hierarchical_loop, kind: internal_test}
lanes: {main: {type: event_loop}}
components: []
edges: []
subgraphs:
  - id: cell
    components:
      - {id: controller, type: topoexec.test.Controller, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
      - {id: estimator, type: topoexec.test.Estimator, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
    edges:
      - {id: controller_estimator, kind: immediate, from: controller.out, to: estimator.in, policy: {mode: latest, copy_policy: shared_view}}
      - {id: estimator_controller, kind: immediate, from: estimator.out, to: controller.in, policy: {mode: latest, copy_policy: shared_view}}
    composite_loops:
      - id: feedback
        components: [controller, estimator]
        loop_policy: {type: fixed_point, max_iterations: 3}
)");

  const auto result = topoexec::validate_graph_structure(graph);

  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  ASSERT_EQ(graph.composite_loops.size(), 1u);
  EXPECT_EQ(graph.composite_loops.front().id, "cell.feedback");
  EXPECT_EQ(graph.hierarchy.front().composite_loops, std::vector<std::string>({"cell.feedback"}));
  ASSERT_EQ(result.compiled_plan.regions.size(), 1u);
  EXPECT_EQ(result.compiled_plan.regions.front().id, "cell.feedback");
  EXPECT_EQ(result.compiled_plan.regions.front().kind, topoexec::CompiledRegionKind::kCompositeLoop);
}

TEST(Graph, TemplateInstanceExpandsDeterministicallyAndValidates) {
  const auto graph_text = R"(
schema_version: 1
graph: {name: templated_pipeline, kind: internal_test}
lanes: {main: {type: event_loop}}
components: []
edges: []
templates:
  - id: source_sink
    parameters: [source_type, sink_type, edge_mode]
    components:
      - {id: source, type: "{{source_type}}", event_sources: [{type: manual}], trigger_policy: {type: manual}, execution: {lane: main}}
      - {id: sink, type: "{{sink_type}}", event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
    edges:
      - {id: source_sink, kind: immediate, from: source.out, to: sink.in, policy: {mode: "{{edge_mode}}", copy_policy: shared_view}}
template_instances:
  - id: cell
    template: source_sink
    parameters: {source_type: topoexec.test.Source, sink_type: topoexec.test.Sink, edge_mode: latest}
)";

  const auto first = topoexec::load_graph_text(graph_text);
  const auto second = topoexec::load_graph_text(graph_text);

  ASSERT_EQ(first.components.size(), 2u);
  ASSERT_EQ(first.edges.size(), 1u);
  EXPECT_EQ(first.components.front().id, "cell.source");
  EXPECT_EQ(first.components.front().type, "topoexec.test.Source");
  EXPECT_EQ(first.components.back().type, "topoexec.test.Sink");
  EXPECT_EQ(first.edges.front().id, "cell.source_sink");
  EXPECT_EQ(first.edges.front().from, "cell.source.out");
  EXPECT_EQ(first.edges.front().to, "cell.sink.in");
  EXPECT_EQ(first.edges.front().policy.mode, "latest");
  EXPECT_EQ(second.components.front().id, first.components.front().id);
  EXPECT_EQ(second.edges.front().id, first.edges.front().id);

  const auto result = topoexec::validate_graph_structure(first);
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_EQ(result.compiled_plan.region_order, std::vector<std::string>({"cell.source", "cell.sink"}));
  const auto plan_json = topoexec::graph_plan_json(first, result.compiled_plan);
  EXPECT_NE(plan_json.find("\"id\": \"cell\""), std::string::npos);
  EXPECT_NE(plan_json.find("\"cell.source_sink\""), std::string::npos);
}

TEST(Graph, TemplateInstanceRejectsMissingOrUnknownParameter) {
  EXPECT_THROW(
      {
        try {
          (void)topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: missing_template_parameter, kind: internal_test}
lanes: {main: {type: event_loop}}
components: []
edges: []
templates:
  - id: single
    parameters: [type_name]
    components:
      - {id: node, type: "{{type_name}}", event_sources: [{type: manual}], trigger_policy: {type: manual}, execution: {lane: main}}
    edges: []
template_instances:
  - id: one
    template: single
    parameters: {}
)");
        } catch (const std::invalid_argument& error) {
          EXPECT_NE(std::string(error.what()).find("missing template parameter type_name"), std::string::npos);
          throw;
        }
      },
      std::invalid_argument);

  EXPECT_THROW(
      {
        try {
          (void)topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: unknown_template_parameter, kind: internal_test}
lanes: {main: {type: event_loop}}
components: []
edges: []
templates:
  - id: single
    parameters: [type_name]
    components:
      - {id: node, type: "{{type_name}}", event_sources: [{type: manual}], trigger_policy: {type: manual}, execution: {lane: main}}
    edges: []
template_instances:
  - id: one
    template: single
    parameters: {type_name: topoexec.test.Node, extra: nope}
)");
        } catch (const std::invalid_argument& error) {
          EXPECT_NE(std::string(error.what()).find("unknown template parameter extra"), std::string::npos);
          throw;
        }
      },
      std::invalid_argument);
}

TEST(Graph, TemplateInstanceRejectsUnknownPlaceholder) {
  EXPECT_THROW(
      {
        try {
          (void)topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: unknown_placeholder, kind: internal_test}
lanes: {main: {type: event_loop}}
components: []
edges: []
templates:
  - id: single
    parameters: [type_name]
    components:
      - {id: node, type: "{{missing}}", event_sources: [{type: manual}], trigger_policy: {type: manual}, execution: {lane: main}}
    edges: []
template_instances:
  - id: one
    template: single
    parameters: {type_name: topoexec.test.Node}
)");
        } catch (const std::invalid_argument& error) {
          EXPECT_NE(std::string(error.what()).find("references missing template parameter missing"), std::string::npos);
          throw;
        }
      },
      std::invalid_argument);
}

TEST(Graph, PayloadTypeMismatchIsRejectedWithDiagnostic) {
  auto graph = minimal_graph();
  graph.edges.front().kind = topoexec::EdgeKind::kState;
  const auto registry = registry_for({
      component_descriptor("topoexec.test.Source", topoexec::ComponentRole::kInputBoundary, {},
                           {port("out", topoexec::kTextPayloadSchema, "TextPayload")}),
      component_descriptor("topoexec.test.Sink", topoexec::ComponentRole::kOutputBoundary,
                           {port("in", topoexec::kTextPayloadSchema, "BinaryBlobPayload")}, {}),
  });

  const auto result = topoexec::validate_graph(graph, registry);

  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors, "payload type mismatch"));
  EXPECT_TRUE(has_diagnostic(result.diagnostics, "payload_type_mismatch", "error", "edges.e"));
}

TEST(Graph, OptionalInputUnconnectedIsAdvisoryAndRequiredInputFails) {
  const auto registry = registry_for({
      component_descriptor("topoexec.test.Source", topoexec::ComponentRole::kInputBoundary, {},
                           {port("out", topoexec::kTextPayloadSchema)}),
      component_descriptor("topoexec.test.Sink", topoexec::ComponentRole::kOutputBoundary,
                           {port("in", topoexec::kTextPayloadSchema, {}, topoexec::PortMultiplicity::kSingle, true),
                            port("side", topoexec::kTextPayloadSchema)},
                           {}),
  });

  auto graph = minimal_graph();
  auto connected = topoexec::validate_graph(graph, registry);
  ASSERT_TRUE(connected.ok) << (connected.errors.empty() ? "" : connected.errors.front());
  EXPECT_TRUE(
      has_diagnostic(connected.diagnostics, "optional_input_unconnected", "advisory", "components.b.inputs.side"));

  graph.edges.clear();
  const auto missing = topoexec::validate_graph(graph, registry);
  EXPECT_FALSE(missing.ok);
  EXPECT_TRUE(has_diagnostic(missing.diagnostics, "missing_required_input", "error", "components.b"));
}

TEST(Graph, MultiOutputDescriptorIsValidated) {
  const auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: multi_output, kind: internal_test}
lanes: {main: {type: event_loop}}
components:
  - id: source
    type: topoexec.test.Source
    boundary: {role: input, descriptor: test}
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: sink
    type: topoexec.test.Sink
    boundary: {role: output, descriptor: test}
    event_sources: [{type: message, inputs: [left, right]}]
    trigger_policy: {type: all_inputs, inputs: [left, right]}
    execution: {lane: main}
edges:
  - {id: left, kind: immediate, from: source.left, to: sink.left, policy: {mode: latest, copy_policy: shared_view}}
  - {id: right, kind: immediate, from: source.right, to: sink.right, policy: {mode: latest, copy_policy: shared_view}}
)");
  const auto registry = registry_for({
      component_descriptor("topoexec.test.Source", topoexec::ComponentRole::kInputBoundary, {},
                           {port("left"), port("right")}),
      component_descriptor("topoexec.test.Sink", topoexec::ComponentRole::kOutputBoundary,
                           {port("left", topoexec::kTextPayloadSchema, {}, topoexec::PortMultiplicity::kSingle, true),
                            port("right", topoexec::kTextPayloadSchema, {}, topoexec::PortMultiplicity::kSingle, true)},
                           {}),
  });

  const auto result = topoexec::validate_graph(graph, registry);

  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_FALSE(
      has_diagnostic(result.diagnostics, "optional_input_unconnected", "advisory", "components.sink.inputs.left"));
}

TEST(Graph, BoundaryRoleMismatchIsRejected) {
  const auto graph = minimal_graph();
  const auto registry = registry_for({
      component_descriptor("topoexec.test.Source", topoexec::ComponentRole::kOutputBoundary, {},
                           {port("out", topoexec::kTextPayloadSchema)}),
      component_descriptor("topoexec.test.Sink", topoexec::ComponentRole::kOutputBoundary,
                           {port("in", topoexec::kTextPayloadSchema)}, {}),
  });

  const auto result = topoexec::validate_graph(graph, registry);

  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors, "boundary role mismatch"));
  EXPECT_TRUE(has_diagnostic(result.diagnostics, "boundary_role_mismatch", "error", "components.a"));
}

TEST(Graph, SingleMultiplicityInputRejectsMultipleIncomingEdges) {
  auto graph = minimal_graph();
  topoexec::ComponentNodeSpec extra_source = graph.components.front();
  extra_source.id = "c";
  graph.components.push_back(extra_source);
  topoexec::EdgeSpec extra_edge = graph.edges.front();
  extra_edge.id = "extra";
  extra_edge.from = "c.out";
  graph.edges.push_back(extra_edge);
  const auto registry = registry_for({
      component_descriptor("topoexec.test.Source", topoexec::ComponentRole::kInputBoundary, {},
                           {port("out", topoexec::kTextPayloadSchema)}),
      component_descriptor("topoexec.test.Sink", topoexec::ComponentRole::kOutputBoundary,
                           {port("in", topoexec::kTextPayloadSchema, {}, topoexec::PortMultiplicity::kSingle, true)},
                           {}),
  });

  const auto result = topoexec::validate_graph(graph, registry);

  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_diagnostic(result.diagnostics, "port_multiplicity_mismatch", "error", "components.b"));
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
    overrun_policy: catch_up_once
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
  EXPECT_EQ(graph.lanes.front().overrun_policy, "catch_up_once");
  const auto result = topoexec::validate_graph_structure(graph);
  ASSERT_TRUE(result.ok) << result.errors.front();
}

TEST(Graph, AdvisorySchedulerFieldsProduceDiagnosticsWithoutFailingValidation) {
  auto graph = minimal_graph();
  auto& lane = graph.lanes.front();
  lane.priority = "high";
  lane.thread_name = "worker-a";
  lane.cpu_affinity = {0, 1};
  lane.rt_policy = "fifo";
  lane.wall_clock_enabled = true;
  graph.components.front().execution.priority = "high";

  const auto result = topoexec::validate_graph_structure(graph);

  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_TRUE(result.errors.empty());
  EXPECT_TRUE(has_diagnostic(result.diagnostics, "advisory_lane_field_ignored", "advisory", "lanes.main.priority"));
  EXPECT_TRUE(has_diagnostic(result.diagnostics, "advisory_lane_field_ignored", "advisory", "lanes.main.cpu_affinity"));
  EXPECT_TRUE(
      has_diagnostic(result.diagnostics, "advisory_lane_field_ignored", "advisory", "lanes.main.wall_clock_enabled"));
  EXPECT_FALSE(has_diagnostic(result.diagnostics, "advisory_execution_field_ignored", "advisory",
                              "components.a.execution.priority"));
}

TEST(Graph, WarningDiagnosticsDoNotFailValidationAndCarryCategory) {
  auto graph = minimal_graph();
  auto& edge = graph.edges.front();
  edge.policy.mode = "queue";
  edge.policy.capacity = 512;
  edge.policy.overflow = "block";
  edge.policy.copy_policy = "copy";
  const auto registry = registry_for({
      component_descriptor("topoexec.test.Source", topoexec::ComponentRole::kInputBoundary, {},
                           {port("out", topoexec::kFrameViewPayloadSchema)}),
      component_descriptor(
          "topoexec.test.Sink", topoexec::ComponentRole::kOutputBoundary,
          {port("in", topoexec::kFrameViewPayloadSchema, {}, topoexec::PortMultiplicity::kSingle, true)}, {}),
  });

  const auto result = topoexec::validate_graph(graph, registry);

  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_TRUE(result.errors.empty());
  const auto* backpressure = find_diagnostic(result.diagnostics, "backpressure_risk");
  ASSERT_NE(backpressure, nullptr);
  EXPECT_EQ(backpressure->severity, "warning");
  EXPECT_EQ(backpressure->category, "channel");
  EXPECT_EQ(backpressure->graph_path, "edges.e");
  const auto* latency = find_diagnostic(result.diagnostics, "high_queue_depth_latency_risk");
  ASSERT_NE(latency, nullptr);
  EXPECT_EQ(latency->category, "channel");
  EXPECT_NE(latency->suggested_fix.find("Reduce capacity"), std::string::npos);
  const auto* payload = find_diagnostic(result.diagnostics, "large_payload_copy");
  ASSERT_NE(payload, nullptr);
  EXPECT_EQ(payload->severity, "warning");
  EXPECT_EQ(payload->category, "payload");
  EXPECT_EQ(payload->involved_edges, std::vector<std::string>({"e"}));
}

TEST(Graph, TriggerNeverReadyWarningDoesNotFailValidation) {
  auto graph = minimal_graph();
  auto& sink = graph.components.back();
  topoexec::EventSourceSpec timer;
  timer.type = "timer";
  timer.period_ms = 10;
  sink.event_sources = {timer};
  sink.trigger_policy.type = "all_inputs";
  sink.trigger_policy.inputs = {"in"};

  const auto result = topoexec::validate_graph_structure(graph);

  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  const auto* diagnostic = find_diagnostic(result.diagnostics, "trigger_never_ready");
  ASSERT_NE(diagnostic, nullptr);
  EXPECT_EQ(diagnostic->severity, "warning");
  EXPECT_EQ(diagnostic->category, "trigger");
  EXPECT_EQ(diagnostic->graph_path, "components.b.trigger_policy");
}

TEST(Graph, RejectsTimerAndInputDrivenEventSourceMix) {
  auto graph = minimal_graph();
  auto& sink = graph.components.back();
  topoexec::EventSourceSpec timer;
  timer.type = "timer";
  timer.period_ms = 10;
  topoexec::EventSourceSpec message;
  message.type = "message";
  message.inputs = {"in"};
  sink.event_sources = {timer, message};

  const auto result = topoexec::validate_graph_structure(graph);

  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(
      has_error_containing(result.errors, "cannot mix timer event_source with input-driven event_source types"));
}

TEST(Graph, RejectsReservedActionEventSourcesUntilImplemented) {
  auto graph = minimal_graph();
  auto& sink = graph.components.back();
  sink.event_sources.front().type = "action_goal";

  const auto result = topoexec::validate_graph_structure(graph);

  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors,
                                   "event_source.type action_goal is reserved but not implemented in schema v1"));
}

TEST(Graph, RejectsNonZeroDebounceWindowMsUntilImplemented) {
  auto graph = minimal_graph();
  auto& sink = graph.components.back();
  sink.trigger_policy.type = "debounce";
  sink.trigger_policy.debounce_window_ms = 25;

  const auto result = topoexec::validate_graph_structure(graph);

  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(
      has_error_containing(result.errors, "trigger_policy.debounce_window_ms is reserved in schema v1 and must be 0"));
}

TEST(Graph, PlanJsonIncludesSchedulerLaneCapabilitySummary) {
  const auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: lane_capabilities, kind: internal_test}
lanes:
  pool:
    type: thread_pool
    max_threads: 2
    queue_capacity: 4
    overflow: reject_new
  clock:
    type: fixed_rate
    period_ms: 10
    wall_clock_enabled: true
    overrun_policy: skip_next
components:
  - id: a
    type: topoexec.test.Source
    boundary: {role: input, descriptor: test}
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: pool}
edges: []
)");

  const auto result = topoexec::validate_graph_structure(graph);
  ASSERT_TRUE(result.ok) << result.errors.front();
  const auto plan_json = topoexec::graph_plan_json(graph, result.compiled_plan);
  EXPECT_NE(plan_json.find("\"lane_capabilities\""), std::string::npos);
  EXPECT_NE(plan_json.find("\"persistent_worker_lifecycle\""), std::string::npos);
  EXPECT_NE(plan_json.find("\"bounded_priority_queue\""), std::string::npos);
  EXPECT_NE(plan_json.find("\"worker_id_trace\""), std::string::npos);
  EXPECT_NE(plan_json.find("\"priority_queue\""), std::string::npos);
  EXPECT_NE(plan_json.find("\"opt_in_wall_clock_cadence\""), std::string::npos);
  EXPECT_NE(plan_json.find("\"overrun_policy\": \"skip_next\""), std::string::npos);
  EXPECT_NE(plan_json.find("\"scheduler_contract_version\": \"0.2\""), std::string::npos);
}

TEST(Graph, PlanJsonIncludesEdgeReaderAndSlowReaderDropSummary) {
  auto graph = minimal_graph();
  graph.edges.front().policy.mode = "queue";
  graph.edges.front().policy.capacity = 2;
  graph.edges.front().policy.overflow = "drop_oldest";
  graph.edges.front().policy.readers = "multi";

  const auto result = topoexec::validate_graph_structure(graph);
  ASSERT_TRUE(result.ok) << result.errors.front();
  const auto plan_json = topoexec::graph_plan_json(graph, result.compiled_plan);
  EXPECT_NE(plan_json.find("\"capacity\": 2"), std::string::npos);
  EXPECT_NE(plan_json.find("\"overflow\": \"drop_oldest\""), std::string::npos);
  EXPECT_NE(plan_json.find("\"readers\": \"multi\""), std::string::npos);
  EXPECT_NE(plan_json.find("\"slow_reader_drop_risk\": true"), std::string::npos);
}

TEST(Graph, RejectsInvalidSchedulerLaneAdmissionFields) {
  auto graph = minimal_graph();
  graph.lanes.front().queue_capacity = -1;
  graph.lanes.front().overflow = "mystery";
  graph.lanes.front().overrun_policy = "mystery";

  const auto result = topoexec::validate_graph_structure(graph);

  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors, "lane main queue_capacity must be non-negative"));
  EXPECT_TRUE(has_error_containing(result.errors, "lane main has unsupported overflow mystery"));
  EXPECT_TRUE(has_error_containing(result.errors, "lane main has unsupported overrun_policy mystery"));
}

TEST(Graph, RejectsUnknownRuntimePriorityClass) {
  auto graph = minimal_graph();
  graph.components.front().execution.priority = "urgent";

  const auto result = topoexec::validate_graph_structure(graph);

  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors, "component a has unsupported execution.priority urgent"));
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
  EXPECT_TRUE(has_diagnostic(result.diagnostics, "invalid_move_only_multireader", "error", "edges.e"));
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

TEST(Graph, TriggerV2PoliciesValidateDeclarativeFields) {
  auto graph = minimal_graph();
  graph.components.back().trigger_policy.type = "watermark";
  graph.components.back().trigger_policy.watermark_lateness_ms = 5;
  auto result = topoexec::validate_graph_structure(graph);
  ASSERT_TRUE(result.ok) << result.errors.front();

  graph = minimal_graph();
  graph.components.back().trigger_policy.type = "condition";
  graph.components.back().trigger_policy.condition = "event_timestamp_present";
  result = topoexec::validate_graph_structure(graph);
  ASSERT_TRUE(result.ok) << result.errors.front();

  graph.components.back().trigger_policy.condition = "payload.text == 'unsafe script'";
  result = topoexec::validate_graph_structure(graph);
  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors, "condition trigger_policy.condition must be one of"));

  graph = minimal_graph();
  graph.components.back().trigger_policy.type = "rate_limit";
  result = topoexec::validate_graph_structure(graph);
  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors, "rate_limit trigger_policy requires positive min_interval_ms"));
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

TEST(Graph, SolverIterationLoopPolicyFieldsValidate) {
  const auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: solver_iteration_loop, kind: internal_test}
lanes: {main: {type: event_loop}}
components:
  - {id: a, type: topoexec.test.A, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
  - {id: b, type: topoexec.test.B, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
edges:
  - {id: a_b, kind: immediate, from: a.out, to: b.in}
  - {id: b_a, kind: immediate, from: b.out, to: a.in}
composite_loops:
  - id: solver
    components: [a, b]
    loop_policy:
      type: solver_iteration
      max_iterations: 4
      residual_threshold: 0.01
      partial_success: discard_outputs
)");

  const auto result = topoexec::validate_graph_structure(graph);

  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  ASSERT_EQ(result.compiled_plan.regions.size(), 1u);
  EXPECT_EQ(result.compiled_plan.regions.front().loop_policy.type, "solver_iteration");
  ASSERT_TRUE(result.compiled_plan.regions.front().loop_policy.residual_threshold.has_value());
  EXPECT_DOUBLE_EQ(*result.compiled_plan.regions.front().loop_policy.residual_threshold, 0.01);
  EXPECT_EQ(result.compiled_plan.regions.front().loop_policy.partial_success, "discard_outputs");
}

TEST(Graph, InvalidSolverIterationPolicyFieldsAreRejected) {
  auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: bad_solver_iteration_loop, kind: internal_test}
lanes: {main: {type: event_loop}}
components:
  - {id: a, type: topoexec.test.A, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
  - {id: b, type: topoexec.test.B, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
edges:
  - {id: a_b, kind: immediate, from: a.out, to: b.in}
  - {id: b_a, kind: immediate, from: b.out, to: a.in}
composite_loops:
  - id: solver
    components: [a, b]
    loop_policy: {type: solver_iteration, max_iterations: 4}
)");
  graph.composite_loops.front().loop_policy.residual_threshold = -0.1;
  graph.composite_loops.front().loop_policy.partial_success = "sometimes";
  graph.composite_loops.front().loop_policy.convergence = "maybe";

  const auto result = topoexec::validate_graph_structure(graph);

  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(has_error_containing(result.errors, "residual_threshold must be non-negative"));
  EXPECT_TRUE(has_error_containing(result.errors, "unsupported loop_policy.partial_success"));
  EXPECT_TRUE(has_error_containing(result.errors, "unsupported loop_policy.convergence"));
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

TEST(Graph, DiagnosticRegistryExposesStableCodesAndFixes) {
  const auto registry = topoexec::graph_diagnostic_registry();
  EXPECT_GE(registry.size(), 25u);
  const auto descriptor = topoexec::graph_diagnostic_descriptor("multi_state_writer");
  ASSERT_TRUE(descriptor.has_value());
  EXPECT_EQ(descriptor->severity, "error");
  EXPECT_EQ(descriptor->category, "channel");
  EXPECT_NE(descriptor->suggested_fix.find("one writer"), std::string::npos);
  const auto warning = topoexec::graph_diagnostic_descriptor("large_payload_copy");
  ASSERT_TRUE(warning.has_value());
  EXPECT_EQ(warning->severity, "warning");
  EXPECT_EQ(warning->category, "payload");
  EXPECT_EQ(topoexec::graph_diagnostic_category("trigger_never_ready"), "trigger");
  EXPECT_FALSE(topoexec::graph_diagnostic_descriptor("missing_code").has_value());
}

TEST(Graph, ParsesGraphLevelConfigSnapshot) {
  const auto spec = topoexec::load_graph_text(R"(
schema_version: 1
graph:
  name: graph_config
  kind: internal_test
  config:
    profile: alpha
    nested: {mode: safe}
lanes: {main: {type: event_loop}}
components:
  - {id: source, type: topoexec.test.Source, event_sources: [{type: manual}], trigger_policy: {type: manual}, execution: {lane: main}}
edges: []
)");

  EXPECT_EQ(spec.config.values.at("profile"), "alpha");
  EXPECT_TRUE(spec.config.is_nested("nested"));
  EXPECT_NE(spec.config.values.at("nested").find("safe"), std::string::npos);
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

TEST(GraphRender, MermaidEscapesUntrustedIdentifiers) {
  // Identifiers pass schema validation with no character-set restriction, so a hostile graph file can carry
  // Mermaid metacharacters. graph_mermaid must neutralize them rather than emit injectable diagram syntax.
  topoexec::GraphSpec graph;
  graph.name = "g";
  topoexec::ComponentNodeSpec component;
  component.id = "evil\"] -->|x| BOOM";
  component.type = "topoexec.test.Source";
  graph.components.push_back(component);

  const topoexec::GraphCompiledPlan plan;
  const auto mermaid = topoexec::graph_mermaid(graph, plan);

  // The raw label breakout (a closing quote+bracket) must never appear verbatim.
  EXPECT_EQ(mermaid.find("evil\"]"), std::string::npos) << mermaid;
  // The quote is entity-escaped inside the quoted label instead.
  EXPECT_NE(mermaid.find("&quot;"), std::string::npos) << mermaid;
  // The bare node id is sanitized so it can no longer carry an injected edge/arrow.
  EXPECT_EQ(mermaid.find("BOOM\n"), std::string::npos) << mermaid;
}

TEST(GraphInputLimits, RejectsExcessiveYamlAliases) {
  // Nested YAML alias expansion ("billion laughs") can make load traversal cost explode. The alias-count
  // guard must reject before YAML::Load, independent of where the aliases appear in the document.
  std::string text = "schema_version: 1\n"
                     "graph: {name: b, kind: internal_test}\n"
                     "lanes: {main: {type: event_loop}}\n"
                     "components: []\n"
                     "edges: []\n"
                     "anchor: &a [x]\n"
                     "fanout: [";
  for (int index = 0; index < 40; ++index) {
    text += "*a,";
  }
  text += "*a]\n";

  EXPECT_THROW(
      {
        try {
          (void)topoexec::load_graph_text(text);
        } catch (const std::invalid_argument& error) {
          EXPECT_NE(std::string(error.what()).find("too many YAML aliases"), std::string::npos);
          throw;
        }
      },
      std::invalid_argument);
}

TEST(GraphInputLimits, RejectsDigitNamedYamlAliasesAboveLimit) {
  // Regression for the digit-named-alias bypass: anchor/alias names may start with digits, so a guard that
  // only recognizes [A-Za-z_] after `*` misses `*1`. The event-based counter must catch these.
  std::string text = "schema_version: 1\n"
                     "graph: {name: b, kind: internal_test}\n"
                     "lanes: {main: {type: event_loop}}\n"
                     "components: []\n"
                     "edges: []\n"
                     "anchor: &1 [x]\n"
                     "fanout: [";
  for (int index = 0; index < 40; ++index) {
    text += "*1,";
  }
  text += "*1]\n";

  EXPECT_THROW(
      {
        try {
          (void)topoexec::load_graph_text(text);
        } catch (const std::invalid_argument& error) {
          EXPECT_NE(std::string(error.what()).find("too many YAML aliases"), std::string::npos);
          throw;
        }
      },
      std::invalid_argument);
}

TEST(GraphInputLimits, AcceptsBlockScalarTextThatResemblesAliases) {
  // Regression for the block-scalar false positive: literal text like `*a` inside a YAML block scalar is
  // scalar content, not alias use, and must not be counted against the alias limit.
  std::string text = "schema_version: 1\n"
                     "graph: {name: g, kind: internal_test}\n"
                     "lanes: {main: {type: event_loop}}\n"
                     "components:\n"
                     "  - id: c\n"
                     "    type: topoexec.test.Identity\n"
                     "    config:\n"
                     "      note: |\n";
  for (int index = 0; index < 60; ++index) {
    text += "        *a\n";
  }
  text += "    event_sources: [{type: manual}]\n"
          "    trigger_policy: {type: manual}\n"
          "    execution: {lane: main}\n"
          "edges: []\n";

  // load_graph_text only parses structure; the block scalar must not trip the alias guard.
  EXPECT_NO_THROW((void)topoexec::load_graph_text(text));
}

TEST(Graph, ConfigIntFieldRejectsNonFiniteAndOutOfRangeWithoutUndefinedBehavior) {
  // H2: int config validation must reject NaN/Inf/out-of-range before the float-to-int narrowing, which is
  // undefined behavior for those inputs. Reachable via any public ConfigValueKind::kInt descriptor field.
  topoexec::ConfigFieldSpec count_field;
  count_field.name = "count";
  count_field.kind = topoexec::ConfigValueKind::kInt;
  auto sink = component_descriptor("topoexec.test.Sink", topoexec::ComponentRole::kOutputBoundary,
                                   {port("in", topoexec::kTextPayloadSchema)}, {});
  sink.config_fields.push_back(count_field);
  const auto registry = registry_for({
      component_descriptor("topoexec.test.Source", topoexec::ComponentRole::kInputBoundary, {},
                           {port("out", topoexec::kTextPayloadSchema)}),
      sink,
  });

  for (const std::string& value :
       {std::string("nan"), std::string("inf"), std::string("3000000000"), std::string("1e300")}) {
    const auto graph = topoexec::load_graph_text(
        "schema_version: 1\n"
        "graph: {name: cfg, kind: internal_test}\n"
        "lanes: {main: {type: event_loop}}\n"
        "components:\n"
        "  - id: a\n"
        "    type: topoexec.test.Source\n"
        "    boundary: {role: input, descriptor: test}\n"
        "    event_sources: [{type: manual}]\n"
        "    trigger_policy: {type: manual}\n"
        "    execution: {lane: main}\n"
        "  - id: b\n"
        "    type: topoexec.test.Sink\n"
        "    boundary: {role: output, descriptor: test}\n"
        "    config: {count: " +
        value +
        "}\n"
        "    event_sources: [{type: message, inputs: [in]}]\n"
        "    trigger_policy: {type: any_input, inputs: [in]}\n"
        "    execution: {lane: main}\n"
        "edges:\n"
        "  - {id: e, kind: immediate, from: a.out, to: b.in, policy: {mode: latest, copy_policy: shared_view}}\n");
    const auto result = topoexec::validate_graph(graph, registry); // must not invoke UB under UBSAN
    EXPECT_FALSE(result.ok) << value;
    EXPECT_TRUE(has_error_containing(result.errors, "must be int")) << value;
  }
}

TEST(GraphRender, MermaidKeepsDistinctIdsDistinctAfterSanitization) {
  // Sanitizing ids independently could map distinct valid ids ("a b" and "a_b") to the same Mermaid node id,
  // collapsing them. The render must keep them distinct while leaving already-safe ids unchanged.
  topoexec::GraphSpec graph;
  graph.name = "g";
  for (const std::string& id : {std::string("a b"), std::string("a_b")}) {
    topoexec::ComponentNodeSpec component;
    component.id = id;
    component.type = "topoexec.test.Source";
    graph.components.push_back(component);
  }

  const topoexec::GraphCompiledPlan plan;
  const auto mermaid = topoexec::graph_mermaid(graph, plan);

  // The already-safe id keeps its exact name; the sanitized one is disambiguated, so two node defs exist.
  EXPECT_NE(mermaid.find("a_b[\""), std::string::npos) << mermaid;
  EXPECT_NE(mermaid.find("a_b_2[\""), std::string::npos) << mermaid;
}
