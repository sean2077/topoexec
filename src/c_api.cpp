#include "topoexec/c_api/topoexec.h"

#include "topoexec/runtime/component.hpp"
#include "topoexec/runtime/component_registry.hpp"
#include "topoexec/runtime/graph_builder.hpp"
#include "topoexec/runtime/runtime_runner.hpp"

#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kNoopComponentType = "topoexec.c_api.Noop";

class CApiNoopComponent final : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = kNoopComponentType;
    descriptor.role = topoexec::ComponentRole::kProcessing;
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}
  void execute(const topoexec::Invocation&, topoexec::GraphContext&) override {}
};

topoexec::ComponentRegistry make_c_api_registry() {
  topoexec::ComponentRegistry registry;
  registry.register_component({kNoopComponentType}, []() { return std::make_unique<CApiNoopComponent>(); });
  return registry;
}

const char* stable_c_str(const std::string& value) {
  return value.empty() ? "" : value.c_str();
}

void set_error(std::string& target, const char* message) {
  target = message == nullptr ? "unknown C API error" : message;
}

void set_error(std::string& target, const std::exception& error) {
  target = error.what();
}

} // namespace

struct topoexec_runtime {
  topoexec::ComponentRegistry registry{make_c_api_registry()};
  std::string last_error;
};

struct topoexec_graph_builder {
  topoexec::GraphSpec graph;
  std::string last_error;
};

struct topoexec_result {
  topoexec::RuntimeRunnerResult result;
  std::string last_error;
};

extern "C" {

topoexec_runtime_t* topoexec_runtime_create(void) {
  try {
    return new topoexec_runtime{};
  } catch (...) {
    return nullptr;
  }
}

void topoexec_runtime_destroy(topoexec_runtime_t* runtime) {
  delete runtime;
}

const char* topoexec_runtime_last_error(const topoexec_runtime_t* runtime) {
  return runtime == nullptr ? "topoexec_runtime is null" : stable_c_str(runtime->last_error);
}

topoexec_graph_builder_t* topoexec_graph_builder_create(const char* graph_name) {
  if (graph_name == nullptr || graph_name[0] == '\0') {
    return nullptr;
  }
  try {
    auto* builder = new topoexec_graph_builder{};
    builder->graph = topoexec::GraphBuilder(graph_name, "internal_test").build();
    return builder;
  } catch (...) {
    return nullptr;
  }
}

void topoexec_graph_builder_destroy(topoexec_graph_builder_t* builder) {
  delete builder;
}

const char* topoexec_graph_builder_last_error(const topoexec_graph_builder_t* builder) {
  return builder == nullptr ? "topoexec_graph_builder is null" : stable_c_str(builder->last_error);
}

topoexec_status_code_t topoexec_graph_builder_add_event_loop_lane(topoexec_graph_builder_t* builder,
                                                                  const char* lane_id) {
  if (builder == nullptr) {
    return TOPOEXEC_STATUS_ERROR;
  }
  if (lane_id == nullptr || lane_id[0] == '\0') {
    set_error(builder->last_error, "lane_id is required");
    return TOPOEXEC_STATUS_ERROR;
  }
  try {
    topoexec::LaneSpec lane;
    lane.id = lane_id;
    lane.type = "event_loop";
    builder->graph.lanes.push_back(std::move(lane));
    builder->last_error.clear();
    return TOPOEXEC_STATUS_OK;
  } catch (const std::exception& error) {
    set_error(builder->last_error, error);
    return TOPOEXEC_STATUS_ERROR;
  }
}

topoexec_status_code_t topoexec_graph_builder_add_noop_component(topoexec_graph_builder_t* builder,
                                                                 const char* component_id, const char* lane_id) {
  if (builder == nullptr) {
    return TOPOEXEC_STATUS_ERROR;
  }
  if (component_id == nullptr || component_id[0] == '\0') {
    set_error(builder->last_error, "component_id is required");
    return TOPOEXEC_STATUS_ERROR;
  }
  if (lane_id == nullptr || lane_id[0] == '\0') {
    set_error(builder->last_error, "lane_id is required");
    return TOPOEXEC_STATUS_ERROR;
  }
  try {
    auto component = topoexec::component_node(component_id, kNoopComponentType, {topoexec::manual_event_source()},
                                              topoexec::manual_trigger(), topoexec::lane_execution(lane_id));
    builder->graph.components.push_back(std::move(component));
    builder->last_error.clear();
    return TOPOEXEC_STATUS_OK;
  } catch (const std::exception& error) {
    set_error(builder->last_error, error);
    return TOPOEXEC_STATUS_ERROR;
  }
}

topoexec_result_t* topoexec_runtime_run(topoexec_runtime_t* runtime, const topoexec_graph_builder_t* builder,
                                        size_t tick_iterations) {
  if (runtime == nullptr || builder == nullptr) {
    return nullptr;
  }
  try {
    topoexec::RuntimeRunner runner(runtime->registry);
    topoexec::RuntimeRunnerOptions options;
    options.mode = topoexec::RuntimeRunMode::kRun;
    options.tick_iterations = tick_iterations;
    auto* output = new topoexec_result{};
    output->result = runner.run(builder->graph, options);
    runtime->last_error = output->result.ok || output->result.runtime_errors.empty()
                              ? std::string{}
                              : output->result.runtime_errors[0].message;
    return output;
  } catch (const std::exception& error) {
    set_error(runtime->last_error, error);
    return nullptr;
  }
}

void topoexec_result_destroy(topoexec_result_t* result) {
  delete result;
}

int topoexec_result_ok(const topoexec_result_t* result) {
  return result != nullptr && result->result.ok ? 1 : 0;
}

size_t topoexec_result_error_count(const topoexec_result_t* result) {
  return result == nullptr ? 0u : result->result.runtime_errors.size();
}

const char* topoexec_result_error_at(const topoexec_result_t* result, size_t index) {
  if (result == nullptr || index >= result->result.runtime_errors.size()) {
    return "";
  }
  return stable_c_str(result->result.runtime_errors[index].message);
}

size_t topoexec_result_metric_count(const topoexec_result_t* result) {
  return result == nullptr ? 0u : result->result.runtime_metrics.size();
}

topoexec_status_code_t topoexec_result_metric_at(const topoexec_result_t* result, size_t index,
                                                 topoexec_metric_sample_t* out_metric) {
  if (result == nullptr || out_metric == nullptr || index >= result->result.runtime_metrics.size()) {
    return TOPOEXEC_STATUS_ERROR;
  }
  const auto& metric = result->result.runtime_metrics[index];
  out_metric->name = stable_c_str(metric.name);
  out_metric->value = metric.value;
  out_metric->component_id = stable_c_str(metric.component_id);
  out_metric->lane = stable_c_str(metric.lane);
  out_metric->channel_id = stable_c_str(metric.channel_id);
  return TOPOEXEC_STATUS_OK;
}

} // extern "C"
