#include "topoexec/runtime/channel.hpp"

#include "topoexec/common/trace.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace topoexec {
namespace {

std::string component_id_from_endpoint(const std::string& endpoint) {
  const auto dot = endpoint.rfind('.');
  if (dot == std::string::npos) {
    return endpoint;
  }
  return endpoint.substr(0, dot);
}

std::string port_name_from_endpoint(const std::string& endpoint) {
  const auto dot = endpoint.rfind('.');
  if (dot == std::string::npos || dot + 1 >= endpoint.size()) {
    return {};
  }
  return endpoint.substr(dot + 1);
}

std::string metadata_id(const std::string& channel_id, std::uint64_t sequence) {
  return channel_id + "#" + std::to_string(sequence);
}

InvocationMetadata metadata_for_source_endpoint(InvocationMetadata metadata, const std::string& source_endpoint) {
  if (metadata.source_component.empty()) {
    metadata.source_component = component_id_from_endpoint(source_endpoint);
  }
  if (metadata.source_port.empty()) {
    metadata.source_port = port_name_from_endpoint(source_endpoint);
  }
  return metadata;
}

void finalize_message_metadata(InvocationMetadata& metadata, const std::string& channel_id, std::uint64_t sequence,
                               const std::string& source_endpoint) {
  metadata = metadata_for_source_endpoint(std::move(metadata), source_endpoint);
  const auto id = metadata_id(channel_id, sequence);
  if (metadata.correlation_id.empty()) {
    metadata.correlation_id = id;
  }
  metadata.causation_id = id;
  if (metadata.transaction_id.empty()) {
    metadata.transaction_id = metadata.correlation_id;
  }
}

std::map<std::string, std::string> metadata_trace_attributes(std::map<std::string, std::string> attributes,
                                                             const InvocationMetadata& metadata) {
  if (!metadata.correlation_id.empty()) {
    attributes["correlation_id"] = metadata.correlation_id;
  }
  if (!metadata.causation_id.empty()) {
    attributes["causation_id"] = metadata.causation_id;
  }
  if (metadata.epoch_id != 0u) {
    attributes["epoch_id"] = std::to_string(metadata.epoch_id);
  }
  if (!metadata.transaction_id.empty()) {
    attributes["transaction_id"] = metadata.transaction_id;
  }
  if (!metadata.source_component.empty()) {
    attributes["source_component"] = metadata.source_component;
  }
  if (!metadata.source_port.empty()) {
    attributes["source_port"] = metadata.source_port;
  }
  if (!metadata.trigger_kind.empty()) {
    attributes["trigger_kind"] = metadata.trigger_kind;
  }
  return attributes;
}

ChannelType channel_type_from_policy(const EdgePolicySpec& policy) {
  if (policy.mode == "latest") {
    return ChannelType::kLatestOnly;
  }
  if (policy.mode == "latched") {
    return ChannelType::kLatchedSnapshot;
  }
  if (policy.mode == "previous_tick") {
    return ChannelType::kPreviousTick;
  }
  if (policy.mode == "barrier") {
    return ChannelType::kBarrier;
  }
  return ChannelType::kEveryMessage;
}

DropPolicy drop_policy_from_string(const std::string& value) {
  if (value == "overwrite") {
    return DropPolicy::kOverwrite;
  }
  if (value == "drop_oldest") {
    return DropPolicy::kDropOldest;
  }
  if (value == "drop_newest") {
    return DropPolicy::kDropNewest;
  }
  if (value == "block") {
    return DropPolicy::kBlockProducer;
  }
  if (value == "fail_fast" || value == "reject") {
    return DropPolicy::kFailFast;
  }
  return DropPolicy::kFailFast;
}

std::string drop_policy_name(DropPolicy policy) {
  switch (policy) {
  case DropPolicy::kOverwrite:
    return "overwrite";
  case DropPolicy::kDropOldest:
    return "drop_oldest";
  case DropPolicy::kDropNewest:
    return "drop_newest";
  case DropPolicy::kBlockProducer:
    return "block";
  case DropPolicy::kFailFast:
    return "fail_fast";
  }
  return "unknown";
}

CopyPolicy copy_policy_from_string(const std::string& value) {
  if (value == "shared_view") {
    return CopyPolicy::kSharedView;
  }
  if (value == "loaned_view") {
    return CopyPolicy::kLoanedView;
  }
  if (value == "move_only") {
    return CopyPolicy::kMoveOnly;
  }
  return CopyPolicy::kCopy;
}

TimestampDomain timestamp_domain_from_string(const std::string& value) {
  const auto parsed = parse_timestamp_domain_value(value);
  return parsed.value_or(TimestampDomain::kSteady);
}

bool is_latest_style(ChannelType type) {
  return type == ChannelType::kLatestOnly || type == ChannelType::kLatchedSnapshot ||
         type == ChannelType::kPreviousTick;
}

bool is_multi_reader(const std::string& readers) {
  return readers == "multi" || readers == "multiple";
}

std::string large_payload_copy_reason(const RuntimePayload& payload) {
  if (!payload.is_large_payload()) {
    return {};
  }
  return "cannot copy large payload schema " + payload.schema + "; use shared_view or loaned_view";
}

void record_trace_event(TraceCollector* trace, const std::string& name,
                        std::map<std::string, std::string> attributes = {}) {
  if (trace == nullptr) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  trace->add(SpanRecord{TraceId::generate(), name, now, now, std::move(attributes)});
}

} // namespace

RuntimeChannelPublishResult RuntimeChannelPublicationStage::stage(RuntimeChannelPublication publication) {
  std::lock_guard lock(mutex_);
  if (publication.id.empty()) {
    return {false, "publication id must not be empty"};
  }
  if (publication.payload == nullptr) {
    return {false, "publication payload must not be null"};
  }
  publications_.push_back(std::move(publication));
  return {true, {}};
}

std::vector<RuntimeChannelPublication> RuntimeChannelPublicationStage::snapshot() const {
  std::lock_guard lock(mutex_);
  return publications_;
}

RuntimeChannelBus::RuntimeChannelBus(const std::vector<EdgeSpec>& specs) {
  for (const auto& spec : specs) {
    ChannelState state;
    state.config.id = spec.id;
    state.config.type = channel_type_from_policy(spec.policy);
    state.config.capacity = static_cast<std::size_t>(std::max(1, spec.policy.capacity));
    state.config.drop_policy = drop_policy_from_string(spec.policy.overflow);
    state.config.emit_health_events = spec.policy.emit_health_events;
    state.config.lifespan = std::chrono::milliseconds(spec.policy.lifespan_ms);
    state.config.deadline = std::chrono::milliseconds(spec.policy.deadline_ms);
    state.config.timestamp_domain = timestamp_domain_from_string(spec.policy.timestamp_domain);
    state.config.copy_policy = copy_policy_from_string(spec.policy.copy_policy);
    state.config.readers = spec.policy.readers;
    state.from = spec.from;
    state.to = spec.to;
    state.metrics.channel_id = spec.id;
    channels_[spec.id] = state;
    source_to_channels_[spec.from].push_back(spec.id);
    component_to_channels_[component_id_from_endpoint(spec.to)].push_back(spec.id);
  }
}

bool RuntimeChannelBus::empty() const {
  std::lock_guard lock(mutex_);
  return channels_.empty();
}

bool RuntimeChannelBus::has_channel(const std::string& channel_id) const {
  std::lock_guard lock(mutex_);
  return channels_.count(channel_id) != 0u;
}

RuntimeChannelPublishResult RuntimeChannelBus::publish(const std::string& channel_id, RuntimePayload payload,
                                                       std::optional<EventTimestamp> event_timestamp) {
  return publish_with_metadata(channel_id, std::move(payload), {}, std::move(event_timestamp));
}

RuntimeChannelPublishResult RuntimeChannelBus::publish_with_metadata(const std::string& channel_id,
                                                                     RuntimePayload payload,
                                                                     InvocationMetadata metadata,
                                                                     std::optional<EventTimestamp> event_timestamp) {
  return publish_shared_with_metadata(channel_id, make_shared_payload(std::move(payload)), std::move(metadata),
                                      std::move(event_timestamp));
}

RuntimeChannelPublishResult RuntimeChannelBus::publish_shared(const std::string& channel_id, RuntimePayloadPtr payload,
                                                              std::optional<EventTimestamp> event_timestamp) {
  return publish_shared_with_metadata(channel_id, std::move(payload), {}, std::move(event_timestamp));
}

RuntimeChannelPublishResult
RuntimeChannelBus::publish_shared_with_metadata(const std::string& channel_id, RuntimePayloadPtr payload,
                                                InvocationMetadata metadata,
                                                std::optional<EventTimestamp> event_timestamp) {
  if (payload == nullptr) {
    return {false, "payload must not be null"};
  }
  std::lock_guard lock(mutex_);
  auto found = channels_.find(channel_id);
  if (found == channels_.end()) {
    return {false, "unknown channel: " + channel_id};
  }
  RuntimePayloadPtr payload_for_channel;
  bool copied = false;
  auto prepared = prepare_payload_for_state(found->second, std::move(payload), payload_for_channel, copied);
  if (!prepared.accepted) {
    return prepared;
  }
  return publish_to_state(found->second, std::move(payload_for_channel), std::move(event_timestamp), copied,
                          std::move(metadata));
}

RuntimeChannelPublishResult RuntimeChannelBus::publish_from(const std::string& source_endpoint, RuntimePayload payload,
                                                            std::optional<EventTimestamp> event_timestamp) {
  return publish_from_with_metadata(source_endpoint, std::move(payload), {}, std::move(event_timestamp));
}

RuntimeChannelPublishResult
RuntimeChannelBus::publish_from_with_metadata(const std::string& source_endpoint, RuntimePayload payload,
                                              InvocationMetadata metadata,
                                              std::optional<EventTimestamp> event_timestamp) {
  return publish_shared_from_with_metadata(source_endpoint, make_shared_payload(std::move(payload)),
                                           std::move(metadata), std::move(event_timestamp));
}

RuntimeChannelPublishResult RuntimeChannelBus::publish_shared_from(const std::string& source_endpoint,
                                                                   RuntimePayloadPtr payload,
                                                                   std::optional<EventTimestamp> event_timestamp) {
  return publish_shared_from_with_metadata(source_endpoint, std::move(payload), {}, std::move(event_timestamp));
}

RuntimeChannelPublishResult
RuntimeChannelBus::publish_shared_from_with_metadata(const std::string& source_endpoint, RuntimePayloadPtr payload,
                                                     InvocationMetadata metadata,
                                                     std::optional<EventTimestamp> event_timestamp) {
  if (payload == nullptr) {
    return {false, "payload must not be null"};
  }
  std::lock_guard lock(mutex_);
  const auto channels = source_to_channels_.find(source_endpoint);
  if (channels == source_to_channels_.end()) {
    return {false, "unknown source endpoint: " + source_endpoint};
  }
  RuntimeChannelPublishResult last{true, {}};
  for (const auto& channel_id : channels->second) {
    auto& state = channels_.at(channel_id);
    RuntimePayloadPtr payload_for_channel;
    bool copied = false;
    auto prepared = prepare_payload_for_state(state, payload, payload_for_channel, copied);
    if (!prepared.accepted) {
      return prepared;
    }
    last = publish_to_state(state, std::move(payload_for_channel), event_timestamp, copied,
                            metadata_for_source_endpoint(metadata, source_endpoint));
    if (!last.accepted) {
      return last;
    }
  }
  return last;
}

RuntimeChannelPublishResult
RuntimeChannelBus::publish_batch(const std::vector<RuntimeChannelPublication>& publications) {
  for (const auto& publication : publications) {
    RuntimeChannelPublishResult result;
    if (publication.target == RuntimeChannelPublishTarget::kChannel) {
      result = publish_shared_with_metadata(publication.id, publication.payload, publication.metadata,
                                            publication.event_timestamp);
    } else {
      result = publish_shared_from_with_metadata(publication.id, publication.payload, publication.metadata,
                                                 publication.event_timestamp);
    }
    if (!result.accepted) {
      return result;
    }
  }
  return {true, {}};
}

void RuntimeChannelBus::advance_epoch() {
  std::lock_guard lock(mutex_);
  bool updated = false;
  for (auto& [id, state] : channels_) {
    (void)id;
    if (state.config.type != ChannelType::kPreviousTick || !state.pending_previous_tick.has_value()) {
      continue;
    }
    state.latest = std::move(state.pending_previous_tick);
    state.pending_previous_tick.reset();
    state.metrics.depth = state.latest.has_value() ? 1u : 0u;
    state.metrics.max_depth = std::max(state.metrics.max_depth, state.metrics.depth);
    updated = true;
  }
  if (updated) {
    ++update_sequence_;
    update_available_.notify_all();
  }
}

RuntimeChannelReadResult RuntimeChannelBus::read_latest_for_reader(const std::string& channel_id,
                                                                   const std::string& reader_id) {
  std::lock_guard lock(mutex_);
  auto found = channels_.find(channel_id);
  if (found == channels_.end()) {
    return {false, std::nullopt, "unknown channel: " + channel_id};
  }
  if (!is_latest_style(found->second.config.type)) {
    return {false, std::nullopt, "channel is not latest-style: " + channel_id};
  }
  return {true, consume_latest_from_state(found->second, reader_id), {}};
}

RuntimeChannelReadResult RuntimeChannelBus::read_latest_update_for_component_port(const std::string& component_id,
                                                                                  const std::string& port_name) {
  std::lock_guard lock(mutex_);
  const auto ids = channel_ids_for_component_port(component_id, port_name);
  for (const auto& channel_id : ids) {
    auto& state = channels_.at(channel_id);
    if (!is_latest_style(state.config.type)) {
      continue;
    }
    auto message = consume_latest_from_state(state, component_id + "." + port_name);
    if (message.has_value()) {
      return {true, std::move(message), {}};
    }
  }
  return {true, std::nullopt, {}};
}

RuntimeChannelReadResult RuntimeChannelBus::peek_latest_for_component_port(const std::string& component_id,
                                                                           const std::string& port_name) {
  std::lock_guard lock(mutex_);
  const auto ids = channel_ids_for_component_port(component_id, port_name);
  for (const auto& channel_id : ids) {
    auto& state = channels_.at(channel_id);
    auto snapshot = snapshot_from_state(state, 1);
    if (!snapshot.empty()) {
      return {true, std::move(snapshot.front()), {}};
    }
  }
  return {true, std::nullopt, {}};
}

std::vector<RuntimeChannelMessage> RuntimeChannelBus::drain_for_reader(const std::string& channel_id,
                                                                       const std::string& reader_id,
                                                                       std::size_t max_batch) {
  std::lock_guard lock(mutex_);
  const auto found = channels_.find(channel_id);
  if (found == channels_.end()) {
    return {};
  }
  auto& state = found->second;
  if (is_latest_style(state.config.type)) {
    auto latest = consume_latest_from_state(state, reader_id);
    if (latest.has_value()) {
      std::vector<RuntimeChannelMessage> messages;
      messages.push_back(std::move(*latest));
      return messages;
    }
    return {};
  }
  return consume_from_state(state, reader_id, max_batch);
}

std::vector<RuntimeChannelMessage> RuntimeChannelBus::snapshot_for_component_port(const std::string& component_id,
                                                                                  const std::string& port_name,
                                                                                  std::size_t max_batch) {
  std::lock_guard lock(mutex_);
  std::vector<RuntimeChannelMessage> messages;
  const auto ids = channel_ids_for_component_port(component_id, port_name);
  for (const auto& channel_id : ids) {
    if (max_batch != 0u && messages.size() >= max_batch) {
      break;
    }
    const auto remaining = max_batch == 0u ? 0u : max_batch - messages.size();
    auto snapshot = snapshot_from_state(channels_.at(channel_id), remaining);
    for (auto& message : snapshot) {
      if (max_batch != 0u && messages.size() >= max_batch) {
        break;
      }
      messages.push_back(std::move(message));
    }
  }
  return messages;
}

std::vector<RuntimeChannelMessage> RuntimeChannelBus::drain_for_component_port(const std::string& component_id,
                                                                               const std::string& port_name,
                                                                               std::size_t max_batch) {
  std::lock_guard lock(mutex_);
  std::vector<RuntimeChannelMessage> messages;
  const auto ids = channel_ids_for_component_port(component_id, port_name);
  for (const auto& channel_id : ids) {
    auto& state = channels_.at(channel_id);
    if (is_latest_style(state.config.type)) {
      auto latest = consume_latest_from_state(state, component_id + "." + port_name);
      if (latest.has_value()) {
        messages.push_back(std::move(*latest));
      }
      continue;
    }
    if (max_batch != 0u && messages.size() >= max_batch) {
      break;
    }
    const auto remaining = max_batch == 0u ? 0u : max_batch - messages.size();
    auto drained = consume_from_state(state, component_id + "." + port_name, remaining);
    for (auto& message : drained) {
      if (max_batch != 0u && messages.size() >= max_batch) {
        break;
      }
      messages.push_back(std::move(message));
    }
  }
  return messages;
}

std::vector<RuntimeChannelMessage> RuntimeChannelBus::consume_for_component(const std::string& component_id) {
  std::lock_guard lock(mutex_);
  std::vector<RuntimeChannelMessage> messages;
  const auto found = component_to_channels_.find(component_id);
  if (found == component_to_channels_.end()) {
    return messages;
  }
  for (const auto& channel_id : found->second) {
    auto& state = channels_.at(channel_id);
    if (is_latest_style(state.config.type)) {
      auto latest = consume_latest_from_state(state, component_id);
      if (latest.has_value()) {
        messages.push_back(std::move(*latest));
      }
    } else {
      auto drained = consume_from_state(state, component_id);
      messages.insert(messages.end(), drained.begin(), drained.end());
    }
  }
  return messages;
}

RuntimeChannelMetrics RuntimeChannelBus::metrics(const std::string& channel_id) const {
  std::lock_guard lock(mutex_);
  const auto found = channels_.find(channel_id);
  if (found == channels_.end()) {
    RuntimeChannelMetrics metrics;
    metrics.channel_id = channel_id;
    return metrics;
  }
  return metrics_from_state(found->second);
}

std::vector<RuntimeChannelMetrics> RuntimeChannelBus::metrics_snapshot() const {
  std::lock_guard lock(mutex_);
  std::vector<RuntimeChannelMetrics> values;
  values.reserve(channels_.size());
  for (const auto& [id, state] : channels_) {
    (void)id;
    values.push_back(metrics_from_state(state));
  }
  return values;
}

void RuntimeChannelBus::set_health_event_sink(HealthEventSink* sink) {
  std::lock_guard lock(mutex_);
  health_events_ = sink;
}

std::uint64_t RuntimeChannelBus::update_sequence() const {
  std::lock_guard lock(mutex_);
  return update_sequence_;
}

bool RuntimeChannelBus::wait_for_update(std::uint64_t last_seen, std::chrono::milliseconds timeout,
                                        const std::function<bool()>& stop_requested) {
  std::unique_lock lock(mutex_);
  return update_available_.wait_for(
      lock, timeout, [&]() { return update_sequence_ != last_seen || (stop_requested && stop_requested()); });
}

RuntimeChannelPublishResult RuntimeChannelBus::prepare_payload_for_state(ChannelState& state, RuntimePayloadPtr source,
                                                                         RuntimePayloadPtr& payload_for_channel,
                                                                         bool& copied) {
  copied = false;
  if (state.config.copy_policy == CopyPolicy::kCopy) {
    const auto reason = large_payload_copy_reason(*source);
    if (!reason.empty()) {
      ++state.metrics.copy_fallback_count;
      state.metrics.degradation_reason = reason;
      return {false, reason};
    }
    payload_for_channel = make_shared_payload(copy_text_payload(*source));
    copied = true;
    return {true, {}};
  }
  payload_for_channel = std::move(source);
  return {true, {}};
}

RuntimeChannelPublishResult RuntimeChannelBus::publish_to_state(ChannelState& state, RuntimePayloadPtr payload,
                                                                std::optional<EventTimestamp> event_timestamp,
                                                                bool payload_was_copied, InvocationMetadata metadata) {
  const auto now = std::chrono::steady_clock::now();
  RuntimeChannelMessage message;
  message.channel_id = state.config.id;
  message.payload = std::move(payload);
  message.published_at = now;
  message.received_at = now;
  message.sequence = state.next_sequence++;
  message.event_timestamp = std::move(event_timestamp);
  finalize_message_metadata(metadata, state.config.id, message.sequence, state.from);
  message.metadata = std::move(metadata);

  if (payload_was_copied) {
    ++state.metrics.payload_copy_count;
  }

  auto accept_message = [&]() {
    if (state.config.type == ChannelType::kPreviousTick) {
      if (state.pending_previous_tick.has_value()) {
        ++state.metrics.drop_count;
        ++state.metrics.overwrite_count;
        ++state.metrics.health_event_count;
        emit_channel_health_event(state, HealthEventKind::kChannelOverflow, message.sequence,
                                  (state.latest.has_value() ? 1u : 0u) + 1u, "previous tick pending value overwritten");
      }
      state.pending_previous_tick = message;
      ++state.metrics.published_count;
      state.metrics.depth = (state.latest.has_value() ? 1u : 0u) + 1u;
      state.metrics.max_depth = std::max(state.metrics.max_depth, state.metrics.depth);
      return RuntimeChannelPublishResult{true, {}};
    }
    state.latest = message;
    if (is_latest_style(state.config.type)) {
      if (state.config.type == ChannelType::kLatestOnly && state.metrics.depth > 0u) {
        ++state.metrics.drop_count;
        ++state.metrics.overwrite_count;
        ++state.metrics.health_event_count;
        emit_channel_health_event(state, HealthEventKind::kChannelOverflow, message.sequence, 1u,
                                  "latest value overwritten");
      }
      state.queue.clear();
    } else {
      state.queue.push_back(message);
      while (state.queue.size() > state.config.capacity) {
        state.queue.pop_front();
        ++state.metrics.drop_count;
        ++state.metrics.overwrite_count;
        ++state.metrics.health_event_count;
        emit_channel_health_event(state, HealthEventKind::kChannelOverflow, message.sequence, state.queue.size(),
                                  "oldest queued payload dropped");
      }
    }
    ++state.metrics.published_count;
    state.metrics.depth =
        is_latest_style(state.config.type) ? (state.latest.has_value() ? 1u : 0u) : state.queue.size();
    state.metrics.max_depth = std::max(state.metrics.max_depth, state.metrics.depth);
    maybe_emit_high_watermark(state, message);
    ++update_sequence_;
    update_available_.notify_all();
    return RuntimeChannelPublishResult{true, {}};
  };

  if (!is_latest_style(state.config.type) && state.queue.size() >= state.config.capacity) {
    if (state.config.drop_policy == DropPolicy::kDropNewest) {
      ++state.metrics.drop_count;
      ++state.metrics.reject_count;
      ++state.metrics.health_event_count;
      emit_channel_health_event(state, HealthEventKind::kChannelOverflow, message.sequence, state.queue.size(),
                                "dropped newest payload");
      return {false, "dropped newest payload"};
    }
    if (state.config.drop_policy == DropPolicy::kBlockProducer) {
      ++state.metrics.reject_count;
      ++state.metrics.health_event_count;
      emit_channel_health_event(state, HealthEventKind::kChannelOverflow, message.sequence, state.queue.size(),
                                "would block producer");
      return {false, "would block producer"};
    }
    if (state.config.drop_policy == DropPolicy::kFailFast) {
      ++state.metrics.drop_count;
      ++state.metrics.reject_count;
      ++state.metrics.health_event_count;
      emit_channel_health_event(state, HealthEventKind::kChannelOverflow, message.sequence, state.queue.size(),
                                "channel capacity exceeded");
      return {false, "channel capacity exceeded"};
    }
  }

  return accept_message();
}

std::optional<RuntimeChannelMessage> RuntimeChannelBus::consume_latest_from_state(ChannelState& state,
                                                                                  const std::string& reader_id) {
  if (!state.latest.has_value()) {
    return std::nullopt;
  }
  const auto now = std::chrono::steady_clock::now();
  if (message_expired(state, *state.latest, now)) {
    const auto expired = *state.latest;
    state.latest.reset();
    mark_stale_drop(state, expired);
    state.metrics.depth = 0;
    return std::nullopt;
  }
  const auto last = state.delivered_latest_sequences.find(reader_id);
  if (last != state.delivered_latest_sequences.end() && last->second == state.latest->sequence) {
    return std::nullopt;
  }
  state.delivered_latest_sequences[reader_id] = state.latest->sequence;
  ++state.metrics.delivered_count;
  auto message = *state.latest;
  mark_delivery_metrics(state, message, now);
  return message;
}

std::vector<RuntimeChannelMessage>
RuntimeChannelBus::consume_from_state(ChannelState& state, const std::string& reader_id, std::size_t max_batch) {
  if (state.config.type == ChannelType::kBarrier && state.queue.size() < state.config.capacity) {
    return {};
  }
  const auto now = std::chrono::steady_clock::now();
  const bool multi_reader = is_multi_reader(state.config.readers);
  const std::uint64_t last_delivered = multi_reader && state.delivered_queue_sequences.count(reader_id) != 0u
                                           ? state.delivered_queue_sequences.at(reader_id)
                                           : 0u;
  std::vector<RuntimeChannelMessage> messages;
  messages.reserve(state.queue.size());
  std::deque<RuntimeChannelMessage> retained;
  for (auto& message : state.queue) {
    if (message_expired(state, message, now)) {
      mark_stale_drop(state, message);
      continue;
    }
    const bool already_delivered_to_reader = multi_reader && message.sequence <= last_delivered;
    const bool batch_full = max_batch != 0u && messages.size() >= max_batch;
    if (already_delivered_to_reader || batch_full) {
      retained.push_back(std::move(message));
      continue;
    }
    auto delivered = message;
    mark_delivery_metrics(state, delivered, now);
    if (multi_reader) {
      state.delivered_queue_sequences[reader_id] = delivered.sequence;
      retained.push_back(std::move(message));
    }
    messages.push_back(std::move(delivered));
  }
  state.metrics.delivered_count += messages.size();
  if (multi_reader || (max_batch != 0u && !retained.empty())) {
    state.queue = std::move(retained);
  } else {
    state.queue.clear();
  }
  state.metrics.depth = state.queue.size();
  return messages;
}

std::vector<RuntimeChannelMessage> RuntimeChannelBus::snapshot_from_state(ChannelState& state, std::size_t max_batch) {
  const auto now = std::chrono::steady_clock::now();
  std::vector<RuntimeChannelMessage> messages;
  if (is_latest_style(state.config.type)) {
    if (state.latest.has_value() && message_expired(state, *state.latest, now)) {
      const auto expired = *state.latest;
      state.latest.reset();
      mark_stale_drop(state, expired);
      state.metrics.depth = 0;
      return {};
    }
    if (state.latest.has_value()) {
      messages.push_back(*state.latest);
    }
    return messages;
  }
  std::deque<RuntimeChannelMessage> retained;
  for (auto& message : state.queue) {
    if (message_expired(state, message, now)) {
      mark_stale_drop(state, message);
      continue;
    }
    if (max_batch == 0u || messages.size() < max_batch) {
      messages.push_back(message);
    }
    retained.push_back(std::move(message));
  }
  state.queue = std::move(retained);
  state.metrics.depth = state.queue.size();
  return messages;
}

bool RuntimeChannelBus::message_expired(const ChannelState& state, const RuntimeChannelMessage& message,
                                        std::chrono::steady_clock::time_point now) const {
  return state.config.lifespan.count() > 0 && now - message.published_at > state.config.lifespan;
}

void RuntimeChannelBus::mark_delivery_metrics(ChannelState& state, RuntimeChannelMessage& message,
                                              std::chrono::steady_clock::time_point now) {
  const auto age = now - message.published_at;
  state.metrics.message_age_ms = std::chrono::duration<double, std::milli>(age).count();
  state.metrics.delivery_latency_ms = state.metrics.message_age_ms;
  if (state.config.deadline.count() > 0 && age > state.config.deadline) {
    message.deadline_missed = true;
    ++state.metrics.deadline_miss_count;
    ++state.metrics.health_event_count;
    state.metrics.degradation_reason = "deadline missed";
    emit_channel_health_event(state, HealthEventKind::kChannelDeadlineMiss, message.sequence, state.metrics.depth,
                              "deadline missed",
                              {{"age_ms", std::to_string(state.metrics.message_age_ms)},
                               {"deadline_ms", std::to_string(state.config.deadline.count())}});
  }
}

void RuntimeChannelBus::mark_stale_drop(ChannelState& state, const RuntimeChannelMessage& message) {
  ++state.metrics.drop_count;
  ++state.metrics.stale_drop_count;
  ++state.metrics.health_event_count;
  state.metrics.degradation_reason = "stale message expired";
  emit_channel_health_event(state, HealthEventKind::kChannelStaleDrop, message.sequence, state.metrics.depth,
                            "stale message expired", {{"lifespan_ms", std::to_string(state.config.lifespan.count())}});
}

void RuntimeChannelBus::emit_channel_health_event(const ChannelState& state, HealthEventKind kind,
                                                  std::uint64_t sequence, std::size_t depth, std::string reason,
                                                  std::map<std::string, std::string> attributes) {
  if (!state.config.emit_health_events || health_events_ == nullptr) {
    return;
  }
  HealthEvent event;
  event.kind = kind;
  event.source = "channel";
  event.channel_id = state.config.id;
  event.edge_id = state.config.id;
  event.policy = drop_policy_name(state.config.drop_policy);
  event.reason = std::move(reason);
  event.sequence = sequence;
  event.depth = depth;
  event.capacity = state.config.capacity;
  event.attributes = std::move(attributes);
  event.attributes["from"] = state.from;
  event.attributes["to"] = state.to;
  health_events_->emit(std::move(event));
}

void RuntimeChannelBus::maybe_emit_high_watermark(ChannelState& state, const RuntimeChannelMessage& message) {
  if (state.high_watermark_reported || is_latest_style(state.config.type) || state.config.capacity == 0u ||
      state.metrics.depth < state.config.capacity) {
    return;
  }
  state.high_watermark_reported = true;
  emit_channel_health_event(state, HealthEventKind::kBackpressureHighWatermark, message.sequence, state.metrics.depth,
                            "channel depth reached capacity");
}

RuntimeChannelMetrics RuntimeChannelBus::metrics_from_state(const ChannelState& state) const {
  auto metrics = state.metrics;
  if (state.config.type == ChannelType::kPreviousTick) {
    metrics.depth = (state.latest.has_value() ? 1u : 0u) + (state.pending_previous_tick.has_value() ? 1u : 0u);
  } else {
    metrics.depth = is_latest_style(state.config.type) ? (state.latest.has_value() ? 1u : 0u) : state.queue.size();
  }
  return metrics;
}

std::vector<std::string> RuntimeChannelBus::channel_ids_for_component_port(const std::string& component_id,
                                                                           const std::string& port_name) const {
  std::vector<std::string> ids;
  const auto found = component_to_channels_.find(component_id);
  if (found == component_to_channels_.end()) {
    return ids;
  }
  for (const auto& channel_id : found->second) {
    const auto& state = channels_.at(channel_id);
    if (port_name.empty() || port_name_from_endpoint(state.to) == port_name) {
      ids.push_back(channel_id);
    }
  }
  return ids;
}

RuntimePublicationRouter::RuntimePublicationRouter(RuntimeChannelBus* channels, const std::vector<EdgeSpec>& specs)
    : channels_(channels) {
  for (const auto& spec : specs) {
    source_to_edges_[spec.from].push_back(RoutedEdge{spec.id, component_id_from_endpoint(spec.from),
                                                     component_id_from_endpoint(spec.to), spec.kind,
                                                     spec.policy.max_inflight, spec.policy.overflow});
  }
}

void RuntimePublicationRouter::set_trace_collector(TraceCollector* trace) {
  trace_ = trace;
}

void RuntimePublicationRouter::begin_composite_region(const std::vector<std::string>& components) {
  std::lock_guard lock(mutex_);
  active_composite_components_.clear();
  active_composite_components_.insert(components.begin(), components.end());
  composite_external_stage_.clear();
}

std::size_t RuntimePublicationRouter::pending_async_count_locked(const std::string& channel_id) const {
  auto matches = [&](const RuntimeChannelPublication& publication) {
    return publication.kind == EdgeKind::kAsync && (channel_id.empty() || publication.id == channel_id);
  };
  auto count =
      static_cast<std::size_t>(std::count_if(deferred_next_epoch_.begin(), deferred_next_epoch_.end(), matches) +
                               std::count_if(deferred_ready_.begin(), deferred_ready_.end(), matches));
  count += static_cast<std::size_t>(
      std::count_if(composite_external_stage_.begin(), composite_external_stage_.end(), [&](const auto& staged) {
        return staged.kind == EdgeKind::kAsync && (channel_id.empty() || staged.publication.id == channel_id);
      }));
  return count;
}

bool RuntimePublicationRouter::drop_oldest_pending_async_locked(const std::string& channel_id) {
  auto matches = [&](const RuntimeChannelPublication& publication) {
    return publication.kind == EdgeKind::kAsync && publication.id == channel_id;
  };
  auto ready = std::find_if(deferred_ready_.begin(), deferred_ready_.end(), matches);
  if (ready != deferred_ready_.end()) {
    deferred_ready_.erase(ready);
    return true;
  }
  auto next = std::find_if(deferred_next_epoch_.begin(), deferred_next_epoch_.end(), matches);
  if (next != deferred_next_epoch_.end()) {
    deferred_next_epoch_.erase(next);
    return true;
  }
  auto staged = std::find_if(composite_external_stage_.begin(), composite_external_stage_.end(),
                             [&](const auto& item) { return matches(item.publication); });
  if (staged != composite_external_stage_.end()) {
    composite_external_stage_.erase(staged);
    return true;
  }
  return false;
}

RuntimeChannelPublishResult RuntimePublicationRouter::admit_async_locked(const RoutedEdge& edge) {
  if (edge.kind != EdgeKind::kAsync) {
    return {true, {}};
  }

  auto pending = pending_async_count_locked(edge.channel_id);
  if (edge.max_inflight > 0 && pending >= static_cast<std::size_t>(edge.max_inflight)) {
    if (edge.overflow == "drop_oldest" || edge.overflow == "overwrite") {
      if (drop_oldest_pending_async_locked(edge.channel_id)) {
        --pending;
        ++metrics_.async_admission_dropped_count;
      }
    } else if (edge.overflow == "drop_newest") {
      ++metrics_.async_admission_dropped_count;
      ++metrics_.async_admission_rejected_count;
      metrics_.async_in_flight_count = pending_async_count_locked();
      return {false, "async admission dropped newest for channel " + edge.channel_id};
    } else {
      ++metrics_.async_admission_rejected_count;
      metrics_.async_in_flight_count = pending_async_count_locked();
      return {false, "async admission max_inflight exceeded for channel " + edge.channel_id};
    }
  }

  ++metrics_.async_admission_accepted_count;
  metrics_.async_in_flight_count = pending + 1u;
  metrics_.async_max_in_flight_count = std::max(metrics_.async_max_in_flight_count, pending + 1u);
  return {true, {}};
}

RuntimeChannelPublishResult RuntimePublicationRouter::commit_composite_region_outputs() {
  std::vector<RuntimeChannelPublication> immediate_publications;
  {
    std::lock_guard lock(mutex_);
    for (auto& staged : composite_external_stage_) {
      if (staged.kind == EdgeKind::kImmediate) {
        immediate_publications.push_back(std::move(staged.publication));
      } else {
        deferred_next_epoch_.push_back(std::move(staged.publication));
      }
    }
    composite_external_stage_.clear();
    active_composite_components_.clear();
  }
  return commit_batch(std::move(immediate_publications));
}

RuntimeChannelPublishResult RuntimePublicationRouter::publish_from(const std::string& source_endpoint,
                                                                   RuntimePayload payload,
                                                                   std::optional<EventTimestamp> event_timestamp) {
  return publish_from_with_metadata(source_endpoint, std::move(payload), {}, std::move(event_timestamp));
}

RuntimeChannelPublishResult
RuntimePublicationRouter::publish_from_with_metadata(const std::string& source_endpoint, RuntimePayload payload,
                                                     InvocationMetadata metadata,
                                                     std::optional<EventTimestamp> event_timestamp) {
  return publish_shared_from_with_metadata(source_endpoint, make_shared_payload(std::move(payload)),
                                           std::move(metadata), std::move(event_timestamp));
}

RuntimeChannelPublishResult
RuntimePublicationRouter::publish_shared_from(const std::string& source_endpoint, RuntimePayloadPtr payload,
                                              std::optional<EventTimestamp> event_timestamp) {
  return publish_shared_from_with_metadata(source_endpoint, std::move(payload), {}, std::move(event_timestamp));
}

RuntimeChannelPublishResult
RuntimePublicationRouter::publish_shared_from_with_metadata(const std::string& source_endpoint,
                                                            RuntimePayloadPtr payload, InvocationMetadata metadata,
                                                            std::optional<EventTimestamp> event_timestamp) {
  if (payload == nullptr) {
    return {false, "payload must not be null"};
  }
  std::lock_guard lock(mutex_);
  const auto found = source_to_edges_.find(source_endpoint);
  if (found == source_to_edges_.end()) {
    return {false, "unknown source endpoint: " + source_endpoint};
  }
  for (const auto& edge : found->second) {
    auto edge_metadata = metadata_for_source_endpoint(metadata, source_endpoint);
    edge_metadata.source_component = edge.source_component;
    edge_metadata.source_port = port_name_from_endpoint(source_endpoint);
    record_trace_event(trace_, "channel_publish",
                       metadata_trace_attributes({{"channel_id", edge.channel_id},
                                                  {"source_component", edge.source_component},
                                                  {"source_port", edge_metadata.source_port},
                                                  {"target_component", edge.target_component},
                                                  {"edge_kind", to_string(edge.kind)}},
                                                 edge_metadata));
    RuntimeChannelPublication publication;
    publication.target = RuntimeChannelPublishTarget::kChannel;
    publication.id = edge.channel_id;
    publication.payload = payload;
    publication.kind = edge.kind;
    publication.event_timestamp = event_timestamp;
    publication.metadata = edge_metadata;
    if (edge.kind == EdgeKind::kAsync) {
      const auto admission = admit_async_locked(edge);
      record_trace_event(trace_, "async_admission",
                         metadata_trace_attributes({{"channel_id", edge.channel_id},
                                                    {"accepted", admission.accepted ? "true" : "false"},
                                                    {"max_inflight", std::to_string(edge.max_inflight)}},
                                                   edge_metadata));
      if (!admission.accepted) {
        return admission;
      }
    }
    if (!active_composite_components_.empty() && active_composite_components_.count(edge.source_component) != 0u &&
        active_composite_components_.count(edge.target_component) == 0u) {
      composite_external_stage_.push_back(StagedRoutedPublication{edge.kind, std::move(publication)});
      if (edge.kind == EdgeKind::kState) {
        ++metrics_.state_staged_count;
      } else if (edge.kind == EdgeKind::kAsync) {
        ++metrics_.async_staged_count;
      } else if (edge.kind == EdgeKind::kDelay) {
        ++metrics_.delayed_staged_count;
      } else {
        ++metrics_.immediate_staged_count;
      }
      ++metrics_.staged_count;
      continue;
    }
    if (edge.kind == EdgeKind::kDelay || edge.kind == EdgeKind::kState || edge.kind == EdgeKind::kAsync) {
      deferred_next_epoch_.push_back(std::move(publication));
      if (edge.kind == EdgeKind::kState) {
        ++metrics_.state_staged_count;
      } else if (edge.kind == EdgeKind::kAsync) {
        ++metrics_.async_staged_count;
      } else {
        ++metrics_.delayed_staged_count;
      }
    } else {
      immediate_stage_.push_back(std::move(publication));
      ++metrics_.immediate_staged_count;
    }
    ++metrics_.staged_count;
  }
  return {true, {}};
}

RuntimeChannelPublishResult RuntimePublicationRouter::begin_epoch() {
  if (channels_ != nullptr) {
    channels_->advance_epoch();
  }
  std::vector<RuntimeChannelPublication> ready;
  std::size_t async_ready_count = 0;
  {
    std::lock_guard lock(mutex_);
    ready.swap(deferred_ready_);
    async_ready_count = static_cast<std::size_t>(std::count_if(
        ready.begin(), ready.end(), [](const auto& publication) { return publication.kind == EdgeKind::kAsync; }));
  }
  auto result = commit_batch(std::move(ready));
  if (result.accepted && async_ready_count > 0u) {
    std::lock_guard lock(mutex_);
    metrics_.async_completion_count += async_ready_count;
    metrics_.async_in_flight_count = pending_async_count_locked();
  }
  return result;
}

RuntimeChannelPublishResult RuntimePublicationRouter::commit_immediate() {
  std::vector<RuntimeChannelPublication> ready;
  {
    std::lock_guard lock(mutex_);
    ready.swap(immediate_stage_);
  }
  return commit_batch(std::move(ready));
}

void RuntimePublicationRouter::end_epoch() {
  std::lock_guard lock(mutex_);
  deferred_ready_.insert(deferred_ready_.end(), deferred_next_epoch_.begin(), deferred_next_epoch_.end());
  deferred_next_epoch_.clear();
}

RuntimePublicationRouterMetrics RuntimePublicationRouter::metrics() const {
  std::lock_guard lock(mutex_);
  return metrics_;
}

RuntimeChannelPublishResult
RuntimePublicationRouter::commit_batch(std::vector<RuntimeChannelPublication> publications) {
  if (publications.empty()) {
    return {true, {}};
  }
  if (channels_ == nullptr) {
    std::lock_guard lock(mutex_);
    ++metrics_.failed_commit_count;
    return {false, "publication router has no channel bus"};
  }
  const auto result = channels_->publish_batch(publications);
  std::lock_guard lock(mutex_);
  if (result.accepted) {
    metrics_.committed_count += publications.size();
    for (std::size_t index = 0; index < publications.size(); ++index) {
      if (publications[index].kind == EdgeKind::kState) {
        ++metrics_.state_commit_count;
        record_trace_event(trace_, "state_commit",
                           metadata_trace_attributes({{"channel_id", publications[index].id},
                                                      {"edge_kind", to_string(publications[index].kind)}},
                                                     publications[index].metadata));
      }
      record_trace_event(trace_, "channel_commit",
                         metadata_trace_attributes({{"channel_id", publications[index].id},
                                                    {"edge_kind", to_string(publications[index].kind)}},
                                                   publications[index].metadata));
    }
  } else {
    ++metrics_.failed_commit_count;
  }
  return result;
}

} // namespace topoexec
