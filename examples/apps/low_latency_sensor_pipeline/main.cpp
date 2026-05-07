#include "topoexec/runtime/channel.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

topoexec::EdgeSpec latest_edge(std::string id, std::string from, std::string to) {
  topoexec::EdgeSpec spec;
  spec.id = std::move(id);
  spec.from = std::move(from);
  spec.to = std::move(to);
  spec.has_kind = true;
  spec.kind = topoexec::EdgeKind::kImmediate;
  spec.policy.mode = "latest";
  spec.policy.capacity = 1;
  spec.policy.overflow = "overwrite";
  spec.policy.copy_policy = "shared_view";
  return spec;
}

std::string expect_one_text(const std::vector<topoexec::RuntimeChannelMessage>& messages, const std::string& stage) {
  if (messages.size() != 1u || messages.front().payload == nullptr) {
    throw std::runtime_error(stage + " expected exactly one payload");
  }
  return messages.front().payload->text();
}

void publish_or_throw(topoexec::RuntimeChannelBus& bus, const std::string& endpoint, std::string payload) {
  const auto result = bus.publish_from(endpoint, topoexec::make_text_payload(std::move(payload)));
  if (!result.accepted) {
    throw std::runtime_error(result.reason);
  }
}

} // namespace

int main() {
  topoexec::RuntimeChannelBus bus({
      latest_edge("source_preprocessor_latest", "source.out", "preprocessor.frame"),
      latest_edge("preprocessor_detector_latest", "preprocessor.out", "detector.frame"),
      latest_edge("detector_tracker_latest", "detector.out", "tracker.detection"),
  });

  try {
    for (int frame = 1; frame <= 3; ++frame) {
      publish_or_throw(bus, "source.out", "frame-" + std::to_string(frame));
    }

    const auto raw_frame = expect_one_text(bus.consume_for_component("preprocessor"), "preprocessor");
    publish_or_throw(bus, "preprocessor.out", "preprocessed:" + raw_frame);

    const auto preprocessed = expect_one_text(bus.consume_for_component("detector"), "detector");
    publish_or_throw(bus, "detector.out", "detection:" + preprocessed);

    const auto detection = expect_one_text(bus.consume_for_component("tracker"), "tracker");
    const auto track = "track:" + detection;
    if (track != "track:detection:preprocessed:frame-3") {
      std::cerr << "error: tracker saw stale or malformed payload: " << track << "\n";
      return 2;
    }

    const auto source_metrics = bus.metrics("source_preprocessor_latest");
    const auto preprocessor_metrics = bus.metrics("preprocessor_detector_latest");
    const auto detector_metrics = bus.metrics("detector_tracker_latest");
    if (source_metrics.overwrite_count != 2u || preprocessor_metrics.payload_copy_count != 0u ||
        detector_metrics.payload_copy_count != 0u) {
      std::cerr << "error: unexpected latest/no-copy metrics\n";
      return 3;
    }

    std::cout << "tracker_latest=" << track << "\n";
    std::cout << "source_latest_overwrite_count=" << source_metrics.overwrite_count << "\n";
    std::cout << "pipeline_payload_copy_count="
              << (source_metrics.payload_copy_count + preprocessor_metrics.payload_copy_count +
                  detector_metrics.payload_copy_count)
              << "\n";
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << "\n";
    return 1;
  }
  return 0;
}
