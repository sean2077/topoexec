#pragma once

// API stability: experimental. Deterministic task helper may change before threaded executor v2.

#include "topoexec/runtime/payload.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace topoexec {

struct TaskExecutorConfig {
  std::size_t max_inflight{1};
  std::size_t queue_capacity{0};
  std::string overflow{"reject"};
};

struct TaskExecutorMetrics {
  std::size_t submitted_count{0};
  std::size_t active_count{0};
  std::size_t completed_count{0};
  std::size_t cancelled_count{0};
  std::size_t rejected_count{0};
  std::size_t failed_count{0};
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

class TaskExecutor {
public:
  using Work = std::function<RuntimePayload()>;
  using CompletionCallback = std::function<void(const TaskCompletion&)>;

  explicit TaskExecutor(TaskExecutorConfig config = {});

  TaskSubmissionResult submit(Work work, CompletionCallback completion = {});
  std::vector<TaskCompletion> run_ready(std::size_t max_tasks = 0);
  std::size_t cancel_pending();
  TaskExecutorMetrics metrics() const;

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

} // namespace topoexec
