#pragma once

// API stability: stable-v0.2. Component, Invocation, and GraphContext are intended embedder API.

#include "topoexec/common/logging.hpp"
#include "topoexec/common/metrics.hpp"
#include "topoexec/runtime/cancellation.hpp"
#include "topoexec/runtime/clock.hpp"
#include "topoexec/runtime/payload.hpp"
#include "topoexec/runtime/status.hpp"
#include "topoexec/runtime/task_executor.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace topoexec {

class RuntimeChannelBus;
class RuntimeStateStore;
class ConfigSnapshotStore;

struct RuntimeChannelPublishResult {
  bool accepted{false};
  std::string reason;
};

struct InvocationMetadata {
  std::string correlation_id;
  std::string causation_id;
  std::uint64_t epoch_id{0};
  std::string transaction_id;
  std::string source_component;
  std::string source_port;
  std::string trigger_kind;
};

struct LoopIterationContext {
  std::string loop_id;
  std::string policy_type;
  std::size_t iteration_index{0};
  std::size_t iteration_number{0};
  std::size_t max_iterations{0};
};

struct LoopConvergenceReport {
  bool converged{false};
  std::optional<double> residual;
  std::string reason;
};

struct ComponentStateSnapshot {
  std::string component_type;
  std::string version;
  RuntimePayloadPtr payload;
  std::size_t size_bytes{0};
};

class GraphOutputPublisher {
public:
  virtual ~GraphOutputPublisher() = default;
  virtual RuntimeChannelPublishResult publish_from(const std::string& source_endpoint, RuntimePayload payload,
                                                   std::optional<EventTimestamp> event_timestamp = std::nullopt) = 0;
  virtual RuntimeChannelPublishResult
  publish_shared_from(const std::string& source_endpoint, RuntimePayloadPtr payload,
                      std::optional<EventTimestamp> event_timestamp = std::nullopt) = 0;
  virtual RuntimeChannelPublishResult
  publish_from_with_metadata(const std::string& source_endpoint, RuntimePayload payload, InvocationMetadata metadata,
                             std::optional<EventTimestamp> event_timestamp = std::nullopt) {
    (void)metadata;
    return publish_from(source_endpoint, std::move(payload), std::move(event_timestamp));
  }
  virtual RuntimeChannelPublishResult
  publish_shared_from_with_metadata(const std::string& source_endpoint, RuntimePayloadPtr payload,
                                    InvocationMetadata metadata,
                                    std::optional<EventTimestamp> event_timestamp = std::nullopt) {
    (void)metadata;
    return publish_shared_from(source_endpoint, std::move(payload), std::move(event_timestamp));
  }
};

struct ConfigView {
  std::map<std::string, std::string> values;
  std::set<std::string> nested_values;

  bool is_nested(const std::string& name) const {
    return nested_values.count(name) != 0u;
  }
};

enum class PortMultiplicity {
  kSingle,
  kMultiple,
};

struct PortDescriptor {
  PortDescriptor() = default;
  PortDescriptor(std::string name_value, std::string schema_value, std::string payload_type_value = {},
                 PortMultiplicity multiplicity_value = PortMultiplicity::kSingle, bool required_value = false)
      : name(std::move(name_value)), schema(std::move(schema_value)), payload_type(std::move(payload_type_value)),
        multiplicity(multiplicity_value), required(required_value) {}

  std::string name;
  std::string schema;
  std::string payload_type;
  PortMultiplicity multiplicity{PortMultiplicity::kSingle};
  bool required{false};
};

struct ServiceDescriptor {
  std::string name;
  std::string request_schema;
  std::string reply_schema;
};

struct ActionDescriptor {
  std::string name;
  std::string goal_schema;
  std::string feedback_schema;
  std::string result_schema;
};

struct SchedulerRequirement {
  std::string lane;
  double tick_hz{0.0};
};

enum class ConfigValueKind {
  kString,
  kInt,
  kDouble,
  kBool,
};

struct ConfigFieldSpec {
  std::string name;
  ConfigValueKind kind{ConfigValueKind::kString};
  bool required{false};
  std::optional<double> min_value;
  std::optional<double> max_value;
  std::vector<std::string> enum_values;
  bool allow_nested{false};
};

enum class ComponentRole {
  kProcessing,
  kInputBoundary,
  kOutputBoundary,
  kInputOutputBoundary,
};

std::string to_string(ComponentRole role);
std::optional<ComponentRole> parse_component_role(const std::string& value);
bool component_role_has_input(ComponentRole role);
bool component_role_has_output(ComponentRole role);

struct BoundaryDescriptor {
  ComponentRole role{ComponentRole::kProcessing};
  std::string descriptor;
};

struct ComponentDescriptor {
  std::string type;
  std::string name;
  ComponentRole role{ComponentRole::kProcessing};
  std::string boundary_descriptor;
  std::vector<ConfigFieldSpec> config_fields;
  std::vector<PortDescriptor> inputs;
  std::vector<PortDescriptor> outputs;
  std::vector<ServiceDescriptor> services;
  std::vector<ActionDescriptor> actions;
  SchedulerRequirement scheduler;
  std::vector<std::string> capabilities;
  bool realtime_allowed{false};
};

struct InputView {
  RuntimeChannelBus* channels{nullptr};
  std::string component_id;

  RuntimePayloadPtr read_latest_update(const std::string& port) const;
  RuntimePayloadPtr peek_latest(const std::string& port) const;
  std::vector<RuntimePayloadPtr> drain(const std::string& port, std::size_t max_batch = 0) const;
};

struct GraphContext {
  MetricRegistry* metrics{nullptr};
  StructuredLogger* logger{nullptr};
  RuntimeChannelBus* channels{nullptr};
  GraphOutputPublisher* publisher{nullptr};
  ITaskExecutor* task_executor{nullptr};
  RuntimeStateStore* state_store{nullptr};
  ConfigSnapshotStore* config_store{nullptr};
  CancellationToken cancel_token;
  InvocationMetadata invocation_metadata;
  LoopIterationContext loop_iteration;
  std::function<void(LoopConvergenceReport)> loop_convergence_reporter;
  std::string graph_name;
  std::string component_id;

  InputView inputs() const {
    return InputView{channels, component_id};
  }

  RuntimeChannelPublishResult publish(const std::string& port, RuntimePayload payload,
                                      std::optional<EventTimestamp> event_timestamp = std::nullopt) const;
  RuntimeChannelPublishResult publish_shared(const std::string& port, RuntimePayloadPtr payload,
                                             std::optional<EventTimestamp> event_timestamp = std::nullopt) const;
  TaskSubmissionResult submit_task(const std::string& completion_port, ITaskExecutor::Work work) const;
  void report_loop_convergence(LoopConvergenceReport report) const;
  bool cancel_requested() const;
};

using ComponentContext = GraphContext;

enum class EventKind {
  kMessage,
  kTimer,
  kRequest,
  kActionGoal,
  kActionCancel,
  kFutureReady,
  kTaskReady,
  kLifecycle,
  kHealth,
  kManual,
};

enum class TriggerKind {
  kOnMessage,
  kOnTick,
  kAllInputs,
  kAnyInput,
  kTimeSync,
  kBatch,
  kRequest,
  kTaskReady,
  kManual,
  kWatermark,
  kCondition,
  kDebounce,
  kRateLimit,
};

struct Invocation {
  EventKind event{EventKind::kManual};
  TriggerKind trigger{TriggerKind::kManual};
  std::string port;
  std::string channel_id;
  std::string correlation_id;
  InvocationMetadata metadata;
  RuntimePayloadPtr payload;
  std::vector<std::string> ready_inputs;
  std::map<std::string, RuntimePayloadPtr> payloads_by_port;
  std::vector<RuntimePayloadPtr> batch_payloads;
  std::chrono::steady_clock::time_point scheduled_at;
  std::chrono::steady_clock::time_point started_at;
  std::chrono::steady_clock::time_point received_at;
  std::chrono::steady_clock::time_point published_at;
  std::uint64_t sequence{0};
  bool deadline_missed{false};
  std::optional<EventTimestamp> event_timestamp;
  std::chrono::milliseconds budget{0};
  std::string lane;
  std::string priority;
  CancellationToken cancel_token;
  std::function<bool()> stop_requested;

  template <typename T> const T* try_payload_as() const {
    return payload == nullptr ? nullptr : topoexec::try_payload_as<T>(*payload);
  }

  template <typename T> const T& payload_as(const std::string& context = {}) const {
    if (payload == nullptr) {
      const auto prefix = context.empty() ? std::string{} : context + ": ";
      throw std::runtime_error(prefix + "invocation payload is null");
    }
    return topoexec::payload_as<T>(*payload, context.empty() ? "invocation payload" : context);
  }

  bool cancel_requested() const {
    return cancel_token.cancel_requested();
  }
};

class Component {
public:
  virtual ~Component() = default;

  virtual ComponentDescriptor describe() const = 0;
  virtual void configure(GraphContext& ctx, const ConfigView& config) = 0;
  virtual void activate() {}
  virtual void deactivate() {}
  virtual void execute(const Invocation& invocation, GraphContext& ctx);

  virtual Status configure_status(GraphContext& ctx, const ConfigView& config);
  virtual Status activate_status();
  virtual Status deactivate_status();
  virtual Status execute_status(const Invocation& invocation, GraphContext& ctx);
  virtual void reset(GraphContext& ctx);
  virtual void pause();
  virtual void resume();
  virtual Status reset_status(GraphContext& ctx);
  virtual Status pause_status();
  virtual Status resume_status();
  virtual Result<ComponentStateSnapshot> snapshot_state() const;
  virtual Status restore_state(const ComponentStateSnapshot& snapshot);
  virtual Status validate_config(const ConfigView& config) const;
  virtual Status apply_config(GraphContext& ctx, const ConfigView& config);
};

struct TickContext {
  std::uint64_t sequence{0};
  std::chrono::steady_clock::time_point scheduled_at;
  std::chrono::steady_clock::time_point started_at;
  CancellationToken cancel_token;
  std::function<bool()> stop_requested;
};

struct MessageContext {
  std::string channel_id;
  std::chrono::steady_clock::time_point received_at;
  std::chrono::steady_clock::time_point published_at;
  std::uint64_t sequence{0};
  bool deadline_missed{false};
  std::optional<EventTimestamp> event_timestamp;
};

struct HealthStatus {
  std::string state;
  std::string reason;
};

class HealthProvider {
public:
  virtual ~HealthProvider() = default;
  virtual HealthStatus health() const = 0;
};

class MetricsProvider {
public:
  virtual ~MetricsProvider() = default;
  virtual void collect_metrics(MetricRegistry& registry) const = 0;
};

} // namespace topoexec
