#include "topoexec/adapters/ros2.hpp"

#include <iostream>

int main() {
  topoexec::adapters::ros2::BoundaryEndpoint endpoint;
  endpoint.kind = topoexec::adapters::ros2::EndpointKind::kSubscription;
  endpoint.external_name = "/input";
  endpoint.boundary_id = "external_input";
  endpoint.port = "out";

  topoexec::adapters::ros2::FakeRos2BoundaryBridge bridge({endpoint}, 1);
  const auto status = bridge.receive(endpoint, topoexec::make_shared_payload(topoexec::make_text_payload("hello")));
  if (!status.ok()) {
    std::cerr << status.message() << "\n";
    return 1;
  }
  const auto input = bridge.poll_input();
  if (!input.ready || input.message.payload == nullptr || input.message.payload->text() != "hello") {
    std::cerr << "ROS 2 adapter fake boundary mapping failed\n";
    return 2;
  }
  if (topoexec::adapters::ros2::kRos2AdapterPreviewContractVersion != "0") {
    std::cerr << "unexpected ROS 2 adapter preview contract version\n";
    return 3;
  }

  std::cout << "ros2_adapter_smoke=external_input\n";
  return 0;
}
