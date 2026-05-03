#include "topoexec/runtime/trigger_policy.hpp"

#include <algorithm>
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

TriggerPolicyEngine::TriggerPolicyEngine(RuntimeChannelBus* channels) : channels_(channels) {}

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
  drain_inputs(component, pending);
  if (rate_limited(component, context.started_at)) {
    return {};
  }
  if (component.trigger_policy.type == "all_inputs" || component.trigger_policy.type == "time_sync") {
    auto invocations = collect_all_inputs(context, component, lane, pending);
    if (!invocations.empty()) {
      record_invocation(component, context.started_at);
    }
    return invocations;
  }
  if (component.trigger_policy.type == "batch") {
    auto invocations = collect_batch(context, component, lane, pending);
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

void TriggerPolicyEngine::record_invocation(const ComponentNodeSpec& component, std::chrono::steady_clock::time_point now) {
  last_invoked_[component.id] = now;
}

Invocation TriggerPolicyEngine::invocation_from_messages(
    EventKind event, TriggerKind trigger, const TickContext& context, const ComponentNodeSpec& component,
    const SchedulerGroupConfig& lane, const std::vector<std::pair<std::string, RuntimeChannelMessage>>& messages) const {
  auto invocation = event_invocation_for(event, context, component, lane);
  invocation.trigger = trigger;
  for (const auto& [port, message] : messages) {
    if (invocation.payload == nullptr) {
      invocation.port = port;
      invocation.channel_id = message.channel_id;
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
  std::vector<std::pair<std::string, RuntimeChannelMessage>> messages;
  for (const auto& input : inputs) {
    auto& queue = pending[input];
    if (queue.empty()) {
      return {};
    }
    messages.emplace_back(input, queue.front());
  }
  for (const auto& input : inputs) {
    pending[input].pop_front();
  }
  return {invocation_from_messages(EventKind::kMessage, trigger_kind_for_policy(component.trigger_policy, EventKind::kMessage),
                                   context, component, lane, messages)};
}

std::vector<Invocation> TriggerPolicyEngine::collect_batch(const TickContext& context, const ComponentNodeSpec& component,
                                                           const SchedulerGroupConfig& lane, PendingMessages& pending) {
  const auto inputs = trigger_policy_inputs_for(component);
  const auto batch_size = component.trigger_policy.batch_size <= 0 ? 1 : component.trigger_policy.batch_size;
  std::size_t available = 0;
  for (const auto& input : inputs) {
    available += pending[input].size();
  }
  if (available < static_cast<std::size_t>(batch_size)) {
    return {};
  }
  std::vector<std::pair<std::string, RuntimeChannelMessage>> messages;
  for (const auto& input : inputs) {
    auto& queue = pending[input];
    while (!queue.empty() && static_cast<int>(messages.size()) < batch_size) {
      messages.emplace_back(input, queue.front());
      queue.pop_front();
    }
  }
  if (messages.empty() || static_cast<int>(messages.size()) < batch_size) {
    return {};
  }
  return {invocation_from_messages(EventKind::kMessage, TriggerKind::kBatch, context, component, lane, messages)};
}

}  // namespace topoexec
