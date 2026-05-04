#include "topoexec/runtime/channel.hpp"

#include <iostream>
#include <string>

int main() {
  topoexec::EdgeSpec edge;
  edge.id = "source_sink";
  edge.from = "source.out";
  edge.to = "sink.in";
  edge.has_kind = true;
  edge.kind = topoexec::EdgeKind::kImmediate;
  edge.policy.mode = "queue";
  edge.policy.capacity = 2;
  edge.policy.overflow = "drop_oldest";
  edge.policy.copy_policy = "shared_view";

  topoexec::RuntimeChannelBus bus({edge});
  const auto published = bus.publish_from("source.out", topoexec::make_text_payload("runtime-only"));
  if (!published.accepted) {
    std::cerr << published.reason << "\n";
    return 1;
  }

  const auto messages = bus.consume_for_component("sink");
  if (messages.size() != 1u || messages.front().payload == nullptr || *messages.front().payload != "runtime-only") {
    std::cerr << "unexpected runtime message flow\n";
    return 1;
  }
  return 0;
}
