#include "topoexec/runtime/event_runtime.hpp"

#include "topoexec/common/trace.hpp"
#include "topoexec/runtime/state.hpp"
#include "topoexec/runtime/trigger_policy.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

#ifdef __linux__
#include <pthread.h>
#endif

namespace topoexec {
namespace {

std::map<std::string, SchedulerGroupConfig> lane_map(const std::vector<EventRuntimeComponent>& components) {
  std::map<std::string, SchedulerGroupConfig> lanes;
  for (const auto& component : components) {
    lanes[component.lane.id] = component.lane;
  }
  return lanes;
}

void record_trace_event(TraceCollector* trace, const std::string& name,
                        std::map<std::string, std::string> attributes = {}) {
  if (trace == nullptr) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  trace->add(SpanRecord{TraceId::generate(), name, now, now, std::move(attributes)});
}

void record_trace_span(TraceCollector* trace, const std::string& name, std::chrono::steady_clock::time_point started_at,
                       std::chrono::steady_clock::time_point finished_at,
                       std::map<std::string, std::string> attributes = {}) {
  if (trace == nullptr) {
    return;
  }
  trace->add(SpanRecord{TraceId::generate(), name, started_at, finished_at, std::move(attributes)});
}

std::uint64_t non_negative_duration_ns(std::chrono::steady_clock::duration duration) {
  const auto count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
  return count < 0 ? 0u : static_cast<std::uint64_t>(count);
}

bool loop_policy_converged_after_iteration(const LoopPolicySpec& policy) {
  return policy.convergence == "single_pass" || policy.convergence == "after_first_iteration" ||
         policy.convergence == "always";
}

std::size_t worker_count_for_lane(const SchedulerGroupConfig& lane) {
  return lane.max_threads > 0 ? static_cast<std::size_t>(lane.max_threads) : 1u;
}

std::size_t queue_capacity_for_lane(const SchedulerGroupConfig& lane, std::size_t ready_count,
                                    std::size_t active_capacity) {
  if (lane.queue_capacity > 0) {
    return static_cast<std::size_t>(lane.queue_capacity);
  }
  return ready_count > active_capacity ? ready_count - active_capacity : 0u;
}

bool lane_overflow_drops_oldest(const std::string& overflow) {
  return overflow == "drop_oldest" || overflow == "overwrite";
}

bool lane_overflow_fails_fast(const std::string& overflow) {
  return overflow == "fail_fast";
}

struct ComponentInvocationOutcome {
  Status status;
  std::chrono::steady_clock::time_point started_at;
  std::chrono::steady_clock::time_point finished_at;
};

struct WorkerInvocationOutcome {
  ComponentInvocationOutcome outcome;
  std::size_t worker_id{0};
};

std::string bounded_thread_name(std::string base, std::size_t worker_id) {
  if (base.empty()) {
    base = "topoexec";
  }
  auto name = base + "-" + std::to_string(worker_id);
  if (name.size() > 15u) {
    name.resize(15u);
  }
  return name;
}

void set_current_thread_name(const std::string& name) {
#ifdef __linux__
  (void)pthread_setname_np(pthread_self(), name.c_str());
#else
  (void)name;
#endif
}

class PersistentWorkerPool {
public:
  using Work = std::function<ComponentInvocationOutcome(std::size_t worker_id)>;

  PersistentWorkerPool(std::string lane_id, std::string thread_name, std::size_t worker_count)
      : lane_id_(std::move(lane_id)), thread_name_(std::move(thread_name)),
        worker_count_(std::max<std::size_t>(worker_count, 1u)) {
    workers_.reserve(worker_count_);
    for (std::size_t worker_id = 0; worker_id < worker_count_; ++worker_id) {
      workers_.emplace_back([this, worker_id]() { this->worker_loop(worker_id); });
    }
  }

  ~PersistentWorkerPool() {
    stop_and_join();
  }

  PersistentWorkerPool(const PersistentWorkerPool&) = delete;
  PersistentWorkerPool& operator=(const PersistentWorkerPool&) = delete;

  std::future<WorkerInvocationOutcome> submit(Work work) {
    WorkItem item;
    item.work = std::move(work);
    auto future = item.promise.get_future();
    {
      std::lock_guard lock(mutex_);
      queue_.push_back(std::move(item));
    }
    cv_.notify_one();
    return future;
  }

  void stop_and_join() {
    {
      std::lock_guard lock(mutex_);
      stop_requested_ = true;
    }
    cv_.notify_all();
    for (auto& worker : workers_) {
      if (worker.joinable()) {
        worker.join();
      }
    }
  }

  std::size_t worker_count() const {
    return worker_count_;
  }

private:
  struct WorkItem {
    Work work;
    std::promise<WorkerInvocationOutcome> promise;
  };

  void worker_loop(std::size_t worker_id) {
    set_current_thread_name(bounded_thread_name(thread_name_.empty() ? lane_id_ : thread_name_, worker_id));
    while (true) {
      WorkItem item;
      {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [&]() { return stop_requested_ || !queue_.empty(); });
        if (stop_requested_ && queue_.empty()) {
          return;
        }
        item = std::move(queue_.front());
        queue_.pop_front();
      }
      try {
        item.promise.set_value(WorkerInvocationOutcome{item.work(worker_id), worker_id});
      } catch (...) {
        item.promise.set_exception(std::current_exception());
      }
    }
  }

  std::string lane_id_;
  std::string thread_name_;
  std::size_t worker_count_{1};
  std::vector<std::thread> workers_;
  std::deque<WorkItem> queue_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool stop_requested_{false};
};

std::string join_worker_ids(const std::vector<WorkerInvocationOutcome>& outcomes) {
  std::ostringstream out;
  for (std::size_t index = 0; index < outcomes.size(); ++index) {
    if (index > 0u) {
      out << ",";
    }
    out << outcomes[index].worker_id;
  }
  return out.str();
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

void EventRuntime::set_state_store(RuntimeStateStore* state_store) {
  state_store_ = state_store;
}

void EventRuntime::set_config_store(ConfigSnapshotStore* config_store) {
  config_store_ = config_store;
}

SchedulerRunResult EventRuntime::run(const SchedulerRunOptions& options) {
  SchedulerRunResult result;
  result.stop_reason = SchedulerStopReason::kTickBound;
  TriggerPolicyEngine trigger(channels_);
  const auto started_at = std::chrono::steady_clock::now();
  const auto lanes = lane_map(components_);
  std::map<std::string, std::unique_ptr<PersistentWorkerPool>> worker_pools;
  for (const auto& [lane_id, lane] : lanes) {
    if (lane.type == "thread_pool") {
      worker_pools.emplace(
          lane_id, std::make_unique<PersistentWorkerPool>(lane_id, lane.thread_name, worker_count_for_lane(lane)));
    }
  }
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

  std::map<std::string, std::size_t> component_in_flight;
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
    const auto iteration_started_at = std::chrono::steady_clock::now();
    record_trace_event(trace_, "scheduler_iteration_begin", {{"iteration", std::to_string(iteration + 1u)}});
    if (state_store_ != nullptr) {
      state_store_->commit_epoch_boundary();
    }
    if (config_store_ != nullptr) {
      config_store_->commit_epoch_boundary();
    }
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
        auto& trigger_metrics = result.trigger_metrics[found->id];
        const auto trigger_stats = trigger.take_last_stats(found->id);
        trigger_metrics.timeout_drop_count += trigger_stats.timeout_drop_count;
        trigger_metrics.batch_flush_count += trigger_stats.batch_flush_count;
        trigger_metrics.time_sync_drop_count += trigger_stats.time_sync_drop_count;
        if (invocations.empty() && has_message_event_source(found->spec)) {
          ++trigger_metrics.suppressed_count;
        } else {
          trigger_metrics.ready_count += invocations.size();
          if (found->spec.trigger_policy.coalesce) {
            trigger_metrics.coalesced_count += invocations.size();
          }
        }
        auto invocation_trace_attributes = [&](std::optional<std::size_t> worker_id) {
          std::map<std::string, std::string> attributes{{"component_id", found->id}, {"lane", found->lane.id}};
          if (worker_id.has_value()) {
            attributes["worker_id"] = std::to_string(*worker_id);
          }
          return attributes;
        };

        auto run_invocation = [&](const Invocation& invocation, std::optional<std::size_t> worker_id = std::nullopt) {
          ComponentInvocationOutcome outcome;
          outcome.started_at = std::chrono::steady_clock::now();
          record_trace_event(trace_, "component_execute_begin", invocation_trace_attributes(worker_id));
          try {
            outcome.status = found->component->execute_status(invocation, *found->context);
          } catch (const std::exception& error) {
            outcome.status = Status::error(error.what());
          }
          outcome.finished_at = std::chrono::steady_clock::now();
          auto span_attributes = invocation_trace_attributes(worker_id);
          span_attributes["trigger"] = std::to_string(static_cast<int>(invocation.trigger));
          record_trace_span(trace_, "component_execute", outcome.started_at, outcome.finished_at,
                            std::move(span_attributes));
          record_trace_event(trace_, "component_execute_end", invocation_trace_attributes(worker_id));
          return outcome;
        };

        auto record_invocation_outcome = [&](const Invocation& invocation, const ComponentInvocationOutcome& outcome,
                                             bool commit_publications) {
          auto& component_metrics = result.component_metrics[found->id];
          const auto duration_ns = non_negative_duration_ns(outcome.finished_at - outcome.started_at);
          ++component_metrics.execution_count;
          component_metrics.last_duration_ns = duration_ns;
          component_metrics.max_duration_ns = std::max(component_metrics.max_duration_ns, duration_ns);
          if (invocation.budget.count() > 0 && outcome.finished_at - outcome.started_at > invocation.budget) {
            ++component_metrics.budget_overrun_count;
          }
          if (!outcome.status.ok()) {
            ++component_metrics.error_count;
            result.ok = false;
            result.stop_reason = SchedulerStopReason::kError;
            result.errors.push_back("component " + found->id + " failed: " + outcome.status.message());
            return false;
          }
          if (commit_publications && publications_ != nullptr) {
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
          return true;
        };

        auto execute_sequential = [&]() {
          for (const auto& invocation : invocations) {
            if (!found->spec.execution.reentrant && component_in_flight[found->id] != 0u) {
              result.ok = false;
              result.stop_reason = SchedulerStopReason::kError;
              result.errors.push_back("non-reentrant component " + found->id + " already has an in-flight invocation");
              return false;
            }
            ++component_in_flight[found->id];
            auto& component_metrics = result.component_metrics[found->id];
            component_metrics.max_in_flight_count =
                std::max(component_metrics.max_in_flight_count, component_in_flight[found->id]);
            const auto outcome = run_invocation(invocation);
            --component_in_flight[found->id];
            if (!record_invocation_outcome(invocation, outcome, true)) {
              return false;
            }
          }
          return true;
        };

        if (found->lane.type != "thread_pool" || invocations.empty()) {
          return execute_sequential();
        }

        auto pool_found = worker_pools.find(found->lane.id);
        if (pool_found == worker_pools.end() || pool_found->second == nullptr) {
          result.ok = false;
          result.stop_reason = SchedulerStopReason::kError;
          result.errors.push_back("thread_pool lane " + found->lane.id + " has no worker pool");
          return false;
        }
        auto& pool = *pool_found->second;
        const auto worker_count = pool.worker_count();
        auto& lane_metrics = result.group_metrics[found->lane.id];
        lane_metrics.worker_count = std::max(lane_metrics.worker_count, worker_count);

        const auto active_capacity = found->spec.execution.reentrant ? worker_count : 1u;
        const auto queue_capacity = queue_capacity_for_lane(found->lane, invocations.size(), active_capacity);
        lane_metrics.queue_capacity = std::max(lane_metrics.queue_capacity, queue_capacity);
        const auto admission_capacity = active_capacity + queue_capacity;
        if (invocations.size() > admission_capacity) {
          const auto overflow_count = invocations.size() - admission_capacity;
          lane_metrics.enqueue_rejected_count += overflow_count;
          if (lane_overflow_fails_fast(found->lane.overflow)) {
            result.ok = false;
            result.stop_reason = SchedulerStopReason::kError;
            result.errors.push_back("thread_pool lane " + found->lane.id + " queue capacity exceeded");
            return false;
          }
          if (lane_overflow_drops_oldest(found->lane.overflow)) {
            invocations.erase(invocations.begin(), invocations.begin() + static_cast<std::ptrdiff_t>(overflow_count));
          } else {
            invocations.resize(admission_capacity);
          }
        }

        auto execute_worker_invocations = [&](std::size_t offset, std::size_t count, bool commit_each) {
          if (options.stop_token.stop_requested()) {
            result.stop_reason = SchedulerStopReason::kStopRequested;
            return false;
          }
          const auto active_count = std::min(active_capacity, count);
          const auto queued_count = count > active_count ? count - active_count : 0u;
          lane_metrics.queue_depth = std::max(lane_metrics.queue_depth, queued_count);
          lane_metrics.active_count = std::max(lane_metrics.active_count, active_count);
          lane_metrics.in_flight_count = std::max(lane_metrics.in_flight_count, count);
          auto& component_metrics = result.component_metrics[found->id];
          component_metrics.max_in_flight_count = std::max(component_metrics.max_in_flight_count, active_count);
          const auto batch_started_at = std::chrono::steady_clock::now();

          std::vector<std::future<WorkerInvocationOutcome>> futures;
          futures.reserve(count);
          for (std::size_t index = 0; index < count; ++index) {
            const auto invocation = invocations[offset + index];
            futures.push_back(
                pool.submit([&, invocation](std::size_t worker_id) { return run_invocation(invocation, worker_id); }));
          }

          std::vector<WorkerInvocationOutcome> outcomes;
          outcomes.reserve(futures.size());
          for (std::size_t index = 0; index < futures.size(); ++index) {
            outcomes.push_back(futures[index].get());
            if (!record_invocation_outcome(invocations[offset + index], outcomes.back().outcome, commit_each)) {
              return false;
            }
          }
          if (!commit_each && publications_ != nullptr) {
            const auto commit = publications_->commit_immediate();
            if (!commit.accepted) {
              result.ok = false;
              result.stop_reason = SchedulerStopReason::kError;
              result.errors.push_back("immediate publication commit failed after thread_pool component " + found->id +
                                      ": " + commit.reason);
              return false;
            }
          }
          const auto batch_finished_at = std::chrono::steady_clock::now();
          record_trace_span(trace_, "thread_pool_batch", batch_started_at, batch_finished_at,
                            {{"component_id", found->id},
                             {"lane", found->lane.id},
                             {"batch_size", std::to_string(count)},
                             {"worker_count", std::to_string(worker_count)},
                             {"queue_capacity", std::to_string(queue_capacity)},
                             {"worker_ids", join_worker_ids(outcomes)}});
          return true;
        };

        if (!found->spec.execution.reentrant) {
          for (std::size_t offset = 0; offset < invocations.size(); ++offset) {
            if (!execute_worker_invocations(offset, 1u, true)) {
              return false;
            }
          }
          return true;
        }

        if (!execute_worker_invocations(0u, invocations.size(), false)) {
          return false;
        }
      } catch (const std::exception& error) {
        if (component_in_flight[found->id] > 0u) {
          --component_in_flight[found->id];
        }
        ++result.component_metrics[found->id].error_count;
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
          const auto loop_iteration_started_at = std::chrono::steady_clock::now();
          record_trace_event(trace_, "loop_iteration_begin",
                             {{"loop_id", region.id}, {"iteration", std::to_string(loop_iteration + 1u)}});
          ++result.loop_iteration_count[region.id];
          for (const auto& component_id : region.components) {
            if (!execute_component(component_id)) {
              ++result.loop_error_count[region.id];
              record_trace_event(trace_, "loop_error",
                                 {{"loop_id", region.id},
                                  {"component_id", component_id},
                                  {"iteration", std::to_string(loop_iteration + 1u)}});
              return result;
            }
          }
          const auto loop_iteration_finished_at = std::chrono::steady_clock::now();
          record_trace_span(trace_, "loop_iteration", loop_iteration_started_at, loop_iteration_finished_at,
                            {{"loop_id", region.id}, {"iteration", std::to_string(loop_iteration + 1u)}});
          record_trace_event(trace_, "loop_iteration_end",
                             {{"loop_id", region.id}, {"iteration", std::to_string(loop_iteration + 1u)}});
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
    const auto iteration_finished_at = std::chrono::steady_clock::now();
    const auto iteration_duration = iteration_finished_at - iteration_started_at;
    for (const auto& [lane_id, lane] : lanes) {
      auto& metrics = result.group_metrics[lane_id];
      metrics.last_callback_duration_ms = std::chrono::duration<double, std::milli>(iteration_duration).count();
      if (lane.type != "fixed_rate") {
        continue;
      }
      auto period = std::chrono::steady_clock::duration::zero();
      if (lane.period.count() > 0) {
        period = lane.period;
      } else if (lane.hz > 0.0) {
        period = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(1.0 / lane.hz));
      }
      if (lane.tick_budget.count() > 0) {
        period = lane.tick_budget;
      }
      if (period.count() > 0 && iteration_duration > period) {
        ++metrics.tick_overrun_count;
        metrics.tick_jitter_ms = std::chrono::duration<double, std::milli>(iteration_duration - period).count();
      }
    }
    record_trace_span(trace_, "scheduler_iteration", iteration_started_at, iteration_finished_at,
                      {{"iteration", std::to_string(iteration + 1u)}});
    record_trace_event(trace_, "scheduler_iteration_end", {{"iteration", std::to_string(iteration + 1u)}});
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
