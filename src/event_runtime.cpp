#include "topoexec/runtime/event_runtime.hpp"

#include "topoexec/runtime/trigger_policy.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <map>
#include <thread>

namespace topoexec {
namespace {

std::map<std::string, SchedulerGroupConfig> lane_map(const std::vector<EventRuntimeComponent>& components) {
  std::map<std::string, SchedulerGroupConfig> lanes;
  for (const auto& component : components) {
    lanes[component.lane.id] = component.lane;
  }
  return lanes;
}

}  // namespace

EventRuntime::EventRuntime(RuntimeChannelBus* channels) : channels_(channels) {}
EventRuntime::EventRuntime(RuntimeChannelBus* channels, GraphCompiledPlan compiled_plan)
    : channels_(channels), compiled_plan_(std::move(compiled_plan)) {}

void EventRuntime::add_component(EventRuntimeComponent component) {
  components_.push_back(std::move(component));
}

void EventRuntime::set_compiled_plan(GraphCompiledPlan compiled_plan) {
  compiled_plan_ = std::move(compiled_plan);
}

SchedulerRunResult EventRuntime::run(const SchedulerRunOptions& options) {
  SchedulerRunResult result;
  result.stop_reason = SchedulerStopReason::kTickBound;
  TriggerPolicyEngine trigger(channels_);
  const auto started_at = std::chrono::steady_clock::now();
  const auto lanes = lane_map(components_);
  const std::size_t iterations = options.tick_iterations == 0u ? 1u : options.tick_iterations;

  std::vector<std::string> order;
  if (!compiled_plan_.region_order.empty()) {
    for (const auto& region_id : compiled_plan_.region_order) {
      for (const auto& region : compiled_plan_.regions) {
        if (region.id == region_id) {
          order.insert(order.end(), region.components.begin(), region.components.end());
        }
      }
    }
  }
  if (order.empty()) {
    for (const auto& component : components_) {
      order.push_back(component.id);
    }
  }

  for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
    if (options.stop_token.stop_requested()) {
      result.stop_reason = SchedulerStopReason::kStopRequested;
      break;
    }
    if (options.run_duration_ms > 0u && std::chrono::steady_clock::now() - started_at >=
                                             std::chrono::milliseconds(options.run_duration_ms)) {
      result.stop_reason = SchedulerStopReason::kDurationBound;
      break;
    }
    for (const auto& component_id : order) {
      auto found = std::find_if(components_.begin(), components_.end(),
                                [&component_id](const auto& item) { return item.id == component_id; });
      if (found == components_.end()) {
        continue;
      }
      const auto now = std::chrono::steady_clock::now();
      TickContext tick;
      tick.sequence = result.iterations + 1u;
      tick.scheduled_at = now;
      tick.started_at = now;
      tick.stop_requested = [token = options.stop_token]() { return token.stop_requested(); };
      try {
        auto invocations = trigger.collect_ready_invocations(tick, found->spec, found->lane);
        for (const auto& invocation : invocations) {
          found->component->execute(invocation, *found->context);
          ++result.tick_calls;
          if (options.max_recorded_ticked_tasks == 0u ||
              result.ticked_tasks.size() < options.max_recorded_ticked_tasks) {
            result.ticked_tasks.push_back(found->id);
          }
          SchedulerMetrics metrics;
          metrics.completed_count = 1;
          result.group_metrics[found->lane.id].completed_count += 1;
        }
      } catch (const std::exception& error) {
        result.ok = false;
        result.stop_reason = SchedulerStopReason::kError;
        result.errors.push_back("component " + found->id + " failed: " + error.what());
        return result;
      }
    }
    ++result.iterations;
    if (options.after_iteration) {
      options.after_iteration(result.iterations);
    }
    if (options.progress_callback) {
      options.progress_callback(result);
    }
  }
  return result;
}

std::size_t EventRuntime::component_count() const {
  return components_.size();
}

}  // namespace topoexec
