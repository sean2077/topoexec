#include "topoexec/runtime/channel.hpp"

#include <gtest/gtest.h>

namespace {

topoexec::EdgeSpec edge(std::string id, std::string mode = "latest", int capacity = 1) {
  topoexec::EdgeSpec spec;
  spec.id = std::move(id);
  spec.from = "producer.out";
  spec.to = "consumer.in";
  spec.has_kind = true;
  spec.kind = topoexec::EdgeKind::kImmediate;
  spec.policy.mode = std::move(mode);
  spec.policy.capacity = capacity;
  spec.policy.overflow = spec.policy.mode == "latest" ? "overwrite" : "drop_oldest";
  spec.policy.copy_policy = "shared_view";
  return spec;
}

}  // namespace

TEST(Channel, LatestChannelDeliversOnlyNewestPayload) {
  topoexec::RuntimeChannelBus bus({edge("frames")});
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("two")).accepted);

  const auto messages = bus.consume_for_component("consumer");
  ASSERT_EQ(messages.size(), 1u);
  EXPECT_EQ(*messages.front().payload, "two");
  EXPECT_EQ(bus.metrics("frames").drop_count, 1u);
}

TEST(Channel, QueueDropsOldestWhenFull) {
  topoexec::RuntimeChannelBus bus({edge("events", "queue", 2)});
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("two")).accepted);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("three")).accepted);

  const auto messages = bus.consume_for_component("consumer");
  ASSERT_EQ(messages.size(), 2u);
  EXPECT_EQ(*messages[0].payload, "two");
  EXPECT_EQ(*messages[1].payload, "three");
  EXPECT_EQ(bus.metrics("events").drop_count, 1u);
}

TEST(Channel, CopyPolicyRejectsLargePayloads) {
  auto spec = edge("frames");
  spec.policy.copy_policy = "copy";
  topoexec::RuntimeChannelBus bus({spec});
  auto buffer = std::make_shared<const topoexec::SharedBuffer>(32);
  const auto result =
      bus.publish_from("producer.out", topoexec::make_binary_blob_payload(buffer, 0, 32, "bytes"));
  EXPECT_FALSE(result.accepted);
  EXPECT_NE(result.reason.find("cannot copy large payload schema topoexec.runtime.BinaryBlob"), std::string::npos);
}

