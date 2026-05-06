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

`thread_pool` can overlap invocations inside a component step, but downstream compiled regions wait until the admitted worker work has drained and immediate publications have been committed.

## Lane Types

| Lane type | Runtime behavior | Enforced today | Not enforced today |
| --- | --- | --- | --- |
| `event_loop` | Deterministic in-process execution in compiled region order. | Region order, trigger readiness, edge commit boundaries, stop-token checks before iterations. | Wall-clock rate, OS priority/affinity/RT policy. |
| `fixed_rate` | Accepted by schema and executed through bounded simulated ticks. | Bounded tick count, component budget metric checks, simulated overrun count, last callback duration, and positive jitter when iteration duration exceeds `hz`, `period_ms`, or `tick_budget_ms`. | Real wall-clock sleep cadence and OS jitter control. |
| `thread_pool` | Persistent worker-pool execution for ready invocations. | `max_threads` persistent lane workers, bounded FIFO queue admission, optional `queue_capacity`, overflow admission, non-reentrant serialization, reentrant overlap within the lane bound, worker-id trace attributes, region barrier before downstream work. | Priority queue/admission ordering, OS priority/affinity/RT policy, hard thread-name guarantee, timeout preemption. |
| future `isolated_thread` | Dedicated thread per lane or component. | Not supported by schema v1/runtime. | All behavior future. |
| future `manual_step` | Host application manually advances a lane. | Not supported by schema v1/runtime. | All behavior future. |

`topoexec graph plan --format json` includes a `lane_capabilities[]` summary so
tooling can see what each lane type actually implements, which fields are
advisory, and which capabilities remain future extensions.

## Persistent Thread Pool v1

`thread_pool` owns a run-scoped persistent worker pool. Workers start when the
runtime run starts, wait on a bounded FIFO queue, and stop/join during runtime
cleanup after admitted work drains. The lane is still an in-process alpha
concurrency surface, not a hard real-time scheduler.

```yaml
lanes:
  pool:
    type: thread_pool
    max_threads: 4
```

Runtime rules:

- `max_threads` is the persistent worker count and maximum active worker width; `0` or an omitted value means one worker.
- `queue_capacity` bounds ready invocations waiting behind active workers when positive; `0` preserves only the active worker width.
- `overflow` controls over-capacity ready invocations: `drop_oldest`/`overwrite` keep the newest admitted work, `drop_newest`/`reject`/`reject_new`/`block` keep the oldest admitted work in the non-blocking runtime, and `fail_fast` stops the run with an error.
- `execution.reentrant: false` permits at most one in-flight invocation for that component.
- `execution.reentrant: true` permits overlap up to the lane `max_threads` bound.
- Lane admission is FIFO for admitted ready invocations; priority queues are future work.
- `thread_name` is applied as a best-effort worker thread name on supported platforms and remains advisory as a portable contract.
- Downstream regions do not run until the current admitted worker work has drained and immediate publications have been committed.

Test coverage:

- `Runtime.ThreadPoolLaneExecutesReentrantInvocationsConcurrently` proves overlap and the `max_threads` upper bound.
- `Runtime.ThreadPoolWorkersPersistAcrossMultipleRuntimeSteps` proves workers remain bounded and reused across runtime steps.
- `Runtime.ThreadPoolLaneSerializesNonReentrantInvocations` proves non-reentrant no-overlap.
- `Runtime.ThreadPoolStopWhileQueueNonEmptyDrainsAdmittedWork` proves stop requests do not deadlock with queued admitted work.
- `Runtime.ThreadPoolLaneQueueCapacityRejectsNewestWhenFull` and `Runtime.ThreadPoolLaneQueueCapacityDropsOldestWhenConfigured` prove explicit lane admission behavior and rejected-count metrics.
- `Runtime.ThreadPoolExecuteStatusFailureKeepsStructuredRuntimeError` proves current fail-fast execute errors remain structured on worker lanes.
- `Runtime.PublishStagesWithoutRecursiveDownstreamExecute` protects the no-recursive-publish boundary that worker lanes must preserve.

## Stop, Drain, And Cleanup

`RuntimeRunnerOptions::stop_token` is checked before each scheduler iteration. If stop is requested:

- no new iteration starts;
- already-started components are deactivated in reverse startup order;
- `RuntimeRunnerResult::scheduler_stop_reason` is `stop_requested`.

For `thread_pool`, a stop request prevents new scheduler iterations and prevents
new worker submissions at the next scheduler stop check. Already-admitted worker
queue items drain cooperatively, then the persistent pool wakes idle workers,
stops, and joins during runtime cleanup. Component code should check
`Invocation::stop_requested` for long-running work. Timeout-based preemption is
not implemented.

Component errors stop the runtime with `SchedulerStopReason::kError`; already-started components still receive reverse-order deactivate cleanup.

## Metrics And Trace

Scheduler metrics are emitted through `RuntimeRunnerResult::runtime_metrics`:

- `runtime.scheduler.completed_count`
- `runtime.scheduler.tick_overrun_count`
- `runtime.scheduler.queue_depth`
- `runtime.scheduler.queue_capacity`
- `runtime.scheduler.worker_count`
- `runtime.scheduler.last_callback_duration_ms`
- `runtime.scheduler.tick_jitter_ms`
- `runtime.scheduler.active_count`
- `runtime.scheduler.in_flight_count`
- `runtime.scheduler.rejected_count`

For `event_loop` and simulated `fixed_rate`, worker/queue metrics remain zero unless a future implementation adds real queues. For `thread_pool`, `worker_count`, `queue_capacity`, `queue_depth`, `active_count`, `in_flight_count`, `completed_count`, and `rejected_count` describe the maximum admitted persistent-pool work observed during the run.

Trace events around scheduler and component execution include:

- `scheduler_iteration_begin`
- `scheduler_iteration`
- `scheduler_iteration_end`
- `component_execute_begin`
- `component_execute`
- `component_execute_end`
- `thread_pool_batch`

`component_execute*` events/spans on a `thread_pool` lane include `worker_id`.
`thread_pool_batch` spans include `worker_ids` for the workers that executed the
admitted work.

## Advisory Policy Fields

The following schema fields are parsed and preserved but advisory in the current runtime:

- lane `priority`;
- lane `thread_name` as a portable guarantee; it is only best-effort for `thread_pool` workers on supported platforms;
- lane `cpu_affinity`;
- lane `nice_priority`;
- lane `rt_policy`;
- lane `rt_priority`;
- lane `isolation_intent`;
- execution `priority`.

The runtime must not claim OS priority, CPU affinity, hard real-time scheduling, or a portable hard worker-name guarantee until platform-specific enforcement and tests exist.

When advisory fields are set to non-default values, validation still succeeds
but emits machine-readable diagnostics:

- `advisory_lane_field_ignored` for lane `priority`, `cpu_affinity`,
  `nice_priority`, `rt_policy`, `rt_priority`, `isolation_intent`, and
  `wall_clock_enabled` where applicable; `thread_name` also uses this diagnostic
  when it cannot be applied to persistent worker threads on the current runtime
  surface.
- `advisory_execution_field_ignored` for component `execution.priority`.

These diagnostics are warnings/advisories, not validation failures. They exist to
prevent schema fields from looking implemented merely because they parse.

## Remaining Work

- Wall-clock fixed-rate sleep cadence and OS jitter controls.
- Priority queue/admission ordering for worker queues.
- Timeout preemption or explicit cancellation policy.
- Platform-specific priority, affinity, and RT helpers.

GitHub Actions has a non-blocking ThreadSanitizer job; keep it green before wider beta concurrency claims.
