#include "topoexec/runtime/task_executor.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <utility>

namespace topoexec {
namespace {

bool drop_oldest_overflow(const std::string& overflow) {
  return overflow == "drop_oldest" || overflow == "overwrite";
}

bool cancel_pending_shutdown_policy(const std::string& policy) {
  return policy == "cancel_pending" || policy == "cancel" || policy == "discard_pending";
}

std::size_t normalize_positive(std::size_t value) {
  return value == 0u ? 1u : value;
}

std::size_t completion_limit(std::size_t max_tasks, std::size_t available) {
  return max_tasks == 0u ? available : std::min(max_tasks, available);
}

} // namespace

DeterministicTaskExecutor::DeterministicTaskExecutor(TaskExecutorConfig config) : config_(std::move(config)) {
  config_.max_inflight = normalize_positive(config_.max_inflight);
}

std::size_t DeterministicTaskExecutor::admission_capacity() const {
  return config_.max_inflight + config_.queue_capacity;
}

bool DeterministicTaskExecutor::drop_oldest_on_overflow() const {
  return drop_oldest_overflow(config_.overflow);
}

TaskSubmissionResult DeterministicTaskExecutor::reject_submission(std::string reason) {
  ++metrics_.rejected_count;
  emit_reject_health_event(reason);
  return {false, 0u, std::move(reason)};
}

void DeterministicTaskExecutor::emit_reject_health_event(const std::string& reason) {
  if (health_events_ == nullptr) {
    return;
  }
  HealthEvent event;
  event.kind = HealthEventKind::kTaskReject;
  event.source = "task_executor";
  event.policy = config_.overflow;
  event.reason = reason;
  event.depth = pending_.size();
  event.capacity = admission_capacity();
  health_events_->emit(std::move(event));
}

void DeterministicTaskExecutor::set_health_event_sink(HealthEventSink* sink) {
  health_events_ = sink;
}

TaskSubmissionResult DeterministicTaskExecutor::submit(Work work, CompletionCallback completion) {
  if (!work) {
    return reject_submission("task work must not be empty");
  }
  const auto capacity = admission_capacity();
  if (pending_.size() >= capacity) {
    if (drop_oldest_on_overflow()) {
      pending_.pop_front();
      ++metrics_.cancelled_count;
    } else {
      return reject_submission(config_.overflow == "fail_fast" ? "task executor capacity exceeded"
                                                               : "task executor queue full");
    }
  }
  PendingTask task;
  task.id = next_task_id_++;
  task.work = std::move(work);
  task.completion = std::move(completion);
  pending_.push_back(std::move(task));
  ++metrics_.submitted_count;
  ++metrics_.queued_count;
  metrics_.queue_depth = std::max(metrics_.queue_depth, pending_.size());
  metrics_.max_inflight_count = std::max(metrics_.max_inflight_count, pending_.size());
  return {true, pending_.back().id, {}};
}

std::vector<TaskCompletion> DeterministicTaskExecutor::run_ready(std::size_t max_tasks,
                                                                 CancellationToken cancel_token) {
  std::vector<TaskCompletion> completions;
  while (!pending_.empty() && (max_tasks == 0u || completions.size() < max_tasks)) {
    if (cancel_token.cancel_requested()) {
      ++metrics_.cancellation_requested_count;
      ++metrics_.cancellation_observed_count;
      cancel_pending();
      break;
    }
    auto task = std::move(pending_.front());
    pending_.pop_front();
    metrics_.active_count = 1u;
    TaskCompletion completion;
    completion.task_id = task.id;
    const auto started_at = std::chrono::steady_clock::now();
    try {
      completion.payload = make_shared_payload(task.work());
      completion.ok = true;
      ++metrics_.completed_count;
    } catch (const std::exception& error) {
      completion.ok = false;
      completion.error = error.what();
      ++metrics_.failed_count;
    }
    const auto finished_at = std::chrono::steady_clock::now();
    if (config_.task_budget.count() > 0 && finished_at - started_at > config_.task_budget) {
      ++metrics_.timeout_budget_exceeded_count;
    }
    metrics_.active_count = 0u;
    if (task.completion) {
      task.completion(completion);
    }
    completions.push_back(std::move(completion));
  }
  metrics_.queue_depth = std::max(metrics_.queue_depth, pending_.size());
  return completions;
}

std::size_t DeterministicTaskExecutor::cancel_pending() {
  const auto count = pending_.size();
  pending_.clear();
  metrics_.cancelled_count += count;
  metrics_.queue_depth = 0u;
  return count;
}

TaskExecutorMetrics DeterministicTaskExecutor::metrics() const {
  auto metrics = metrics_;
  metrics.queue_depth = pending_.size();
  metrics.completed_backlog_depth = 0u;
  return metrics;
}

ThreadedTaskExecutor::ThreadedTaskExecutor(ThreadedTaskExecutorConfig config) : config_(std::move(config)) {
  config_.max_inflight = normalize_positive(config_.max_inflight);
  config_.max_workers = normalize_positive(config_.max_workers);
  start_workers();
}

ThreadedTaskExecutor::~ThreadedTaskExecutor() {
  shutdown_workers();
}

std::size_t ThreadedTaskExecutor::admission_capacity_locked() const {
  return config_.max_inflight + config_.queue_capacity;
}

bool ThreadedTaskExecutor::drop_oldest_on_overflow_locked() const {
  return drop_oldest_overflow(config_.overflow);
}

bool ThreadedTaskExecutor::cancel_pending_on_shutdown_locked() const {
  return cancel_pending_shutdown_policy(config_.shutdown_policy);
}

ThreadedTaskExecutor::RejectedSubmission ThreadedTaskExecutor::reject_submission_locked(std::string reason) {
  ++metrics_.rejected_count;
  RejectedSubmission rejected;
  rejected.result = {false, 0u, reason};
  rejected.sink = health_events_;
  rejected.event = make_reject_health_event_locked(reason);
  return rejected;
}

std::optional<HealthEvent> ThreadedTaskExecutor::make_reject_health_event_locked(const std::string& reason) const {
  if (health_events_ == nullptr) {
    return std::nullopt;
  }
  HealthEvent event;
  event.kind = HealthEventKind::kTaskReject;
  event.source = "task_executor";
  event.policy = config_.overflow;
  event.reason = reason;
  event.depth = pending_.size() + metrics_.active_count + completed_.size();
  event.capacity = admission_capacity_locked();
  return event;
}

void ThreadedTaskExecutor::emit_reject_health_event(RejectedSubmission& rejected) {
  if (rejected.sink != nullptr && rejected.event.has_value()) {
    rejected.sink->emit(std::move(*rejected.event));
  }
}

void ThreadedTaskExecutor::set_health_event_sink(HealthEventSink* sink) {
  std::lock_guard lock(mutex_);
  health_events_ = sink;
}

void ThreadedTaskExecutor::cancel_pending_locked() {
  metrics_.cancelled_count += pending_.size();
  pending_.clear();
  metrics_.queue_depth = 0u;
}

void ThreadedTaskExecutor::start_workers() {
  workers_.reserve(config_.max_workers);
  for (std::size_t index = 0; index < config_.max_workers; ++index) {
    workers_.emplace_back([this]() { worker_loop(); });
  }
}

TaskSubmissionResult ThreadedTaskExecutor::submit(Work work, CompletionCallback completion) {
  if (!work) {
    RejectedSubmission rejected;
    {
      std::lock_guard lock(mutex_);
      rejected = reject_submission_locked("task work must not be empty");
    }
    emit_reject_health_event(rejected);
    return rejected.result;
  }

  TaskSubmissionResult accepted;
  RejectedSubmission rejected;
  bool was_rejected = false;
  {
    std::lock_guard lock(mutex_);
    if (stopping_) {
      rejected = reject_submission_locked("task executor is shutting down");
      was_rejected = true;
    } else {
      const auto capacity = admission_capacity_locked();
      const auto outstanding = pending_.size() + metrics_.active_count + completed_.size();
      if (outstanding >= capacity) {
        if (drop_oldest_on_overflow_locked() && !pending_.empty()) {
          pending_.pop_front();
          ++metrics_.cancelled_count;
        } else {
          rejected = reject_submission_locked(config_.overflow == "fail_fast" ? "task executor capacity exceeded"
                                                                              : "task executor queue full");
          was_rejected = true;
        }
      }
    }
    if (!was_rejected) {
      PendingTask task;
      task.id = next_task_id_++;
      task.work = std::move(work);
      task.completion = std::move(completion);
      pending_.push_back(std::move(task));
      ++metrics_.submitted_count;
      ++metrics_.queued_count;
      metrics_.queue_depth = pending_.size();
      metrics_.completed_backlog_depth = completed_.size();
      metrics_.max_inflight_count =
          std::max(metrics_.max_inflight_count, pending_.size() + metrics_.active_count + completed_.size());
      accepted = {true, pending_.back().id, {}};
    }
  }
  if (was_rejected) {
    emit_reject_health_event(rejected);
    return rejected.result;
  }
  work_available_.notify_one();
  return accepted;
}

std::vector<TaskCompletion> ThreadedTaskExecutor::run_ready(std::size_t max_tasks, CancellationToken cancel_token) {
  std::lock_guard lock(mutex_);
  if (cancel_token.cancel_requested()) {
    ++metrics_.cancellation_requested_count;
    ++metrics_.cancellation_observed_count;
    cancel_pending_locked();
    work_available_.notify_all();
  }

  std::vector<TaskCompletion> completions;
  const auto count = completion_limit(max_tasks, completed_.size());
  completions.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    completions.push_back(std::move(completed_.front()));
    completed_.pop_front();
  }
  metrics_.queue_depth = pending_.size();
  metrics_.completed_backlog_depth = completed_.size();
  return completions;
}

std::size_t ThreadedTaskExecutor::cancel_pending() {
  std::lock_guard lock(mutex_);
  const auto count = pending_.size();
  cancel_pending_locked();
  work_available_.notify_all();
  if (metrics_.active_count == 0u && pending_.empty()) {
    idle_.notify_all();
  }
  return count;
}

TaskExecutorMetrics ThreadedTaskExecutor::metrics() const {
  std::lock_guard lock(mutex_);
  auto metrics = metrics_;
  metrics.queue_depth = pending_.size();
  metrics.completed_backlog_depth = completed_.size();
  return metrics;
}

void ThreadedTaskExecutor::shutdown() {
  shutdown_workers();
}

void ThreadedTaskExecutor::shutdown_workers() {
  std::vector<std::thread> workers;
  {
    std::lock_guard lock(mutex_);
    if (stopping_ && workers_.empty()) {
      return;
    }
    stopping_ = true;
    if (cancel_pending_on_shutdown_locked()) {
      cancel_pending_locked();
    }
    workers = std::move(workers_);
  }
  work_available_.notify_all();
  for (auto& worker : workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

bool ThreadedTaskExecutor::wait_for_idle(std::chrono::milliseconds timeout) {
  std::unique_lock lock(mutex_);
  return idle_.wait_for(lock, timeout, [this]() { return pending_.empty() && metrics_.active_count == 0u; });
}

TaskCompletion ThreadedTaskExecutor::execute_task(PendingTask& task) {
  TaskCompletion completion;
  completion.task_id = task.id;
  const auto started_at = std::chrono::steady_clock::now();
  try {
    completion.payload = make_shared_payload(task.work());
    completion.ok = true;
  } catch (const std::exception& error) {
    completion.ok = false;
    completion.error = error.what();
  }
  const auto finished_at = std::chrono::steady_clock::now();
  if (config_.task_budget.count() > 0 && finished_at - started_at > config_.task_budget) {
    std::lock_guard lock(mutex_);
    ++metrics_.timeout_budget_exceeded_count;
  }
  return completion;
}

void ThreadedTaskExecutor::worker_loop() {
  for (;;) {
    PendingTask task;
    {
      std::unique_lock lock(mutex_);
      work_available_.wait(lock, [this]() {
        const auto can_start = !pending_.empty() && metrics_.active_count < config_.max_inflight;
        return can_start || (stopping_ && (pending_.empty() || cancel_pending_on_shutdown_locked()));
      });
      if (stopping_ && (pending_.empty() || cancel_pending_on_shutdown_locked())) {
        break;
      }
      if (pending_.empty() || metrics_.active_count >= config_.max_inflight) {
        continue;
      }
      task = std::move(pending_.front());
      pending_.pop_front();
      ++metrics_.active_count;
      metrics_.queue_depth = pending_.size();
      metrics_.max_inflight_count = std::max(metrics_.max_inflight_count, pending_.size() + metrics_.active_count);
    }

    auto completion = execute_task(task);
    if (task.completion) {
      task.completion(completion);
    }

    {
      std::lock_guard lock(mutex_);
      if (completion.ok) {
        ++metrics_.completed_count;
      } else {
        ++metrics_.failed_count;
      }
      if (metrics_.active_count > 0u) {
        --metrics_.active_count;
      }
      completed_.push_back(std::move(completion));
      metrics_.queue_depth = pending_.size();
      metrics_.completed_backlog_depth = completed_.size();
      if (pending_.empty() && metrics_.active_count == 0u) {
        idle_.notify_all();
      }
    }
    work_available_.notify_all();
  }
}

} // namespace topoexec
