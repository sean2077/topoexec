# Async Tasks

Async edges and async task execution are separate concepts in TopoExec.

- `async` edges are graph semantics: a completion/event publication is deferred to a later epoch and is admitted through `policy.max_inflight` plus channel capacity.
- `ITaskExecutor` is the optional task-executor interface for bounded task submission, cancellation, completion callbacks, and metrics.
- `DeterministicTaskExecutor` is the explicit deterministic implementation.
- `ThreadedTaskExecutor` is an opt-in preview for bounded worker-backed task execution. It is not used unless an embedder attaches it to `GraphContext::task_executor`.

The core runtime does not require a task executor. Applications can attach any `ITaskExecutor` implementation to `GraphContext::task_executor` and call `GraphContext::submit_task(port, work)` from a component. The completion callback publishes the returned payload to `component_id.port`, so normal async-edge routing can carry the completion to downstream `task_ready` / `future_ready` triggers. Completion callbacks route through the runtime publisher or channel bus; they do not call downstream components directly. Task completions inherit the submitting invocation metadata so downstream `task_ready` / `future_ready` invocations keep the original correlation id and receive a new channel-message causation id.

## Bounded admission

`TaskExecutorConfig` fields shared by deterministic and threaded executors:

- `max_inflight`: maximum outstanding deterministic tasks or concurrently active threaded tasks; `0` is normalized to `1`.
- `queue_capacity`: additional pending backlog beyond `max_inflight`.
- `overflow`: `reject` / `drop_newest` / `block` reject new work in the current non-blocking executor; `drop_oldest` / `overwrite` cancel the oldest pending work to accept new work; `fail_fast` reports a capacity exceeded reason.
- `task_budget`: optional cooperative duration budget for each task. Exceeding it records `timeout_budget_exceeded_count` after the task returns; it does not interrupt the task.

`ThreadedTaskExecutorConfig` adds:

- `max_workers`: maximum worker threads in the preview pool; `0` is normalized to `1`.
- `shutdown_policy`: `drain` by default. `cancel_pending` / `cancel` / `discard_pending` cancel pending work during shutdown. Active work is not forcibly killed.

The executor never creates an unbounded task backlog. For
`ThreadedTaskExecutor`, completion records waiting for `run_ready()` count
toward the admission bound, so an embedder that never drains completions cannot
grow memory without limit.

Rejected task submissions increment `TaskExecutorMetrics::rejected_count` and, when a bounded `HealthEventSink` is attached, emit a `task_reject` health event with the executor overflow policy, reason, depth, and capacity. The threaded executor builds the event while holding its internal mutex but emits it after releasing that mutex. The event is observer-only; it does not run graph components or retry work.

## Deterministic execution

`DeterministicTaskExecutor::run_ready(max_tasks, cancel_token)` runs pending work synchronously in FIFO order on the caller thread. If the token is already requested before the next task starts, the executor cancels pending tasks and returns without forced termination. This remains the default test-friendly mode.

## Threaded preview

`ThreadedTaskExecutor` starts a bounded worker pool at construction. `submit()` admits work into a bounded pending queue, workers execute admitted tasks up to `min(max_workers, max_inflight)` active tasks, and completion callbacks run exactly once per completed task. `run_ready(max_tasks, cancel_token)` drains completion records already produced by workers; it does not execute tasks synchronously. `wait_for_idle(timeout)` is a preview-only helper for tests and embedders that need to wait until pending and active work reach zero. If completed records are not drained, `completed_backlog_depth` grows only up to the same admission capacity and new submissions are rejected or handled by overflow policy.

Cancellation remains cooperative: `cancel_pending()` and cancellation passed to `run_ready()` remove pending tasks, while already active C++ work runs until it returns. `shutdown()` joins worker threads after draining or cancelling pending tasks according to `shutdown_policy`; no hard thread termination is attempted.

## Metrics

`TaskExecutorMetrics` exposes:

- `submitted_count`
- `queued_count`
- `active_count`
- `completed_count`
- `cancelled_count`
- `rejected_count`
- `failed_count`
- `cancellation_requested_count`
- `cancellation_observed_count`
- `timeout_budget_exceeded_count`
- `max_inflight_count`
- `queue_depth`
- `completed_backlog_depth`

Task failures become `TaskCompletion{ok=false, error=...}`; they do not throw out of `run_ready()`.
