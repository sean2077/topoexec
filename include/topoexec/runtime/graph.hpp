#pragma once

#include "topoexec/runtime/component.hpp"
#include "topoexec/runtime/component_registry.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace topoexec {

struct LaneSpec {
  std::string id;
  std::string type;
  double hz{0.0};
  std::string priority;
  int max_callback_ms{0};
  int max_threads{0};
  std::string thread_name;
  std::vector<int> cpu_affinity;
  int nice_priority{0};
  std::string rt_policy{"none"};
  int rt_priority{0};
  std::string isolation_intent{"none"};
};

struct EventSourceSpec {
  std::string id;
  std::string type{"manual"};
  std::vector<std::string> inputs;
  std::string input;
  int period_ms{0};
};

struct TriggerPolicySpec {
  std::string type{"manual"};
  std::vector<std::string> inputs;
  std::string input;
  int batch_size{0};
  int batch_window_ms{0};
  int sync_slop_ms{0};
  int min_interval_ms{0};
  int max_latency_ms{0};
  bool coalesce{false};
};

struct ExecutionSpec {
  std::string lane;
  bool reentrant{false};
  std::string priority{"normal"};
  int budget_ms{0};
};

struct ComponentNodeSpec {
  std::string id;
  std::string type;
  std::vector<EventSourceSpec> event_sources;
  TriggerPolicySpec trigger_policy;
  ExecutionSpec execution;
  std::vector<std::string> depends_on;
  BoundaryDescriptor boundary;
  ConfigView config;
};

struct EdgePolicySpec {
  std::string mode{"latest"};
  int capacity{1};
  std::string overflow{"overwrite"};
  int lifespan_ms{0};
  int deadline_ms{0};
  bool preserve_order{true};
  bool allow_drop{true};
  bool emit_health_events{true};
  std::string timestamp_domain{"steady"};
  std::string copy_policy{"copy"};
  std::string owner{"runtime"};
  std::string readers{"single"};
};

enum class EdgeKind {
  kImmediate,
  kDelay,
  kState,
  kAsync,
};

struct EdgeSpec {
  std::string id;
  std::string from;
  std::string to;
  EdgeKind kind{EdgeKind::kImmediate};
  bool has_kind{false};
  std::string invalid_kind;
  EdgePolicySpec policy;
};

struct LoopPolicySpec {
  std::string type;
  int budget_ms{0};
  int max_iterations{0};
  int max_inflight{0};
  std::string drop_policy;
  int min_interval_ms{0};
  std::string convergence;
};

struct CompositeLoopSpec {
  std::string id;
  std::vector<std::string> components;
  LoopPolicySpec loop_policy;
};

struct ClockPolicySpec {
  std::string runtime_domain{"steady"};
  std::string event_domain{"steady"};
};

enum class CompiledRegionKind {
  kComponent,
  kCompositeLoop,
};

struct CompiledGraphRegion {
  std::string id;
  CompiledRegionKind kind{CompiledRegionKind::kComponent};
  std::vector<std::string> components;
  std::string composite_loop_id;
  LoopPolicySpec loop_policy;
  std::vector<std::string> incoming_regions;
  std::vector<std::string> outgoing_regions;
};

struct GraphCompiledPlan {
  std::vector<CompiledGraphRegion> regions;
  std::vector<std::string> region_order;
  std::map<std::string, std::string> component_region;
  std::vector<std::vector<std::string>> immediate_sccs;
};

struct GraphCompileResult {
  bool ok{true};
  std::vector<std::string> errors;
  GraphCompiledPlan plan;
};

struct GraphSpec {
  int schema_version{0};
  std::string name;
  std::string kind{"internal_test"};
  ClockPolicySpec clock;
  std::vector<LaneSpec> lanes;
  std::vector<ComponentNodeSpec> components;
  std::vector<EdgeSpec> edges;
  std::vector<CompositeLoopSpec> composite_loops;
};

struct GraphValidationResult {
  bool ok{true};
  std::vector<std::string> errors;
  GraphCompiledPlan compiled_plan;
};

struct RuntimeMetricSample {
  std::string name;
  double value{0.0};
  std::string component_id;
  std::string lane;
  std::string channel_id;
  std::vector<std::string> tags;
};

struct GraphDryRunResult {
  bool ok{true};
  std::vector<std::string> errors;
  std::size_t instantiated_components{0};
  std::size_t configured_components{0};
  std::size_t started_components{0};
  std::size_t stopped_components{0};
  std::size_t tick_calls{0};
  std::size_t metric_samples{0};
  std::size_t channel_publish_count{0};
  std::size_t channel_delivery_count{0};
  std::size_t channel_drop_count{0};
  std::size_t channel_deadline_miss_count{0};
  std::size_t payload_copy_count{0};
  std::vector<std::string> ticked_components;
  std::vector<RuntimeMetricSample> runtime_metrics;
};

GraphSpec load_graph_text(const std::string& text);
GraphSpec load_graph_file(const std::string& path);
std::string to_string(EdgeKind kind);
std::string to_string(CompiledRegionKind kind);
GraphCompileResult compile_graph(const GraphSpec& graph);
GraphValidationResult validate_graph_structure(const GraphSpec& graph);
GraphValidationResult validate_graph(const GraphSpec& graph, const ComponentRegistry& registry);
std::vector<std::size_t> component_lifecycle_order(const GraphSpec& graph);
GraphDryRunResult dry_run_graph(const GraphSpec& graph, const ComponentRegistry& registry,
                                std::size_t tick_iterations = 1);

std::string graph_plan_text(const GraphSpec& graph, const GraphCompiledPlan& plan);
std::string graph_plan_json(const GraphSpec& graph, const GraphCompiledPlan& plan);
std::string graph_mermaid(const GraphSpec& graph, const GraphCompiledPlan& plan);

} // namespace topoexec
