#pragma once

// API stability: experimental. Live observe numeric event records may change before beta.

#include <cstdint>
#include <type_traits>

namespace topoexec::runtime_observe {

#ifndef TOPOEXEC_ENABLE_LIVE_OBSERVE
#define TOPOEXEC_ENABLE_LIVE_OBSERVE 1
#endif

inline constexpr bool kLiveObserveCompiledIn = TOPOEXEC_ENABLE_LIVE_OBSERVE != 0;

enum class LiveEventKind : std::uint16_t {
  kRunStarted = 1,
  kRunFinished,
  kSchedulerEpochBegin,
  kSchedulerEpochEnd,
  kComponentBegin,
  kComponentEnd,
  kComponentError,
  kChannelPublishSummary,
  kChannelCommitSummary,
  kChannelDrop,
  kChannelReject,
  kChannelOverwrite,
  kTriggerReadySummary,
  kTriggerSuppressedSummary,
  kAsyncAdmission,
  kAsyncReject,
  kAsyncDrop,
  kLoopIterationBegin,
  kLoopIterationEnd,
  kLoopConverged,
  kLoopBudgetOverrun,
  kLoopMaxIterationsHit,
  kLoopError,
  kHealthEvent,
  kRuntimeError,
  kObserverDropSummary,
};

enum class LiveEventExactness : std::uint16_t {
  kExact = 1,
  kAggregated,
  kSampled,
  kLossy,
  kPartial,
};

enum class LiveEventSeverity : std::uint16_t {
  kTrace = 1,
  kDebug,
  kInfo,
  kWarning,
  kError,
};

struct alignas(64) LiveEvent {
  std::uint64_t local_seq{0};
  std::uint64_t mono_ns{0};
  std::uint32_t stream_id{0};
  std::uint32_t epoch_id{0};
  std::uint32_t kind{0};
  std::uint32_t flags{0};

  std::uint32_t lane_id{0};
  std::uint32_t worker_id{0};
  std::uint32_t component_id{0};
  std::uint32_t channel_id{0};

  std::uint32_t loop_id{0};
  std::uint32_t policy_id{0};
  std::uint32_t reason_id{0};
  std::uint32_t reserved0{0};

  std::uint64_t value0{0};
  std::uint64_t value1{0};
  std::uint64_t value2{0};
  std::uint64_t value3{0};
};

static_assert(std::is_trivially_copyable_v<LiveEvent>);
static_assert(alignof(LiveEvent) == 64);
static_assert(sizeof(LiveEvent) <= 128);

constexpr std::uint32_t encode_kind(LiveEventKind kind) noexcept {
  return static_cast<std::uint32_t>(kind);
}

constexpr std::uint32_t encode_exactness(LiveEventExactness exactness) noexcept {
  return static_cast<std::uint32_t>(exactness);
}

constexpr std::uint32_t encode_severity(LiveEventSeverity severity) noexcept {
  return static_cast<std::uint32_t>(severity);
}

} // namespace topoexec::runtime_observe
