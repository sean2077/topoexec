#include "topoexec/adapters/ros2.hpp"
#include "topoexec/runtime/graph_builder.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

topoexec::ComponentNodeSpec boundary_component(std::string id, topoexec::ComponentRole role) {
  topoexec::BoundaryDescriptor boundary;
  boundary.role = role;
  boundary.descriptor = id + "_descriptor";
  return topoexec::component_node(std::move(id), "topoexec.test.Ros2Boundary", {topoexec::manual_event_source()},
                                  topoexec::manual_trigger(), topoexec::lane_execution("main"), std::move(boundary));
}

topoexec::GraphSpec ros2_boundary_graph() {
  return topoexec::GraphBuilder("ros2_boundary_preview")
      .event_loop_lane("main")
      .component(boundary_component("camera_in", topoexec::ComponentRole::kInputBoundary))
      .component(boundary_component("cmd_out", topoexec::ComponentRole::kOutputBoundary))
      .component(boundary_component("service_in", topoexec::ComponentRole::kInputOutputBoundary))
      .build();
}

topoexec::adapters::ros2::BoundaryEndpoint camera_subscription() {
  topoexec::adapters::ros2::BoundaryEndpoint endpoint;
  endpoint.kind = topoexec::adapters::ros2::EndpointKind::kSubscription;
  endpoint.external_name = "/camera";
  endpoint.boundary_id = "camera_in";
  endpoint.port = "out";
  endpoint.qos.reliability = topoexec::adapters::ros2::ReliabilityPolicy::kBestEffort;
  endpoint.qos.depth = 2;
  return endpoint;
}

topoexec::adapters::ros2::BoundaryEndpoint command_publisher() {
  topoexec::adapters::ros2::BoundaryEndpoint endpoint;
  endpoint.kind = topoexec::adapters::ros2::EndpointKind::kPublisher;
  endpoint.external_name = "/cmd";
  endpoint.boundary_id = "cmd_out";
  endpoint.port = "in";
  endpoint.qos.depth = 1;
  return endpoint;
}

} // namespace

TEST(Ros2Adapter, ValidatesBoundaryMappingAndKeepsQosExternal) {
  const auto graph = ros2_boundary_graph();
  const auto subscription = camera_subscription();
  const auto publisher = command_publisher();

  const auto validation = topoexec::adapters::ros2::validate_boundary_mapping(graph, {subscription, publisher});

  ASSERT_TRUE(validation.ok) << (validation.errors.empty() ? "" : validation.errors.front());
  EXPECT_EQ(graph.components.front().boundary.descriptor, "camera_in_descriptor");
  EXPECT_EQ(subscription.qos.reliability, topoexec::adapters::ros2::ReliabilityPolicy::kBestEffort);
  EXPECT_EQ(subscription.qos.depth, 2u);
}

TEST(Ros2Adapter, RejectsBoundaryRoleMismatchesBeforeRuntime) {
  const auto graph = ros2_boundary_graph();
  auto invalid = command_publisher();
  invalid.boundary_id = "camera_in";

  const auto validation = topoexec::adapters::ros2::validate_boundary_mapping(graph, {invalid});

  EXPECT_FALSE(validation.ok);
  ASSERT_FALSE(validation.errors.empty());
  EXPECT_NE(validation.errors.front().find("outbound"), std::string::npos);
}

TEST(Ros2Adapter, InjectsSubscriptionMessageThroughFakeBoundaryBridge) {
  const auto subscription = camera_subscription();
  topoexec::adapters::ros2::FakeRos2BoundaryBridge bridge({subscription}, 1);

  const auto status =
      bridge.receive(subscription, topoexec::make_shared_payload(topoexec::make_text_payload("frame-1")),
                     topoexec::make_event_timestamp(topoexec::TimestampDomain::kExternal, 42), "trace-1");

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(bridge.status().accepted_input_count, 1u);

  const auto input = bridge.poll_input();
  ASSERT_TRUE(input.ok()) << input.reason;
  ASSERT_TRUE(input.ready);
  ASSERT_NE(input.message.payload, nullptr);
  EXPECT_EQ(input.message.boundary_id, "camera_in");
  EXPECT_EQ(input.message.port, "out");
  EXPECT_EQ(input.message.payload->text(), "frame-1");
  EXPECT_EQ(input.message.metadata.correlation_id, "trace-1");
  EXPECT_EQ(input.message.metadata.source_component, "/camera");
  EXPECT_EQ(input.message.metadata.source_port, "subscription");
}

TEST(Ros2Adapter, PublishesBoundaryOutputThroughFakePublisherBridge) {
  const auto publisher = command_publisher();
  topoexec::adapters::ros2::FakeRos2BoundaryBridge bridge({publisher}, 1);
  topoexec::adapters::BoundaryMessage output;
  output.boundary_id = "cmd_out";
  output.port = "in";
  output.payload = topoexec::make_shared_payload(topoexec::make_text_payload("cmd-1"));
  output.metadata.correlation_id = "trace-1";

  const auto status = bridge.publish_output(output);

  ASSERT_TRUE(status.ok()) << status.message();
  ASSERT_EQ(bridge.published_messages().size(), 1u);
  EXPECT_EQ(bridge.published_messages().front().external_name, "/cmd");
  EXPECT_EQ(bridge.published_messages().front().payload->text(), "cmd-1");
  EXPECT_EQ(bridge.published_messages().front().metadata.correlation_id, "trace-1");
  EXPECT_EQ(bridge.published_messages().front().qos.depth, 1u);
}

TEST(Ros2Adapter, ServiceAndActionKindsStayAdapterSide) {
  topoexec::adapters::ros2::BoundaryEndpoint service_request;
  service_request.kind = topoexec::adapters::ros2::EndpointKind::kServiceRequest;
  service_request.external_name = "/solve";
  service_request.boundary_id = "service_in";
  service_request.port = "request";
  service_request.correlation_key = "request_id";

  topoexec::adapters::ros2::BoundaryEndpoint action_result;
  action_result.kind = topoexec::adapters::ros2::EndpointKind::kActionResult;
  action_result.external_name = "/plan";
  action_result.boundary_id = "service_in";
  action_result.port = "result";
  action_result.correlation_key = "goal_id";

  const auto validation =
      topoexec::adapters::ros2::validate_boundary_mapping(ros2_boundary_graph(), {service_request, action_result});

  ASSERT_TRUE(validation.ok) << (validation.errors.empty() ? "" : validation.errors.front());
  EXPECT_TRUE(topoexec::adapters::ros2::is_inbound(service_request.kind));
  EXPECT_TRUE(topoexec::adapters::ros2::is_outbound(action_result.kind));
  EXPECT_EQ(topoexec::adapters::ros2::endpoint_kind_name(action_result.kind), "action_result");
}
