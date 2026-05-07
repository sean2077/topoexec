#include "topoexec/runtime/live_observe.hpp"

#include <chrono>

namespace topoexec::runtime_observe {

LiveEventRingBuffer::LiveEventRingBuffer(std::size_t capacity)
    : buffer_(capacity == 0 ? 0 : capacity + 1), capacity_(capacity) {}

std::size_t LiveEventRingBuffer::capacity() const noexcept {
  return capacity_;
}

bool LiveEventRingBuffer::empty() const noexcept {
  return read_index_.load(std::memory_order_acquire) == write_index_.load(std::memory_order_acquire);
}

bool LiveEventRingBuffer::try_push(const LiveEvent& event) noexcept {
  if (capacity_ == 0) {
    dropped_event_count_.fetch_add(1, std::memory_order_relaxed);
    pending_drop_count_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }

  const auto write_index = write_index_.load(std::memory_order_relaxed);
  const auto next_write_index = increment(write_index);
  if (next_write_index == read_index_.load(std::memory_order_acquire)) {
    dropped_event_count_.fetch_add(1, std::memory_order_relaxed);
    pending_drop_count_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }

  buffer_[write_index] = event;
  write_index_.store(next_write_index, std::memory_order_release);
  return true;
}

bool LiveEventRingBuffer::try_pop(LiveEvent& event) noexcept {
  if (capacity_ == 0) {
    return false;
  }

  const auto read_index = read_index_.load(std::memory_order_relaxed);
  if (read_index == write_index_.load(std::memory_order_acquire)) {
    return false;
  }

  event = buffer_[read_index];
  read_index_.store(increment(read_index), std::memory_order_release);
  return true;
}

std::uint64_t LiveEventRingBuffer::dropped_event_count() const noexcept {
  return dropped_event_count_.load(std::memory_order_relaxed);
}

std::uint64_t LiveEventRingBuffer::take_pending_drop_count() noexcept {
  return pending_drop_count_.exchange(0, std::memory_order_relaxed);
}

std::size_t LiveEventRingBuffer::increment(std::size_t value) const noexcept {
  return value + 1 == buffer_.size() ? 0 : value + 1;
}

LiveEventStream::LiveEventStream(std::uint32_t stream_id, std::size_t capacity)
    : stream_id_(stream_id), buffer_(capacity) {}

std::uint32_t LiveEventStream::stream_id() const noexcept {
  return stream_id_;
}

std::size_t LiveEventStream::capacity() const noexcept {
  return buffer_.capacity();
}

bool LiveEventStream::try_publish(const LiveEvent& event) noexcept {
  LiveEvent queued = event;
  queued.stream_id = stream_id_;
  queued.local_seq = next_local_seq();
  return buffer_.try_push(queued);
}

bool LiveEventStream::try_pop(LiveEvent& event) noexcept {
  return buffer_.try_pop(event);
}

std::optional<LiveEvent> LiveEventStream::take_drop_summary(std::uint64_t mono_ns,
                                                            std::uint32_t affected_kind) noexcept {
  const auto pending_drops = buffer_.take_pending_drop_count();
  if (pending_drops == 0) {
    return std::nullopt;
  }

  LiveEvent event;
  event.local_seq = next_local_seq();
  event.mono_ns = mono_ns;
  event.stream_id = stream_id_;
  event.kind = encode_kind(LiveEventKind::kObserverDropSummary);
  event.flags = encode_exactness(LiveEventExactness::kLossy);
  event.reason_id = encode_severity(LiveEventSeverity::kWarning);
  event.value0 = pending_drops;
  event.value1 = buffer_.dropped_event_count();
  event.value2 = affected_kind;
  return event;
}

std::uint64_t LiveEventStream::dropped_event_count() const noexcept {
  return buffer_.dropped_event_count();
}

std::uint64_t LiveEventStream::next_local_seq() noexcept {
  return local_seq_.fetch_add(1, std::memory_order_relaxed) + 1;
}

LiveObserveSession::LiveObserveSession(LiveObserveOptions options)
    : options_(options), stream_(options.stream_id, options.event_buffer_capacity) {}

bool LiveObserveSession::enabled() const noexcept {
  return kLiveObserveCompiledIn && options_.level != LiveObserveLevel::kOff;
}

LiveObserveLevel LiveObserveSession::level() const noexcept {
  return options_.level;
}

bool LiveObserveSession::try_publish(const LiveEvent& event) noexcept {
  if (!enabled()) {
    return false;
  }
  return stream_.try_publish(event);
}

std::vector<LiveEvent> LiveObserveSession::drain(std::size_t max_events) {
  std::vector<LiveEvent> events;
  if (!enabled()) {
    return events;
  }

  if (max_events != 0) {
    events.reserve(max_events);
  }

  LiveEvent event;
  while ((max_events == 0 || events.size() < max_events) && stream_.try_pop(event)) {
    events.push_back(event);
  }
  return events;
}

std::optional<LiveEvent> LiveObserveSession::take_drop_summary(std::uint64_t mono_ns,
                                                               std::uint32_t affected_kind) noexcept {
  if (!enabled()) {
    return std::nullopt;
  }
  return stream_.take_drop_summary(mono_ns, affected_kind);
}

std::uint64_t LiveObserveSession::dropped_event_count() const noexcept {
  if (!enabled()) {
    return 0;
  }
  return stream_.dropped_event_count();
}

std::uint64_t live_observe_steady_time_ns() noexcept {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

} // namespace topoexec::runtime_observe
