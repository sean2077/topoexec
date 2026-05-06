#include "topoexec/adapters/sdk.hpp"

#include <iostream>

int main() {
  topoexec::adapters::BoundaryMessage message;
  message.boundary_id = "adapter-smoke";
  message.port = "out";
  message.payload = topoexec::make_shared_payload(topoexec::make_text_payload("adapter-sdk"));

  const auto poll = topoexec::adapters::BoundaryPollResult::input(message);
  if (!poll.ok() || !poll.ready || poll.message.payload == nullptr || poll.message.payload->text() != "adapter-sdk") {
    std::cerr << "adapter SDK boundary poll contract failed\n";
    return 1;
  }
  if (topoexec::adapters::kAdapterSdkContractVersion[0] != '0') {
    std::cerr << "unexpected adapter SDK contract version\n";
    return 2;
  }
  topoexec::adapters::InMemoryRuntimeObserver observer(1);
  if (observer.status().dropped_event_count != 0u) {
    std::cerr << "unexpected observer initial status\n";
    return 3;
  }
  std::cout << "adapter_sdk_smoke=" << poll.message.payload->text() << "\n";
  return 0;
}
