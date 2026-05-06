#include "topoexec/runtime/channel.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

topoexec::EdgeSpec edge(std::string id, std::string from, std::string to, std::string copy_policy) {
  topoexec::EdgeSpec spec;
  spec.id = std::move(id);
  spec.from = std::move(from);
  spec.to = std::move(to);
  spec.has_kind = true;
  spec.kind = topoexec::EdgeKind::kImmediate;
  spec.policy.mode = "latest";
  spec.policy.capacity = 1;
  spec.policy.overflow = "overwrite";
  spec.policy.copy_policy = std::move(copy_policy);
  return spec;
}

std::vector<topoexec::RuntimeChannelMessage> consume_one(topoexec::RuntimeChannelBus& bus,
                                                         const std::string& component) {
  auto messages = bus.consume_for_component(component);
  if (messages.size() != 1u || messages.front().payload == nullptr) {
    throw std::runtime_error(component + " expected exactly one payload");
  }
  return messages;
}

} // namespace

int main() {
  topoexec::RuntimeChannelBus bus({
      edge("copied_metadata", "source.metadata", "copy_sink.in", "copy"),
      edge("shared_metadata", "source.shared", "shared_sink.in", "shared_view"),
      edge("loaned_frame", "camera.frame", "frame_sink.in", "loaned_view"),
  });

  try {
    auto metadata = topoexec::make_shared_payload(topoexec::make_text_payload("frame-metadata"));
    if (!bus.publish_shared_from("source.metadata", metadata).accepted) {
      throw std::runtime_error("copy metadata publish failed");
    }
    if (!bus.publish_shared_from("source.shared", metadata).accepted) {
      throw std::runtime_error("shared metadata publish failed");
    }

    topoexec::BufferPoolConfig config;
    config.fixed_block_size = 64;
    config.max_bytes = 128;
    topoexec::BufferPool pool(config);
    auto loan = pool.loan_frame(32, 4, 4, 8, "gray8");
    if (!loan.valid()) {
      throw std::runtime_error("buffer pool did not loan frame");
    }
    const auto* loaned_address = loan.view().payload_address();
    const auto frame_publish = bus.publish_from("camera.frame", topoexec::make_frame_payload(loan.detach()));
    if (!frame_publish.accepted) {
      throw std::runtime_error(frame_publish.reason);
    }

    (void)consume_one(bus, "copy_sink");
    (void)consume_one(bus, "shared_sink");
    const auto frame_messages = consume_one(bus, "frame_sink");
    const auto* frame = topoexec::try_payload_as<topoexec::FrameView>(*frame_messages.front().payload);
    if (frame == nullptr || !frame->valid() || frame->payload_address() != loaned_address) {
      std::cerr << "error: loaned frame identity was not preserved\n";
      return 2;
    }

    const auto copy_metrics = bus.metrics("copied_metadata");
    const auto shared_metrics = bus.metrics("shared_metadata");
    const auto loaned_metrics = bus.metrics("loaned_frame");
    const auto stats = pool.stats();
    if (copy_metrics.payload_copy_count != 1u || shared_metrics.payload_copy_count != 0u ||
        loaned_metrics.payload_copy_count != 0u || stats.detached_count != 1u || stats.active_count != 0u) {
      std::cerr << "error: unexpected copy or pool metrics\n";
      return 3;
    }

    std::cout << "copy_payload_copy_count=" << copy_metrics.payload_copy_count << "\n";
    std::cout << "shared_payload_copy_count=" << shared_metrics.payload_copy_count << "\n";
    std::cout << "loaned_payload_copy_count=" << loaned_metrics.payload_copy_count << "\n";
    std::cout << "loaned_address_preserved=true\n";
    std::cout << "pool_detached_count=" << stats.detached_count << "\n";
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << "\n";
    return 1;
  }
  return 0;
}
