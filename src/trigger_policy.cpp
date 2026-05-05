#include "topoexec/runtime/trigger_policy.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace topoexec {

bool is_timer_event_source(const EventSourceSpec& source) {
  return source.type == "timer";
}

bool is_message_event_source(const EventSourceSpec& source) {
  return source.type == "message" || source.type == "request" || source.type == "task_ready" ||
         source.type == "future_ready";
}

bool is_manual_event_source(const EventSourceSpec& source) {
  return source.type == "manual";
}

bool has_timer_event_source(const ComponentNodeSpec& component) {
  return std::any_of(component.event_sources.begin(), component.event_sources.end(), is_timer_event_source);
}

bool has_message_event_source(const ComponentNodeSpec& component) {
  return std::any_of(component.event_sources.begin(), component.event_sources.end(), is_message_event_source);
}

bool has_event_driven_source(const ComponentNodeSpec& component) {
  return has_message_event_source(component) || has_timer_event_source(component);
}

EventKind event_kind_for_component(const ComponentNodeSpec& component) {
  if (component.trigger_policy.type == "request") {
    return EventKind::kRequest;
  }
  if (component.trigger_policy.type == "task_ready") {
    return EventKind::kTaskReady;
  }
  for (const auto& source : component.event_sources) {
    if (source.type == "request") {
      return EventKind::kRequest;
    }
    if (source.type == "task_ready") {
      return EventKind::kTaskReady;
    }
    if (source.type == "future_ready") {
      return EventKind::kFutureReady;
    }
  }
  return EventKind::kMessage;
}

std::optional<int> timer_period_ms_for(const ComponentNodeSpec& component) {
  for (const auto& source : component.event_sources) {
    if (source.type == "timer") {
      return source.period_ms;
    }
  }
  return std::nullopt;
}

double event_task_hz_for(const ComponentNodeSpec& component) {
  const auto period = timer_period_ms_for(component);
  if (!period.has_value() || *period <= 0) {
    return 0.0;
  }
  return 1000.0 / static_cast<double>(*period);
}

std::vector<std::string> trigger_policy_inputs_for(const ComponentNodeSpec& component) {
  if (!component.trigger_policy.inputs.empty()) {
    return component.trigger_policy.inputs;
  }
  std::vector<std::string> inputs;
  for (const auto& source : component.event_sources) {
    inputs.insert(inputs.end(), source.inputs.begin(), source.inputs.end());
  }
  return inputs;
}

TriggerKind trigger_kind_for_policy(const TriggerPolicySpec& policy, EventKind event) {
  if (event == EventKind::kTimer) {
    return TriggerKind::kOnTick;
  }
  if (policy.type == "all_inputs") {
    return TriggerKind::kAllInputs;
  }
  if (policy.type == "any_input" || policy.type == "on_event") {
    return TriggerKind::kAnyInput;
  }
  if (policy.type == "time_sync") {
    return TriggerKind::kTimeSync;
  }
  if (policy.type == "batch") {
    return TriggerKind::kBatch;
  }
  if (policy.type == "request") {
    return TriggerKind::kRequest;
  }
  if (policy.type == "task_ready") {
    return TriggerKind::kTaskReady;
  }
  return TriggerKind::kManual;
}

namespace {

using PendingMessageQueues = std::map<std::string, std::deque<RuntimeChannelMessage>>;

bool messages_have_comparable_timestamps(const std::vector<std::pair<std::string, RuntimeChannelMessage>>& messages) {
  if (messages.empty() || !messages.front().second.event_timestamp.has_value()) {
    return false;
  }
  const auto domain = messages.front().second.event_timestamp->domain;
  return std::all_of(messages.begin(), messages.end(), [domain](const auto& item) {
    return item.second.event_timestamp.has_value() && item.second.event_timestamp->domain == domain;
  });
}

std::int64_t timestamp_slop_ns(const TriggerPolicySpec& policy) {
  return static_cast<std::int64_t>(policy.sync_slop_ms) * 1000000LL;
}

std::optional<std::chrono::steady_clock::time_point> oldest_pending_batch_time(const std::vector<std::string>& inputs,
                                                                               PendingMessageQueues& pending) {
  std::optional<std::chrono::steady_clock::time_point> oldest;
  for (const auto& input : inputs) {
    auto& queue = pending[input];
    if (queue.empty()) {
      continue;
    }
    if (!oldest.has_value() || queue.front().received_at < *oldest) {
      oldest = queue.front().received_at;
    }
  }
  return oldest;
}

std::optional<std::vector<std::pair<std::string, RuntimeChannelMessage>>>
front_messages_for_inputs(const std::vector<std::string>& inputs, PendingMessageQueues& pending) {
  std::vector<std::pair<std::string, RuntimeChannelMessage>> messages;
  for (const auto& input : inputs) {
    auto& queue = pending[input];
    if (queue.empty()) {
      return std::nullopt;
    }
    messages.emplace_back(input, queue.front());
  }
  return messages;
}

void consume_front_messages(const std::vector<std::string>& inputs, PendingMessageQueues& pending) {
  for (const auto& input : inputs) {
    pending[input].pop_front();
  }
}

} // namespace

TriggerPolicyEngine::TriggerPolicyEngine(RuntimeChannelBus* channels) : channels_(channels) {}

TriggerRuntimeMetrics TriggerPolicyEngine::take_last_stats(const std::string& component_id) {
  std::lock_guard lock(mutex_);
  auto found = last_stats_.find(component_id);
  if (found == last_stats_.end()) {
    return {};
  }
  auto stats = found->second;
  found->second = TriggerRuntimeMetrics{};
  return stats;
}

Invocation TriggerPolicyEngine::timer_invocation_for(const TickContext& context, const ComponentNodeSpec& component,
                                                     const SchedulerGroupConfig& lane) const {
  Invocation invocation;
  invocation.event = EventKind::kTimer;
  invocation.trigger = TriggerKind::kOnTick;
  invocation.scheduled_at = context.scheduled_at;
  invocation.started_at = context.started_at;
  invocation.sequence = context.sequence;
  invocation.stop_requested = context.stop_requested;
  invocation.budget = std::chrono::milliseconds(component.execution.budget_ms);
  invocation.lane = lane.id;
  invocation.priority = component.execution.priority;
  return invocation;
}

Invocation TriggerPolicyEngine::event_invocation_for(EventKind event, const TickContext& context,
                                                     const ComponentNodeSpec& component,
                                                     const SchedulerGroupConfig& lane) const {
  Invocation invocation;
  invocation.event = event;
  invocation.trigger = trigger_kind_for_policy(component.trigger_policy, event);
  invocation.scheduled_at = context.scheduled_at;
  invocation.started_at = context.started_at;
  invocation.sequence = context.sequence;
  invocation.stop_requested = context.stop_requested;
  invocation.budget = std::chrono::milliseconds(component.execution.budget_ms);
  invocation.lane = lane.id;
  invocation.priority = component.execution.priority;
  return invocation;
}

std::vector<Invocation> TriggerPolicyEngine::collect_ready_invocations(const TickContext& context,
                                                                       const ComponentNodeSpec& component,
                                                                       const SchedulerGroupConfig& lane) {
  if (has_timer_event_source(component)) {
    return {timer_invocation_for(context, component, lane)};
  }
  if (!has_message_event_source(component)) {
    return {event_invocation_for(EventKind::kManual, context, component, lane)};
  }
  std::lock_guard lock(mutex_);
  auto& pending = pending_[component.id];
  auto& stats = last_stats_[component.id];
  stats = TriggerRuntimeMetrics{};
  drain_inputs(component, pending);
  stats.timeout_drop_count += prune_timed_out_messages(component, pending, context.started_at);
  if (rate_limited(component, context.started_at)) {
    return {};
  }
  if (component.trigger_policy.type == "all_inputs") {
    auto invocations = collect_all_inputs(context, component, lane, pending);
    if (!invocations.empty()) {
      record_invocation(component, context.started_at);
    }
    return invocations;
  }
  if (component.trigger_policy.type == "time_sync") {
    auto invocations = collect_time_sync(context, component, lane, pending, stats);
    if (!invocations.empty()) {
      record_invocation(component, context.started_at);
    }
    return invocations;
  }
  if (component.trigger_policy.type == "batch") {
    auto invocations = collect_batch(context, component, lane, pending, stats);
    if (!invocations.empty()) {
      record_invocation(component, context.started_at);
    }
    return invocations;
  }
  if (component.trigger_policy.coalesce) {
    auto invocations = collect_coalesced_any_input(context, component, lane, pending);
    if (!invocations.empty()) {
      record_invocation(component, context.started_at);
    }
    return invocations;
  }
  auto invocations = collect_any_input(context, component, lane, pending);
  if (!invocations.empty()) {
    record_invocation(component, context.started_at);
  }
  return invocations;
}

void TriggerPolicyEngine::drain_inputs(const ComponentNodeSpec& component, PendingMessages& pending) {
  if (channels_ == nullptr) {
    return;
  }
  for (const auto& input : trigger_policy_inputs_for(component)) {
    auto messages = channels_->drain_for_component_port(component.id, input);
    auto& queue = pending[input];
    for (auto& message : messages) {
      queue.push_back(std::move(message));
    }
  }
}

std::size_t TriggerPolicyEngine::prune_timed_out_messages(const ComponentNodeSpec& component, PendingMessages& pending,
                                                          std::chrono::steady_clock::time_point now) {
  if (component.trigger_policy.max_latency_ms <= 0) {
    return 0u;
  }
  const auto max_latency = std::chrono::milliseconds(component.trigger_policy.max_latency_ms);
  std::size_t dropped = 0;
  for (auto& [port, queue] : pending) {
    (void)port;
    std::deque<RuntimeChannelMessage> retained;
    while (!queue.empty()) {
      auto message = std::move(queue.front());
      queue.pop_front();
      if (now - message.received_at > max_latency) {
        ++dropped;
        continue;
      }
      retained.push_back(std::move(message));
    }
    queue = std::move(retained);
  }
  return dropped;
}

bool TriggerPolicyEngine::rate_limited(const ComponentNodeSpec& component,
                                       std::chrono::steady_clock::time_point now) const {
  if (component.trigger_policy.min_interval_ms <= 0) {
    return false;
  }
  const auto found = last_invoked_.find(component.id);
  if (found == last_invoked_.end()) {
    return false;
  }
  return now - found->second < std::chrono::milliseconds(component.trigger_policy.min_interval_ms);
}

void TriggerPolicyEngine::record_invocation(const ComponentNodeSpec& component,
                                            std::chrono::steady_clock::time_point now) {
  last_invoked_[component.id] = now;
}

Invocation TriggerPolicyEngine::invocation_from_messages(
    EventKind event, TriggerKind trigger, const TickContext& context, const ComponentNodeSpec& component,
    const SchedulerGroupConfig& lane,
    const std::vector<std::pair<std::string, RuntimeChannelMessage>>& messages) const {
  auto invocation = event_invocation_for(event, context, component, lane);
  invocation.trigger = trigger;
  for (const auto& [port, message] : messages) {
    if (invocation.payload == nullptr) {
      invocation.port = port;
      invocation.channel_id = message.channel_id;
      invocation.correlation_id = message.channel_id + "#" + std::to_string(message.sequence);
      invocation.payload = message.payload;
      invocation.received_at = message.received_at;
      invocation.published_at = message.published_at;
      invocation.deadline_missed = message.deadline_missed;
      invocation.event_timestamp = message.event_timestamp;
    }
    invocation.ready_inputs.push_back(port);
    invocation.payloads_by_port[port] = message.payload;
    invocation.batch_payloads.push_back(message.payload);
  }
  return invocation;
}

std::vector<Invocation> TriggerPolicyEngine::collect_any_input(const TickContext& context,
                                                               const ComponentNodeSpec& component,
                                                               const SchedulerGroupConfig& lane,
                                                               PendingMessages& pending) {
  std::vector<Invocation> invocations;
  const bool single_invocation = component.trigger_policy.min_interval_ms > 0;
  for (auto& [port, queue] : pending) {
    while (!queue.empty()) {
      auto message = queue.front();
      queue.pop_front();
      const auto event = event_kind_for_component(component);
      invocations.push_back(invocation_from_messages(event, trigger_kind_for_policy(component.trigger_policy, event),
                                                     context, component, lane, {{port, std::move(message)}}));
      if (single_invocation) {
        return invocations;
      }
    }
  }
  return invocations;
}

std::vector<Invocation> TriggerPolicyEngine::collect_coalesced_any_input(const TickContext& context,
                                                                         const ComponentNodeSpec& component,
                                                                         const SchedulerGroupConfig& lane,
                                                                         PendingMessages& pending) {
  std::vector<std::pair<std::string, RuntimeChannelMessage>> messages;
  for (auto& [port, queue] : pending) {
    if (queue.empty()) {
      continue;
    }
    messages.emplace_back(port, queue.back());
    queue.clear();
  }
  if (messages.empty()) {
    return {};
  }
  const auto event = event_kind_for_component(component);
  return {invocation_from_messages(event, trigger_kind_for_policy(component.trigger_policy, event), context, component,
                                   lane, messages)};
}

std::vector<Invocation> TriggerPolicyEngine::collect_all_inputs(const TickContext& context,
                                                                const ComponentNodeSpec& component,
                                                                const SchedulerGroupConfig& lane,
                                                                PendingMessages& pending) {
  const auto inputs = trigger_policy_inputs_for(component);
  auto messages = front_messages_for_inputs(inputs, pending);
  if (!messages.has_value()) {
    return {};
  }
  consume_front_messages(inputs, pending);
  return {invocation_from_messages(EventKind::kMessage,
                                   trigger_kind_for_policy(component.trigger_policy, EventKind::kMessage), context,
                                   component, lane, *messages)};
}

std::vector<Invocation> TriggerPolicyEngine::collect_time_sync(const TickContext& context,
                                                               const ComponentNodeSpec& component,
                                                               const SchedulerGroupConfig& lane,
                                                               PendingMessages& pending, TriggerRuntimeMetrics& stats) {
  const auto inputs = trigger_policy_inputs_for(component);
  const auto slop_ns = timestamp_slop_ns(component.trigger_policy);
  while (true) {
    auto messages = front_messages_for_inputs(inputs, pending);
    if (!messages.has_value()) {
      return {};
    }

    if (slop_ns <= 0 || !messages_have_comparable_timestamps(*messages)) {
      consume_front_messages(inputs, pending);
      return {
          invocation_from_messages(EventKind::kMessage, TriggerKind::kTimeSync, context, component, lane, *messages)};
    }

    auto min_item = messages->begin();
    auto max_item = messages->begin();
    for (auto item = messages->begin(); item != messages->end(); ++item) {
      if (item->second.event_timestamp->nanoseconds < min_item->second.event_timestamp->nanoseconds) {
        min_item = item;
      }
      if (item->second.event_timestamp->nanoseconds > max_item->second.event_timestamp->nanoseconds) {
        max_item = item;
      }
    }
    if (max_item->second.event_timestamp->nanoseconds - min_item->second.event_timestamp->nanoseconds <= slop_ns) {
      consume_front_messages(inputs, pending);
      return {
          invocation_from_messages(EventKind::kMessage, TriggerKind::kTimeSync, context, component, lane, *messages)};
    }
    pending[min_item->first].pop_front();
    ++stats.time_sync_drop_count;
  }
}

std::vector<Invocation> TriggerPolicyEngine::collect_batch(const TickContext& context,
                                                           const ComponentNodeSpec& component,
                                                           const SchedulerGroupConfig& lane, PendingMessages& pending,
                                                           TriggerRuntimeMetrics& stats) {
  const auto inputs = trigger_policy_inputs_for(component);
  const auto batch_size = component.trigger_policy.batch_size;
  std::size_t available = 0;
  for (const auto& input : inputs) {
    available += pending[input].size();
  }
  if (available == 0u) {
    return {};
  }
  std::size_t target_count = 0;
  if (batch_size > 0 && available >= static_cast<std::size_t>(batch_size)) {
    target_count = static_cast<std::size_t>(batch_size);
  } else if (component.trigger_policy.batch_window_ms > 0) {
    const auto oldest = oldest_pending_batch_time(inputs, pending);
    if (oldest.has_value() &&
        context.started_at - *oldest >= std::chrono::milliseconds(component.trigger_policy.batch_window_ms)) {
      target_count =
          batch_size > 0 ? std::min<std::size_t>(available, static_cast<std::size_t>(batch_size)) : available;
    }
  }
  if (target_count == 0u) {
    return {};
  }

  std::vector<std::pair<std::string, RuntimeChannelMessage>> messages;
  for (const auto& input : inputs) {
    auto& queue = pending[input];
    while (!queue.empty() && messages.size() < target_count) {
      messages.emplace_back(input, queue.front());
      queue.pop_front();
    }
  }
  if (messages.empty()) {
    return {};
  }
  ++stats.batch_flush_count;
  return {invocation_from_messages(EventKind::kMessage, TriggerKind::kBatch, context, component, lane, messages)};
}

} // namespace topoexec
