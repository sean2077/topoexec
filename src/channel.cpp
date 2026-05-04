#include "topoexec/runtime/channel.hpp"

#include "topoexec/common/trace.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace topoexec {
namespace {

std::string component_id_from_endpoint(const std::string& endpoint) {
  const auto dot = endpoint.find('.');
  if (dot == std::string::npos) {
    return endpoint;
  }
  return endpoint.substr(0, dot);
}

std::string port_name_from_endpoint(const std::string& endpoint) {
  const auto dot = endpoint.find('.');
  if (dot == std::string::npos || dot + 1 >= endpoint.size()) {
    return {};
  }
  return endpoint.substr(dot + 1);
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
  if (value == "fail_fast") {
    return DropPolicy::kFailFast;
  }
  return DropPolicy::kFailFast;
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
    state.config.deadline = std::chrono::milliseconds(spec.policy.deadline_ms);
    state.config.timestamp_domain = timestamp_domain_from_string(spec.policy.timestamp_domain);
    state.config.copy_policy = copy_policy_from_string(spec.policy.copy_policy);
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
  return publish_shared(channel_id, make_shared_payload(std::move(payload)), std::move(event_timestamp));
}

RuntimeChannelPublishResult RuntimeChannelBus::publish_shared(const std::string& channel_id, RuntimePayloadPtr payload,
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
  return publish_to_state(found->second, std::move(payload_for_channel), std::move(event_timestamp), copied);
}

RuntimeChannelPublishResult RuntimeChannelBus::publish_from(const std::string& source_endpoint, RuntimePayload payload,
                                                            std::optional<EventTimestamp> event_timestamp) {
  return publish_shared_from(source_endpoint, make_shared_payload(std::move(payload)), std::move(event_timestamp));
}

RuntimeChannelPublishResult RuntimeChannelBus::publish_shared_from(const std::string& source_endpoint,
                                                                   RuntimePayloadPtr payload,
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
    last = publish_to_state(state, std::move(payload_for_channel), event_timestamp, copied);
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
      result = publish_shared(publication.id, publication.payload, publication.event_timestamp);
    } else {
      result = publish_shared_from(publication.id, publication.payload, publication.event_timestamp);
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
    if (state.latest.has_value()) {
      return {true, state.latest, {}};
    }
    if (!state.queue.empty()) {
      return {true, state.queue.back(), {}};
    }
  }
  return {true, std::nullopt, {}};
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
    auto drained = consume_from_state(state);
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
      auto drained = consume_from_state(state);
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
                                                                bool payload_was_copied) {
  const auto now = std::chrono::steady_clock::now();
  RuntimeChannelMessage message;
  message.channel_id = state.config.id;
  message.payload = std::move(payload);
  message.published_at = now;
  message.received_at = now;
  message.sequence = state.next_sequence++;
  message.event_timestamp = std::move(event_timestamp);

  if (payload_was_copied) {
    ++state.metrics.payload_copy_count;
  }

  auto accept_message = [&]() {
    if (state.config.type == ChannelType::kPreviousTick) {
      if (state.pending_previous_tick.has_value()) {
        ++state.metrics.drop_count;
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
      }
      state.queue.clear();
    } else {
      state.queue.push_back(message);
      while (state.queue.size() > state.config.capacity) {
        state.queue.pop_front();
        ++state.metrics.drop_count;
      }
    }
    ++state.metrics.published_count;
    state.metrics.depth =
        is_latest_style(state.config.type) ? (state.latest.has_value() ? 1u : 0u) : state.queue.size();
    state.metrics.max_depth = std::max(state.metrics.max_depth, state.metrics.depth);
    ++update_sequence_;
    update_available_.notify_all();
    return RuntimeChannelPublishResult{true, {}};
  };

  if (!is_latest_style(state.config.type) && state.queue.size() >= state.config.capacity) {
    if (state.config.drop_policy == DropPolicy::kDropNewest) {
      ++state.metrics.drop_count;
      return {false, "dropped newest payload"};
    }
    if (state.config.drop_policy == DropPolicy::kBlockProducer) {
      return {false, "would block producer"};
    }
    if (state.config.drop_policy == DropPolicy::kFailFast) {
      ++state.metrics.drop_count;
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
  const auto last = state.delivered_latest_sequences.find(reader_id);
  if (last != state.delivered_latest_sequences.end() && last->second == state.latest->sequence) {
    return std::nullopt;
  }
  state.delivered_latest_sequences[reader_id] = state.latest->sequence;
  ++state.metrics.delivered_count;
  return state.latest;
}

std::vector<RuntimeChannelMessage> RuntimeChannelBus::consume_from_state(ChannelState& state) {
  if (state.config.type == ChannelType::kBarrier && state.queue.size() < state.config.capacity) {
    return {};
  }
  std::vector<RuntimeChannelMessage> messages(state.queue.begin(), state.queue.end());
  state.metrics.delivered_count += messages.size();
  state.queue.clear();
  state.metrics.depth = 0;
  return messages;
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
    source_to_edges_[spec.from].push_back(
        RoutedEdge{spec.id, component_id_from_endpoint(spec.from), component_id_from_endpoint(spec.to), spec.kind});
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
  return publish_shared_from(source_endpoint, make_shared_payload(std::move(payload)), std::move(event_timestamp));
}

RuntimeChannelPublishResult
RuntimePublicationRouter::publish_shared_from(const std::string& source_endpoint, RuntimePayloadPtr payload,
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
    record_trace_event(trace_, "channel_publish",
                       {{"channel_id", edge.channel_id},
                        {"source_component", edge.source_component},
                        {"target_component", edge.target_component}});
    RuntimeChannelPublication publication;
    publication.target = RuntimeChannelPublishTarget::kChannel;
    publication.id = edge.channel_id;
    publication.payload = payload;
    publication.event_timestamp = event_timestamp;
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
  {
    std::lock_guard lock(mutex_);
    ready.swap(deferred_ready_);
  }
  return commit_batch(std::move(ready));
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
      record_trace_event(trace_, "channel_commit", {{"channel_id", publications[index].id}});
    }
  } else {
    ++metrics_.failed_commit_count;
  }
  return result;
}

} // namespace topoexec
