#include "topoexec/runtime/diagnostics.hpp"
#include "topoexec/runtime/graph.hpp"
#include "topoexec/runtime/metric_schema.hpp"
#include "topoexec/runtime/runtime_runner.hpp"

#include <CLI/CLI.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace {

std::string g_executable_dir;

struct LintFinding {
  std::string severity;
  std::string rule;
  std::string message;
  std::string id;
};

enum class DemoBehavior {
  kSource,
  kForward,
  kLargeSource,
  kEstimator,
  kController,
  kSink,
};

class DemoComponent : public topoexec::Component {
public:
  DemoComponent(topoexec::ComponentDescriptor descriptor, DemoBehavior behavior)
      : descriptor_(std::move(descriptor)), behavior_(behavior) {}

  topoexec::ComponentDescriptor describe() const override {
    return descriptor_;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    switch (behavior_) {
    case DemoBehavior::kSource:
      publish_or_throw(context, "out", "payload-" + std::to_string(invocation.sequence));
      return;
    case DemoBehavior::kForward:
      if (invocation.payload != nullptr) {
        publish_or_throw(context, "out", invocation.payload->text());
      }
      return;
    case DemoBehavior::kLargeSource:
      publish_or_throw(context, "out", "large-payload-placeholder-" + std::to_string(invocation.sequence));
      return;
    case DemoBehavior::kEstimator:
      if (invocation.payload != nullptr) {
        publish_or_throw(context, "estimate", "estimate:" + invocation.payload->text());
      }
      return;
    case DemoBehavior::kController:
      if (invocation.payload != nullptr) {
        publish_or_throw(context, "command", "command:" + invocation.payload->text());
        publish_or_throw(context, "correction", "correction:" + invocation.payload->text());
      }
      return;
    case DemoBehavior::kSink:
      return;
    }
  }

private:
  static void publish_or_throw(topoexec::GraphContext& context, const std::string& port, std::string payload) {
    const auto result = context.publish(port, topoexec::make_text_payload(std::move(payload)));
    if (!result.accepted) {
      throw std::runtime_error(result.reason);
    }
  }

  topoexec::ComponentDescriptor descriptor_;
  DemoBehavior behavior_;
};

topoexec::ComponentDescriptor descriptor(std::string type, topoexec::ComponentRole role,
                                         std::vector<topoexec::PortDescriptor> inputs,
                                         std::vector<topoexec::PortDescriptor> outputs) {
  topoexec::ComponentDescriptor value;
  value.type = std::move(type);
  value.name = value.type;
  value.role = role;
  value.inputs = std::move(inputs);
  value.outputs = std::move(outputs);
  return value;
}

void register_demo_component(topoexec::ComponentRegistry& registry, topoexec::ComponentDescriptor component,
                             DemoBehavior behavior) {
  const auto type = component.type;
  registry.register_component({type}, [component = std::move(component), behavior]() {
    return std::make_unique<DemoComponent>(component, behavior);
  });
}

topoexec::ComponentRegistry demo_registry() {
  topoexec::ComponentRegistry registry;
  const topoexec::PortDescriptor text_in{"in", topoexec::kTextPayloadSchema};
  const topoexec::PortDescriptor text_out{"out", topoexec::kTextPayloadSchema};

  register_demo_component(
      registry, descriptor("topoexec.boundary.Input", topoexec::ComponentRole::kInputBoundary, {}, {text_out}),
      DemoBehavior::kSource);
  register_demo_component(
      registry, descriptor("topoexec.transforms.Identity", topoexec::ComponentRole::kProcessing, {text_in}, {text_out}),
      DemoBehavior::kForward);
  register_demo_component(registry,
                          descriptor("topoexec.transforms.Join", topoexec::ComponentRole::kProcessing,
                                     {{"left", topoexec::kTextPayloadSchema}, {"right", topoexec::kTextPayloadSchema}},
                                     {text_out}),
                          DemoBehavior::kForward);
  register_demo_component(registry,
                          descriptor("topoexec.transforms.Validator", topoexec::ComponentRole::kProcessing,
                                     {{"request", topoexec::kTextPayloadSchema}}, {text_out}),
                          DemoBehavior::kForward);
  register_demo_component(registry,
                          descriptor("topoexec.transforms.AsyncWorker", topoexec::ComponentRole::kProcessing,
                                     {{"ready", topoexec::kTextPayloadSchema}}, {text_out}),
                          DemoBehavior::kForward);
  register_demo_component(
      registry, descriptor("topoexec.boundary.Output", topoexec::ComponentRole::kOutputBoundary, {text_in}, {}),
      DemoBehavior::kSink);
  register_demo_component(registry,
                          descriptor("topoexec.boundary.FrameInput", topoexec::ComponentRole::kInputBoundary, {},
                                     {{"out", topoexec::kFrameViewPayloadSchema}}),
                          DemoBehavior::kLargeSource);
  register_demo_component(registry,
                          descriptor("topoexec.boundary.BinaryInput", topoexec::ComponentRole::kInputBoundary, {},
                                     {{"out", topoexec::kBinaryBlobPayloadSchema}}),
                          DemoBehavior::kLargeSource);

  register_demo_component(
      registry, descriptor("topoexec.example.Sensor", topoexec::ComponentRole::kInputBoundary, {}, {text_out}),
      DemoBehavior::kSource);
  register_demo_component(
      registry,
      descriptor("topoexec.example.Estimator", topoexec::ComponentRole::kProcessing,
                 {{"sensor", topoexec::kTextPayloadSchema}, {"correction", topoexec::kTextPayloadSchema}},
                 {{"estimate", topoexec::kTextPayloadSchema}}),
      DemoBehavior::kEstimator);
  register_demo_component(
      registry,
      descriptor("topoexec.example.Controller", topoexec::ComponentRole::kProcessing,
                 {{"estimate", topoexec::kTextPayloadSchema}},
                 {{"command", topoexec::kTextPayloadSchema}, {"correction", topoexec::kTextPayloadSchema}}),
      DemoBehavior::kController);
  register_demo_component(registry,
                          descriptor("topoexec.example.Actuator", topoexec::ComponentRole::kOutputBoundary,
                                     {{"command", topoexec::kTextPayloadSchema}}, {}),
                          DemoBehavior::kSink);
  return registry;
}

std::string diagnostic_category(const topoexec::GraphDiagnostic& diagnostic) {
  return diagnostic.category.empty() ? topoexec::graph_diagnostic_category(diagnostic.code) : diagnostic.category;
}

nlohmann::json diagnostic_json(const topoexec::GraphDiagnostic& diagnostic) {
  return {{"code", diagnostic.code},
          {"severity", diagnostic.severity},
          {"category", diagnostic_category(diagnostic)},
          {"message", diagnostic.message},
          {"graph_path", diagnostic.graph_path},
          {"involved_components", diagnostic.involved_components},
          {"involved_edges", diagnostic.involved_edges},
          {"suggested_fix", diagnostic.suggested_fix}};
}

nlohmann::json grouped_diagnostics_json(const std::vector<topoexec::GraphDiagnostic>& diagnostics) {
  nlohmann::json groups = nlohmann::json::object();
  for (const auto& category : {"graph_structure", "scheduler", "channel", "payload", "trigger"}) {
    groups[category] = nlohmann::json::array();
  }
  for (const auto& diagnostic : diagnostics) {
    groups[diagnostic_category(diagnostic)].push_back(diagnostic_json(diagnostic));
  }
  return groups;
}

void apply_strict_diagnostics(topoexec::GraphValidationResult& result) {
  std::vector<std::string> warning_codes;
  for (const auto& diagnostic : result.diagnostics) {
    if (diagnostic.severity == "warning") {
      warning_codes.push_back(diagnostic.code);
    }
  }
  if (warning_codes.empty()) {
    return;
  }
  result.ok = false;
  for (const auto& code : warning_codes) {
    result.errors.push_back("strict diagnostics rejected warning: " + code);
  }
}

void add_input_limit_options(CLI::App* command, topoexec::GraphInputLimits& limits) {
  command->add_option("--max-graph-input-bytes", limits.max_graph_input_bytes, "Maximum graph YAML input bytes")
      ->check(CLI::PositiveNumber);
  command->add_option("--max-lanes", limits.max_lanes, "Maximum lane entries in one graph")->check(CLI::PositiveNumber);
  command->add_option("--max-components", limits.max_components, "Maximum component entries in one graph")
      ->check(CLI::PositiveNumber);
  command->add_option("--max-edges", limits.max_edges, "Maximum edge entries in one graph")->check(CLI::PositiveNumber);
  command->add_option("--max-composite-loops", limits.max_composite_loops, "Maximum composite loop entries")
      ->check(CLI::PositiveNumber);
  command->add_option("--max-identifier-bytes", limits.max_identifier_bytes, "Maximum graph id bytes")
      ->check(CLI::PositiveNumber);
  command->add_option("--max-config-depth", limits.max_config_depth, "Maximum graph/component config nesting depth")
      ->check(CLI::PositiveNumber);
  command
      ->add_option("--max-config-value-bytes", limits.max_config_value_bytes,
                   "Maximum graph/component config scalar or serialized nested value bytes")
      ->check(CLI::PositiveNumber);
  command->add_option("--max-string-bytes", limits.max_string_bytes, "Maximum non-config string field bytes")
      ->check(CLI::PositiveNumber);
}

nlohmann::json graph_input_limits_json(const topoexec::GraphInputLimits& limits) {
  return {{"max_graph_input_bytes", limits.max_graph_input_bytes},
          {"max_lanes", limits.max_lanes},
          {"max_components", limits.max_components},
          {"max_edges", limits.max_edges},
          {"max_composite_loops", limits.max_composite_loops},
          {"max_identifier_bytes", limits.max_identifier_bytes},
          {"max_config_depth", limits.max_config_depth},
          {"max_config_value_bytes", limits.max_config_value_bytes},
          {"max_string_bytes", limits.max_string_bytes},
          {"valid_text_encoding", "utf-8"}};
}

int print_validation(const topoexec::GraphValidationResult& result, const std::string& format) {
  if (format == "json") {
    nlohmann::json value;
    value["ok"] = result.ok;
    value["errors"] = result.errors;
    value["diagnostics_schema_version"] = topoexec::kGraphDiagnosticSchemaVersion;
    value["diagnostics"] = nlohmann::json::array();
    for (const auto& diagnostic : result.diagnostics) {
      value["diagnostics"].push_back(diagnostic_json(diagnostic));
    }
    value["region_order"] = result.compiled_plan.region_order;
    std::cout << value.dump(2) << "\n";
  } else {
    std::cout << (result.ok ? "ok" : "error") << "\n";
    for (const auto& error : result.errors) {
      std::cout << "- " << error << "\n";
    }
    for (const auto& diagnostic : result.diagnostics) {
      if (diagnostic.severity == "error") {
        continue;
      }
      std::cout << "- " << diagnostic.severity << " " << diagnostic.code;
      if (!diagnostic.graph_path.empty()) {
        std::cout << " " << diagnostic.graph_path;
      }
      std::cout << ": " << diagnostic.message << "\n";
    }
  }
  return result.ok ? 0 : 1;
}

topoexec::GraphValidationResult load_graph_file_result(const std::string& path, topoexec::GraphSpec& graph,
                                                       const topoexec::GraphInputLimits& limits) {
  topoexec::GraphValidationResult result;
  try {
    graph = topoexec::load_graph_file(path, limits);
    result.ok = true;
  } catch (const std::exception& error) {
    result.ok = false;
    result.errors.push_back(error.what());
  }
  return result;
}

topoexec::GraphValidationResult load_and_validate(const std::string& path, topoexec::GraphSpec& graph,
                                                  const topoexec::GraphInputLimits& limits) {
  auto result = load_graph_file_result(path, graph, limits);
  if (!result.ok) {
    return result;
  }
  return topoexec::validate_graph_structure(graph);
}

std::string component_id_from_endpoint(const std::string& endpoint) {
  const auto dot = endpoint.rfind('.');
  if (dot == std::string::npos) {
    return endpoint;
  }
  return endpoint.substr(0, dot);
}

std::string port_name_from_endpoint(const std::string& endpoint) {
  const auto dot = endpoint.rfind('.');
  if (dot == std::string::npos || dot + 1 >= endpoint.size()) {
    return {};
  }
  return endpoint.substr(dot + 1);
}

const topoexec::PortDescriptor* find_port(const std::vector<topoexec::PortDescriptor>& ports, const std::string& name) {
  const auto found = std::find_if(ports.begin(), ports.end(), [&](const auto& port) { return port.name == name; });
  if (found == ports.end()) {
    return nullptr;
  }
  return &*found;
}

bool is_large_payload_schema(const std::string& schema) {
  return schema == topoexec::kFrameViewPayloadSchema || schema == topoexec::kBinaryBlobPayloadSchema;
}

bool is_multi_reader_value(const std::string& readers) {
  return readers == "multi" || readers == "multiple";
}

bool slow_reader_drop_risk(const topoexec::EdgePolicySpec& policy) {
  return is_multi_reader_value(policy.readers) && (policy.overflow == "drop_oldest" || policy.overflow == "overwrite");
}

nlohmann::json edge_policy_json(const topoexec::EdgeSpec& edge) {
  return {{"id", edge.id},
          {"from", edge.from},
          {"to", edge.to},
          {"kind", topoexec::to_string(edge.kind)},
          {"mode", edge.policy.mode},
          {"capacity", edge.policy.capacity},
          {"overflow", edge.policy.overflow},
          {"copy_policy", edge.policy.copy_policy},
          {"readers", edge.policy.readers},
          {"slow_reader_drop_risk", slow_reader_drop_risk(edge.policy)}};
}

topoexec::RuntimeRunnerResult
run_graph_file(const std::string& path, std::size_t steps, std::uint64_t duration_ms, bool until_idle,
               const topoexec::GraphInputLimits& limits,
               std::optional<topoexec::runtime_observe::LiveObserveOptions> live_observe_options = std::nullopt) {
  const auto registry = demo_registry();
  topoexec::RuntimeRunner runner(registry);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = steps;
  options.run_duration_ms = duration_ms;
  options.run_until_idle = until_idle;
  if (live_observe_options.has_value()) {
    options.live_observe = *live_observe_options;
  }
  return runner.run(topoexec::load_graph_file(path, limits), options);
}

std::string join(const std::vector<std::string>& values) {
  std::string output;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index > 0u) {
      output += ",";
    }
    output += values[index];
  }
  return output;
}

std::string benchmark_case_name(const std::string& path) {
  const auto slash = path.find_last_of("/\\");
  const auto begin = slash == std::string::npos ? 0u : slash + 1u;
  const auto dot = path.find_last_of('.');
  const auto end = dot == std::string::npos || dot < begin ? path.size() : dot;
  return path.substr(begin, end - begin);
}

std::string trim(std::string value) {
  const auto begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return {};
  }
  const auto end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1u);
}

std::string read_text_file(const std::string& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("failed to open file: " + path);
  }
  std::ostringstream text;
  text << input.rdbuf();
  return text.str();
}

std::string fnv1a64_hex(const std::string& text) {
  std::uint64_t hash = 14695981039346656037ull;
  for (const auto byte : text) {
    hash ^= static_cast<unsigned char>(byte);
    hash *= 1099511628211ull;
  }
  std::ostringstream output;
  output << std::hex << std::setw(16) << std::setfill('0') << hash;
  return output.str();
}

std::string graph_hash(const std::string& path) {
  return "fnv1a64:" + fnv1a64_hex(read_text_file(path));
}

std::string compiler_name() {
#if defined(__clang__)
  return "clang";
#elif defined(__GNUC__)
  return "gcc";
#elif defined(_MSC_VER)
  return "msvc";
#else
  return "unknown";
#endif
}

std::string compiler_version() {
#if defined(__clang__)
  return std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__) + "." +
         std::to_string(__clang_patchlevel__);
#elif defined(__GNUC__)
  return std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__) + "." + std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
  return std::to_string(_MSC_VER);
#else
  return "unknown";
#endif
}

std::string build_type() {
#ifdef TOPOEXEC_CMAKE_BUILD_TYPE
  std::string configured = TOPOEXEC_CMAKE_BUILD_TYPE;
  if (!configured.empty()) {
    return configured;
  }
#endif
#ifdef NDEBUG
  return "Release";
#else
  return "Debug";
#endif
}

std::string cpu_model() {
  std::ifstream input("/proc/cpuinfo");
  std::string line;
  while (std::getline(input, line)) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) {
      continue;
    }
    if (trim(line.substr(0, colon)) == "model name") {
      return trim(line.substr(colon + 1u));
    }
  }
  return "unknown";
}

std::string read_git_head_commit(const std::string& git_dir) {
  std::ifstream head(git_dir + "/HEAD");
  std::string value;
  if (!std::getline(head, value)) {
    return "unknown";
  }
  constexpr std::string_view ref_prefix{"ref: "};
  if (value.rfind(ref_prefix, 0u) == 0u) {
    std::ifstream ref(git_dir + "/" + value.substr(ref_prefix.size()));
    if (std::getline(ref, value)) {
      return value.size() > 12u ? value.substr(0, 12u) : value;
    }
    return "unknown";
  }
  return value.size() > 12u ? value.substr(0, 12u) : value;
}

std::string git_commit() {
  if (const auto* env = std::getenv("TOPOEXEC_GIT_COMMIT"); env != nullptr && std::string(env).size() > 0u) {
    return env;
  }
#ifdef TOPOEXEC_GIT_COMMIT_DEFAULT
  std::string configured = TOPOEXEC_GIT_COMMIT_DEFAULT;
  if (!configured.empty() && configured != "unknown") {
    return configured;
  }
#endif
  return read_git_head_commit(".git");
}

std::uint64_t hardware_threads() {
  return static_cast<std::uint64_t>(std::thread::hardware_concurrency());
}

std::string cpp_standard() {
  return std::to_string(__cplusplus);
}

std::string benchmark_schema_version() {
  return "2";
}

std::string executable_directory(const char* argv0) {
#if defined(__linux__)
  std::error_code link_error;
  const auto self = std::filesystem::read_symlink("/proc/self/exe", link_error);
  if (!link_error && !self.empty()) {
    return self.parent_path().string();
  }
#endif
  if (argv0 == nullptr || std::string(argv0).empty()) {
    return {};
  }
  std::error_code path_error;
  const auto absolute = std::filesystem::absolute(argv0, path_error);
  if (path_error) {
    return {};
  }
  return absolute.parent_path().string();
}

std::string find_schema_path() {
  if (const auto* env = std::getenv("TOPOEXEC_SCHEMA_PATH"); env != nullptr && std::string(env).size() > 0u) {
    return env;
  }
  std::vector<std::string> candidates;
  if (!g_executable_dir.empty()) {
    candidates.push_back(g_executable_dir + "/../share/topoexec/schema/topoexec.schema.v1.json");
  }
  candidates.insert(candidates.end(), {
                                          "schema/topoexec.schema.v1.json",
                                          "../share/topoexec/schema/topoexec.schema.v1.json",
                                          "share/topoexec/schema/topoexec.schema.v1.json",
                                      });
  for (const auto& candidate : candidates) {
    std::ifstream input(candidate);
    if (input) {
      return candidate;
    }
  }
  return {};
}

std::vector<std::string> existing_yaml_files(const std::string& directory) {
  std::vector<std::string> values;
  const std::vector<std::string> names = {
      directory + "/minimal.yaml",
      directory + "/control_feedback_delay.yaml",
      directory + "/composite_loop.yaml",
      directory + "/large_payload_copy.yaml",
      directory + "/diagnostic_warnings.yaml",
      directory + "/single_component.yaml",
      directory + "/immediate_chain.yaml",
      directory + "/fan_out.yaml",
      directory + "/fan_in.yaml",
      directory + "/latest_vs_queue.yaml",
      directory + "/deferred_edges.yaml",
      directory + "/thread_pool.yaml",
      directory + "/composite_loop_iterations.yaml",
      directory + "/payload_policies.yaml",
      directory + "/channel_modes.yaml",
      directory + "/trigger_policies.yaml",
  };
  for (const auto& name : names) {
    std::ifstream input(name);
    if (input) {
      values.push_back(name);
    }
  }
  return values;
}

double percentile(std::vector<double> values, double ratio) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const auto position = ratio * static_cast<double>(values.size() - 1u);
  const auto lower = static_cast<std::size_t>(std::floor(position));
  const auto upper = static_cast<std::size_t>(std::ceil(position));
  if (lower == upper) {
    return values[lower];
  }
  const auto fraction = position - static_cast<double>(lower);
  return values[lower] + ((values[upper] - values[lower]) * fraction);
}

nlohmann::json runtime_metrics_json(const topoexec::RuntimeRunnerResult& result) {
  nlohmann::json metrics = nlohmann::json::array();
  for (const auto& sample : result.runtime_metrics) {
    metrics.push_back({{"name", sample.name},
                       {"value", sample.value},
                       {"component_id", sample.component_id},
                       {"lane", sample.lane},
                       {"channel_id", sample.channel_id},
                       {"tags", sample.tags}});
  }
  return metrics;
}

nlohmann::json runtime_trace_json(const topoexec::RuntimeRunnerResult& result) {
  nlohmann::json events = nlohmann::json::array();
  for (const auto& event : result.trace) {
    events.push_back({{"name", event.name},
                      {"trace_id", event.trace_id},
                      {"phase", event.phase},
                      {"component_id", event.component_id},
                      {"channel_id", event.channel_id},
                      {"lane", event.lane},
                      {"worker_id", event.worker_id},
                      {"epoch_id", event.epoch_id},
                      {"transaction_id", event.transaction_id},
                      {"correlation_id", event.correlation_id},
                      {"causation_id", event.causation_id},
                      {"start_offset_ns", event.start_offset_ns},
                      {"duration_ns", event.duration_ns},
                      {"attributes", event.attributes}});
  }
  return events;
}

nlohmann::json runtime_errors_json(const topoexec::RuntimeRunnerResult& result) {
  nlohmann::json errors = nlohmann::json::array();
  for (const auto& error : result.runtime_errors) {
    errors.push_back({{"phase", error.phase},
                      {"component_id", error.component_id},
                      {"lane", error.lane},
                      {"message", error.message},
                      {"code", error.code},
                      {"trace_id", error.trace_id},
                      {"fatal", error.fatal}});
  }
  return errors;
}

nlohmann::json runtime_health_events_json(const topoexec::RuntimeRunnerResult& result) {
  nlohmann::json events = nlohmann::json::array();
  for (const auto& event : result.health_events) {
    events.push_back({{"kind", topoexec::to_string(event.kind)},
                      {"source", event.source},
                      {"component_id", event.component_id},
                      {"lane", event.lane},
                      {"channel_id", event.channel_id},
                      {"edge_id", event.edge_id},
                      {"policy", event.policy},
                      {"reason", event.reason},
                      {"sequence", event.sequence},
                      {"depth", event.depth},
                      {"capacity", event.capacity},
                      {"occurrence_count", event.occurrence_count},
                      {"attributes", event.attributes}});
  }
  return events;
}

std::string live_kind_name(std::uint32_t kind) {
  using Kind = topoexec::runtime_observe::LiveEventKind;
  switch (static_cast<Kind>(kind)) {
  case Kind::kRunStarted:
    return "run_started";
  case Kind::kRunFinished:
    return "run_finished";
  case Kind::kSchedulerEpochBegin:
    return "scheduler_epoch_begin";
  case Kind::kSchedulerEpochEnd:
    return "scheduler_epoch_end";
  case Kind::kComponentBegin:
    return "component_begin";
  case Kind::kComponentEnd:
    return "component_end";
  case Kind::kComponentError:
    return "component_error";
  case Kind::kChannelPublishSummary:
    return "channel_publish_summary";
  case Kind::kChannelCommitSummary:
    return "channel_commit_summary";
  case Kind::kChannelDrop:
    return "channel_drop";
  case Kind::kChannelReject:
    return "channel_reject";
  case Kind::kChannelOverwrite:
    return "channel_overwrite";
  case Kind::kTriggerReadySummary:
    return "trigger_ready_summary";
  case Kind::kTriggerSuppressedSummary:
    return "trigger_suppressed_summary";
  case Kind::kAsyncAdmission:
    return "async_admission";
  case Kind::kAsyncReject:
    return "async_reject";
  case Kind::kAsyncDrop:
    return "async_drop";
  case Kind::kLoopIterationBegin:
    return "loop_iteration_begin";
  case Kind::kLoopIterationEnd:
    return "loop_iteration_end";
  case Kind::kLoopConverged:
    return "loop_converged";
  case Kind::kLoopBudgetOverrun:
    return "loop_budget_overrun";
  case Kind::kLoopMaxIterationsHit:
    return "loop_max_iterations_hit";
  case Kind::kLoopError:
    return "loop_error";
  case Kind::kHealthEvent:
    return "health_event";
  case Kind::kRuntimeError:
    return "runtime_error";
  case Kind::kObserverDropSummary:
    return "observer_drop_summary";
  }
  return "unknown";
}

std::string live_exactness_name(std::uint32_t exactness) {
  using Exactness = topoexec::runtime_observe::LiveEventExactness;
  switch (static_cast<Exactness>(exactness)) {
  case Exactness::kExact:
    return "exact";
  case Exactness::kAggregated:
    return "aggregated";
  case Exactness::kSampled:
    return "sampled";
  case Exactness::kLossy:
    return "lossy";
  case Exactness::kPartial:
    return "partial";
  }
  return "partial";
}

std::string live_severity_name(std::uint32_t severity) {
  using Severity = topoexec::runtime_observe::LiveEventSeverity;
  switch (static_cast<Severity>(severity)) {
  case Severity::kTrace:
    return "trace";
  case Severity::kDebug:
    return "debug";
  case Severity::kInfo:
    return "info";
  case Severity::kWarning:
    return "warning";
  case Severity::kError:
    return "error";
  }
  return "info";
}

nlohmann::json live_symbol_table_json(const topoexec::GraphSpec& graph, const std::string& run_id) {
  auto component_id_map = [](const std::vector<topoexec::ComponentNodeSpec>& records) {
    nlohmann::json values = nlohmann::json::object();
    std::uint32_t id = 1;
    for (const auto& record : records) {
      values[std::to_string(id++)] = record.id;
    }
    return values;
  };
  auto edge_id_map = [](const std::vector<topoexec::EdgeSpec>& records) {
    nlohmann::json values = nlohmann::json::object();
    std::uint32_t id = 1;
    for (const auto& record : records) {
      values[std::to_string(id++)] = record.id;
    }
    return values;
  };
  auto lane_id_map = [](const std::vector<topoexec::LaneSpec>& records) {
    nlohmann::json values = nlohmann::json::object();
    std::uint32_t id = 1;
    for (const auto& record : records) {
      values[std::to_string(id++)] = record.id;
    }
    return values;
  };
  nlohmann::json loops = nlohmann::json::object();
  std::uint32_t loop_id = 1;
  for (const auto& loop : graph.composite_loops) {
    loops[std::to_string(loop_id++)] = loop.id;
  }
  return {{"observe_schema_version", "1"},
          {"kind", "symbol_table"},
          {"run_id", run_id},
          {"components", component_id_map(graph.components)},
          {"channels", edge_id_map(graph.edges)},
          {"lanes", lane_id_map(graph.lanes)},
          {"loops", loops},
          {"policies", nlohmann::json::object()},
          {"reasons", nlohmann::json::object()}};
}

std::map<std::uint32_t, std::string> reverse_live_ids(const nlohmann::json& table, const std::string& key) {
  std::map<std::uint32_t, std::string> values;
  if (!table.contains(key) || !table.at(key).is_object()) {
    return values;
  }
  for (const auto& [id, name] : table.at(key).items()) {
    values[static_cast<std::uint32_t>(std::stoul(id))] = name.get<std::string>();
  }
  return values;
}

std::string live_name_for(const std::map<std::uint32_t, std::string>& names, std::uint32_t id) {
  const auto found = names.find(id);
  return found == names.end() ? std::string{} : found->second;
}

nlohmann::json live_event_json(const topoexec::runtime_observe::LiveEvent& event, const std::string& run_id,
                               std::uint64_t display_seq, const std::map<std::uint32_t, std::string>& component_names,
                               const std::map<std::uint32_t, std::string>& channel_names,
                               const std::map<std::uint32_t, std::string>& lane_names,
                               const std::map<std::uint32_t, std::string>& loop_names) {
  nlohmann::json value = {
      {"observe_schema_version", "1"},
      {"run_id", run_id},
      {"display_seq", display_seq},
      {"stream_id", "stream:" + std::to_string(event.stream_id)},
      {"local_seq", event.local_seq},
      {"kind", live_kind_name(event.kind)},
      {"severity", live_severity_name(event.reason_id)},
      {"exactness", live_exactness_name(event.flags)},
      {"mono_ns", event.mono_ns},
      {"epoch_id", event.epoch_id == 0u ? "" : std::to_string(event.epoch_id)},
      {"lane", live_name_for(lane_names, event.lane_id)},
      {"worker_id", event.worker_id == 0u ? "" : std::to_string(event.worker_id)},
      {"component_id", live_name_for(component_names, event.component_id)},
      {"channel_id", live_name_for(channel_names, event.channel_id)},
      {"loop_id", live_name_for(loop_names, event.loop_id)},
      {"trace_id", ""},
      {"transaction_id", ""},
      {"correlation_id", ""},
      {"causation_id", ""},
      {"attributes",
       {{"value0", event.value0}, {"value1", event.value1}, {"value2", event.value2}, {"value3", event.value3}}}};
  if (event.kind ==
      topoexec::runtime_observe::encode_kind(topoexec::runtime_observe::LiveEventKind::kObserverDropSummary)) {
    value["dropped_event_count"] = event.value0;
    value["total_dropped_event_count"] = event.value1;
    value["affected_kind"] = event.value2 == 0u ? "" : live_kind_name(static_cast<std::uint32_t>(event.value2));
  }
  return value;
}

topoexec::runtime_observe::LiveObserveLevel parse_live_observe_level(const std::string& level) {
  if (level == "off") {
    return topoexec::runtime_observe::LiveObserveLevel::kOff;
  }
  if (level == "summary") {
    return topoexec::runtime_observe::LiveObserveLevel::kSummary;
  }
  if (level == "detailed") {
    return topoexec::runtime_observe::LiveObserveLevel::kDetailed;
  }
  if (level == "debug") {
    return topoexec::runtime_observe::LiveObserveLevel::kDebug;
  }
  throw std::runtime_error("unknown observe level: " + level);
}

std::string observe_run_id(const std::string& graph_path) {
  const auto hash = fnv1a64_hex(read_text_file(graph_path));
  return "run-" + hash.substr(0, 12);
}

bool json_event_matches_where(const nlohmann::json& event, const YAML::Node& where) {
  if (!where || !where.IsMap()) {
    return true;
  }
  for (const auto& item : where) {
    const auto key = item.first.as<std::string>();
    const auto expected = item.second.as<std::string>();
    if (event.contains(key)) {
      if (event.at(key).is_string() && event.at(key).get<std::string>() == expected) {
        continue;
      }
      if (!event.at(key).is_string() && event.at(key).dump() == expected) {
        continue;
      }
    }
    if (event.contains("attributes") && event.at("attributes").contains(key)) {
      const auto& value = event.at("attributes").at(key);
      if ((value.is_string() && value.get<std::string>() == expected) ||
          (!value.is_string() && value.dump() == expected)) {
        continue;
      }
    }
    return false;
  }
  return true;
}

bool json_event_matches_assertion(const nlohmann::json& event, const YAML::Node& assertion) {
  if (assertion["event"] && event.value("kind", "") != assertion["event"].as<std::string>()) {
    return false;
  }
  return json_event_matches_where(event, assertion["where"]);
}

double metric_value_for_assertion(const topoexec::RuntimeRunnerResult& result, const std::string& metric_name) {
  double value = 0.0;
  for (const auto& sample : result.runtime_metrics) {
    if (sample.name == metric_name) {
      value += sample.value;
    }
  }
  return value;
}

std::size_t counter_value_for_assertion(const topoexec::RuntimeRunnerResult& result, const std::string& source) {
  if (source == "runtime_errors") {
    return result.runtime_errors.size();
  }
  if (source == "observer_drops" || source == "runtime.observer.dropped_event_count") {
    return result.live_observe_dropped_event_count;
  }
  if (source == "events") {
    return result.live_events.size();
  }
  return static_cast<std::size_t>(metric_value_for_assertion(result, source));
}

struct LiveAssertionEvaluation {
  bool ok{true};
  nlohmann::json events = nlohmann::json::array();
  nlohmann::json result = nlohmann::json::object();
};

LiveAssertionEvaluation evaluate_live_assertions(const std::string& assert_file,
                                                 const topoexec::RuntimeRunnerResult& result,
                                                 const nlohmann::json& observe_events) {
  LiveAssertionEvaluation evaluation;
  evaluation.result["assertion_schema_version"] = "1";
  evaluation.result["kind"] = "assertion_result";
  evaluation.result["ok"] = true;
  evaluation.result["passed"] = 0;
  evaluation.result["failed"] = 0;
  evaluation.result["pending"] = 0;
  evaluation.result["failures"] = nlohmann::json::array();
  if (assert_file.empty()) {
    return evaluation;
  }

  const auto root = YAML::LoadFile(assert_file);
  if (!root["assertion_schema_version"] || root["assertion_schema_version"].as<std::string>() != "1") {
    throw std::runtime_error("assertion file must set assertion_schema_version: \"1\"");
  }
  const auto assertions = root["assertions"];
  if (!assertions || !assertions.IsSequence()) {
    throw std::runtime_error("assertion file must contain assertions list");
  }

  for (const auto& assertion : assertions) {
    const auto id = assertion["id"] ? assertion["id"].as<std::string>() : std::string{"unnamed"};
    const auto type = assertion["type"] ? assertion["type"].as<std::string>() : std::string{};
    evaluation.events.push_back({{"kind", "assertion_registered"},
                                 {"assertion_schema_version", "1"},
                                 {"assertion_id", id},
                                 {"exactness", "exact"}});

    bool passed = false;
    bool pending = false;
    std::string reason;
    if (type == "counter_equals") {
      const auto source = assertion["source"] ? assertion["source"].as<std::string>() : std::string{};
      const auto expected = assertion["value"].as<std::size_t>();
      const auto actual = counter_value_for_assertion(result, source);
      passed = actual == expected;
      if (!passed) {
        reason = "counter " + source + " expected " + std::to_string(expected) + " got " + std::to_string(actual);
      }
    } else if (type == "metric_equals" || type == "metric_lte" || type == "metric_gte") {
      const auto metric = assertion["metric"].as<std::string>();
      const auto expected = assertion["value"].as<double>();
      const auto actual = metric_value_for_assertion(result, metric);
      if (type == "metric_equals") {
        passed = std::fabs(actual - expected) < 0.000001;
      } else if (type == "metric_lte") {
        passed = actual <= expected;
      } else {
        passed = actual >= expected;
      }
      if (!passed) {
        reason = "metric " + metric + " check failed";
      }
    } else if (type == "eventually" || type == "within_events" || type == "within_epochs") {
      std::size_t limit = observe_events.size();
      if (assertion["within_events"]) {
        limit = std::min(limit, assertion["within_events"].as<std::size_t>());
      }
      if (type == "within_events" && assertion["value"]) {
        limit = std::min(limit, assertion["value"].as<std::size_t>());
      }
      for (std::size_t index = 0; index < limit; ++index) {
        if (json_event_matches_assertion(observe_events.at(index), assertion)) {
          passed = true;
          break;
        }
      }
      if (!passed && type == "eventually" && !assertion["within_events"] && !assertion["within_epochs"]) {
        pending = true;
        reason = "eventually condition still pending";
      } else if (!passed) {
        reason = "event condition not satisfied";
      }
    } else if (type == "never" || type == "always") {
      bool any_match = false;
      bool any_mismatch = false;
      for (const auto& event : observe_events) {
        const bool matches = json_event_matches_assertion(event, assertion);
        any_match = any_match || matches;
        any_mismatch = any_mismatch || !matches;
      }
      passed = type == "never" ? !any_match : !any_mismatch;
      if (!passed) {
        reason = type == "never" ? "forbidden event observed" : "not all events matched";
      }
    } else if (type == "sequence") {
      const auto sequence = assertion["sequence"];
      if (!sequence || !sequence.IsSequence()) {
        reason = "sequence assertion requires sequence list";
      } else {
        std::size_t cursor = 0;
        passed = true;
        for (const auto& step : sequence) {
          bool found = false;
          for (; cursor < observe_events.size(); ++cursor) {
            if (json_event_matches_assertion(observe_events.at(cursor), step)) {
              found = true;
              ++cursor;
              break;
            }
          }
          if (!found) {
            passed = false;
            reason = "sequence step not observed";
            break;
          }
        }
      }
    } else {
      reason = "unsupported assertion type: " + type;
    }

    if (passed) {
      evaluation.result["passed"] = evaluation.result["passed"].get<int>() + 1;
      evaluation.events.push_back({{"kind", "assertion_pass"},
                                   {"assertion_schema_version", "1"},
                                   {"assertion_id", id},
                                   {"exactness", "exact"}});
    } else if (pending) {
      evaluation.ok = false;
      evaluation.result["pending"] = evaluation.result["pending"].get<int>() + 1;
      evaluation.events.push_back({{"kind", "assertion_pending"},
                                   {"assertion_schema_version", "1"},
                                   {"assertion_id", id},
                                   {"reason", reason},
                                   {"exactness", "exact"}});
    } else {
      evaluation.ok = false;
      evaluation.result["failed"] = evaluation.result["failed"].get<int>() + 1;
      evaluation.result["failures"].push_back({{"id", id}, {"reason", reason}});
      evaluation.events.push_back({{"kind", "assertion_fail"},
                                   {"assertion_schema_version", "1"},
                                   {"assertion_id", id},
                                   {"reason", reason},
                                   {"exactness", "exact"}});
    }
  }
  evaluation.result["ok"] = evaluation.ok;
  return evaluation;
}

struct ObserveOutputFilter {
  std::vector<std::string> include_components;
  std::vector<std::string> include_channels;
  std::vector<std::string> include_events;
  std::vector<std::string> exclude_events;
  std::vector<std::string> sample_events;
};

bool contains_string(const std::vector<std::string>& values, const std::string& value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

std::map<std::string, std::size_t> parse_sample_rules(const std::vector<std::string>& rules) {
  std::map<std::string, std::size_t> parsed;
  for (const auto& rule : rules) {
    const auto separator = rule.find(':');
    if (separator == std::string::npos || separator == 0u || separator + 1u >= rule.size()) {
      throw std::runtime_error("--sample-event must use KIND:RATIO");
    }
    auto ratio = static_cast<std::size_t>(std::stoull(rule.substr(separator + 1u)));
    if (ratio == 0u) {
      throw std::runtime_error("--sample-event ratio must be positive");
    }
    parsed[rule.substr(0, separator)] = ratio;
  }
  return parsed;
}

bool observe_event_selected(const nlohmann::json& event, const ObserveOutputFilter& filter,
                            const std::map<std::string, std::size_t>& sample_rules,
                            std::map<std::string, std::size_t>& sample_counts) {
  const auto kind = event.value("kind", std::string{});
  if (!filter.include_events.empty() && !contains_string(filter.include_events, kind)) {
    return false;
  }
  if (contains_string(filter.exclude_events, kind)) {
    return false;
  }
  if (!filter.include_components.empty()) {
    const auto component = event.value("component_id", std::string{});
    if (component.empty() || !contains_string(filter.include_components, component)) {
      return false;
    }
  }
  if (!filter.include_channels.empty()) {
    const auto channel = event.value("channel_id", std::string{});
    if (channel.empty() || !contains_string(filter.include_channels, channel)) {
      return false;
    }
  }
  const auto sample_rule = sample_rules.find(kind);
  if (sample_rule != sample_rules.end()) {
    const auto count = ++sample_counts[kind];
    return (count - 1u) % sample_rule->second == 0u;
  }
  return true;
}

void write_text_file(const std::filesystem::path& path, const std::string& text) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("failed to write file: " + path.string());
  }
  output << text;
}

nlohmann::json chrome_trace_json(const topoexec::RuntimeRunnerResult& result);

nlohmann::json normalized_graph_json(const topoexec::GraphSpec& graph) {
  nlohmann::json components = nlohmann::json::array();
  for (const auto& component : graph.components) {
    components.push_back({{"id", component.id}, {"type", component.type}, {"lane", component.execution.lane}});
  }
  nlohmann::json edges = nlohmann::json::array();
  for (const auto& edge : graph.edges) {
    edges.push_back({{"id", edge.id},
                     {"from", edge.from},
                     {"to", edge.to},
                     {"kind", topoexec::to_string(edge.kind)},
                     {"mode", edge.policy.mode},
                     {"capacity", edge.policy.capacity},
                     {"overflow", edge.policy.overflow}});
  }
  nlohmann::json lanes = nlohmann::json::array();
  for (const auto& lane : graph.lanes) {
    lanes.push_back({{"id", lane.id}, {"type", lane.type}, {"hz", lane.hz}, {"max_threads", lane.max_threads}});
  }
  nlohmann::json loops = nlohmann::json::array();
  for (const auto& loop : graph.composite_loops) {
    loops.push_back({{"id", loop.id}, {"components", loop.components}, {"policy", loop.loop_policy.type}});
  }
  return {{"graph_schema_version", topoexec::kTopoExecSchemaVersion},
          {"graph_name", graph.name},
          {"components", components},
          {"edges", edges},
          {"lanes", lanes},
          {"composite_loops", loops}};
}

void write_record_artifact(const std::string& record_dir, const std::string& graph_path,
                           const topoexec::GraphSpec& graph, const topoexec::GraphValidationResult& validation,
                           const topoexec::RuntimeRunnerResult& result, const std::string& run_id,
                           const std::string& observe_level, const nlohmann::json& ndjson_records,
                           const nlohmann::json& final_summary, const nlohmann::json& assertion_result,
                           const std::string& assert_file) {
  if (record_dir.empty()) {
    return;
  }
  const std::filesystem::path dir(record_dir);
  std::filesystem::create_directories(dir);
  std::error_code error;
  std::filesystem::copy_file(graph_path, dir / "graph.yaml", std::filesystem::copy_options::overwrite_existing, error);
  if (error) {
    throw std::runtime_error("failed to copy graph into record artifact: " + error.message());
  }
  write_text_file(dir / "graph.normalized.json", normalized_graph_json(graph).dump(2) + "\n");
  write_text_file(dir / "plan.json", topoexec::graph_plan_json(graph, validation.compiled_plan) + "\n");
  write_text_file(dir / "render.mmd", topoexec::graph_mermaid(graph, validation.compiled_plan));

  std::ostringstream observe_stream;
  for (const auto& record : ndjson_records) {
    observe_stream << record.dump() << "\n";
  }
  write_text_file(dir / "observe.ndjson", observe_stream.str());
  write_text_file(dir / "observe.summary.json", final_summary.dump(2) + "\n");
  if (!assert_file.empty()) {
    std::filesystem::copy_file(assert_file, dir / "assertions.yaml", std::filesystem::copy_options::overwrite_existing,
                               error);
    if (error) {
      throw std::runtime_error("failed to copy assertions into record artifact: " + error.message());
    }
  } else {
    write_text_file(dir / "assertions.yaml", "assertion_schema_version: \"1\"\nassertions: []\n");
  }
  write_text_file(dir / "assertion_result.json", assertion_result.dump(2) + "\n");
  write_text_file(dir / "metrics.final.json",
                  nlohmann::json{{"metric_schema_version", topoexec::kRuntimeMetricSchemaVersion},
                                 {"metrics", runtime_metrics_json(result)}}
                          .dump(2) +
                      "\n");
  write_text_file(dir / "trace.final.json",
                  nlohmann::json{{"trace_schema_version", topoexec::kRuntimeTraceSchemaVersion},
                                 {"trace", runtime_trace_json(result)}}
                          .dump(2) +
                      "\n");
  write_text_file(dir / "trace.chrome.json", chrome_trace_json(result).dump(2) + "\n");
  write_text_file(dir / "health.final.json",
                  nlohmann::json{{"health_events", runtime_health_events_json(result)}}.dump(2) + "\n");
  write_text_file(dir / "dashboard.html",
                  "<!doctype html><meta charset=\"utf-8\"><title>TopoExec Live Artifact</title>"
                  "<h1>TopoExec Live Artifact</h1><p>Replay this directory with tools/topoexec_live_server.py.</p>\n");
  const nlohmann::json manifest = {{"artifact_schema_version", "1"},
                                   {"run_id", run_id},
                                   {"graph_name", graph.name},
                                   {"observe_schema_version", "1"},
                                   {"assertion_schema_version", "1"},
                                   {"observe_level", observe_level},
                                   {"files",
                                    {{"graph", "graph.yaml"},
                                     {"graph_normalized", "graph.normalized.json"},
                                     {"plan", "plan.json"},
                                     {"render", "render.mmd"},
                                     {"observe", "observe.ndjson"},
                                     {"observe_summary", "observe.summary.json"},
                                     {"assertions", "assertions.yaml"},
                                     {"assertion_result", "assertion_result.json"},
                                     {"metrics", "metrics.final.json"},
                                     {"trace", "trace.final.json"},
                                     {"chrome_trace", "trace.chrome.json"},
                                     {"health", "health.final.json"},
                                     {"dashboard", "dashboard.html"}}},
                                   {"summary",
                                    {{"runtime_ok", result.ok},
                                     {"assertions_ok", assertion_result.value("ok", true)},
                                     {"observer_dropped_event_count", result.live_observe_dropped_event_count}}}};
  write_text_file(dir / "manifest.json", manifest.dump(2) + "\n");
}

int print_observe_result(const std::string& path, const topoexec::GraphSpec& graph,
                         const topoexec::GraphValidationResult& validation, const topoexec::RuntimeRunnerResult& result,
                         const std::string& observe_level, const std::string& format, std::uint64_t ui_frame_ms,
                         const std::string& assert_file, const std::string& record_dir, bool fail_on_assertion_fail,
                         const ObserveOutputFilter& output_filter = {}) {
  const auto run_id = observe_run_id(path);
  const auto symbols = live_symbol_table_json(graph, run_id);
  const auto component_names = reverse_live_ids(symbols, "components");
  const auto channel_names = reverse_live_ids(symbols, "channels");
  const auto lane_names = reverse_live_ids(symbols, "lanes");
  const auto loop_names = reverse_live_ids(symbols, "loops");
  const bool has_output_filter = !output_filter.include_components.empty() || !output_filter.include_channels.empty() ||
                                 !output_filter.include_events.empty() || !output_filter.exclude_events.empty() ||
                                 !output_filter.sample_events.empty();
  const bool need_full_event_records =
      format != "json-summary" || !assert_file.empty() || !record_dir.empty() || has_output_filter;
  nlohmann::json event_lines = nlohmann::json::array();
  nlohmann::json event_kind_counts = nlohmann::json::object();
  auto sample_rules = parse_sample_rules(output_filter.sample_events);
  std::map<std::string, std::size_t> sample_counts;
  std::uint64_t display_seq = 1;
  for (const auto& event : result.live_events) {
    const auto kind = live_kind_name(event.kind);
    event_kind_counts[kind] = event_kind_counts.value(kind, 0u) + 1u;
    if (need_full_event_records) {
      auto json_event =
          live_event_json(event, run_id, display_seq, component_names, channel_names, lane_names, loop_names);
      if (observe_event_selected(json_event, output_filter, sample_rules, sample_counts)) {
        event_lines.push_back(std::move(json_event));
        ++display_seq;
      }
    }
  }
  auto assertion_evaluation = evaluate_live_assertions(assert_file, result, event_lines);
  for (auto& assertion_event : assertion_evaluation.events) {
    assertion_event["observe_schema_version"] = "1";
    assertion_event["run_id"] = run_id;
  }
  assertion_evaluation.result["observe_schema_version"] = "1";
  assertion_evaluation.result["run_id"] = run_id;

  const nlohmann::json final_summary = {
      {"observe_schema_version", "1"},
      {"kind", "final_summary"},
      {"run_id", run_id},
      {"runtime_ok", result.ok},
      {"assertions_ok", assertion_evaluation.ok},
      {"tick_calls", result.tick_calls},
      {"runtime_error_count", result.runtime_errors.size()},
      {"observer_dropped_event_count", result.live_observe_dropped_event_count},
      {"observe_level", observe_level},
      {"ui_frame_ms", ui_frame_ms},
      {"exactness", result.live_observe_dropped_event_count == 0u ? "exact" : "lossy"}};
  nlohmann::json ndjson_records = nlohmann::json::array();
  ndjson_records.push_back(symbols);
  ndjson_records.push_back({{"observe_schema_version", "1"},
                            {"kind", "graph_validated"},
                            {"run_id", run_id},
                            {"ok", validation.ok},
                            {"errors", validation.errors},
                            {"exactness", "exact"}});
  ndjson_records.push_back({{"observe_schema_version", "1"},
                            {"kind", "plan_ready"},
                            {"run_id", run_id},
                            {"ok", validation.ok},
                            {"region_order", validation.compiled_plan.region_order},
                            {"exactness", "exact"}});
  for (const auto& event : event_lines) {
    ndjson_records.push_back(event);
  }
  for (const auto& event : assertion_evaluation.events) {
    ndjson_records.push_back(event);
  }
  if (!assert_file.empty()) {
    ndjson_records.push_back(assertion_evaluation.result);
  }
  ndjson_records.push_back(final_summary);
  write_record_artifact(record_dir, path, graph, validation, result, run_id, observe_level, ndjson_records,
                        final_summary, assertion_evaluation.result, assert_file);
  if (format == "json-summary") {
    std::cout << nlohmann::json{{"observe_schema_version", "1"},
                                {"run_id", run_id},
                                {"symbol_table", symbols},
                                {"graph_validated", validation.ok},
                                {"plan_ready", validation.ok},
                                {"event_count", result.live_events.size()},
                                {"event_kind_counts", event_kind_counts},
                                {"events", need_full_event_records ? event_lines : nlohmann::json::array()},
                                {"assertion_events", assertion_evaluation.events},
                                {"assertion_result", assertion_evaluation.result},
                                {"final_summary", final_summary}}
                     .dump(2)
              << "\n";
    if (!result.ok) {
      return 1;
    }
    return (!assertion_evaluation.ok && fail_on_assertion_fail) ? 3 : 0;
  }

  std::cout << symbols.dump() << "\n";
  std::cout << ndjson_records.at(1).dump() << "\n";
  std::cout << ndjson_records.at(2).dump() << "\n";
  for (const auto& event : event_lines) {
    std::cout << event.dump() << "\n";
  }
  for (const auto& event : assertion_evaluation.events) {
    std::cout << event.dump() << "\n";
  }
  if (!assert_file.empty()) {
    std::cout << assertion_evaluation.result.dump() << "\n";
  }
  std::cout << final_summary.dump() << "\n";
  if (!result.ok) {
    return 1;
  }
  return (!assertion_evaluation.ok && fail_on_assertion_fail) ? 3 : 0;
}

nlohmann::json chrome_trace_json(const topoexec::RuntimeRunnerResult& result) {
  nlohmann::json events = nlohmann::json::array();
  std::map<std::string, std::size_t> track_ids;
  auto track_id_for = [&](const topoexec::RuntimeTraceEvent& event) {
    auto key = event.phase;
    if (!event.channel_id.empty()) {
      key += ":channel:" + event.channel_id;
    } else if (!event.lane.empty()) {
      key += ":lane:" + event.lane;
    } else if (!event.component_id.empty()) {
      key += ":component:" + event.component_id;
    } else {
      key += ":runtime";
    }
    auto [found, inserted] = track_ids.emplace(std::move(key), track_ids.size());
    (void)inserted;
    return found->second;
  };
  for (const auto& event : result.trace) {
    nlohmann::json chrome_event;
    chrome_event["name"] = event.name;
    chrome_event["cat"] = "topoexec";
    chrome_event["ph"] = "X";
    chrome_event["ts"] = static_cast<double>(event.start_offset_ns) / 1000.0;
    chrome_event["dur"] = static_cast<double>(event.duration_ns) / 1000.0;
    chrome_event["pid"] = 1;
    chrome_event["tid"] = track_id_for(event);
    chrome_event["args"] = event.attributes;
    chrome_event["args"]["trace_id"] = event.trace_id;
    chrome_event["args"]["phase"] = event.phase;
    chrome_event["args"]["component_id"] = event.component_id;
    chrome_event["args"]["channel_id"] = event.channel_id;
    chrome_event["args"]["lane"] = event.lane;
    events.push_back(std::move(chrome_event));
  }
  return {{"traceEvents", events},
          {"displayTimeUnit", "ns"},
          {"trace_schema_version", topoexec::kRuntimeTraceSchemaVersion}};
}

nlohmann::json runner_result_json(const topoexec::RuntimeRunnerResult& result) {
  return {{"ok", result.ok},
          {"errors", result.errors},
          {"runtime_errors", runtime_errors_json(result)},
          {"graph_name", result.graph_name},
          {"component_count", result.component_count},
          {"channel_count", result.channel_count},
          {"tick_calls", result.tick_calls},
          {"stop_reason", topoexec::to_string(result.scheduler_stop_reason)},
          {"ticked_components", result.ticked_components},
          {"channel_publish_count", result.channel_publish_count},
          {"channel_delivery_count", result.channel_delivery_count},
          {"channel_drop_count", result.channel_drop_count},
          {"payload_copy_count", result.payload_copy_count},
          {"staged_publication_count", result.staged_publication_count},
          {"committed_publication_count", result.committed_publication_count},
          {"delayed_publication_count", result.delayed_publication_count},
          {"state_publication_count", result.state_publication_count},
          {"state_commit_count", result.state_commit_count},
          {"async_publication_count", result.async_publication_count},
          {"failed_publication_commit_count", result.failed_publication_commit_count},
          {"health_event_count", result.health_event_count},
          {"health_event_dropped_count", result.health_event_dropped_count},
          {"health_event_coalesced_count", result.health_event_coalesced_count},
          {"health_events", runtime_health_events_json(result)},
          {"trace_event_count", result.trace_event_count},
          {"trace_schema_version", topoexec::kRuntimeTraceSchemaVersion},
          {"trace_events", result.trace_events},
          {"trace", runtime_trace_json(result)},
          {"loop_iteration_count", result.loop_iteration_count},
          {"loop_converged_count", result.loop_converged_count},
          {"loop_budget_overrun_count", result.loop_budget_overrun_count},
          {"loop_max_iteration_hit_count", result.loop_max_iteration_hit_count},
          {"loop_output_discarded_count", result.loop_output_discarded_count},
          {"loop_last_residual", result.loop_last_residual},
          {"loop_stop_reason", result.loop_stop_reason},
          {"metric_schema_version", std::string(topoexec::kRuntimeMetricSchemaVersion)},
          {"metrics", runtime_metrics_json(result)}};
}

int print_runner_result(const topoexec::RuntimeRunnerResult& result, const std::string& format) {
  if (format == "json") {
    std::cout << runner_result_json(result).dump(2) << "\n";
  } else {
    std::cout << (result.ok ? "ok" : "error") << "\n";
    std::cout << "graph: " << result.graph_name << "\n";
    std::cout << "ticks: " << result.tick_calls << "\n";
    std::cout << "stop_reason: " << topoexec::to_string(result.scheduler_stop_reason) << "\n";
    std::cout << "order: " << join(result.ticked_components) << "\n";
    std::cout << "channel_publish_count: " << result.channel_publish_count << "\n";
    std::cout << "channel_delivery_count: " << result.channel_delivery_count << "\n";
    std::cout << "channel_drop_count: " << result.channel_drop_count << "\n";
    std::cout << "health_event_count: " << result.health_event_count << "\n";
    std::cout << "runtime_publication_committed: " << result.committed_publication_count << "\n";
    for (const auto& error : result.errors) {
      std::cout << "- " << error << "\n";
    }
  }
  return result.ok ? 0 : 1;
}

int print_metrics_result(const topoexec::RuntimeRunnerResult& result, const std::string& format) {
  if (format == "json") {
    std::cout << runner_result_json(result).dump(2) << "\n";
  } else {
    std::cout << (result.ok ? "ok" : "error") << "\n";
    for (const auto& sample : result.runtime_metrics) {
      std::cout << sample.name << "=" << sample.value << "\n";
    }
    std::cout << "channel_publish_count=" << result.channel_publish_count << "\n";
    std::cout << "channel_delivery_count=" << result.channel_delivery_count << "\n";
    std::cout << "channel_drop_count=" << result.channel_drop_count << "\n";
    std::cout << "health_event_count=" << result.health_event_count << "\n";
    std::cout << "health_event_dropped_count=" << result.health_event_dropped_count << "\n";
  }
  return result.ok ? 0 : 1;
}

int print_trace_result(const topoexec::RuntimeRunnerResult& result, const std::string& format) {
  if (format == "chrome") {
    std::cout << chrome_trace_json(result).dump(2) << "\n";
  } else if (format == "json") {
    nlohmann::json value;
    value["ok"] = result.ok;
    value["errors"] = result.errors;
    value["graph_name"] = result.graph_name;
    value["trace_event_count"] = result.trace_event_count;
    value["trace_schema_version"] = topoexec::kRuntimeTraceSchemaVersion;
    value["trace_events"] = result.trace_events;
    value["trace"] = runtime_trace_json(result);
    std::cout << value.dump(2) << "\n";
  } else {
    std::cout << (result.ok ? "ok" : "error") << "\n";
    for (const auto& event : result.trace_events) {
      std::cout << event << "\n";
    }
    for (const auto& error : result.errors) {
      std::cout << "- " << error << "\n";
    }
  }
  return result.ok ? 0 : 1;
}

std::vector<LintFinding> lint_graph(const topoexec::GraphSpec& graph,
                                    const topoexec::ComponentRegistry* registry = nullptr) {
  std::vector<LintFinding> findings;
  const auto validation = topoexec::validate_graph_structure(graph);
  if (!validation.diagnostics.empty()) {
    for (const auto& diagnostic : validation.diagnostics) {
      if (diagnostic.severity != "error") {
        continue;
      }
      findings.push_back({diagnostic.severity, diagnostic.code, diagnostic.message,
                          diagnostic.graph_path.empty() ? graph.name : diagnostic.graph_path});
    }
  } else {
    for (const auto& error : validation.errors) {
      findings.push_back({"error", "validation", error, graph.name});
    }
  }

  std::map<std::string, std::string> component_lane;
  for (const auto& component : graph.components) {
    component_lane[component.id] = component.execution.lane;
  }
  std::map<std::string, topoexec::LaneSpec> lanes;
  for (const auto& lane : graph.lanes) {
    lanes[lane.id] = lane;
  }
  std::map<std::string, topoexec::ComponentDescriptor> descriptors;
  if (registry != nullptr) {
    for (const auto& component : graph.components) {
      if (!registry->contains(component.type)) {
        continue;
      }
      try {
        descriptors[component.id] = registry->create(component.type)->describe();
      } catch (const std::exception&) {
        continue;
      }
    }
  }

  std::map<std::string, int> state_writers;
  for (const auto& edge : graph.edges) {
    if (edge.policy.capacity > 1024) {
      findings.push_back(
          {"warning", "suspicious_capacity", "edge capacity is unusually high for an in-process runtime", edge.id});
    }
    if (edge.policy.overflow == "block") {
      const auto target_component = component_id_from_endpoint(edge.to);
      const auto lane_id = component_lane[target_component];
      const auto lane = lanes.find(lane_id);
      if (lane != lanes.end() && lane->second.type == "event_loop") {
        findings.push_back({"warning", "blocking_overflow_event_loop",
                            "blocking overflow can stall a single-thread event_loop", edge.id});
      }
    }
    if (edge.kind == topoexec::EdgeKind::kAsync && edge.policy.max_inflight <= 0 && edge.policy.capacity <= 1) {
      findings.push_back(
          {"info", "async_capacity", "async edge uses capacity <= 1; excess completions may be dropped", edge.id});
    }
    if (edge.kind == topoexec::EdgeKind::kDelay) {
      findings.push_back(
          {"info", "delay_epoch_boundary", "delay edge becomes visible at the next epoch boundary", edge.id});
    }
    if (edge.kind == topoexec::EdgeKind::kState) {
      ++state_writers[edge.to];
    }
    if (slow_reader_drop_risk(edge.policy)) {
      findings.push_back({"info", "slow_reader_drop_risk",
                          "multi-reader edge keeps only bounded history; slow readers can miss dropped messages",
                          edge.id});
    }
    if (edge.policy.copy_policy == "loaned_view" && edge.policy.owner != "producer") {
      findings.push_back({"warning", "loaned_view_without_pool_owner",
                          "loaned_view should declare owner: producer until pool-return callbacks exist", edge.id});
    }
    if (edge.policy.copy_policy == "copy") {
      const auto source_component = component_id_from_endpoint(edge.from);
      const auto source_port = port_name_from_endpoint(edge.from);
      const auto descriptor = descriptors.find(source_component);
      if (descriptor != descriptors.end()) {
        const auto* port = find_port(descriptor->second.outputs, source_port);
        if (port != nullptr && is_large_payload_schema(port->schema)) {
          findings.push_back({"warning", "large_payload_copy",
                              "large payload output uses copy policy; use shared_view or loaned_view", edge.id});
        }
      }
    }
  }
  for (const auto& [target, count] : state_writers) {
    if (count > 1) {
      findings.push_back({"warning", "state_multiple_writers", "state target has multiple writers: " + target, target});
    }
  }
  for (const auto& component : graph.components) {
    const auto lane = lanes.find(component.execution.lane);
    if (lane == lanes.end() || lane->second.hz <= 0.0 || component.execution.budget_ms <= 0) {
      continue;
    }
    const auto period_ms = 1000.0 / lane->second.hz;
    if (static_cast<double>(component.execution.budget_ms) > period_ms) {
      findings.push_back({"warning", "budget_exceeds_lane_period",
                          "component budget exceeds lane period: " + component.id, component.id});
    }
  }
  return findings;
}

int print_lint_findings(const std::vector<LintFinding>& findings, const std::string& format) {
  if (format == "json") {
    nlohmann::json value;
    value["ok"] =
        std::none_of(findings.begin(), findings.end(), [](const auto& finding) { return finding.severity == "error"; });
    value["findings"] = nlohmann::json::array();
    for (const auto& finding : findings) {
      value["findings"].push_back(
          {{"severity", finding.severity}, {"rule", finding.rule}, {"message", finding.message}, {"id", finding.id}});
    }
    std::cout << value.dump(2) << "\n";
  } else {
    if (findings.empty()) {
      std::cout << "ok\n";
    }
    for (const auto& finding : findings) {
      std::cout << finding.severity << " " << finding.rule << " " << finding.id << ": " << finding.message << "\n";
    }
  }
  return std::any_of(findings.begin(), findings.end(), [](const auto& finding) { return finding.severity == "error"; })
             ? 1
             : 0;
}

int print_explain(const topoexec::GraphSpec& graph, const topoexec::GraphValidationResult& validation,
                  const std::string& format) {
  if (format == "json") {
    nlohmann::json value;
    value["ok"] = validation.ok;
    value["errors"] = validation.errors;
    value["graph_name"] = graph.name;
    value["semantics"] = {
        {"publish", "GraphContext::publish stages output; scheduler/runtime commit decides visibility"},
        {"immediate", "visible during the current epoch according to compiled region order"},
        {"delay", "visible at the next epoch boundary"},
        {"state", "staged during the current epoch and committed at the next epoch boundary"},
        {"async", "deferred to a later epoch; no recursive downstream execution"},
    };
    value["edge_policies"] = nlohmann::json::array();
    for (const auto& edge : graph.edges) {
      value["edge_policies"].push_back(edge_policy_json(edge));
    }
    value["diagnostics_schema_version"] = topoexec::kGraphDiagnosticSchemaVersion;
    value["diagnostic_groups"] = grouped_diagnostics_json(validation.diagnostics);
    value["region_order"] = validation.compiled_plan.region_order;
    std::cout << value.dump(2) << "\n";
  } else {
    std::cout << "graph: " << graph.name << "\n";
    std::cout << "publish: staged by runtime; never directly executes downstream components\n";
    std::cout << "immediate: current epoch by compiled region order\n";
    std::cout << "delay/state/async: deferred to next epoch boundary\n";
    const auto groups = grouped_diagnostics_json(validation.diagnostics);
    if (!validation.diagnostics.empty()) {
      std::cout << "diagnostics:\n";
      for (const auto& category : {"graph_structure", "scheduler", "channel", "payload", "trigger"}) {
        if (!groups.contains(category) || groups.at(category).empty()) {
          continue;
        }
        std::cout << "- " << category << ":\n";
        for (const auto& diagnostic : groups.at(category)) {
          std::cout << "  - " << diagnostic.at("severity").get<std::string>() << " "
                    << diagnostic.at("code").get<std::string>() << " " << diagnostic.at("graph_path").get<std::string>()
                    << ": " << diagnostic.at("message").get<std::string>() << "\n";
        }
      }
    }
    std::cout << topoexec::graph_plan_text(graph, validation.compiled_plan);
  }
  return validation.ok ? 0 : 1;
}

nlohmann::json region_json(const topoexec::GraphCompiledPlan& plan) {
  nlohmann::json regions = nlohmann::json::array();
  for (const auto& region : plan.regions) {
    regions.push_back({{"id", region.id},
                       {"kind", topoexec::to_string(region.kind)},
                       {"components", region.components},
                       {"incoming_regions", region.incoming_regions},
                       {"outgoing_regions", region.outgoing_regions}});
  }
  return regions;
}

int print_diff_plan(const std::string& left_path, const std::string& right_path, const std::string& format,
                    const topoexec::GraphInputLimits& limits) {
  topoexec::GraphSpec left_graph;
  topoexec::GraphSpec right_graph;
  const auto left = load_and_validate(left_path, left_graph, limits);
  const auto right = load_and_validate(right_path, right_graph, limits);
  const bool same_region_order = left.compiled_plan.region_order == right.compiled_plan.region_order;
  const bool same_component_regions = left.compiled_plan.component_region == right.compiled_plan.component_region;
  const bool ok = left.ok && right.ok;
  if (format == "json") {
    nlohmann::json value;
    value["ok"] = ok;
    value["left_errors"] = left.errors;
    value["right_errors"] = right.errors;
    value["same_region_order"] = same_region_order;
    value["same_component_regions"] = same_component_regions;
    value["left_region_order"] = left.compiled_plan.region_order;
    value["right_region_order"] = right.compiled_plan.region_order;
    value["left_regions"] = region_json(left.compiled_plan);
    value["right_regions"] = region_json(right.compiled_plan);
    std::cout << value.dump(2) << "\n";
  } else {
    std::cout << (ok ? "ok" : "error") << "\n";
    std::cout << "same_region_order: " << (same_region_order ? "true" : "false") << "\n";
    std::cout << "same_component_regions: " << (same_component_regions ? "true" : "false") << "\n";
    std::cout << "left_order: " << join(left.compiled_plan.region_order) << "\n";
    std::cout << "right_order: " << join(right.compiled_plan.region_order) << "\n";
  }
  return ok ? 0 : 1;
}

int print_bench_result(const std::string& path, std::size_t steps, std::size_t runs, const std::string& format,
                       const topoexec::GraphInputLimits& limits) {
  if (runs == 0u) {
    throw std::runtime_error("bench --runs must be positive");
  }
  const auto started = std::chrono::steady_clock::now();
  std::size_t ok_runs = 0;
  std::size_t tick_calls = 0;
  std::vector<std::string> errors;
  std::vector<double> run_elapsed_ms;
  run_elapsed_ms.reserve(runs);
  for (std::size_t run_index = 0; run_index < runs; ++run_index) {
    const auto run_started = std::chrono::steady_clock::now();
    const auto result = run_graph_file(path, steps, 0, false, limits);
    const auto run_elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - run_started).count();
    run_elapsed_ms.push_back(run_elapsed);
    if (result.ok) {
      ++ok_runs;
      tick_calls += result.tick_calls;
    } else {
      errors.insert(errors.end(), result.errors.begin(), result.errors.end());
    }
  }
  const auto elapsed = std::chrono::steady_clock::now() - started;
  const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
  const auto elapsed_seconds = std::chrono::duration<double>(elapsed).count();
  const auto throughput_tick_calls = elapsed_seconds > 0.0 ? static_cast<double>(tick_calls) / elapsed_seconds : 0.0;
  const auto throughput_runs = elapsed_seconds > 0.0 ? static_cast<double>(ok_runs) / elapsed_seconds : 0.0;
  const auto case_name = benchmark_case_name(path);
  const auto hash = graph_hash(path);
  const nlohmann::json environment = {{"benchmark_schema", 2},
                                      {"clock", "steady_clock"},
                                      {"runtime", "RuntimeRunner"},
                                      {"compiler", compiler_name()},
                                      {"compiler_version", compiler_version()},
                                      {"cpp_standard", cpp_standard()},
                                      {"build_type", build_type()},
                                      {"cpu_model", cpu_model()},
                                      {"cpu_threads", hardware_threads()},
                                      {"commit", git_commit()}};
  if (format == "json") {
    nlohmann::json value;
    value["ok"] = errors.empty();
    value["case"] = case_name;
    value["runs"] = runs;
    value["ok_runs"] = ok_runs;
    value["steps"] = steps;
    value["params"] = {{"file", path}, {"steps", steps}, {"runs", runs}};
    value["graph_hash"] = hash;
    value["tick_calls"] = tick_calls;
    value["elapsed_ms"] = elapsed_ms;
    value["run_elapsed_ms"] = run_elapsed_ms;
    value["p50_run_elapsed_ms"] = percentile(run_elapsed_ms, 0.50);
    value["p95_run_elapsed_ms"] = percentile(run_elapsed_ms, 0.95);
    value["p99_run_elapsed_ms"] = percentile(run_elapsed_ms, 0.99);
    value["throughput_tick_calls_per_sec"] = throughput_tick_calls;
    value["throughput_runs_per_sec"] = throughput_runs;
    value["environment"] = environment;
    value["errors"] = errors;
    std::cout << value.dump(2) << "\n";
  } else {
    std::cout << (errors.empty() ? "ok" : "error") << "\n";
    std::cout << "case: " << case_name << "\n";
    std::cout << "benchmark_schema: " << benchmark_schema_version() << "\n";
    std::cout << "graph_hash: " << hash << "\n";
    std::cout << "runs: " << runs << "\n";
    std::cout << "ok_runs: " << ok_runs << "\n";
    std::cout << "steps: " << steps << "\n";
    std::cout << "tick_calls: " << tick_calls << "\n";
    std::cout << "elapsed_ms: " << elapsed_ms << "\n";
    std::cout << "throughput_tick_calls_per_sec: " << throughput_tick_calls << "\n";
    std::cout << "p50_run_elapsed_ms: " << percentile(run_elapsed_ms, 0.50) << "\n";
    std::cout << "p95_run_elapsed_ms: " << percentile(run_elapsed_ms, 0.95) << "\n";
    std::cout << "p99_run_elapsed_ms: " << percentile(run_elapsed_ms, 0.99) << "\n";
    std::cout << "compiler: " << environment["compiler"].get<std::string>() << " "
              << environment["compiler_version"].get<std::string>() << "\n";
    std::cout << "build_type: " << environment["build_type"].get<std::string>() << "\n";
    std::cout << "commit: " << environment["commit"].get<std::string>() << "\n";
    for (const auto& error : errors) {
      std::cout << "- " << error << "\n";
    }
  }
  return errors.empty() ? 0 : 1;
}

int print_doctor(const std::string& format) {
  const auto schema_path = find_schema_path();
  const auto examples = existing_yaml_files("examples");
  const auto benchmarks = existing_yaml_files("benchmarks");
  const bool ok = !schema_path.empty();
  if (format == "json") {
    nlohmann::json value;
    value["ok"] = ok;
    value["version"] = "0.1.0";
    value["semantic_contract_version"] = topoexec::kTopoExecSemanticContractVersion;
    value["schema_version"] = topoexec::kTopoExecSchemaVersion;
    value["cxx_standard"] = static_cast<long>(__cplusplus);
    value["schema_found"] = !schema_path.empty();
    value["schema_path"] = schema_path;
    value["examples"] = examples;
    value["benchmarks"] = benchmarks;
    value["features"] = {
        {"runtime", true}, {"yaml", true}, {"json", true}, {"health_events", true}, {"sanitizers", "external-ci"}};
    value["health_events"] = {{"default_emit", true},
                              {"default_capacity", topoexec::kDefaultHealthEventCapacity},
                              {"bounded_sink", true},
                              {"control_flow", "observer_only"}};
    value["graph_input_limits"] = graph_input_limits_json(topoexec::default_graph_input_limits());
    std::cout << value.dump(2) << "\n";
  } else {
    std::cout << (ok ? "ok" : "error") << "\n";
    std::cout << "version: 0.1.0\n";
    std::cout << "semantic_contract_version: " << topoexec::kTopoExecSemanticContractVersion << "\n";
    std::cout << "schema_version: " << topoexec::kTopoExecSchemaVersion << "\n";
    std::cout << "cxx_standard: " << __cplusplus << "\n";
    std::cout << "health_events: observer_only bounded default_capacity=" << topoexec::kDefaultHealthEventCapacity
              << "\n";
    std::cout << "graph_input_limits: max_graph_input_bytes="
              << topoexec::default_graph_input_limits().max_graph_input_bytes
              << " max_components=" << topoexec::default_graph_input_limits().max_components
              << " max_edges=" << topoexec::default_graph_input_limits().max_edges << "\n";
    std::cout << "schema_found: " << (!schema_path.empty() ? "true" : "false") << "\n";
    if (!schema_path.empty()) {
      std::cout << "schema_path: " << schema_path << "\n";
    }
    std::cout << "examples: " << examples.size() << "\n";
    std::cout << "benchmarks: " << benchmarks.size() << "\n";
    std::cout << "sanitizers: external-ci\n";
  }
  return ok ? 0 : 1;
}

int print_schema_dump(const std::string& format) {
  const auto schema_path = find_schema_path();
  if (schema_path.empty()) {
    throw std::runtime_error("schema/topoexec.schema.v1.json not found; set TOPOEXEC_SCHEMA_PATH");
  }
  const auto text = read_text_file(schema_path);
  if (format == "json") {
    std::cout << nlohmann::json::parse(text).dump(2) << "\n";
  } else {
    std::cout << text;
    if (!text.empty() && text.back() != '\n') {
      std::cout << "\n";
    }
  }
  return 0;
}

int print_schema_check(const std::string& path, const std::string& format, const topoexec::GraphInputLimits& limits) {
  topoexec::GraphValidationResult result;
  try {
    (void)topoexec::load_graph_file(path, limits);
    result.ok = true;
  } catch (const std::exception& error) {
    result.ok = false;
    result.errors.push_back(error.what());
  }
  return print_validation(result, format);
}

} // namespace

int main(int argc, char** argv) {
  g_executable_dir = executable_directory(argc > 0 ? argv[0] : nullptr);
  CLI::App app{"TopoExec graph tooling"};
  app.require_subcommand(1);
  auto input_limits = topoexec::default_graph_input_limits();

  auto* graph_cmd = app.add_subcommand("graph", "Graph inspection commands");
  graph_cmd->require_subcommand(1);

  std::string doctor_format{"text"};
  auto* doctor = app.add_subcommand("doctor", "Check local TopoExec CLI/runtime assets");
  doctor->add_option("--format", doctor_format, "Output format")->check(CLI::IsMember({"text", "json"}));

  auto* schema_cmd = app.add_subcommand("schema", "Schema tooling");
  schema_cmd->require_subcommand(1);
  std::string schema_dump_format{"json"};
  auto* schema_dump = schema_cmd->add_subcommand("dump", "Print the bundled schema v1 JSON");
  schema_dump->add_option("--format", schema_dump_format, "Output format")->check(CLI::IsMember({"text", "json"}));
  std::string schema_check_path;
  std::string schema_check_format{"text"};
  auto* schema_check = schema_cmd->add_subcommand("check", "Check graph file against the strict schema loader");
  schema_check->add_option("file", schema_check_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  schema_check->add_option("--format", schema_check_format, "Output format")->check(CLI::IsMember({"text", "json"}));
  add_input_limit_options(schema_check, input_limits);

  std::string validate_path;
  std::string validate_format{"text"};
  bool validate_schema_only{false};
  bool validate_semantic{false};
  bool validate_strict_diagnostics{false};
  auto* validate = graph_cmd->add_subcommand("validate", "Validate a TopoExec graph");
  validate->add_option("file", validate_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  validate->add_option("--format", validate_format, "Output format")->check(CLI::IsMember({"text", "json"}));
  validate->add_flag("--schema-only", validate_schema_only,
                     "Only parse the strict schema/field contract; skip semantic graph validation");
  validate->add_flag("--semantic", validate_semantic, "Run full semantic validation; this is the default");
  validate->add_flag("--strict-diagnostics", validate_strict_diagnostics,
                     "Fail validation when warning diagnostics are emitted");
  add_input_limit_options(validate, input_limits);

  std::string plan_path;
  std::string plan_format{"text"};
  auto* plan = graph_cmd->add_subcommand("plan", "Print compiled graph plan");
  plan->add_option("file", plan_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  plan->add_option("--format", plan_format, "Output format")->check(CLI::IsMember({"text", "json"}));
  add_input_limit_options(plan, input_limits);

  std::string render_path;
  std::string render_format{"mermaid"};
  auto* render = graph_cmd->add_subcommand("render", "Render a graph");
  render->add_option("file", render_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  render->add_option("--format", render_format, "Output format")->check(CLI::IsMember({"mermaid", "text", "json"}));
  add_input_limit_options(render, input_limits);

  std::string run_path;
  std::string run_format{"text"};
  std::size_t run_steps{1};
  std::uint64_t run_duration_ms{0};
  bool run_until_idle{false};
  auto* run = graph_cmd->add_subcommand("run", "Run a graph with built-in demo components");
  run->add_option("file", run_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  run->add_option("--steps", run_steps, "Bounded event-loop steps");
  run->add_option("--duration-ms", run_duration_ms, "Optional duration bound in milliseconds");
  run->add_flag("--until-idle", run_until_idle, "Stop early after an event-loop iteration executes no components");
  run->add_option("--format", run_format, "Output format")->check(CLI::IsMember({"text", "json"}));
  add_input_limit_options(run, input_limits);

  std::string metrics_path;
  std::string metrics_format{"text"};
  std::size_t metrics_steps{1};
  std::uint64_t metrics_duration_ms{0};
  bool metrics_until_idle{false};
  auto* metrics = graph_cmd->add_subcommand("metrics", "Run a graph and print runtime metrics");
  metrics->add_option("file", metrics_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  metrics->add_option("--steps", metrics_steps, "Bounded event-loop steps");
  metrics->add_option("--duration-ms", metrics_duration_ms, "Optional duration bound in milliseconds");
  metrics->add_flag("--until-idle", metrics_until_idle,
                    "Stop early after an event-loop iteration executes no components");
  metrics->add_option("--format", metrics_format, "Output format")->check(CLI::IsMember({"text", "json"}));
  add_input_limit_options(metrics, input_limits);

  std::string trace_path;
  std::string trace_format{"text"};
  std::size_t trace_steps{1};
  std::uint64_t trace_duration_ms{0};
  bool trace_until_idle{false};
  auto* trace = graph_cmd->add_subcommand("trace", "Run a graph and print runtime trace events");
  trace->add_option("file", trace_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  trace->add_option("--steps", trace_steps, "Bounded event-loop steps");
  trace->add_option("--duration-ms", trace_duration_ms, "Optional duration bound in milliseconds");
  trace->add_flag("--until-idle", trace_until_idle, "Stop early after an event-loop iteration executes no components");
  trace->add_option("--format", trace_format, "Output format")->check(CLI::IsMember({"text", "json", "chrome"}));
  add_input_limit_options(trace, input_limits);

  std::string observe_path;
  std::string observe_format{"ndjson"};
  std::string observe_level{"summary"};
  std::size_t observe_steps{1};
  std::uint64_t observe_duration_ms{0};
  bool observe_until_idle{false};
  std::size_t observe_event_buffer_capacity{1024};
  std::uint64_t observe_ui_frame_ms{50};
  std::vector<std::string> observe_include_components;
  std::vector<std::string> observe_include_channels;
  std::vector<std::string> observe_include_events;
  std::vector<std::string> observe_exclude_events;
  std::vector<std::string> observe_sample_events;
  std::size_t observe_payload_preview_bytes{0};
  std::string observe_assert_file;
  std::string observe_record_dir;
  bool observe_fail_on_assertion_fail{true};
  bool observe_fail_on_observer_drop{false};
  auto* observe = graph_cmd->add_subcommand("observe", "Run a graph and emit live observe NDJSON");
  observe->add_option("file", observe_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  observe->add_option("--steps", observe_steps, "Bounded event-loop steps");
  observe->add_option("--duration-ms", observe_duration_ms, "Optional duration bound in milliseconds");
  observe->add_flag("--until-idle", observe_until_idle,
                    "Stop early after an event-loop iteration executes no components");
  observe->add_option("--observe-level", observe_level, "Observe level")
      ->check(CLI::IsMember({"off", "summary", "detailed", "debug"}));
  observe->add_option("--event-buffer-capacity", observe_event_buffer_capacity,
                      "Bounded per-stream live event buffer capacity");
  observe->add_option("--ui-frame-ms", observe_ui_frame_ms, "Collector/UI frame window in milliseconds");
  observe->add_option("--include-component", observe_include_components, "Component id to observe in detail");
  observe->add_option("--include-channel", observe_include_channels, "Channel id to observe in detail");
  observe->add_option("--include-event", observe_include_events, "Event kind to include");
  observe->add_option("--exclude-event", observe_exclude_events, "Event kind to exclude");
  observe->add_option("--sample-event", observe_sample_events, "Event sampling rule KIND:RATIO");
  observe->add_option("--payload-preview-bytes", observe_payload_preview_bytes,
                      "Debug-only bounded payload preview byte count");
  observe->add_option("--assert", observe_assert_file, "Live assertion YAML file");
  observe->add_option("--record", observe_record_dir, "Record artifact directory");
  observe->add_flag("--fail-on-assertion-fail,!--no-fail-on-assertion-fail", observe_fail_on_assertion_fail,
                    "Return exit code 3 when live assertions fail");
  observe->add_flag("--fail-on-observer-drop", observe_fail_on_observer_drop,
                    "Return non-zero when the live observe stream drops events");
  observe->add_option("--format", observe_format, "Output format")->check(CLI::IsMember({"ndjson", "json-summary"}));
  add_input_limit_options(observe, input_limits);

  std::string lint_path;
  std::string lint_format{"text"};
  auto* lint = graph_cmd->add_subcommand("lint", "Lint a graph for suspicious runtime contracts");
  lint->add_option("file", lint_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  lint->add_option("--format", lint_format, "Output format")->check(CLI::IsMember({"text", "json"}));
  add_input_limit_options(lint, input_limits);

  std::string explain_path;
  std::string explain_format{"text"};
  auto* explain = graph_cmd->add_subcommand("explain", "Explain graph runtime semantics and compiled plan");
  explain->add_option("file", explain_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  explain->add_option("--format", explain_format, "Output format")->check(CLI::IsMember({"text", "json"}));
  add_input_limit_options(explain, input_limits);

  std::string diff_left_path;
  std::string diff_right_path;
  std::string diff_format{"text"};
  auto* diff_plan = graph_cmd->add_subcommand("diff-plan", "Compare two compiled graph plans");
  diff_plan->add_option("left", diff_left_path, "Left graph YAML file")->required()->check(CLI::ExistingFile);
  diff_plan->add_option("right", diff_right_path, "Right graph YAML file")->required()->check(CLI::ExistingFile);
  diff_plan->add_option("--format", diff_format, "Output format")->check(CLI::IsMember({"text", "json"}));
  add_input_limit_options(diff_plan, input_limits);

  std::string bench_path;
  std::string bench_format{"text"};
  std::size_t bench_steps{1};
  std::size_t bench_runs{3};
  auto* bench = graph_cmd->add_subcommand("bench", "Run a small local RuntimeRunner benchmark");
  bench->add_option("file", bench_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  bench->add_option("--steps", bench_steps, "Bounded event-loop steps per run")->check(CLI::PositiveNumber);
  bench->add_option("--runs", bench_runs, "Number of repeated runs")->check(CLI::PositiveNumber);
  bench->add_option("--format", bench_format, "Output format")->check(CLI::IsMember({"text", "json"}));
  add_input_limit_options(bench, input_limits);

  try {
    app.parse(argc, argv);
    if (*doctor) {
      return print_doctor(doctor_format);
    }
    if (*schema_dump) {
      return print_schema_dump(schema_dump_format);
    }
    if (*schema_check) {
      return print_schema_check(schema_check_path, schema_check_format, input_limits);
    }
    if (*validate) {
      if (validate_schema_only && validate_semantic) {
        throw std::runtime_error("--schema-only and --semantic are mutually exclusive");
      }
      topoexec::GraphSpec graph;
      topoexec::GraphValidationResult result;
      if (validate_schema_only) {
        result = load_graph_file_result(validate_path, graph, input_limits);
      } else {
        result = load_and_validate(validate_path, graph, input_limits);
      }
      if (validate_strict_diagnostics) {
        apply_strict_diagnostics(result);
      }
      return print_validation(result, validate_format);
    }
    if (*plan) {
      topoexec::GraphSpec graph;
      const auto result = load_and_validate(plan_path, graph, input_limits);
      if (!result.ok) {
        return print_validation(result, plan_format == "json" ? "json" : "text");
      }
      if (plan_format == "json") {
        std::cout << topoexec::graph_plan_json(graph, result.compiled_plan) << "\n";
      } else {
        std::cout << topoexec::graph_plan_text(graph, result.compiled_plan);
      }
      return 0;
    }
    if (*render) {
      topoexec::GraphSpec graph;
      const auto result = load_and_validate(render_path, graph, input_limits);
      if (!result.ok) {
        return print_validation(result, render_format == "json" ? "json" : "text");
      }
      if (render_format == "json") {
        std::cout << topoexec::graph_plan_json(graph, result.compiled_plan) << "\n";
      } else if (render_format == "text") {
        std::cout << topoexec::graph_plan_text(graph, result.compiled_plan);
      } else {
        std::cout << topoexec::graph_mermaid(graph, result.compiled_plan);
      }
      return 0;
    }
    if (*run) {
      return print_runner_result(run_graph_file(run_path, run_steps, run_duration_ms, run_until_idle, input_limits),
                                 run_format);
    }
    if (*metrics) {
      return print_metrics_result(
          run_graph_file(metrics_path, metrics_steps, metrics_duration_ms, metrics_until_idle, input_limits),
          metrics_format);
    }
    if (*trace) {
      return print_trace_result(
          run_graph_file(trace_path, trace_steps, trace_duration_ms, trace_until_idle, input_limits), trace_format);
    }
    if (*observe) {
      if (observe_payload_preview_bytes > 0u && observe_level != "debug") {
        throw std::runtime_error("--payload-preview-bytes requires --observe-level debug");
      }
      topoexec::GraphSpec graph;
      auto validation = load_graph_file_result(observe_path, graph, input_limits);
      if (validation.ok) {
        validation = topoexec::validate_graph(graph, demo_registry());
      }
      if (!validation.ok) {
        topoexec::RuntimeRunnerResult invalid_result;
        invalid_result.ok = false;
        invalid_result.graph_name = graph.name;
        invalid_result.component_count = graph.components.size();
        invalid_result.channel_count = graph.edges.size();
        invalid_result.errors = validation.errors;
        (void)print_observe_result(observe_path, graph, validation, invalid_result, observe_level, observe_format,
                                   observe_ui_frame_ms, {}, {}, false);
        return 2;
      }
      topoexec::runtime_observe::LiveObserveOptions live_options;
      live_options.level = parse_live_observe_level(observe_level);
      live_options.event_buffer_capacity = observe_event_buffer_capacity;
      live_options.stream_id = 1;
      const auto result = run_graph_file(observe_path, observe_steps, observe_duration_ms, observe_until_idle,
                                         input_limits, live_options);
      const ObserveOutputFilter observe_output_filter{observe_include_components, observe_include_channels,
                                                      observe_include_events, observe_exclude_events,
                                                      observe_sample_events};
      const auto exit_code = print_observe_result(
          observe_path, graph, validation, result, observe_level, observe_format, observe_ui_frame_ms,
          observe_assert_file, observe_record_dir, observe_fail_on_assertion_fail, observe_output_filter);
      if (observe_fail_on_observer_drop && result.live_observe_dropped_event_count != 0u) {
        return 4;
      }
      return exit_code;
    }
    if (*lint) {
      const auto registry = demo_registry();
      return print_lint_findings(lint_graph(topoexec::load_graph_file(lint_path, input_limits), &registry),
                                 lint_format);
    }
    if (*explain) {
      topoexec::GraphSpec graph;
      const auto validation = load_and_validate(explain_path, graph, input_limits);
      return print_explain(graph, validation, explain_format);
    }
    if (*diff_plan) {
      return print_diff_plan(diff_left_path, diff_right_path, diff_format, input_limits);
    }
    if (*bench) {
      return print_bench_result(bench_path, bench_steps, bench_runs, bench_format, input_limits);
    }
  } catch (const CLI::ParseError& error) {
    return app.exit(error);
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << "\n";
    return 1;
  }
  return 0;
}
