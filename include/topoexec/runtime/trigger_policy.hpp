#pragma once

// API stability: experimental. Trigger readiness internals may change before trigger v2.

#include "topoexec/runtime/channel.hpp"
#include "topoexec/runtime/graph.hpp"
#include "topoexec/runtime/scheduler.hpp"

#include <chrono>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace topoexec {

bool is_timer_event_source(const EventSourceSpec& source);
bool is_message_event_source(const EventSourceSpec& source);
bool is_manual_event_source(const EventSourceSpec& source);
bool has_timer_event_source(const ComponentNodeSpec& component);
bool has_message_event_source(const ComponentNodeSpec& component);
bool has_event_driven_source(const ComponentNodeSpec& component);
std::optional<int> timer_period_ms_for(const ComponentNodeSpec& component);
double event_task_hz_for(const ComponentNodeSpec& component);
std::vector<std::string> trigger_policy_inputs_for(const ComponentNodeSpec& component);
TriggerKind trigger_kind_for_policy(const TriggerPolicySpec& policy, EventKind event);

class TriggerPolicyEngine {
public:
  explicit TriggerPolicyEngine(RuntimeChannelBus* channels);

  Invocation timer_invocation_for(const TickContext& context, const ComponentNodeSpec& component,
                                  const SchedulerGroupConfig& lane) const;
  Invocation event_invocation_for(EventKind event, const TickContext& context, const ComponentNodeSpec& component,
                                  const SchedulerGroupConfig& lane) const;
  std::vector<Invocation> collect_ready_invocations(const TickContext& context, const ComponentNodeSpec& component,
                                                    const SchedulerGroupConfig& lane);
  TriggerRuntimeMetrics take_last_stats(const std::string& component_id);

private:
  using PendingMessages = std::map<std::string, std::deque<RuntimeChannelMessage>>;

  void drain_inputs(const ComponentNodeSpec& component, PendingMessages& pending);
  std::size_t prune_timed_out_messages(const ComponentNodeSpec& component, PendingMessages& pending,
                                       std::chrono::steady_clock::time_point now);
  bool rate_limited(const ComponentNodeSpec& component, std::chrono::steady_clock::time_point now) const;
  void record_invocation(const ComponentNodeSpec& component, std::chrono::steady_clock::time_point now);
  Invocation invocation_from_messages(EventKind event, TriggerKind trigger, const TickContext& context,
                                      const ComponentNodeSpec& component, const SchedulerGroupConfig& lane,
                                      const std::vector<std::pair<std::string, RuntimeChannelMessage>>& messages) const;
  std::vector<Invocation> collect_any_input(const TickContext& context, const ComponentNodeSpec& component,
                                            const SchedulerGroupConfig& lane, PendingMessages& pending);
  std::vector<Invocation> collect_coalesced_any_input(const TickContext& context, const ComponentNodeSpec& component,
                                                      const SchedulerGroupConfig& lane, PendingMessages& pending);
  std::vector<Invocation> collect_all_inputs(const TickContext& context, const ComponentNodeSpec& component,
                                             const SchedulerGroupConfig& lane, PendingMessages& pending);
  std::vector<Invocation> collect_time_sync(const TickContext& context, const ComponentNodeSpec& component,
                                            const SchedulerGroupConfig& lane, PendingMessages& pending,
                                            TriggerRuntimeMetrics& stats);
  std::vector<Invocation> collect_batch(const TickContext& context, const ComponentNodeSpec& component,
                                        const SchedulerGroupConfig& lane, PendingMessages& pending,
                                        TriggerRuntimeMetrics& stats);

  RuntimeChannelBus* channels_{nullptr};
  std::mutex mutex_;
  std::map<std::string, PendingMessages> pending_;
  std::map<std::string, std::chrono::steady_clock::time_point> last_invoked_;
  std::map<std::string, TriggerRuntimeMetrics> last_stats_;
};

} // namespace topoexec
