#pragma once

// API stability: experimental. Low-level channel and publication routing APIs may change before beta; prefer
// RuntimeRunner/GraphContext.

#include "topoexec/runtime/clock.hpp"
#include "topoexec/runtime/component.hpp"
#include "topoexec/runtime/graph.hpp"
#include "topoexec/runtime/health.hpp"
#include "topoexec/runtime/payload.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace topoexec {

class TraceCollector;

enum class RuntimeChannelPublishTarget {
  kChannel,
  kSourceEndpoint,
};

struct RuntimeChannelPublication {
  RuntimeChannelPublishTarget target{RuntimeChannelPublishTarget::kSourceEndpoint};
  std::string id;
  RuntimePayloadPtr payload;
  EdgeKind kind{EdgeKind::kImmediate};
  std::optional<EventTimestamp> event_timestamp;
  InvocationMetadata metadata;
};

class RuntimeChannelPublicationStage {
public:
  RuntimeChannelPublishResult stage(RuntimeChannelPublication publication);
  std::vector<RuntimeChannelPublication> snapshot() const;

private:
  mutable std::mutex mutex_;
  std::vector<RuntimeChannelPublication> publications_;
};

enum class ChannelType {
  kLatestOnly,
  kEveryMessage,
  kLatchedSnapshot,
  kPreviousTick,
  kBarrier,
};

enum class DropPolicy {
  kOverwrite,
  kDropOldest,
  kDropNewest,
  kBlockProducer,
  kFailFast,
};

enum class CopyPolicy {
  kCopy,
  kSharedView,
  kLoanedView,
  kMoveOnly,
};

struct ChannelConfig {
  std::string id;
  ChannelType type{ChannelType::kLatestOnly};
  std::size_t capacity{1};
  DropPolicy drop_policy{DropPolicy::kOverwrite};
  bool emit_health_events{true};
  std::chrono::milliseconds lifespan{0};
  std::chrono::milliseconds deadline{0};
  TimestampDomain timestamp_domain{TimestampDomain::kSteady};
  CopyPolicy copy_policy{CopyPolicy::kCopy};
  std::string readers{"single"};
};

struct RuntimeChannelMessage {
  std::string channel_id;
  RuntimePayloadPtr payload;
  std::chrono::steady_clock::time_point published_at;
  std::chrono::steady_clock::time_point received_at;
  std::uint64_t sequence{0};
  bool deadline_missed{false};
  std::optional<EventTimestamp> event_timestamp;
  InvocationMetadata metadata;
};

struct RuntimeChannelMetrics {
  std::string channel_id;
  std::size_t published_count{0};
  std::size_t delivered_count{0};
  std::size_t drop_count{0};
  std::size_t deadline_miss_count{0};
  std::size_t stale_drop_count{0};
  std::size_t reject_count{0};
  std::size_t overwrite_count{0};
  std::size_t health_event_count{0};
  std::size_t payload_copy_count{0};
  std::size_t copy_fallback_count{0};
  double message_age_ms{0.0};
  double delivery_latency_ms{0.0};
  std::size_t depth{0};
  std::size_t max_depth{0};
  std::string degradation_reason;
};

struct RuntimePublicationRouterMetrics {
  std::size_t staged_count{0};
  std::size_t immediate_staged_count{0};
  std::size_t delayed_staged_count{0};
  std::size_t state_staged_count{0};
  std::size_t state_commit_count{0};
  std::size_t async_staged_count{0};
  std::size_t async_admission_accepted_count{0};
  std::size_t async_admission_rejected_count{0};
  std::size_t async_admission_dropped_count{0};
  std::size_t async_admission_overwrite_count{0};
  std::size_t async_completion_count{0};
  std::size_t async_in_flight_count{0};
  std::size_t async_max_in_flight_count{0};
  std::size_t async_cancelled_count{0};
  std::size_t committed_count{0};
  std::size_t composite_discarded_count{0};
  std::size_t failed_commit_count{0};
};

struct RuntimeChannelReadResult {
  bool ok{false};
  std::optional<RuntimeChannelMessage> message;
  std::string reason;
};

/// @brief Low-level bounded channel bus used by EventRuntime and tests.
/// @ingroup topoexec_channel_api
class RuntimeChannelBus : public GraphOutputPublisher {
public:
  RuntimeChannelBus() = default;
  explicit RuntimeChannelBus(const std::vector<EdgeSpec>& specs);

  bool empty() const;
  bool has_channel(const std::string& channel_id) const;

  RuntimeChannelPublishResult publish(const std::string& channel_id, RuntimePayload payload,
                                      std::optional<EventTimestamp> event_timestamp = std::nullopt);
  RuntimeChannelPublishResult publish_with_metadata(const std::string& channel_id, RuntimePayload payload,
                                                    InvocationMetadata metadata,
                                                    std::optional<EventTimestamp> event_timestamp = std::nullopt);
  RuntimeChannelPublishResult publish_shared(const std::string& channel_id, RuntimePayloadPtr payload,
                                             std::optional<EventTimestamp> event_timestamp = std::nullopt);
  RuntimeChannelPublishResult
  publish_shared_with_metadata(const std::string& channel_id, RuntimePayloadPtr payload, InvocationMetadata metadata,
                               std::optional<EventTimestamp> event_timestamp = std::nullopt);
  RuntimeChannelPublishResult publish_from(const std::string& source_endpoint, RuntimePayload payload,
                                           std::optional<EventTimestamp> event_timestamp = std::nullopt) override;
  RuntimeChannelPublishResult
  publish_from_with_metadata(const std::string& source_endpoint, RuntimePayload payload, InvocationMetadata metadata,
                             std::optional<EventTimestamp> event_timestamp = std::nullopt) override;
  RuntimeChannelPublishResult
  publish_shared_from(const std::string& source_endpoint, RuntimePayloadPtr payload,
                      std::optional<EventTimestamp> event_timestamp = std::nullopt) override;
  RuntimeChannelPublishResult
  publish_shared_from_with_metadata(const std::string& source_endpoint, RuntimePayloadPtr payload,
                                    InvocationMetadata metadata,
                                    std::optional<EventTimestamp> event_timestamp = std::nullopt) override;
  RuntimeChannelPublishResult publish_batch(const std::vector<RuntimeChannelPublication>& publications);
  void advance_epoch();

  RuntimeChannelReadResult read_latest_for_reader(const std::string& channel_id, const std::string& reader_id);
  RuntimeChannelReadResult read_latest_update_for_component_port(const std::string& component_id,
                                                                 const std::string& port_name);
  RuntimeChannelReadResult peek_latest_for_component_port(const std::string& component_id,
                                                          const std::string& port_name);
  std::vector<RuntimeChannelMessage> drain_for_reader(const std::string& channel_id, const std::string& reader_id,
                                                      std::size_t max_batch = 0);
  std::vector<RuntimeChannelMessage>
  snapshot_for_component_port(const std::string& component_id, const std::string& port_name, std::size_t max_batch = 0);
  std::vector<RuntimeChannelMessage> drain_for_component_port(const std::string& component_id,
                                                              const std::string& port_name, std::size_t max_batch = 0);
  std::vector<RuntimeChannelMessage> consume_for_component(const std::string& component_id);

  RuntimeChannelMetrics metrics(const std::string& channel_id) const;
  std::vector<RuntimeChannelMetrics> metrics_snapshot() const;
  /// @brief Return the configured capacity for the first channel feeding a component input port.
  /// @ingroup topoexec_channel_api
  std::size_t configured_capacity_for_component_port(const std::string& component_id,
                                                     const std::string& port_name) const;
  void set_health_event_sink(HealthEventSink* sink);
  std::uint64_t update_sequence() const;
  bool wait_for_update(std::uint64_t last_seen, std::chrono::milliseconds timeout,
                       const std::function<bool()>& stop_requested);

private:
  struct ChannelState {
    ChannelConfig config;
    std::string from;
    std::string to;
    std::uint64_t next_sequence{1};
    std::optional<RuntimeChannelMessage> latest;
    std::optional<RuntimeChannelMessage> pending_previous_tick;
    std::deque<RuntimeChannelMessage> queue;
    std::map<std::string, std::uint64_t> delivered_latest_sequences;
    std::map<std::string, std::uint64_t> delivered_queue_sequences;
    RuntimeChannelMetrics metrics;
    bool high_watermark_reported{false};
  };
  struct PendingChannelHealthEvent {
    HealthEventSink* sink{nullptr};
    HealthEvent event;
  };

  RuntimeChannelPublishResult prepare_payload_for_state(ChannelState& state, RuntimePayloadPtr source,
                                                        RuntimePayloadPtr& payload_for_channel, bool& copied);
  RuntimeChannelPublishResult preflight_payload_for_state(ChannelState& state, const RuntimePayload& payload);
  RuntimeChannelPublishResult preflight_publish_to_state(ChannelState& state, std::size_t planned_publications);
  RuntimeChannelPublishResult publish_to_state(ChannelState& state, RuntimePayloadPtr payload,
                                               std::optional<EventTimestamp> event_timestamp, bool payload_was_copied,
                                               InvocationMetadata metadata);
  std::optional<RuntimeChannelMessage> consume_latest_from_state(ChannelState& state, const std::string& reader_id);
  std::vector<RuntimeChannelMessage> consume_from_state(ChannelState& state, const std::string& reader_id,
                                                        std::size_t max_batch = 0);
  std::vector<RuntimeChannelMessage> snapshot_from_state(ChannelState& state, std::size_t max_batch = 0);
  bool message_expired(const ChannelState& state, const RuntimeChannelMessage& message,
                       std::chrono::steady_clock::time_point now) const;
  void mark_delivery_metrics(ChannelState& state, RuntimeChannelMessage& message,
                             std::chrono::steady_clock::time_point now);
  void mark_stale_drop(ChannelState& state, const RuntimeChannelMessage& message);
  void emit_channel_health_event(const ChannelState& state, HealthEventKind kind, std::uint64_t sequence,
                                 std::size_t depth, std::string reason,
                                 std::map<std::string, std::string> attributes = {});
  std::vector<PendingChannelHealthEvent> drain_pending_health_events_locked();
  static void emit_pending_health_events(std::vector<PendingChannelHealthEvent> events);
  void maybe_emit_high_watermark(ChannelState& state, const RuntimeChannelMessage& message);
  RuntimeChannelMetrics metrics_from_state(const ChannelState& state) const;
  std::vector<std::string> channel_ids_for_component_port(const std::string& component_id,
                                                          const std::string& port_name) const;

  std::map<std::string, ChannelState> channels_;
  std::map<std::string, std::vector<std::string>> source_to_channels_;
  std::map<std::string, std::vector<std::string>> component_to_channels_;
  mutable std::mutex mutex_;
  std::condition_variable update_available_;
  std::uint64_t update_sequence_{0};
  HealthEventSink* health_events_{nullptr};
  std::vector<PendingChannelHealthEvent> pending_health_events_;
};

/// @brief Routes GraphContext publications across immediate, deferred, async, and CompositeLoop visibility boundaries.
/// @ingroup topoexec_channel_api
class RuntimePublicationRouter : public GraphOutputPublisher {
public:
  RuntimePublicationRouter(RuntimeChannelBus* channels, const std::vector<EdgeSpec>& specs);
  void set_trace_collector(TraceCollector* trace);
  void begin_composite_region(const std::vector<std::string>& components);
  RuntimeChannelPublishResult commit_composite_region_outputs();
  void discard_composite_region_outputs();

  RuntimeChannelPublishResult publish_from(const std::string& source_endpoint, RuntimePayload payload,
                                           std::optional<EventTimestamp> event_timestamp = std::nullopt) override;
  RuntimeChannelPublishResult
  publish_from_with_metadata(const std::string& source_endpoint, RuntimePayload payload, InvocationMetadata metadata,
                             std::optional<EventTimestamp> event_timestamp = std::nullopt) override;
  RuntimeChannelPublishResult
  publish_shared_from(const std::string& source_endpoint, RuntimePayloadPtr payload,
                      std::optional<EventTimestamp> event_timestamp = std::nullopt) override;
  RuntimeChannelPublishResult
  publish_shared_from_with_metadata(const std::string& source_endpoint, RuntimePayloadPtr payload,
                                    InvocationMetadata metadata,
                                    std::optional<EventTimestamp> event_timestamp = std::nullopt) override;

  RuntimeChannelPublishResult begin_epoch();
  RuntimeChannelPublishResult commit_immediate();
  void end_epoch();
  RuntimePublicationRouterMetrics metrics() const;

private:
  struct RoutedEdge {
    std::string channel_id;
    std::string source_component;
    std::string target_component;
    EdgeKind kind{EdgeKind::kImmediate};
    int max_inflight{0};
    std::string overflow;
  };

  struct StagedRoutedPublication {
    EdgeKind kind{EdgeKind::kImmediate};
    RuntimeChannelPublication publication;
  };

  RuntimeChannelPublishResult commit_batch(std::vector<RuntimeChannelPublication> publications);
  RuntimeChannelPublishResult admit_async_locked(const RoutedEdge& edge);
  std::size_t pending_async_count_locked(const std::string& channel_id = {}) const;
  bool drop_oldest_pending_async_locked(const std::string& channel_id);

  RuntimeChannelBus* channels_{nullptr};
  TraceCollector* trace_{nullptr};
  std::map<std::string, std::vector<RoutedEdge>> source_to_edges_;
  std::set<std::string> active_composite_components_;
  std::vector<StagedRoutedPublication> composite_external_stage_;
  std::vector<RuntimeChannelPublication> immediate_stage_;
  std::vector<RuntimeChannelPublication> deferred_next_epoch_;
  std::vector<RuntimeChannelPublication> deferred_ready_;
  RuntimePublicationRouterMetrics metrics_;
  mutable std::mutex mutex_;
};

} // namespace topoexec
