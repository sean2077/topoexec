# Scheduler Semantics

TopoExec schema v1 names scheduler lanes and keeps runtime behavior conservative at the graph boundary: compiled region order is still the semantic ordering contract, even when a lane uses worker threads internally.

## Lane Types

- `event_loop`: production-supported in `v0.1.0-alpha`. Components execute deterministically in compiled region order inside one process.
- `fixed_rate`: accepted by schema and simulated through bounded runtime ticks. It does not yet sleep to real wall-clock periods.
- `thread_pool`: bounded MVP support for ready invocations. The runtime executes ready invocations on worker threads up to `max_threads`, waits at the component/region barrier, then continues compiled region order.

`max_threads` controls the worker batch width. If omitted or zero, the runtime uses one worker. Priority, affinity, nice priority, RT policy, thread names, and isolation intent are retained in schema data but are advisory in the current MVP.

## Reentrancy

`execution.reentrant` records whether a component may overlap with itself.

- `reentrant: false` permits at most one in-flight invocation per component, even on `thread_pool`.
- `reentrant: true` may overlap within the lane `max_threads` bound.
- `runtime.component.max_in_flight_count` records the observed maximum overlap.

The worker lane is barriered: downstream regions do not run until the current component's admitted worker batch finishes and immediate publications are committed. This preserves the existing no-recursive-publish and compiled-order contracts.

## Stop And Cleanup

`RuntimeRunnerOptions::stop_token` is checked before each scheduler iteration. If stop is requested:

- no new iteration starts;
- already-started components are deactivated in reverse startup order;
- `RuntimeRunnerResult::scheduler_stop_reason` is `stop_requested`.

For a `thread_pool` lane, a stop request also prevents new worker batches from being launched. Already-started component invocations are drained cooperatively; component code should check `Invocation::stop_requested` for long-running work.

Component errors stop the runtime with `SchedulerStopReason::kError`; already-started components still receive deactivate cleanup.

## Metrics

Scheduler metrics are part of the observability contract even before worker-pool scheduling:

- `runtime.scheduler.completed_count`
- `runtime.scheduler.queue_depth`
- `runtime.scheduler.active_count`
- `runtime.scheduler.in_flight_count`
- `runtime.scheduler.rejected_count`

For `event_loop`, worker/queue metrics remain zero. For `thread_pool`, `worker_count`, `queue_capacity`, `active_count`, and `in_flight_count` reflect the bounded worker batches observed during the run.

## Remaining Work

The MVP does not yet enforce OS priority, CPU affinity, real-time policy, named persistent worker threads, or timeout-based preemption. GitHub Actions has a non-blocking ThreadSanitizer job; require it to stay green before wider beta claims.
