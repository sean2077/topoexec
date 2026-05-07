#include "topoexec/runtime/live_observe.hpp"

#include <gtest/gtest.h>

#include <type_traits>

namespace observe = topoexec::runtime_observe;

namespace {

observe::LiveEvent make_event(observe::LiveEventKind kind = observe::LiveEventKind::kComponentEnd) {
  observe::LiveEvent event;
  event.kind = observe::encode_kind(kind);
  event.flags = observe::encode_exactness(observe::LiveEventExactness::kExact);
  event.reason_id = observe::encode_severity(observe::LiveEventSeverity::kInfo);
  event.component_id = 7;
  event.value0 = 42;
  return event;
}

} // namespace

TEST(LiveObserve, LiveEventIsFixedSizeAndTriviallyCopyable) {
  EXPECT_TRUE(std::is_trivially_copyable_v<observe::LiveEvent>);
  EXPECT_EQ(alignof(observe::LiveEvent), 64u);
  EXPECT_LE(sizeof(observe::LiveEvent), 128u);
}

TEST(LiveObserve, RingBufferCapacityIsExactAndNonBlocking) {
  observe::LiveEventRingBuffer buffer(2);
  EXPECT_EQ(buffer.capacity(), 2u);

  EXPECT_TRUE(buffer.try_push(make_event()));
  EXPECT_TRUE(buffer.try_push(make_event(observe::LiveEventKind::kComponentBegin)));
  EXPECT_FALSE(buffer.try_push(make_event(observe::LiveEventKind::kRuntimeError)));
  EXPECT_EQ(buffer.dropped_event_count(), 1u);

  observe::LiveEvent first;
  observe::LiveEvent second;
  EXPECT_TRUE(buffer.try_pop(first));
  EXPECT_TRUE(buffer.try_pop(second));
  EXPECT_FALSE(buffer.try_pop(second));
  EXPECT_EQ(first.kind, observe::encode_kind(observe::LiveEventKind::kComponentEnd));
  EXPECT_EQ(second.kind, observe::encode_kind(observe::LiveEventKind::kComponentBegin));
}

TEST(LiveObserve, OverflowIncrementsCounterAndProducesDropSummary) {
  observe::LiveEventStream stream(3, 1);
  EXPECT_TRUE(stream.try_publish(make_event(observe::LiveEventKind::kChannelPublishSummary)));
  EXPECT_FALSE(stream.try_publish(make_event(observe::LiveEventKind::kChannelPublishSummary)));
  EXPECT_EQ(stream.dropped_event_count(), 1u);

  observe::LiveEvent stored;
  ASSERT_TRUE(stream.try_pop(stored));
  EXPECT_EQ(stored.local_seq, 1u);

  const auto summary =
      stream.take_drop_summary(99, observe::encode_kind(observe::LiveEventKind::kChannelPublishSummary));
  ASSERT_TRUE(summary.has_value());
  EXPECT_EQ(summary->stream_id, 3u);
  EXPECT_EQ(summary->local_seq, 3u);
  EXPECT_EQ(summary->mono_ns, 99u);
  EXPECT_EQ(summary->kind, observe::encode_kind(observe::LiveEventKind::kObserverDropSummary));
  EXPECT_EQ(summary->flags, observe::encode_exactness(observe::LiveEventExactness::kLossy));
  EXPECT_EQ(summary->reason_id, observe::encode_severity(observe::LiveEventSeverity::kWarning));
  EXPECT_EQ(summary->value0, 1u);
  EXPECT_EQ(summary->value1, 1u);
  EXPECT_EQ(summary->value2, observe::encode_kind(observe::LiveEventKind::kChannelPublishSummary));
  EXPECT_FALSE(stream.take_drop_summary(100).has_value());
}

TEST(LiveObserve, DisabledSessionProducesNoEvents) {
  observe::LiveObserveSession session;
  EXPECT_FALSE(session.enabled());
  EXPECT_FALSE(session.try_publish(make_event()));
  EXPECT_TRUE(session.drain().empty());
  EXPECT_FALSE(session.take_drop_summary(1).has_value());
  EXPECT_EQ(session.dropped_event_count(), 0u);
}

TEST(LiveObserve, EnabledSessionUsesStreamLocalSequences) {
  observe::LiveObserveOptions options;
  options.level = observe::LiveObserveLevel::kSummary;
  options.event_buffer_capacity = 2;
  options.stream_id = 9;
  observe::LiveObserveSession session(options);
  EXPECT_EQ(session.enabled(), observe::kLiveObserveCompiledIn);

  if (!observe::kLiveObserveCompiledIn) {
    GTEST_SKIP() << "live observe compiled out";
  }

  EXPECT_TRUE(session.try_publish(make_event(observe::LiveEventKind::kRunStarted)));
  EXPECT_TRUE(session.try_publish(make_event(observe::LiveEventKind::kRunFinished)));
  EXPECT_FALSE(session.try_publish(make_event(observe::LiveEventKind::kRuntimeError)));

  const auto events = session.drain();
  ASSERT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0].stream_id, 9u);
  EXPECT_EQ(events[0].local_seq, 1u);
  EXPECT_EQ(events[0].kind, observe::encode_kind(observe::LiveEventKind::kRunStarted));
  EXPECT_EQ(events[1].local_seq, 2u);
  EXPECT_EQ(session.dropped_event_count(), 1u);

  const auto summary = session.take_drop_summary(123, observe::encode_kind(observe::LiveEventKind::kRuntimeError));
  ASSERT_TRUE(summary.has_value());
  EXPECT_EQ(summary->local_seq, 4u);
  EXPECT_EQ(summary->value0, 1u);
}

TEST(LiveObserve, ZeroCapacityDropsWithoutRejectingRuntimeCaller) {
  observe::LiveEventStream stream(1, 0);
  EXPECT_FALSE(stream.try_publish(make_event()));
  EXPECT_FALSE(stream.try_publish(make_event()));
  EXPECT_EQ(stream.dropped_event_count(), 2u);
  const auto summary = stream.take_drop_summary(5);
  ASSERT_TRUE(summary.has_value());
  EXPECT_EQ(summary->value0, 2u);
  EXPECT_EQ(summary->value1, 2u);
}
