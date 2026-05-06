# Async Tasks

Async edges and async task execution are separate concepts in TopoExec.

- `async` edges are graph semantics: a completion/event publication is deferred to a later epoch and is admitted through `policy.max_inflight` plus channel capacity.
- `TaskExecutor` is an optional embeddable helper for deterministic task submission, bounded backlog, cancellation, and completion callbacks.

The core runtime does not require a task executor. Applications can attach one to `GraphContext::task_executor` and call `GraphContext::submit_task(port, work)` from a component. The completion callback publishes the returned payload to `component_id.port`, so normal async-edge routing can carry the completion to downstream `task_ready` / `future_ready` triggers.

## Bounded admission

`TaskExecutorConfig` fields:

- `max_inflight`: maximum outstanding deterministic tasks; `0` is normalized to `1`.
- `queue_capacity`: additional pending backlog beyond `max_inflight`.
- `overflow`: `reject` / `drop_newest` / `block` reject new work in the current non-blocking executor; `drop_oldest` / `overwrite` cancel the oldest pending work to accept new work; `fail_fast` reports a capacity exceeded reason.
- `task_budget`: optional cooperative duration budget for each task. Exceeding it records `timeout_budget_exceeded_count` after the task returns; it does not interrupt the task.

The executor never creates an unbounded task backlog.

## Deterministic execution

`run_ready(max_tasks, cancel_token)` runs pending work synchronously in FIFO order on the caller thread. If the token is already requested before the next task starts, the executor cancels pending tasks and returns without forced termination. This makes tests deterministic and keeps the helper independent from scheduler lane implementation. A future threaded executor can reuse the same admission and metrics contract.

## Metrics

`TaskExecutorMetrics` exposes:

- `submitted_count`
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

Task failures become `TaskCompletion{ok=false, error=...}`; they do not throw out of `run_ready()`.
