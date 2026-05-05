#include "topoexec/runtime/channel.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <variant>

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

} // namespace

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

TEST(Channel, QueueDropNewestRejectsIncomingPayloadWhenFull) {
  auto spec = edge("events", "queue", 1);
  spec.policy.overflow = "drop_newest";
  topoexec::RuntimeChannelBus bus({spec});
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);

  const auto result = bus.publish_from("producer.out", topoexec::make_text_payload("two"));
  EXPECT_FALSE(result.accepted);
  EXPECT_EQ(result.reason, "dropped newest payload");

  const auto messages = bus.consume_for_component("consumer");
  ASSERT_EQ(messages.size(), 1u);
  EXPECT_EQ(*messages.front().payload, "one");
  EXPECT_EQ(bus.metrics("events").drop_count, 1u);
}

TEST(Channel, QueueBlockReturnsWouldBlockWithoutDroppingExistingPayload) {
  auto spec = edge("events", "queue", 1);
  spec.policy.overflow = "block";
  topoexec::RuntimeChannelBus bus({spec});
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);

  const auto result = bus.publish_from("producer.out", topoexec::make_text_payload("two"));
  EXPECT_FALSE(result.accepted);
  EXPECT_EQ(result.reason, "would block producer");

  const auto messages = bus.consume_for_component("consumer");
  ASSERT_EQ(messages.size(), 1u);
  EXPECT_EQ(*messages.front().payload, "one");
  EXPECT_EQ(bus.metrics("events").drop_count, 0u);
}

TEST(Channel, QueueFailFastReturnsCapacityError) {
  auto spec = edge("events", "queue", 1);
  spec.policy.overflow = "fail_fast";
  topoexec::RuntimeChannelBus bus({spec});
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);

  const auto result = bus.publish_from("producer.out", topoexec::make_text_payload("two"));
  EXPECT_FALSE(result.accepted);
  EXPECT_EQ(result.reason, "channel capacity exceeded");
  EXPECT_EQ(bus.metrics("events").drop_count, 1u);
}

TEST(Channel, PreviousTickExposesPayloadOnlyAfterEpochAdvance) {
  topoexec::RuntimeChannelBus bus({edge("previous", "previous_tick", 1)});
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);

  auto before = bus.read_latest_update_for_component_port("consumer", "in");
  ASSERT_TRUE(before.ok);
  EXPECT_FALSE(before.message.has_value());

  bus.advance_epoch();

  auto after = bus.read_latest_update_for_component_port("consumer", "in");
  ASSERT_TRUE(after.ok);
  ASSERT_TRUE(after.message.has_value());
  EXPECT_EQ(*after.message->payload, "one");
}

TEST(Channel, LatchedSnapshotIsAvailableToLateReader) {
  topoexec::RuntimeChannelBus bus({edge("latched", "latched", 1)});
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("snapshot")).accepted);

  auto first_reader = bus.read_latest_update_for_component_port("consumer", "in");
  ASSERT_TRUE(first_reader.ok);
  ASSERT_TRUE(first_reader.message.has_value());
  EXPECT_EQ(*first_reader.message->payload, "snapshot");

  auto late_reader = bus.read_latest_for_reader("latched", "late_consumer.in");
  ASSERT_TRUE(late_reader.ok);
  ASSERT_TRUE(late_reader.message.has_value());
  EXPECT_EQ(*late_reader.message->payload, "snapshot");
}

TEST(Channel, BarrierWaitsUntilCapacityBeforeDelivery) {
  topoexec::RuntimeChannelBus bus({edge("barrier", "barrier", 2)});
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);
  EXPECT_TRUE(bus.consume_for_component("consumer").empty());
  EXPECT_EQ(bus.metrics("barrier").delivered_count, 0u);

  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("two")).accepted);
  const auto messages = bus.consume_for_component("consumer");
  ASSERT_EQ(messages.size(), 2u);
  EXPECT_EQ(*messages[0].payload, "one");
  EXPECT_EQ(*messages[1].payload, "two");
  EXPECT_EQ(bus.metrics("barrier").delivered_count, 2u);
}

TEST(Channel, CopyPolicyRejectsLargePayloads) {
  auto spec = edge("frames");
  spec.policy.copy_policy = "copy";
  topoexec::RuntimeChannelBus bus({spec});
  auto buffer = std::make_shared<const topoexec::SharedBuffer>(32);
  const auto result = bus.publish_from("producer.out", topoexec::make_binary_blob_payload(buffer, 0, 32, "bytes"));
  EXPECT_FALSE(result.accepted);
  EXPECT_NE(result.reason.find("cannot copy large payload schema topoexec.runtime.BinaryBlob"), std::string::npos);
}

TEST(Channel, SharedAndLoanedViewDoNotCopyPayloads) {
  for (const auto* copy_policy : {"shared_view", "loaned_view"}) {
    auto spec = edge(std::string("frames_") + copy_policy);
    spec.policy.copy_policy = copy_policy;
    topoexec::RuntimeChannelBus bus({spec});
    ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("payload")).accepted);
    EXPECT_EQ(bus.metrics(spec.id).payload_copy_count, 0u);
  }
}

TEST(Channel, LoanedViewPreservesLoanedFrameBufferWithoutCopying) {
  auto spec = edge("loaned_frames");
  spec.policy.copy_policy = "loaned_view";
  topoexec::RuntimeChannelBus bus({spec});
  topoexec::BufferPool pool;
  auto loan = pool.loan_frame(32, 4, 4, 8, "gray8");
  ASSERT_TRUE(loan.valid());
  const auto* address = loan.view().payload_address();

  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_frame_payload(loan.detach())).accepted);
  const auto messages = bus.consume_for_component("consumer");

  ASSERT_EQ(messages.size(), 1u);
  const auto& payload = *messages.front().payload;
  ASSERT_TRUE(std::holds_alternative<topoexec::FrameView>(payload.value));
  const auto& frame = std::get<topoexec::FrameView>(payload.value);
  EXPECT_TRUE(frame.valid());
  EXPECT_EQ(frame.payload_address(), address);
  EXPECT_EQ(bus.metrics("loaned_frames").payload_copy_count, 0u);
  EXPECT_EQ(pool.stats().alloc_count, 1u);
}

TEST(Payload, TypedHelpersReturnExpectedPayloadVariants) {
  const auto text_payload = topoexec::make_text_payload("hello");
  ASSERT_TRUE(topoexec::payload_is<topoexec::TextPayload>(text_payload));
  ASSERT_NE(topoexec::try_payload_as<topoexec::TextPayload>(text_payload), nullptr);
  EXPECT_EQ(topoexec::payload_as<topoexec::TextPayload>(text_payload).text, "hello");
  EXPECT_EQ(topoexec::try_payload_as<topoexec::BinaryBlobPayload>(text_payload), nullptr);
  EXPECT_THROW((void)topoexec::payload_as<topoexec::BinaryBlobPayload>(text_payload, "test"), std::runtime_error);

  topoexec::Invocation invocation;
  invocation.payload = topoexec::make_shared_payload(text_payload);
  ASSERT_NE(invocation.try_payload_as<topoexec::TextPayload>(), nullptr);
  EXPECT_EQ(invocation.payload_as<topoexec::TextPayload>().text, "hello");
  EXPECT_THROW((void)invocation.payload_as<topoexec::FrameView>(), std::runtime_error);
}

TEST(Payload, MissingInvocationPayloadReportsContext) {
  topoexec::Invocation invocation;

  EXPECT_EQ(invocation.try_payload_as<topoexec::TextPayload>(), nullptr);
  try {
    (void)invocation.payload_as<topoexec::TextPayload>("consumer.in");
    FAIL() << "expected payload_as to throw";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("consumer.in: invocation payload is null"), std::string::npos);
  }
}

TEST(Payload, InputViewMissingPortReturnsNullAndEmptyBatch) {
  topoexec::RuntimeChannelBus bus({edge("input", "queue", 2)});
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);

  topoexec::GraphContext context;
  context.channels = &bus;
  context.component_id = "consumer";
  const auto inputs = context.inputs();

  EXPECT_EQ(inputs.peek_latest("missing"), nullptr);
  EXPECT_EQ(inputs.read_latest_update("missing"), nullptr);
  EXPECT_TRUE(inputs.drain("missing").empty());
  ASSERT_NE(inputs.peek_latest("in"), nullptr);
}

TEST(Payload, BatchPayloadsCanUseTypedHelpersInOrder) {
  topoexec::Invocation invocation;
  invocation.batch_payloads = {topoexec::make_shared_payload(topoexec::make_text_payload("one")),
                               topoexec::make_shared_payload(topoexec::make_text_payload("two"))};

  ASSERT_EQ(invocation.batch_payloads.size(), 2u);
  ASSERT_NE(invocation.batch_payloads[0], nullptr);
  ASSERT_NE(invocation.batch_payloads[1], nullptr);
  EXPECT_EQ(topoexec::payload_as<topoexec::TextPayload>(*invocation.batch_payloads[0]).text, "one");
  EXPECT_EQ(topoexec::payload_as<topoexec::TextPayload>(*invocation.batch_payloads[1]).text, "two");
}
