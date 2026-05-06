#pragma once

// API stability: experimental. Deterministic and threaded task helpers may change before beta.

#include "topoexec/runtime/cancellation.hpp"
#include "topoexec/runtime/payload.hpp"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace topoexec {

struct TaskExecutorConfig {
  std::size_t max_inflight{1};
  std::size_t queue_capacity{0};
  std::string overflow{"reject"};
  std::chrono::milliseconds task_budget{0};
};

struct ThreadedTaskExecutorConfig : TaskExecutorConfig {
  std::size_t max_workers{1};
  std::string shutdown_policy{"drain"};
};

struct TaskExecutorMetrics {
  std::size_t submitted_count{0};
  std::size_t queued_count{0};
  std::size_t active_count{0};
  std::size_t completed_count{0};
  std::size_t cancelled_count{0};
  std::size_t rejected_count{0};
  std::size_t failed_count{0};
  std::size_t cancellation_requested_count{0};
  std::size_t cancellation_observed_count{0};
  std::size_t timeout_budget_exceeded_count{0};
  std::size_t max_inflight_count{0};
  std::size_t queue_depth{0};
};

struct TaskSubmissionResult {
  bool accepted{false};
  std::uint64_t task_id{0};
  std::string reason;
};

struct TaskCompletion {
  std::uint64_t task_id{0};
  bool ok{false};
  RuntimePayloadPtr payload;
  std::string error;
};

class ITaskExecutor {
public:
  using Work = std::function<RuntimePayload()>;
  using CompletionCallback = std::function<void(const TaskCompletion&)>;

  virtual ~ITaskExecutor() = default;

  virtual TaskSubmissionResult submit(Work work, CompletionCallback completion = {}) = 0;
  virtual std::vector<TaskCompletion> run_ready(std::size_t max_tasks = 0, CancellationToken cancel_token = {}) = 0;
  virtual std::size_t cancel_pending() = 0;
  virtual TaskExecutorMetrics metrics() const = 0;
  virtual void shutdown() {}
};

class DeterministicTaskExecutor : public ITaskExecutor {
public:
  explicit DeterministicTaskExecutor(TaskExecutorConfig config = {});

  TaskSubmissionResult submit(Work work, CompletionCallback completion = {}) override;
  std::vector<TaskCompletion> run_ready(std::size_t max_tasks = 0, CancellationToken cancel_token = {}) override;
  std::size_t cancel_pending() override;
  TaskExecutorMetrics metrics() const override;

private:
  struct PendingTask {
    std::uint64_t id{0};
    Work work;
    CompletionCallback completion;
  };

  std::size_t admission_capacity() const;
  bool drop_oldest_on_overflow() const;
  TaskSubmissionResult reject_submission(std::string reason);

  TaskExecutorConfig config_;
  std::uint64_t next_task_id_{1};
  std::deque<PendingTask> pending_;
  TaskExecutorMetrics metrics_;
};

class ThreadedTaskExecutor : public ITaskExecutor {
public:
  explicit ThreadedTaskExecutor(ThreadedTaskExecutorConfig config = {});
  ~ThreadedTaskExecutor() override;

  ThreadedTaskExecutor(const ThreadedTaskExecutor&) = delete;
  ThreadedTaskExecutor& operator=(const ThreadedTaskExecutor&) = delete;

  TaskSubmissionResult submit(Work work, CompletionCallback completion = {}) override;
  std::vector<TaskCompletion> run_ready(std::size_t max_tasks = 0, CancellationToken cancel_token = {}) override;
  std::size_t cancel_pending() override;
  TaskExecutorMetrics metrics() const override;
  void shutdown() override;
  bool wait_for_idle(std::chrono::milliseconds timeout);

private:
  struct PendingTask {
    std::uint64_t id{0};
    Work work;
    CompletionCallback completion;
  };

  std::size_t admission_capacity_locked() const;
  bool drop_oldest_on_overflow_locked() const;
  bool cancel_pending_on_shutdown_locked() const;
  TaskSubmissionResult reject_submission_locked(std::string reason);
  void cancel_pending_locked();
  void start_workers();
  void worker_loop();
  TaskCompletion execute_task(PendingTask& task);

  ThreadedTaskExecutorConfig config_;
  mutable std::mutex mutex_;
  std::condition_variable work_available_;
  std::condition_variable idle_;
  std::uint64_t next_task_id_{1};
  std::deque<PendingTask> pending_;
  std::deque<TaskCompletion> completed_;
  std::vector<std::thread> workers_;
  TaskExecutorMetrics metrics_;
  bool stopping_{false};
};

class TaskExecutor : public DeterministicTaskExecutor {
public:
  explicit TaskExecutor(TaskExecutorConfig config = {}) : DeterministicTaskExecutor(std::move(config)) {}
};

} // namespace topoexec
