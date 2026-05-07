#pragma once

// API stability: experimental. Live observe transport/session APIs may change before beta.

#include "topoexec/runtime/live_event.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace topoexec::runtime_observe {

enum class LiveObserveLevel : std::uint8_t {
  kOff = 0,
  kSummary,
  kDetailed,
  kDebug,
};

struct LiveObserveOptions {
  LiveObserveLevel level{LiveObserveLevel::kOff};
  std::size_t event_buffer_capacity{1024};
  std::uint32_t stream_id{0};
};

class LiveEventRingBuffer {
public:
  explicit LiveEventRingBuffer(std::size_t capacity = 1024);

  [[nodiscard]] std::size_t capacity() const noexcept;
  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] bool try_push(const LiveEvent& event) noexcept;
  [[nodiscard]] bool try_pop(LiveEvent& event) noexcept;
  [[nodiscard]] std::uint64_t dropped_event_count() const noexcept;
  [[nodiscard]] std::uint64_t take_pending_drop_count() noexcept;

private:
  [[nodiscard]] std::size_t increment(std::size_t value) const noexcept;

  std::vector<LiveEvent> buffer_;
  std::size_t capacity_{0};
  std::atomic_size_t read_index_{0};
  std::atomic_size_t write_index_{0};
  std::atomic_uint64_t dropped_event_count_{0};
  std::atomic_uint64_t pending_drop_count_{0};
};

class LiveEventStream {
public:
  explicit LiveEventStream(std::uint32_t stream_id = 0, std::size_t capacity = 1024);

  [[nodiscard]] std::uint32_t stream_id() const noexcept;
  [[nodiscard]] std::size_t capacity() const noexcept;
  [[nodiscard]] bool try_publish(const LiveEvent& event) noexcept;
  [[nodiscard]] bool try_pop(LiveEvent& event) noexcept;
  [[nodiscard]] std::optional<LiveEvent> take_drop_summary(std::uint64_t mono_ns,
                                                           std::uint32_t affected_kind = 0) noexcept;
  [[nodiscard]] std::uint64_t dropped_event_count() const noexcept;
  [[nodiscard]] std::uint64_t next_local_seq() noexcept;

private:
  std::uint32_t stream_id_{0};
  LiveEventRingBuffer buffer_;
  std::atomic_uint64_t local_seq_{0};
};

class LiveObserveSession {
public:
  explicit LiveObserveSession(LiveObserveOptions options = {});

  [[nodiscard]] bool enabled() const noexcept;
  [[nodiscard]] LiveObserveLevel level() const noexcept;
  [[nodiscard]] bool try_publish(const LiveEvent& event) noexcept;
  [[nodiscard]] std::vector<LiveEvent> drain(std::size_t max_events = 0);
  [[nodiscard]] std::optional<LiveEvent> take_drop_summary(std::uint64_t mono_ns,
                                                           std::uint32_t affected_kind = 0) noexcept;
  [[nodiscard]] std::uint64_t dropped_event_count() const noexcept;

private:
  LiveObserveOptions options_;
  LiveEventStream stream_;
};

[[nodiscard]] std::uint64_t live_observe_steady_time_ns() noexcept;

} // namespace topoexec::runtime_observe
