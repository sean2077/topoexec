#pragma once

// API stability: experimental. ROS 2 adapter preview is dependency-free boundary mapping evidence.

#include "topoexec/adapters/sdk.hpp"
#include "topoexec/runtime/graph.hpp"

#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace topoexec::adapters::ros2 {

inline constexpr std::string_view kRos2AdapterPreviewContractVersion{"0"};

enum class EndpointKind {
  kSubscription,
  kPublisher,
  kServiceRequest,
  kServiceResponse,
  kActionGoal,
  kActionFeedback,
  kActionResult,
  kActionCancel,
};

enum class ReliabilityPolicy {
  kReliable,
  kBestEffort,
};

enum class DurabilityPolicy {
  kVolatile,
  kTransientLocal,
};

struct QosProfilePreview {
  ReliabilityPolicy reliability{ReliabilityPolicy::kReliable};
  DurabilityPolicy durability{DurabilityPolicy::kVolatile};
  std::size_t depth{10};
  int deadline_ms{0};
  int lifespan_ms{0};
};

struct BoundaryEndpoint {
  EndpointKind kind{EndpointKind::kSubscription};
  std::string external_name;
  std::string boundary_id;
  std::string port;
  QosProfilePreview qos;
  std::string correlation_key;
};

struct MappingValidationResult {
  bool ok{true};
  std::vector<std::string> errors;
};

struct PublishedMessagePreview {
  std::string external_name;
  RuntimePayloadPtr payload;
  InvocationMetadata metadata;
  std::optional<EventTimestamp> event_timestamp;
  QosProfilePreview qos;
};

inline bool is_inbound(EndpointKind kind) {
  return kind == EndpointKind::kSubscription || kind == EndpointKind::kServiceRequest ||
         kind == EndpointKind::kActionGoal || kind == EndpointKind::kActionCancel;
}

inline bool is_outbound(EndpointKind kind) {
  return !is_inbound(kind);
}

inline std::string_view endpoint_kind_name(EndpointKind kind) {
  switch (kind) {
  case EndpointKind::kSubscription:
    return "subscription";
  case EndpointKind::kPublisher:
    return "publisher";
  case EndpointKind::kServiceRequest:
    return "service_request";
  case EndpointKind::kServiceResponse:
    return "service_response";
  case EndpointKind::kActionGoal:
    return "action_goal";
  case EndpointKind::kActionFeedback:
    return "action_feedback";
  case EndpointKind::kActionResult:
    return "action_result";
  case EndpointKind::kActionCancel:
    return "action_cancel";
  }
  return "unknown";
}

inline const ComponentNodeSpec* find_boundary_component(const GraphSpec& graph, std::string_view boundary_id) {
  for (const auto& component : graph.components) {
    if (component.id == boundary_id) {
      return &component;
    }
  }
  return nullptr;
}

namespace detail {

inline void add_error(MappingValidationResult& result, std::string message) {
  result.ok = false;
  result.errors.push_back(std::move(message));
}

} // namespace detail

inline MappingValidationResult validate_boundary_mapping(const GraphSpec& graph,
                                                         const std::vector<BoundaryEndpoint>& endpoints) {
  MappingValidationResult result;
  for (const auto& endpoint : endpoints) {
    if (endpoint.external_name.empty()) {
      detail::add_error(result, "ROS 2 endpoint external_name is required");
    }
    if (endpoint.boundary_id.empty()) {
      detail::add_error(result, "ROS 2 endpoint boundary_id is required");
      continue;
    }
    if (endpoint.port.empty()) {
      detail::add_error(result, "ROS 2 endpoint " + endpoint.boundary_id + " port is required");
    }
    const auto* component = find_boundary_component(graph, endpoint.boundary_id);
    if (component == nullptr) {
      detail::add_error(result, "ROS 2 endpoint " + endpoint.boundary_id + " does not reference a graph component");
      continue;
    }
    if (is_inbound(endpoint.kind) && !component_role_has_input(component->boundary.role)) {
      detail::add_error(result, "ROS 2 inbound endpoint " + endpoint.boundary_id + " must map to an input boundary");
    }
    if (is_outbound(endpoint.kind) && !component_role_has_output(component->boundary.role)) {
      detail::add_error(result, "ROS 2 outbound endpoint " + endpoint.boundary_id + " must map to an output boundary");
    }
  }
  return result;
}

class FakeRos2BoundaryBridge final : public BoundaryBridge {
public:
  FakeRos2BoundaryBridge(std::vector<BoundaryEndpoint> endpoints, std::size_t input_capacity)
      : endpoints_(std::move(endpoints)), input_capacity_(input_capacity) {}

  Status receive(const BoundaryEndpoint& endpoint, RuntimePayloadPtr payload,
                 std::optional<EventTimestamp> event_timestamp = std::nullopt, std::string correlation_id = {}) {
    if (!is_inbound(endpoint.kind)) {
      ++status_.dropped_input_count;
      return Status::error("ROS 2 endpoint " + endpoint.external_name + " is not inbound");
    }
    if (payload == nullptr) {
      ++status_.dropped_input_count;
      return Status::error("ROS 2 inbound payload is null");
    }
    if (find_endpoint(endpoint.boundary_id, endpoint.port, false) == nullptr) {
      ++status_.dropped_input_count;
      return Status::error("ROS 2 inbound boundary endpoint not registered: " + endpoint.boundary_id + "." +
                           endpoint.port);
    }
    if (pending_inputs_.size() >= input_capacity_) {
      ++status_.dropped_input_count;
      status_.pending_input_count = pending_inputs_.size();
      return Status::error("ROS 2 fake boundary input queue full");
    }

    BoundaryMessage message;
    message.boundary_id = endpoint.boundary_id;
    message.port = endpoint.port;
    message.payload = std::move(payload);
    message.event_timestamp = std::move(event_timestamp);
    message.metadata.correlation_id = std::move(correlation_id);
    message.metadata.source_component = endpoint.external_name;
    message.metadata.source_port = std::string(endpoint_kind_name(endpoint.kind));
    pending_inputs_.push_back(std::move(message));
    ++status_.accepted_input_count;
    status_.pending_input_count = pending_inputs_.size();
    return Status::success();
  }

  BoundaryPollResult poll_input() override {
    if (pending_inputs_.empty()) {
      status_.pending_input_count = 0;
      return BoundaryPollResult::no_input();
    }
    auto message = std::move(pending_inputs_.front());
    pending_inputs_.pop_front();
    status_.pending_input_count = pending_inputs_.size();
    return BoundaryPollResult::input(std::move(message));
  }

  Status publish_output(const BoundaryMessage& message) override {
    const auto* endpoint = find_endpoint(message.boundary_id, message.port, true);
    if (endpoint == nullptr) {
      ++status_.failed_output_count;
      return Status::error("ROS 2 outbound boundary endpoint not registered: " + message.boundary_id + "." +
                           message.port);
    }
    if (message.payload == nullptr) {
      ++status_.failed_output_count;
      return Status::error("ROS 2 outbound payload is null");
    }
    published_messages_.push_back(PublishedMessagePreview{endpoint->external_name, message.payload, message.metadata,
                                                          message.event_timestamp, endpoint->qos});
    ++status_.published_output_count;
    return Status::success();
  }

  BoundaryBridgeStatus status() const override {
    return status_;
  }

  const std::vector<PublishedMessagePreview>& published_messages() const {
    return published_messages_;
  }

private:
  const BoundaryEndpoint* find_endpoint(std::string_view boundary_id, std::string_view port, bool outbound) const {
    for (const auto& endpoint : endpoints_) {
      if (endpoint.boundary_id == boundary_id && endpoint.port == port && is_outbound(endpoint.kind) == outbound) {
        return &endpoint;
      }
    }
    return nullptr;
  }

  std::vector<BoundaryEndpoint> endpoints_;
  std::size_t input_capacity_{0};
  std::deque<BoundaryMessage> pending_inputs_;
  std::vector<PublishedMessagePreview> published_messages_;
  BoundaryBridgeStatus status_;
};

} // namespace topoexec::adapters::ros2
