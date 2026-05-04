#include "topoexec/runtime/channel.hpp"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

topoexec::EdgeSpec edge(std::string id, std::string mode, int capacity, std::string overflow) {
  topoexec::EdgeSpec spec;
  spec.id = std::move(id);
  spec.from = "fast_source.out";
  spec.to = "slow_processor.in";
  spec.has_kind = true;
  spec.kind = topoexec::EdgeKind::kImmediate;
  spec.policy.mode = std::move(mode);
  spec.policy.capacity = capacity;
  spec.policy.overflow = std::move(overflow);
  spec.policy.copy_policy = "shared_view";
  return spec;
}

std::string join_payloads(const std::vector<topoexec::RuntimeChannelMessage>& messages) {
  std::string output;
  for (std::size_t index = 0; index < messages.size(); ++index) {
    if (index > 0u) {
      output += ",";
    }
    output += messages[index].payload->text();
  }
  return output;
}

} // namespace

int main() {
  topoexec::RuntimeChannelBus latest({edge("latest_frames", "latest", 1, "overwrite")});
  for (int index = 1; index <= 3; ++index) {
    const auto result =
        latest.publish_from("fast_source.out", topoexec::make_text_payload("frame-" + std::to_string(index)));
    if (!result.accepted) {
      std::cerr << "latest publish failed: " << result.reason << "\n";
      return 1;
    }
  }
  const auto latest_messages = latest.consume_for_component("slow_processor");
  const auto latest_metrics = latest.metrics("latest_frames");

  topoexec::RuntimeChannelBus queue({edge("queued_events", "queue", 2, "drop_oldest")});
  for (int index = 1; index <= 3; ++index) {
    const auto result =
        queue.publish_from("fast_source.out", topoexec::make_text_payload("event-" + std::to_string(index)));
    if (!result.accepted) {
      std::cerr << "queue publish failed: " << result.reason << "\n";
      return 1;
    }
  }
  const auto queue_messages = queue.consume_for_component("slow_processor");
  const auto queue_metrics = queue.metrics("queued_events");

  const auto latest_payloads = join_payloads(latest_messages);
  const auto queue_payloads = join_payloads(queue_messages);
  if (latest_payloads != "frame-3" || queue_payloads != "event-2,event-3") {
    std::cerr << "unexpected overload result: latest=" << latest_payloads << " queue=" << queue_payloads << "\n";
    return 2;
  }

  std::cout << "latest_payloads=" << latest_payloads << "\n";
  std::cout << "latest_drop_count=" << latest_metrics.drop_count << "\n";
  std::cout << "latest_max_depth=" << latest_metrics.max_depth << "\n";
  std::cout << "queue_payloads=" << queue_payloads << "\n";
  std::cout << "queue_drop_count=" << queue_metrics.drop_count << "\n";
  std::cout << "queue_max_depth=" << queue_metrics.max_depth << "\n";
  return 0;
}
