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

std::map<std::string, std::string> with_invocation_metadata(std::map<std::string, std::string> attributes,
                                                            const Invocation& invocation) {
  const auto& metadata = invocation.metadata;
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

std::uint64_t non_negative_duration_ns(std::chrono::steady_clock::duration duration) {
  const auto count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
  return count < 0 ? 0u : static_cast<std::uint64_t>(count);
}

bool loop_policy_converged_after_iteration(const LoopPolicySpec& policy) {
  return policy.convergence == "single_pass" || policy.convergence == "after_first_iteration" ||
         policy.convergence == "always";
}

bool loop_policy_is_solver_iteration(const LoopPolicySpec& policy) {
  return policy.type == "solver_iteration";
}

std::string effective_partial_success_policy(const LoopPolicySpec& policy) {
  if (!policy.partial_success.empty()) {
    return policy.partial_success;
  }
  return loop_policy_is_solver_iteration(policy) ? "discard_outputs" : "commit_outputs";
}

std::string double_to_string(double value) {
  std::ostringstream out;
  out << value;
  return out.str();
}

struct LoopConvergenceSnapshot {
  bool reported{false};
  bool converged{false};
  std::optional<double> residual;
  std::string reason;
};

class LoopConvergenceState {
public:
  void begin_iteration() {
    std::lock_guard lock(mutex_);
    snapshot_ = {};
  }

  void record(LoopConvergenceReport report) {
    std::lock_guard lock(mutex_);
    snapshot_.reported = true;
    snapshot_.converged = snapshot_.converged || report.converged;
    if (report.residual.has_value()) {
      snapshot_.residual = report.residual;
    }
    if (!report.reason.empty()) {
      snapshot_.reason = std::move(report.reason);
    }
  }

  LoopConvergenceSnapshot snapshot() const {
    std::lock_guard lock(mutex_);
    return snapshot_;
  }

private:
  mutable std::mutex mutex_;
  LoopConvergenceSnapshot snapshot_;
};

std::map<std::string, std::string> loop_iteration_attributes(const std::string& loop_id, std::size_t iteration,
                                                             const LoopPolicySpec& policy,
                                                             const LoopConvergenceSnapshot& snapshot,
                                                             const std::string& reason = {}) {
  std::map<std::string, std::string> attributes{
      {"loop_id", loop_id}, {"iteration", std::to_string(iteration)}, {"policy", policy.type}};
  if (snapshot.residual.has_value()) {
    attributes["residual"] = double_to_string(*snapshot.residual);
  }
  if (policy.residual_threshold.has_value()) {
    attributes["residual_threshold"] = double_to_string(*policy.residual_threshold);
  }
  const auto& selected_reason = reason.empty() ? snapshot.reason : reason;
  if (!selected_reason.empty()) {
    attributes["reason"] = selected_reason;
  }
  return attributes;
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

int runtime_priority_rank(const std::string& priority) {
  if (priority == "high") {
    return 3;
  }
  if (priority == "low") {
    return 1;
  }
  if (priority == "background") {
    return 0;
  }
  return 2;
}

void record_priority_metric(SchedulerMetrics& metrics, const std::string& priority) {
  if (priority == "high") {
    ++metrics.priority_high_count;
  } else if (priority == "low") {
    ++metrics.priority_low_count;
  } else if (priority == "background") {
    ++metrics.priority_background_count;
  } else {
    ++metrics.priority_normal_count;
  }
}

bool is_low_priority_for_rejection_metric(const Invocation& invocation) {
  return invocation.priority == "low" || invocation.priority == "background";
}

std::chrono::steady_clock::duration fixed_rate_period_for_lane(const SchedulerGroupConfig& lane) {
  if (lane.period.count() > 0) {
    return lane.period;
  }
  if (lane.hz > 0.0) {
    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(1.0 / lane.hz));
  }
  return std::chrono::steady_clock::duration::zero();
}

std::chrono::steady_clock::duration fixed_rate_budget_for_lane(const SchedulerGroupConfig& lane) {
  if (lane.tick_budget.count() > 0) {
    return lane.tick_budget;
  }
  return fixed_rate_period_for_lane(lane);
}

std::size_t missed_periods(std::chrono::steady_clock::duration lateness, std::chrono::steady_clock::duration period) {
  if (lateness <= std::chrono::steady_clock::duration::zero() ||
      period <= std::chrono::steady_clock::duration::zero()) {
    return 0u;
  }
  const auto late_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(lateness).count();
  const auto period_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(period).count();
  if (late_ns <= 0 || period_ns <= 0) {
    return 0u;
  }
  return static_cast<std::size_t>(late_ns / period_ns);
}

struct ComponentInvocationOutcome {
  Status status;
  std::chrono::steady_clock::time_point started_at;
  std::chrono::steady_clock::time_point finished_at;
  std::size_t cancellation_observed_before{0};
  std::size_t cancellation_observed_after{0};
  bool cancellation_requested_after{false};
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

  std::future<WorkerInvocationOutcome> submit(Work work, int priority_rank, std::string component_id) {
    WorkItem item;
    item.work = std::move(work);
    item.priority_rank = priority_rank;
    item.component_id = std::move(component_id);
    auto future = item.promise.get_future();
    {
      std::lock_guard lock(mutex_);
      item.enqueue_order = next_enqueue_order_++;
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
    int priority_rank{2};
    std::size_t enqueue_order{0};
    std::string component_id;
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
        auto best = std::min_element(queue_.begin(), queue_.end(), [](const auto& lhs, const auto& rhs) {
          if (lhs.priority_rank != rhs.priority_rank) {
            return lhs.priority_rank > rhs.priority_rank;
          }
          if (lhs.enqueue_order != rhs.enqueue_order) {
            return lhs.enqueue_order < rhs.enqueue_order;
          }
          return lhs.component_id < rhs.component_id;
        });
        item = std::move(*best);
        queue_.erase(best);
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
  std::size_t next_enqueue_order_{0};
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

void EventRuntime::set_health_event_sink(HealthEventSink* sink) {
  health_events_ = sink;
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
  const auto runtime_cancel_token =
      options.cancel_token.valid()
          ? options.cancel_token
          : CancellationToken::from_callback([token = options.stop_token]() { return token.stop_requested(); });
  const auto started_at = std::chrono::steady_clock::now();
  const auto lanes = lane_map(components_);
  std::map<std::string, std::chrono::steady_clock::time_point> next_wall_clock_ticks;
  std::map<std::string, std::chrono::steady_clock::time_point> current_wall_clock_ticks;
  for (const auto& [lane_id, lane] : lanes) {
    if (lane.type == "fixed_rate" && lane.wall_clock_enabled && fixed_rate_period_for_lane(lane).count() > 0) {
      next_wall_clock_ticks[lane_id] = started_at;
    }
  }
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
    std::map<std::string, const CompiledGraphRegion*> regions_by_id;
    for (const auto& region : compiled_plan_.regions) {
      regions_by_id.emplace(region.id, &region);
    }
    order.reserve(compiled_plan_.region_order.size());
    for (const auto& region_id : compiled_plan_.region_order) {
      const auto found = regions_by_id.find(region_id);
      if (found != regions_by_id.end()) {
        order.push_back(*found->second);
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
  std::map<std::string, std::size_t> component_indexes;
  for (std::size_t index = 0; index < components_.size(); ++index) {
    component_indexes.emplace(components_[index].id, index);
  }
  auto component_for_id = [&](const std::string& component_id) -> EventRuntimeComponent* {
    const auto found = component_indexes.find(component_id);
    if (found == component_indexes.end()) {
      return nullptr;
    }
    return &components_[found->second];
  };
  auto apply_config_transaction = [&]() {
    if (config_store_ == nullptr) {
      return true;
    }
    const auto pending = config_store_->pending_component_config_updates();
    if (pending.empty()) {
      config_store_->commit_epoch_boundary();
      return true;
    }
    std::map<std::string, ConfigView> previous_configs;
    for (const auto& [component_id, config] : pending) {
      auto* component = component_for_id(component_id);
      if (component == nullptr || component->component == nullptr) {
        config_store_->rollback_pending_updates();
        result.ok = false;
        result.stop_reason = SchedulerStopReason::kError;
        result.errors.push_back("config transaction references missing component " + component_id);
        record_trace_event(trace_, "config_transaction_rollback",
                           {{"component_id", component_id}, {"reason", "missing_component"}});
        return false;
      }
      const auto validation = component->component->validate_config(config);
      record_trace_event(trace_, "config_transaction_validate",
                         {{"component_id", component_id}, {"status", validation.ok() ? "ok" : "error"}});
      if (!validation.ok()) {
        config_store_->rollback_pending_updates();
        result.ok = false;
        result.stop_reason = SchedulerStopReason::kError;
        result.errors.push_back("config transaction validation failed for component " + component_id + ": " +
                                validation.message());
        record_trace_event(trace_, "config_transaction_rollback",
                           {{"component_id", component_id}, {"reason", "validation"}});
        return false;
      }
      previous_configs[component_id] = config_store_->component_config(component_id);
    }

    std::vector<std::string> applied_components;
    for (const auto& [component_id, config] : pending) {
      auto* component = component_for_id(component_id);
      const auto apply = component->component->apply_config(*component->context, config);
      record_trace_event(trace_, "config_transaction_apply",
                         {{"component_id", component_id}, {"status", apply.ok() ? "ok" : "error"}});
      if (!apply.ok()) {
        const auto restored_failed =
            component->component->apply_config(*component->context, previous_configs[component_id]);
        record_trace_event(
            trace_, "config_transaction_rollback",
            {{"component_id", component_id}, {"reason", "apply"}, {"status", restored_failed.ok() ? "ok" : "error"}});
        if (!restored_failed.ok()) {
          result.errors.push_back("config transaction rollback failed for component " + component_id + ": " +
                                  restored_failed.message());
        }
        for (auto rollback = applied_components.rbegin(); rollback != applied_components.rend(); ++rollback) {
          auto* applied = component_for_id(*rollback);
          if (applied == nullptr || applied->component == nullptr) {
            continue;
          }
          const auto restored = applied->component->apply_config(*applied->context, previous_configs[*rollback]);
          record_trace_event(trace_, "config_transaction_rollback",
                             {{"component_id", *rollback}, {"status", restored.ok() ? "ok" : "error"}});
          if (!restored.ok()) {
            result.errors.push_back("config transaction rollback failed for component " + *rollback + ": " +
                                    restored.message());
          }
        }
        config_store_->rollback_pending_updates();
        result.ok = false;
        result.stop_reason = SchedulerStopReason::kError;
        result.errors.push_back("config transaction apply failed for component " + component_id + ": " +
                                apply.message());
        return false;
      }
      applied_components.push_back(component_id);
    }

    const auto committed = config_store_->commit_epoch_boundary();
    record_trace_event(trace_, "config_transaction_commit", {{"applied_components", std::to_string(committed)}});
    return true;
  };
  for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
    const auto tick_calls_before_iteration = result.tick_calls;
    if (options.stop_token.stop_requested() || runtime_cancel_token.requested()) {
      result.stop_reason = SchedulerStopReason::kStopRequested;
      break;
    }
    if (options.run_duration_ms > 0u &&
        std::chrono::steady_clock::now() - started_at >= std::chrono::milliseconds(options.run_duration_ms)) {
      result.stop_reason = SchedulerStopReason::kDurationBound;
      break;
    }
    current_wall_clock_ticks.clear();
    for (auto& [lane_id, scheduled_tick] : next_wall_clock_ticks) {
      auto& metrics = result.group_metrics[lane_id];
      auto now = std::chrono::steady_clock::now();
      if (iteration > 0u && now < scheduled_tick) {
        const auto wait_started_at = now;
        std::this_thread::sleep_until(scheduled_tick);
        now = std::chrono::steady_clock::now();
        metrics.blocked_duration_ms = std::max(
            metrics.blocked_duration_ms, std::chrono::duration<double, std::milli>(now - wait_started_at).count());
      }
      current_wall_clock_ticks[lane_id] = scheduled_tick;
    }
    const auto iteration_started_at = std::chrono::steady_clock::now();
    record_trace_event(trace_, "scheduler_iteration_begin", {{"iteration", std::to_string(iteration + 1u)}});
    std::map<std::string, std::chrono::steady_clock::time_point> fixed_rate_tick_starts;
    for (const auto& [lane_id, lane] : lanes) {
      if (lane.type != "fixed_rate") {
        continue;
      }
      fixed_rate_tick_starts[lane_id] = iteration_started_at;
      auto attributes =
          std::map<std::string, std::string>{{"lane", lane_id},
                                             {"iteration", std::to_string(iteration + 1u)},
                                             {"wall_clock_enabled", lane.wall_clock_enabled ? "true" : "false"},
                                             {"overrun_policy", lane.overrun_policy}};
      if (auto scheduled = current_wall_clock_ticks.find(lane_id); scheduled != current_wall_clock_ticks.end()) {
        attributes["wall_clock_scheduled"] = "true";
      }
      record_trace_event(trace_, "fixed_rate_tick_begin", std::move(attributes));
    }
    if (state_store_ != nullptr) {
      state_store_->commit_epoch_boundary();
    }
    if (!apply_config_transaction()) {
      return result;
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
    LoopConvergenceState* active_loop_convergence = nullptr;
    LoopIterationContext active_loop_iteration;
    auto execute_component = [&](const std::string& component_id) {
      auto* found = component_for_id(component_id);
      if (found == nullptr) {
        return true;
      }
      const auto now = std::chrono::steady_clock::now();
      TickContext tick;
      tick.sequence = result.iterations + 1u;
      tick.scheduled_at = now;
      tick.started_at = now;
      tick.cancel_token = runtime_cancel_token;
      tick.stop_requested = [token = runtime_cancel_token]() { return token.cancel_requested(); };
      try {
        auto invocations = trigger.collect_ready_invocations(tick, found->spec, found->lane);
        auto& trigger_metrics = result.trigger_metrics[found->id];
        const auto trigger_stats = trigger.take_last_stats(found->id);
        trigger_metrics.timeout_drop_count += trigger_stats.timeout_drop_count;
        trigger_metrics.batch_flush_count += trigger_stats.batch_flush_count;
        trigger_metrics.time_sync_drop_count += trigger_stats.time_sync_drop_count;
        trigger_metrics.late_drop_count += trigger_stats.late_drop_count;
        trigger_metrics.pending_drop_count += trigger_stats.pending_drop_count;
        trigger_metrics.condition_suppressed_count += trigger_stats.condition_suppressed_count;
        trigger_metrics.rate_limit_suppressed_count += trigger_stats.rate_limit_suppressed_count;
        if (invocations.empty() && has_message_event_source(found->spec)) {
          ++trigger_metrics.suppressed_count;
        } else {
          trigger_metrics.ready_count += invocations.size();
          if (found->spec.trigger_policy.coalesce || found->spec.trigger_policy.type == "debounce") {
            trigger_metrics.coalesced_count += invocations.size();
          }
        }
        auto invocation_trace_attributes = [&](const Invocation& invocation, std::optional<std::size_t> worker_id) {
          std::map<std::string, std::string> attributes{{"component_id", found->id}, {"lane", found->lane.id}};
          if (worker_id.has_value()) {
            attributes["worker_id"] = std::to_string(*worker_id);
          }
          return with_invocation_metadata(std::move(attributes), invocation);
        };

        auto run_invocation = [&](const Invocation& invocation, std::optional<std::size_t> worker_id = std::nullopt) {
          auto invocation_for_execute = invocation;
          const auto invocation_cancel_token =
              CancellationToken::from_callback([token = runtime_cancel_token]() { return token.requested(); });
          invocation_for_execute.cancel_token = invocation_cancel_token;
          invocation_for_execute.stop_requested = [token = invocation_cancel_token]() {
            return token.cancel_requested();
          };
          auto invocation_context = *found->context;
          invocation_context.cancel_token = invocation_cancel_token;
          invocation_context.invocation_metadata = invocation_for_execute.metadata;
          if (active_loop_convergence != nullptr) {
            invocation_context.loop_iteration = active_loop_iteration;
            invocation_context.loop_convergence_reporter = [active_loop_convergence](LoopConvergenceReport report) {
              active_loop_convergence->record(std::move(report));
            };
          } else {
            invocation_context.loop_iteration = {};
            invocation_context.loop_convergence_reporter = {};
          }

          ComponentInvocationOutcome outcome;
          outcome.cancellation_observed_before = invocation_cancel_token.observed_count();
          outcome.started_at = std::chrono::steady_clock::now();
          record_trace_event(trace_, "component_execute_begin",
                             invocation_trace_attributes(invocation_for_execute, worker_id));
          try {
            outcome.status = found->component->execute_status(invocation_for_execute, invocation_context);
          } catch (const std::exception& error) {
            outcome.status = Status::error(error.what());
          }
          outcome.finished_at = std::chrono::steady_clock::now();
          outcome.cancellation_observed_after = invocation_cancel_token.observed_count();
          outcome.cancellation_requested_after = invocation_cancel_token.requested();
          auto span_attributes = invocation_trace_attributes(invocation_for_execute, worker_id);
          span_attributes["trigger"] = std::to_string(static_cast<int>(invocation.trigger));
          record_trace_span(trace_, "component_execute", outcome.started_at, outcome.finished_at,
                            std::move(span_attributes));
          record_trace_event(trace_, "component_execute_end",
                             invocation_trace_attributes(invocation_for_execute, worker_id));
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
            ++component_metrics.timeout_budget_exceeded_count;
            record_trace_event(trace_, "component_timeout_budget_exceeded",
                               with_invocation_metadata({{"component_id", found->id},
                                                         {"lane", found->lane.id},
                                                         {"budget_ms", std::to_string(invocation.budget.count())},
                                                         {"duration_ns", std::to_string(duration_ns)}},
                                                        invocation));
          }
          if (outcome.cancellation_requested_after) {
            ++component_metrics.cancellation_requested_count;
            record_trace_event(
                trace_, "component_cancellation_requested",
                with_invocation_metadata({{"component_id", found->id}, {"lane", found->lane.id}}, invocation));
          }
          if (outcome.cancellation_observed_after > outcome.cancellation_observed_before) {
            ++component_metrics.cancellation_observed_count;
            record_trace_event(
                trace_, "component_cancellation_observed",
                with_invocation_metadata({{"component_id", found->id}, {"lane", found->lane.id}}, invocation));
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
          auto& lane_metrics = result.group_metrics[found->lane.id];
          lane_metrics.completed_count += 1;
          record_priority_metric(lane_metrics, invocation.priority);
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
          if (health_events_ != nullptr) {
            HealthEvent event;
            event.kind = HealthEventKind::kSchedulerReject;
            event.source = "scheduler";
            event.component_id = found->id;
            event.lane = found->lane.id;
            event.policy = found->lane.overflow;
            event.reason = lane_overflow_fails_fast(found->lane.overflow) ? "thread_pool queue capacity exceeded"
                                                                          : "thread_pool invocation admission rejected";
            event.depth = invocations.size();
            event.capacity = admission_capacity;
            event.occurrence_count = overflow_count;
            event.attributes["rejected_count"] = std::to_string(overflow_count);
            health_events_->emit(std::move(event));
          }
          const auto rejected_begin = lane_overflow_drops_oldest(found->lane.overflow)
                                          ? invocations.begin()
                                          : invocations.begin() + static_cast<std::ptrdiff_t>(admission_capacity);
          const auto rejected_end = rejected_begin + static_cast<std::ptrdiff_t>(overflow_count);
          lane_metrics.low_priority_rejected_count += static_cast<std::size_t>(
              std::count_if(rejected_begin, rejected_end, is_low_priority_for_rejection_metric));
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
            const auto priority = runtime_priority_rank(invocations[offset + index].priority);
            futures.push_back(pool.submit([&, invocation = invocations[offset + index]](
                                              std::size_t worker_id) { return run_invocation(invocation, worker_id); },
                                          priority, found->id));
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
        LoopConvergenceState loop_convergence;
        bool converged = false;
        bool budget_overrun = false;
        bool cancelled = false;
        std::string stop_reason;
        const auto loop_started_at = std::chrono::steady_clock::now();
        if (publications_ != nullptr) {
          publications_->begin_composite_region(region.components);
        }
        for (std::size_t loop_iteration = 0; loop_iteration < max_iterations; ++loop_iteration) {
          if (loop_iteration > 0u && runtime_cancel_token.requested()) {
            cancelled = true;
            result.stop_reason = SchedulerStopReason::kStopRequested;
            stop_reason = "cancelled";
            result.loop_stop_reason[region.id] = stop_reason;
            ++result.loop_cancellation_requested_count[region.id];
            ++result.loop_cancellation_observed_count[region.id];
            record_trace_event(trace_, "loop_cancellation_requested",
                               {{"loop_id", region.id}, {"iteration", std::to_string(loop_iteration + 1u)}});
            record_trace_event(trace_, "loop_cancellation_observed",
                               {{"loop_id", region.id}, {"iteration", std::to_string(loop_iteration + 1u)}});
            break;
          }
          const auto loop_iteration_started_at = std::chrono::steady_clock::now();
          loop_convergence.begin_iteration();
          active_loop_convergence = &loop_convergence;
          active_loop_iteration = LoopIterationContext{region.id, region.loop_policy.type, loop_iteration,
                                                       loop_iteration + 1u, max_iterations};
          record_trace_event(trace_, "loop_iteration_begin",
                             {{"loop_id", region.id},
                              {"iteration", std::to_string(loop_iteration + 1u)},
                              {"policy", region.loop_policy.type}});
          ++result.loop_iteration_count[region.id];
          for (const auto& component_id : region.components) {
            if (!execute_component(component_id)) {
              active_loop_convergence = nullptr;
              active_loop_iteration = {};
              ++result.loop_error_count[region.id];
              result.loop_stop_reason[region.id] = "component_error";
              record_trace_event(trace_, "loop_error",
                                 {{"loop_id", region.id},
                                  {"component_id", component_id},
                                  {"iteration", std::to_string(loop_iteration + 1u)}});
              return result;
            }
          }
          active_loop_convergence = nullptr;
          active_loop_iteration = {};
          const auto loop_iteration_finished_at = std::chrono::steady_clock::now();
          const auto convergence_snapshot = loop_convergence.snapshot();
          if (convergence_snapshot.residual.has_value()) {
            result.loop_last_residual[region.id] = *convergence_snapshot.residual;
          }
          auto iteration_attributes =
              loop_iteration_attributes(region.id, loop_iteration + 1u, region.loop_policy, convergence_snapshot);
          record_trace_span(trace_, "loop_iteration", loop_iteration_started_at, loop_iteration_finished_at,
                            iteration_attributes);
          record_trace_event(trace_, "loop_iteration_end", std::move(iteration_attributes));
          std::string convergence_reason;
          if (loop_policy_converged_after_iteration(region.loop_policy)) {
            convergence_reason = region.loop_policy.convergence;
          } else if (convergence_snapshot.converged) {
            convergence_reason = convergence_snapshot.reason.empty() ? "component_report" : convergence_snapshot.reason;
          } else if (convergence_snapshot.residual.has_value() && region.loop_policy.residual_threshold.has_value() &&
                     *convergence_snapshot.residual <= *region.loop_policy.residual_threshold) {
            convergence_reason = "residual_threshold";
          }
          if (!convergence_reason.empty()) {
            converged = true;
            stop_reason = convergence_reason;
            result.loop_stop_reason[region.id] = stop_reason;
            ++result.loop_converged_count[region.id];
            record_trace_event(trace_, "loop_converged",
                               loop_iteration_attributes(region.id, loop_iteration + 1u, region.loop_policy,
                                                         convergence_snapshot, convergence_reason));
            break;
          }
          if (region.loop_policy.budget_ms > 0 && std::chrono::steady_clock::now() - loop_started_at >=
                                                      std::chrono::milliseconds(region.loop_policy.budget_ms)) {
            budget_overrun = true;
            stop_reason = "budget_overrun";
            result.loop_stop_reason[region.id] = stop_reason;
            ++result.loop_budget_overrun_count[region.id];
            record_trace_event(trace_, "loop_budget_overrun",
                               loop_iteration_attributes(region.id, loop_iteration + 1u, region.loop_policy,
                                                         convergence_snapshot, stop_reason));
            break;
          }
        }
        if (!converged && !budget_overrun && !cancelled) {
          stop_reason = "max_iterations";
          result.loop_stop_reason[region.id] = stop_reason;
          if (region.loop_policy.max_iterations > 0) {
            ++result.loop_max_iteration_hit_count[region.id];
            record_trace_event(trace_, "loop_max_iterations_hit",
                               {{"loop_id", region.id}, {"iteration", std::to_string(max_iterations)}});
          }
        }
        const auto partial_policy = effective_partial_success_policy(region.loop_policy);
        const auto partial_solver_stop = loop_policy_is_solver_iteration(region.loop_policy) && !converged;
        if (partial_solver_stop && partial_policy == "fail_run") {
          if (publications_ != nullptr) {
            publications_->discard_composite_region_outputs();
          }
          ++result.loop_output_discarded_count[region.id];
          result.ok = false;
          result.stop_reason = SchedulerStopReason::kError;
          result.errors.push_back("composite_loop " + region.id + " stopped without convergence: " + stop_reason);
          return result;
        }
        const auto commit_outputs = !partial_solver_stop || partial_policy == "commit_outputs";
        if (publications_ != nullptr) {
          if (commit_outputs) {
            const auto commit = publications_->commit_composite_region_outputs();
            if (!commit.accepted) {
              result.ok = false;
              result.stop_reason = SchedulerStopReason::kError;
              result.errors.push_back("composite region publication commit failed after region " + region.id + ": " +
                                      commit.reason);
              return result;
            }
          } else {
            publications_->discard_composite_region_outputs();
            ++result.loop_output_discarded_count[region.id];
            record_trace_event(trace_, "loop_output_discarded",
                               {{"loop_id", region.id}, {"reason", stop_reason}, {"policy", partial_policy}});
          }
        }
        if (cancelled) {
          return result;
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
      ++metrics.tick_count;
      const auto period = fixed_rate_period_for_lane(lane);
      const auto budget = fixed_rate_budget_for_lane(lane);
      auto tick_attributes =
          std::map<std::string, std::string>{{"lane", lane_id},
                                             {"iteration", std::to_string(iteration + 1u)},
                                             {"wall_clock_enabled", lane.wall_clock_enabled ? "true" : "false"},
                                             {"overrun_policy", lane.overrun_policy}};
      if (budget.count() > 0 && iteration_duration > budget) {
        const auto jitter = iteration_duration - budget;
        ++metrics.tick_overrun_count;
        metrics.tick_jitter_ms = std::chrono::duration<double, std::milli>(jitter).count();
        metrics.max_lateness_ms = std::max(metrics.max_lateness_ms, metrics.tick_jitter_ms);
        auto overrun_attributes = tick_attributes;
        overrun_attributes["lateness_ms"] = std::to_string(metrics.tick_jitter_ms);
        record_trace_event(trace_, "fixed_rate_overrun", std::move(overrun_attributes));
      }
      auto scheduled = current_wall_clock_ticks.find(lane_id);
      if (scheduled != current_wall_clock_ticks.end() && period.count() > 0) {
        const auto next_nominal_tick = scheduled->second + period;
        const auto wall_clock_lateness = iteration_finished_at > next_nominal_tick
                                             ? iteration_finished_at - next_nominal_tick
                                             : std::chrono::steady_clock::duration::zero();
        metrics.max_lateness_ms =
            std::max(metrics.max_lateness_ms, std::chrono::duration<double, std::milli>(wall_clock_lateness).count());
        auto skipped_ticks = missed_periods(wall_clock_lateness, period);
        if (lane.overrun_policy == "skip_next" && wall_clock_lateness.count() > 0) {
          ++skipped_ticks;
          next_wall_clock_ticks[lane_id] = next_nominal_tick + period;
        } else if (lane.overrun_policy == "catch_up_once" && wall_clock_lateness.count() > 0) {
          next_wall_clock_ticks[lane_id] = iteration_finished_at;
        } else if (wall_clock_lateness.count() > 0) {
          next_wall_clock_ticks[lane_id] = iteration_finished_at + period;
        } else {
          next_wall_clock_ticks[lane_id] = next_nominal_tick;
        }
        if (skipped_ticks > 0u) {
          metrics.skipped_tick_count += skipped_ticks;
          auto skipped_attributes = tick_attributes;
          skipped_attributes["skipped_ticks"] = std::to_string(skipped_ticks);
          record_trace_event(trace_, "fixed_rate_skipped_tick", std::move(skipped_attributes));
        }
      }
      auto tick_started_at = fixed_rate_tick_starts.find(lane_id);
      if (tick_started_at != fixed_rate_tick_starts.end()) {
        record_trace_span(trace_, "fixed_rate_tick", tick_started_at->second, iteration_finished_at, tick_attributes);
        record_trace_event(trace_, "fixed_rate_tick_end", std::move(tick_attributes));
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
