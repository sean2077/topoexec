# Scheduler Semantics

TopoExec schema v1 names scheduler lanes, but alpha runtime behavior is intentionally conservative.

## Lane Types

- `event_loop`: production-supported in `v0.1.0-alpha`. Components execute deterministically in compiled region order inside one process.
- `fixed_rate`: accepted by schema and simulated through bounded runtime ticks. It does not yet sleep to real wall-clock periods.
- `thread_pool`: schema-visible for forward compatibility, but `RuntimeRunner` rejects it in `run` mode until the worker-pool MVP lands.

Priority, affinity, nice priority, RT policy, thread names, and isolation intent are retained in schema data but are advisory/unused until real worker-pool scheduling is implemented.

## Reentrancy

`execution.reentrant` records whether a component may overlap with itself. The current event-loop runtime also guards non-reentrant components with an in-flight check. Because execution is single-threaded today, normal graphs observe `runtime.component.max_in_flight_count <= 1`.

When worker-pool scheduling lands:

- `reentrant: false` must permit at most one in-flight invocation per component.
- `reentrant: true` may overlap only within lane worker and queue bounds.
- queue capacity, admission rejection, and shutdown policy must be deterministic and metrics-backed.

## Stop And Cleanup

`RuntimeRunnerOptions::stop_token` is checked before each scheduler iteration. If stop is requested:

- no new iteration starts;
- already-started components are deactivated in reverse startup order;
- `RuntimeRunnerResult::scheduler_stop_reason` is `stop_requested`.

Component errors stop the runtime with `SchedulerStopReason::kError`; already-started components still receive deactivate cleanup.

## Metrics

Scheduler metrics are part of the observability contract even before worker-pool scheduling:

- `runtime.scheduler.completed_count`
- `runtime.scheduler.queue_depth`
- `runtime.scheduler.active_count`
- `runtime.scheduler.in_flight_count`
- `runtime.scheduler.rejected_count`

For `event_loop`, worker/queue metrics remain zero unless a future scheduler backend populates them.

## Deferred Work

Worker-pool scheduling and async max-inflight admission are deferred beyond `v0.1.0-alpha`. They must land with bounded queues, deterministic shutdown, non-reentrant protection, and TSAN coverage before being described as production runtime behavior.
