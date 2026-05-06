#include "topoexec/runtime/task_executor.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <utility>

namespace topoexec {

TaskExecutor::TaskExecutor(TaskExecutorConfig config) : config_(std::move(config)) {
  if (config_.max_inflight == 0u) {
    config_.max_inflight = 1u;
  }
}

std::size_t TaskExecutor::admission_capacity() const {
  return config_.max_inflight + config_.queue_capacity;
}

bool TaskExecutor::drop_oldest_on_overflow() const {
  return config_.overflow == "drop_oldest" || config_.overflow == "overwrite";
}

TaskSubmissionResult TaskExecutor::reject_submission(std::string reason) {
  ++metrics_.rejected_count;
  return {false, 0u, std::move(reason)};
}

TaskSubmissionResult TaskExecutor::submit(Work work, CompletionCallback completion) {
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
  metrics_.queue_depth = std::max(metrics_.queue_depth, pending_.size());
  metrics_.max_inflight_count = std::max(metrics_.max_inflight_count, pending_.size());
  return {true, pending_.back().id, {}};
}

std::vector<TaskCompletion> TaskExecutor::run_ready(std::size_t max_tasks, CancellationToken cancel_token) {
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

std::size_t TaskExecutor::cancel_pending() {
  const auto count = pending_.size();
  pending_.clear();
  metrics_.cancelled_count += count;
  metrics_.queue_depth = 0u;
  return count;
}

TaskExecutorMetrics TaskExecutor::metrics() const {
  auto metrics = metrics_;
  metrics.queue_depth = pending_.size();
  return metrics;
}

} // namespace topoexec
