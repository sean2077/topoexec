#include "topoexec/runtime/channel.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
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

std::size_t health_count(const std::vector<topoexec::HealthEvent>& events, topoexec::HealthEventKind kind) {
  return static_cast<std::size_t>(
      std::count_if(events.begin(), events.end(), [&](const auto& event) { return event.kind == kind; }));
}

} // namespace

TEST(Channel, LatestChannelDeliversOnlyNewestPayload) {
  topoexec::RuntimeChannelBus bus({edge("frames")});
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("two")).accepted);

  const auto messages = bus.consume_for_component("consumer");
  ASSERT_EQ(messages.size(), 1u);
  EXPECT_EQ(*messages.front().payload, "two");
  const auto metrics = bus.metrics("frames");
  EXPECT_EQ(metrics.drop_count, 1u);
  EXPECT_EQ(metrics.overwrite_count, 1u);
  EXPECT_EQ(metrics.health_event_count, 1u);
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

TEST(Channel, HealthEventsReportHighWatermarkOnceAndCoalesceOverflow) {
  topoexec::HealthEventSink sink(8);
  topoexec::RuntimeChannelBus bus({edge("events", "queue", 2)});
  bus.set_health_event_sink(&sink);

  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("two")).accepted);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("three")).accepted);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("four")).accepted);

  const auto events = sink.snapshot();
  EXPECT_EQ(health_count(events, topoexec::HealthEventKind::kBackpressureHighWatermark), 1u);
  EXPECT_EQ(health_count(events, topoexec::HealthEventKind::kChannelOverflow), 1u);
  const auto overflow = std::find_if(events.begin(), events.end(), [](const auto& event) {
    return event.kind == topoexec::HealthEventKind::kChannelOverflow;
  });
  ASSERT_NE(overflow, events.end());
  EXPECT_EQ(overflow->edge_id, "events");
  EXPECT_EQ(overflow->policy, "drop_oldest");
  EXPECT_EQ(overflow->occurrence_count, 2u);
  EXPECT_EQ(sink.coalesced_count(), 1u);
}

TEST(Channel, HealthEventsRespectEdgeEmissionFlag) {
  auto spec = edge("events", "queue", 1);
  spec.policy.emit_health_events = false;
  topoexec::HealthEventSink sink(8);
  topoexec::RuntimeChannelBus bus({spec});
  bus.set_health_event_sink(&sink);

  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("two")).accepted);

  EXPECT_TRUE(sink.snapshot().empty());
  EXPECT_EQ(bus.metrics("events").health_event_count, 1u);
}

TEST(Channel, HealthEventSinkBoundsStoredEventsWithoutRejectingRuntimePath) {
  topoexec::HealthEventSink sink(1);
  topoexec::RuntimeChannelBus bus({edge("events", "queue", 1)});
  bus.set_health_event_sink(&sink);

  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("two")).accepted);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("three")).accepted);

  const auto events = sink.snapshot();
  ASSERT_LE(events.size(), 1u);
  EXPECT_GE(sink.dropped_count(), 1u);
  EXPECT_EQ(bus.metrics("events").published_count, 3u);
}

TEST(Channel, ZeroCapacityHealthEventSinkDoesNotBlockOrRejectPublish) {
  topoexec::HealthEventSink sink(0);
  topoexec::RuntimeChannelBus bus({edge("events", "queue", 1)});
  bus.set_health_event_sink(&sink);

  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("two")).accepted);

  EXPECT_TRUE(sink.snapshot().empty());
  EXPECT_GE(sink.dropped_count(), 1u);
  EXPECT_EQ(bus.metrics("events").published_count, 2u);
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
  const auto metrics = bus.metrics("events");
  EXPECT_EQ(metrics.drop_count, 1u);
  EXPECT_EQ(metrics.reject_count, 1u);
  EXPECT_EQ(metrics.health_event_count, 1u);
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
  const auto metrics = bus.metrics("events");
  EXPECT_EQ(metrics.drop_count, 0u);
  EXPECT_EQ(metrics.reject_count, 1u);
  EXPECT_EQ(metrics.health_event_count, 1u);
}

TEST(Channel, QueueFailFastReturnsCapacityError) {
  auto spec = edge("events", "queue", 1);
  spec.policy.overflow = "fail_fast";
  topoexec::RuntimeChannelBus bus({spec});
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);

  const auto result = bus.publish_from("producer.out", topoexec::make_text_payload("two"));
  EXPECT_FALSE(result.accepted);
  EXPECT_EQ(result.reason, "channel capacity exceeded");
  const auto metrics = bus.metrics("events");
  EXPECT_EQ(metrics.drop_count, 1u);
  EXPECT_EQ(metrics.reject_count, 1u);
  EXPECT_EQ(metrics.health_event_count, 1u);
}

TEST(Channel, QueueDrainMaxBatchPreservesRemainingMessages) {
  topoexec::RuntimeChannelBus bus({edge("events", "queue", 3)});
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("two")).accepted);

  auto first = bus.drain_for_component_port("consumer", "in", 1);
  ASSERT_EQ(first.size(), 1u);
  EXPECT_EQ(*first.front().payload, "one");
  EXPECT_EQ(bus.metrics("events").depth, 1u);

  auto second = bus.drain_for_component_port("consumer", "in", 1);
  ASSERT_EQ(second.size(), 1u);
  EXPECT_EQ(*second.front().payload, "two");
}

TEST(Channel, SnapshotDoesNotConsumeQueuedMessages) {
  topoexec::RuntimeChannelBus bus({edge("events", "queue", 3)});
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("two")).accepted);

  const auto snapshot = bus.snapshot_for_component_port("consumer", "in", 2);

  ASSERT_EQ(snapshot.size(), 2u);
  EXPECT_EQ(*snapshot[0].payload, "one");
  EXPECT_EQ(*snapshot[1].payload, "two");
  EXPECT_EQ(bus.metrics("events").delivered_count, 0u);
  const auto drained = bus.consume_for_component("consumer");
  ASSERT_EQ(drained.size(), 2u);
}

TEST(Channel, QueueMultiReaderMaintainsPerReaderCursor) {
  auto spec = edge("events", "queue", 3);
  spec.policy.readers = "multi";
  topoexec::RuntimeChannelBus bus({spec});
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("two")).accepted);

  const auto first_reader = bus.drain_for_reader("events", "reader_a");
  const auto first_reader_again = bus.drain_for_reader("events", "reader_a");
  const auto second_reader = bus.drain_for_reader("events", "reader_b");

  ASSERT_EQ(first_reader.size(), 2u);
  EXPECT_TRUE(first_reader_again.empty());
  ASSERT_EQ(second_reader.size(), 2u);
  EXPECT_EQ(*second_reader[0].payload, "one");
  EXPECT_EQ(*second_reader[1].payload, "two");
  const auto metrics = bus.metrics("events");
  EXPECT_EQ(metrics.delivered_count, 4u);
  EXPECT_EQ(metrics.depth, 2u);
}

TEST(Channel, QueueMultiReaderSlowReaderMissesDroppedHistory) {
  auto spec = edge("events", "queue", 2);
  spec.policy.readers = "multi";
  topoexec::RuntimeChannelBus bus({spec});
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("two")).accepted);

  const auto fast_initial = bus.drain_for_reader("events", "fast");
  ASSERT_EQ(fast_initial.size(), 2u);
  EXPECT_EQ(*fast_initial[0].payload, "one");
  EXPECT_EQ(*fast_initial[1].payload, "two");

  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("three")).accepted);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("four")).accepted);

  const auto slow = bus.drain_for_reader("events", "slow");
  ASSERT_EQ(slow.size(), 2u);
  EXPECT_EQ(*slow[0].payload, "three");
  EXPECT_EQ(*slow[1].payload, "four");
  const auto metrics = bus.metrics("events");
  EXPECT_EQ(metrics.drop_count, 2u);
  EXPECT_EQ(metrics.depth, 2u);
}

TEST(Channel, QueueMultiReaderCursorSurvivesOverflow) {
  auto spec = edge("events", "queue", 2);
  spec.policy.readers = "multi";
  topoexec::RuntimeChannelBus bus({spec});
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("two")).accepted);

  const auto first_a = bus.drain_for_reader("events", "reader_a");
  ASSERT_EQ(first_a.size(), 2u);
  EXPECT_EQ(*first_a[0].payload, "one");
  EXPECT_EQ(*first_a[1].payload, "two");

  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("three")).accepted);

  const auto second_a = bus.drain_for_reader("events", "reader_a");
  ASSERT_EQ(second_a.size(), 1u);
  EXPECT_EQ(*second_a[0].payload, "three");

  const auto reader_b = bus.drain_for_reader("events", "reader_b");
  ASSERT_EQ(reader_b.size(), 2u);
  EXPECT_EQ(*reader_b[0].payload, "two");
  EXPECT_EQ(*reader_b[1].payload, "three");
  const auto metrics = bus.metrics("events");
  EXPECT_EQ(metrics.drop_count, 1u);
  EXPECT_EQ(metrics.delivered_count, 5u);
}

TEST(Channel, DeadlineMissIsMarkedOnLateConsume) {
  auto spec = edge("events", "queue", 2);
  spec.policy.deadline_ms = 1;
  topoexec::HealthEventSink sink(4);
  topoexec::RuntimeChannelBus bus({spec});
  bus.set_health_event_sink(&sink);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("late")).accepted);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  const auto messages = bus.consume_for_component("consumer");

  ASSERT_EQ(messages.size(), 1u);
  EXPECT_TRUE(messages.front().deadline_missed);
  const auto metrics = bus.metrics("events");
  EXPECT_EQ(metrics.deadline_miss_count, 1u);
  EXPECT_EQ(metrics.health_event_count, 1u);
  EXPECT_GT(metrics.message_age_ms, 0.0);
  EXPECT_EQ(health_count(sink.snapshot(), topoexec::HealthEventKind::kChannelDeadlineMiss), 1u);
}

TEST(Channel, LifespanDropsStaleMessageBeforeDelivery) {
  auto spec = edge("events", "queue", 2);
  spec.policy.lifespan_ms = 1;
  topoexec::HealthEventSink sink(4);
  topoexec::RuntimeChannelBus bus({spec});
  bus.set_health_event_sink(&sink);
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("stale")).accepted);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  const auto messages = bus.consume_for_component("consumer");

  EXPECT_TRUE(messages.empty());
  const auto metrics = bus.metrics("events");
  EXPECT_EQ(metrics.drop_count, 1u);
  EXPECT_EQ(metrics.stale_drop_count, 1u);
  EXPECT_EQ(metrics.health_event_count, 1u);
  EXPECT_EQ(metrics.degradation_reason, "stale message expired");
  EXPECT_EQ(health_count(sink.snapshot(), topoexec::HealthEventKind::kChannelStaleDrop), 1u);
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

TEST(Channel, PreviousTickNotifiesWaitersExactlyOnceWhenPendingValueBecomesVisible) {
  topoexec::RuntimeChannelBus bus({edge("previous", "previous_tick", 1)});
  const auto initial_sequence = bus.update_sequence();
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("one")).accepted);
  EXPECT_EQ(bus.update_sequence(), initial_sequence);
  EXPECT_FALSE(bus.wait_for_update(initial_sequence, std::chrono::milliseconds(2), {}));

  std::atomic_bool waiter_done{false};
  bool waiter_result = false;
  std::thread waiter([&]() {
    waiter_result = bus.wait_for_update(initial_sequence, std::chrono::milliseconds(200), {});
    waiter_done.store(true);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  EXPECT_FALSE(waiter_done.load());
  bus.advance_epoch();
  waiter.join();

  EXPECT_TRUE(waiter_result);
  EXPECT_TRUE(waiter_done.load());
  EXPECT_EQ(bus.update_sequence(), initial_sequence + 1u);
  EXPECT_FALSE(bus.wait_for_update(initial_sequence + 1u, std::chrono::milliseconds(2), {}));
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

TEST(Channel, CopyPolicyCopiesTextPayloadAndOwnsResult) {
  auto spec = edge("copied_events", "queue", 1);
  spec.policy.copy_policy = "copy";
  topoexec::RuntimeChannelBus bus({spec});
  auto original = topoexec::make_shared_payload(topoexec::make_text_payload("copy-me"));
  std::weak_ptr<const topoexec::RuntimePayload> weak = original;

  ASSERT_TRUE(bus.publish_shared_from("producer.out", original).accepted);
  original.reset();
  EXPECT_TRUE(weak.expired());

  const auto messages = bus.consume_for_component("consumer");
  ASSERT_EQ(messages.size(), 1u);
  EXPECT_EQ(*messages.front().payload, "copy-me");
  EXPECT_EQ(bus.metrics("copied_events").payload_copy_count, 1u);
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

TEST(Channel, SharedViewKeepsPayloadAliveUntilRetainedMessageIsReleased) {
  auto spec = edge("shared_events", "queue", 1);
  spec.policy.copy_policy = "shared_view";
  topoexec::RuntimeChannelBus bus({spec});
  auto payload = topoexec::make_shared_payload(topoexec::make_text_payload("shared"));
  std::weak_ptr<const topoexec::RuntimePayload> weak = payload;

  ASSERT_TRUE(bus.publish_shared_from("producer.out", payload).accepted);
  payload.reset();
  EXPECT_FALSE(weak.expired());

  auto messages = bus.consume_for_component("consumer");
  ASSERT_EQ(messages.size(), 1u);
  EXPECT_EQ(*messages.front().payload, "shared");
  EXPECT_FALSE(weak.expired());

  messages.clear();
  EXPECT_FALSE(weak.expired());
  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_text_payload("replacement")).accepted);
  EXPECT_TRUE(weak.expired());
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
  const auto stats = pool.stats();
  EXPECT_EQ(stats.alloc_count, 1u);
  EXPECT_EQ(stats.loan_count, 1u);
  EXPECT_EQ(stats.release_count, 0u);
  EXPECT_EQ(stats.detached_count, 1u);
  EXPECT_EQ(stats.active_count, 0u);
  EXPECT_EQ(stats.bytes_allocated, 32u);
}

TEST(Channel, MoveOnlySingleReaderAcceptsLargePayloadWithoutCopying) {
  auto spec = edge("move_frames", "queue", 1);
  spec.policy.copy_policy = "move_only";
  spec.policy.readers = "single";
  topoexec::RuntimeChannelBus bus({spec});
  auto buffer = std::make_shared<const topoexec::SharedBuffer>(16);

  ASSERT_TRUE(bus.publish_from("producer.out", topoexec::make_binary_blob_payload(buffer, 0, 16, "bytes")).accepted);

  const auto messages = bus.consume_for_component("consumer");
  ASSERT_EQ(messages.size(), 1u);
  EXPECT_TRUE(std::holds_alternative<topoexec::BinaryBlobPayload>(messages.front().payload->value));
  const auto metrics = bus.metrics("move_frames");
  EXPECT_EQ(metrics.payload_copy_count, 0u);
  EXPECT_EQ(metrics.copy_fallback_count, 0u);
}

TEST(Channel, BufferPoolReusesReleasedFramesAndReportsMetrics) {
  topoexec::BufferPool pool;
  {
    auto loan = pool.loan_frame(16, 4, 4, 4, "gray8");
    ASSERT_TRUE(loan.valid());
    EXPECT_TRUE(pool.has_outstanding_loans());
    EXPECT_EQ(pool.stats().active_count, 1u);
  }
  auto reused = pool.loan_frame(8, 2, 2, 4, "gray8");

  EXPECT_TRUE(reused.valid());
  const auto stats = pool.stats();
  EXPECT_EQ(stats.alloc_count, 1u);
  EXPECT_EQ(stats.reuse_count, 1u);
  EXPECT_EQ(stats.loan_count, 2u);
  EXPECT_EQ(stats.release_count, 1u);
  EXPECT_EQ(stats.active_count, 1u);
  EXPECT_EQ(stats.available_count, 0u);
  EXPECT_EQ(stats.high_watermark_bytes, 16u);
}

TEST(Channel, BufferPoolBoundsAllocationWithBucketsAlignmentAndMaxBytes) {
  topoexec::BufferPoolConfig config;
  config.bucket_sizes = {64u, 256u};
  config.alignment = 16u;
  config.max_bytes = 320u;
  topoexec::BufferPool pool(config);

  auto small = pool.loan_frame(17, 1, 1, 17, "bytes");
  auto large = pool.loan_frame(200, 1, 1, 200, "bytes");
  ASSERT_TRUE(small.valid());
  ASSERT_TRUE(large.valid());
  EXPECT_EQ(small.view().buffer->size(), 64u);
  EXPECT_EQ(large.view().buffer->size(), 256u);
  EXPECT_EQ(pool.stats().bytes_owned, 320u);
  EXPECT_EQ(pool.stats().active_bytes, 320u);
  EXPECT_EQ(pool.stats().high_watermark_bytes, 320u);

  auto exhausted = pool.loan_frame(1, 1, 1, 1, "bytes");
  EXPECT_FALSE(exhausted.valid());
  EXPECT_EQ(pool.stats().exhausted_count, 1u);
  EXPECT_EQ(pool.stats().max_bytes, 320u);
}

TEST(Channel, BufferPoolDetachTransfersOwnershipOutOfPoolAccounting) {
  topoexec::BufferPoolConfig config;
  config.fixed_block_size = 64u;
  config.max_bytes = 64u;
  topoexec::BufferPool pool(config);
  auto loan = pool.loan_frame(32, 4, 4, 8, "gray8");
  ASSERT_TRUE(loan.valid());

  const auto frame = loan.detach();

  EXPECT_TRUE(frame.valid());
  const auto stats = pool.stats();
  EXPECT_EQ(stats.detached_count, 1u);
  EXPECT_EQ(stats.active_count, 0u);
  EXPECT_EQ(stats.bytes_owned, 0u);
  EXPECT_EQ(stats.bytes_available, 0u);
  EXPECT_FALSE(pool.has_outstanding_loans());
}

TEST(Payload, OpaqueCustomPayloadPreservesSchemaAddressAndSummary) {
  auto value = std::make_shared<const int>(42);
  auto payload = topoexec::make_custom_payload(value, "example.Answer", "answer");

  ASSERT_TRUE(topoexec::payload_is<topoexec::OpaquePayload>(payload));
  const auto opaque = topoexec::require_opaque_payload(payload);
  EXPECT_TRUE(opaque.valid());
  EXPECT_EQ(opaque.size_bytes, sizeof(int));
  EXPECT_EQ(opaque.debug_summary, "answer");
  EXPECT_EQ(topoexec::payload_address(payload), value.get());
}

TEST(Payload, DescribesBuiltInPayloadSchemaAndSize) {
  const auto text = topoexec::describe_payload_schema(topoexec::make_text_payload("hello"));
  EXPECT_EQ(text.type_name, "TextPayload");
  EXPECT_EQ(text.schema_id, topoexec::kTextPayloadSchema);
  EXPECT_EQ(text.summary, "hello");
  EXPECT_EQ(text.size_estimate, 5u);
  EXPECT_FALSE(text.large);

  auto buffer = std::make_shared<const topoexec::SharedBuffer>(128);
  const auto blob = topoexec::describe_payload_schema(topoexec::make_binary_blob_payload(buffer, 0, 64, "bytes"));
  EXPECT_EQ(blob.type_name, "BinaryBlobPayload");
  EXPECT_EQ(blob.schema_id, topoexec::kBinaryBlobPayloadSchema);
  EXPECT_EQ(blob.summary, "bytes");
  EXPECT_EQ(blob.size_estimate, 64u);
  EXPECT_TRUE(blob.large);

  auto value = std::make_shared<const int>(42);
  const auto opaque =
      topoexec::describe_payload_schema(topoexec::make_custom_payload(value, "example.Answer", "answer"));
  EXPECT_EQ(opaque.type_name, "OpaquePayload");
  EXPECT_EQ(opaque.schema_id, "example.Answer");
  EXPECT_EQ(opaque.summary, "answer");
  EXPECT_EQ(opaque.size_estimate, sizeof(int));
  EXPECT_TRUE(opaque.large);
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
