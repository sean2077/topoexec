#include "topoexec/runtime/event_runtime.hpp"

#include "topoexec/common/trace.hpp"
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

void record_trace_event(TraceCollector* trace, const std::string& name) {
  if (trace == nullptr) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  trace->add(SpanRecord{TraceId::generate(), name, now, now});
}

bool loop_policy_converged_after_iteration(const LoopPolicySpec& policy) {
  return policy.convergence == "single_pass" || policy.convergence == "after_first_iteration" ||
         policy.convergence == "always";
}

constexpr std::size_t kDefaultRunUntilIdleIterationBound = 1000u;

} // namespace

EventRuntime::EventRuntime(RuntimeChannelBus* channels) : channels_(channels) {}
EventRuntime::EventRuntime(RuntimeChannelBus* channels, GraphCompiledPlan compiled_plan)
    : channels_(channels), compiled_plan_(std::move(compiled_plan)) {}
EventRuntime::EventRuntime(RuntimeChannelBus* channels, GraphCompiledPlan compiled_plan,
                           RuntimePublicationRouter* publications)
    : channels_(channels), publications_(publications), compiled_plan_(std::move(compiled_plan)) {}

void EventRuntime::add_component(EventRuntimeComponent component) {
  components_.push_back(std::move(component));
}

void EventRuntime::set_compiled_plan(GraphCompiledPlan compiled_plan) {
  compiled_plan_ = std::move(compiled_plan);
}

void EventRuntime::set_publication_router(RuntimePublicationRouter* publications) {
  publications_ = publications;
}

void EventRuntime::set_trace_collector(TraceCollector* trace) {
  trace_ = trace;
}

SchedulerRunResult EventRuntime::run(const SchedulerRunOptions& options) {
  SchedulerRunResult result;
  result.stop_reason = SchedulerStopReason::kTickBound;
  TriggerPolicyEngine trigger(channels_);
  const auto started_at = std::chrono::steady_clock::now();
  const auto lanes = lane_map(components_);
  const std::size_t iterations = options.tick_iterations == 0u
                                     ? (options.run_until_idle ? kDefaultRunUntilIdleIterationBound : 1u)
                                     : options.tick_iterations;

  std::vector<CompiledGraphRegion> order;
  if (!compiled_plan_.region_order.empty()) {
    for (const auto& region_id : compiled_plan_.region_order) {
      const auto found = std::find_if(compiled_plan_.regions.begin(), compiled_plan_.regions.end(),
                                      [&region_id](const auto& region) { return region.id == region_id; });
      if (found != compiled_plan_.regions.end()) {
        order.push_back(*found);
      }
    }
  }
  if (order.empty()) {
    for (const auto& component : components_) {
      CompiledGraphRegion region;
      region.id = component.id;
      region.components = {component.id};
      order.push_back(std::move(region));
    }
  }

  for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
    const auto tick_calls_before_iteration = result.tick_calls;
    if (options.stop_token.stop_requested()) {
      result.stop_reason = SchedulerStopReason::kStopRequested;
      break;
    }
    if (options.run_duration_ms > 0u &&
        std::chrono::steady_clock::now() - started_at >= std::chrono::milliseconds(options.run_duration_ms)) {
      result.stop_reason = SchedulerStopReason::kDurationBound;
      break;
    }
    record_trace_event(trace_, "scheduler_iteration_begin");
    if (publications_ != nullptr) {
      const auto commit = publications_->begin_epoch();
      if (!commit.accepted) {
        result.ok = false;
        result.stop_reason = SchedulerStopReason::kError;
        result.errors.push_back("epoch deferred publication commit failed: " + commit.reason);
        return result;
      }
    }
    auto execute_component = [&](const std::string& component_id) {
      auto found = std::find_if(components_.begin(), components_.end(),
                                [&component_id](const auto& item) { return item.id == component_id; });
      if (found == components_.end()) {
        return true;
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
          record_trace_event(trace_, "component_execute_begin");
          found->component->execute(invocation, *found->context);
          record_trace_event(trace_, "component_execute_end");
          if (publications_ != nullptr) {
            const auto commit = publications_->commit_immediate();
            if (!commit.accepted) {
              result.ok = false;
              result.stop_reason = SchedulerStopReason::kError;
              result.errors.push_back("immediate publication commit failed after component " + found->id + ": " +
                                      commit.reason);
              return false;
            }
          }
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
        return false;
      }
      return true;
    };
    for (const auto& region : order) {
      if (region.kind == CompiledRegionKind::kCompositeLoop) {
        const auto max_iterations =
            region.loop_policy.max_iterations > 0 ? static_cast<std::size_t>(region.loop_policy.max_iterations) : 1u;
        bool converged = false;
        bool budget_overrun = false;
        const auto loop_started_at = std::chrono::steady_clock::now();
        if (publications_ != nullptr) {
          publications_->begin_composite_region(region.components);
        }
        for (std::size_t loop_iteration = 0; loop_iteration < max_iterations; ++loop_iteration) {
          record_trace_event(trace_, "loop_iteration_begin");
          ++result.loop_iteration_count[region.id];
          for (const auto& component_id : region.components) {
            if (!execute_component(component_id)) {
              return result;
            }
          }
          record_trace_event(trace_, "loop_iteration_end");
          if (loop_policy_converged_after_iteration(region.loop_policy)) {
            converged = true;
            ++result.loop_converged_count[region.id];
            break;
          }
          if (region.loop_policy.budget_ms > 0 && std::chrono::steady_clock::now() - loop_started_at >=
                                                      std::chrono::milliseconds(region.loop_policy.budget_ms)) {
            budget_overrun = true;
            ++result.loop_budget_overrun_count[region.id];
            break;
          }
        }
        if (!converged && !budget_overrun && region.loop_policy.max_iterations > 0) {
          ++result.loop_max_iteration_hit_count[region.id];
        }
        if (publications_ != nullptr) {
          const auto commit = publications_->commit_composite_region_outputs();
          if (!commit.accepted) {
            result.ok = false;
            result.stop_reason = SchedulerStopReason::kError;
            result.errors.push_back("composite region publication commit failed after region " + region.id + ": " +
                                    commit.reason);
            return result;
          }
        }
        continue;
      }
      for (const auto& component_id : region.components) {
        if (!execute_component(component_id)) {
          return result;
        }
      }
    }
    if (publications_ != nullptr) {
      publications_->end_epoch();
    }
    record_trace_event(trace_, "scheduler_iteration_end");
    ++result.iterations;
    if (options.after_iteration) {
      options.after_iteration(result.iterations);
    }
    if (options.progress_callback) {
      options.progress_callback(result);
    }
    if (options.run_until_idle && result.tick_calls == tick_calls_before_iteration) {
      result.stop_reason = SchedulerStopReason::kIdle;
      break;
    }
  }
  return result;
}

std::size_t EventRuntime::component_count() const {
  return components_.size();
}

} // namespace topoexec
