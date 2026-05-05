# Scheduler Semantics

TopoExec schema v1 names scheduler lanes, but compiled region order remains the semantic ordering contract. Scheduler lanes decide how a ready component invocation is executed; they do not change edge visibility, CompositeLoop ownership, or the rule that `GraphContext::publish()` stages through runtime-owned routing.

## Execution Shape

```text
epoch
  -> begin deferred commit boundary
  -> compiled region 1
       -> collect ready invocations
       -> execute according to lane
       -> commit immediate publications at component/region barrier
  -> compiled region N
  -> end epoch
```

`thread_pool` can overlap invocations inside a component step, but downstream compiled regions wait until that worker batch has drained and immediate publications have been committed.

## Lane Types

| Lane type | Runtime behavior | Enforced today | Not enforced today |
| --- | --- | --- | --- |
| `event_loop` | Deterministic in-process execution in compiled region order. | Region order, trigger readiness, edge commit boundaries, stop-token checks before iterations. | Wall-clock rate, OS priority/affinity/RT policy. |
| `fixed_rate` | Accepted by schema and executed through bounded simulated ticks. | Bounded tick count, component budget metric checks, and simulated overrun count when iteration duration exceeds `1 / hz`. | Real wall-clock sleep cadence and jitter control. |
| `thread_pool` | Bounded worker-batch execution for ready invocations. | `max_threads` batch width, non-reentrant serialization, reentrant overlap within the lane bound, region barrier before downstream work. | Persistent worker lifecycle, OS priority/affinity/RT policy, worker naming, timeout preemption. |

## Thread Pool MVP

`thread_pool` is a bounded MVP, not a persistent production worker pool yet.

```yaml
lanes:
  pool:
    type: thread_pool
    max_threads: 4
```

Runtime rules:

- `max_threads` is the maximum worker batch width; `0` or an omitted value means one worker.
- `execution.reentrant: false` permits at most one in-flight invocation for that component.
- `execution.reentrant: true` permits overlap up to the lane `max_threads` bound.
- The current implementation launches bounded batches and waits for them. It does not keep named persistent worker threads alive between batches.
- The ready queue is bounded by the ready invocation set for the current component step; excess persistent queueing is not implemented.
- Downstream regions do not run until the current worker batch has drained and immediate publications have been committed.

Test coverage:

- `Runtime.ThreadPoolLaneExecutesReentrantInvocationsConcurrently` proves overlap and the `max_threads` upper bound.
- `Runtime.ThreadPoolLaneSerializesNonReentrantInvocations` proves non-reentrant no-overlap.
- `Runtime.PublishStagesWithoutRecursiveDownstreamExecute` protects the no-recursive-publish boundary that worker lanes must preserve.

## Stop, Drain, And Cleanup

`RuntimeRunnerOptions::stop_token` is checked before each scheduler iteration. If stop is requested:

- no new iteration starts;
- already-started components are deactivated in reverse startup order;
- `RuntimeRunnerResult::scheduler_stop_reason` is `stop_requested`.

For `thread_pool`, a stop request also prevents new worker batches from being launched. Already-started invocations are drained cooperatively; component code should check `Invocation::stop_requested` for long-running work. Timeout-based preemption is not implemented.

Component errors stop the runtime with `SchedulerStopReason::kError`; already-started components still receive reverse-order deactivate cleanup.

## Metrics And Trace

Scheduler metrics are emitted through `RuntimeRunnerResult::runtime_metrics`:

- `runtime.scheduler.completed_count`
- `runtime.scheduler.tick_overrun_count`
- `runtime.scheduler.queue_depth`
- `runtime.scheduler.active_count`
- `runtime.scheduler.in_flight_count`
- `runtime.scheduler.rejected_count`

For `event_loop` and simulated `fixed_rate`, worker/queue metrics remain zero unless a future implementation adds real queues. For `thread_pool`, `worker_count`, `queue_capacity`, `active_count`, and `in_flight_count` describe the maximum bounded batch observed during the run.

Trace events around scheduler and component execution include:

- `scheduler_iteration_begin`
- `scheduler_iteration`
- `scheduler_iteration_end`
- `component_execute_begin`
- `component_execute`
- `component_execute_end`

The trace surface can show component execution spans and lane names, but current trace output does not yet expose persistent worker ids because persistent workers are not implemented.

## Advisory Policy Fields

The following schema fields are parsed and preserved but advisory in the current runtime:

- lane `priority`;
- lane `thread_name`;
- lane `cpu_affinity`;
- lane `nice_priority`;
- lane `rt_policy`;
- lane `rt_priority`;
- lane `isolation_intent`;
- execution `priority`.

The runtime must not claim OS priority, CPU affinity, hard real-time scheduling, or named persistent workers until platform-specific enforcement and tests exist.

## Remaining Work

- Persistent worker-pool lifecycle.
- Wall-clock fixed-rate sleep and jitter controls.
- Queue rejection policy for a persistent worker queue.
- Timeout preemption or explicit cancellation policy.
- Platform-specific priority, affinity, and RT helpers.

GitHub Actions has a non-blocking ThreadSanitizer job; keep it green before wider beta concurrency claims.
