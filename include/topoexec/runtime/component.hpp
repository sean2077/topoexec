#pragma once

// Public API category: stable 0.x component, invocation, and graph context
// surface.

#include "topoexec/common/logging.hpp"
#include "topoexec/common/metrics.hpp"
#include "topoexec/runtime/clock.hpp"
#include "topoexec/runtime/payload.hpp"
#include "topoexec/runtime/status.hpp"

#include <chrono>
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

struct RuntimeChannelPublishResult {
  bool accepted{false};
  std::string reason;
};

class GraphOutputPublisher {
public:
  virtual ~GraphOutputPublisher() = default;
  virtual RuntimeChannelPublishResult publish_from(const std::string& source_endpoint, RuntimePayload payload,
                                                   std::optional<EventTimestamp> event_timestamp = std::nullopt) = 0;
  virtual RuntimeChannelPublishResult
  publish_shared_from(const std::string& source_endpoint, RuntimePayloadPtr payload,
                      std::optional<EventTimestamp> event_timestamp = std::nullopt) = 0;
};

struct ConfigView {
  std::map<std::string, std::string> values;
  std::set<std::string> nested_values;

  bool is_nested(const std::string& name) const {
    return nested_values.count(name) != 0u;
  }
};

struct PortDescriptor {
  std::string name;
  std::string schema;
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
  std::string graph_name;
  std::string component_id;

  InputView inputs() const {
    return InputView{channels, component_id};
  }

  RuntimeChannelPublishResult publish(const std::string& port, RuntimePayload payload,
                                      std::optional<EventTimestamp> event_timestamp = std::nullopt) const;
  RuntimeChannelPublishResult publish_shared(const std::string& port, RuntimePayloadPtr payload,
                                             std::optional<EventTimestamp> event_timestamp = std::nullopt) const;
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
};

struct Invocation {
  EventKind event{EventKind::kManual};
  TriggerKind trigger{TriggerKind::kManual};
  std::string port;
  std::string channel_id;
  std::string correlation_id;
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
};

struct TickContext {
  std::uint64_t sequence{0};
  std::chrono::steady_clock::time_point scheduled_at;
  std::chrono::steady_clock::time_point started_at;
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
