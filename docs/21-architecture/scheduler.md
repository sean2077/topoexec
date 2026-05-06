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
| `event_loop` | Deterministic in-process execution in compiled region order. | Region order, runtime `execution.priority` ordering for independent ready regions, cooperative cancellation/timeout-budget observation, trigger readiness, edge commit boundaries, stop-token checks before iterations. | Wall-clock rate, OS priority/affinity/RT policy, hard timeout preemption. |
| `fixed_rate` | Deterministic simulated ticks by default, with opt-in wall-clock cadence v1. | Bounded tick count, runtime `execution.priority` ordering for independent ready regions, cooperative cancellation/timeout-budget observation, component budget metric checks, simulated overrun count, opt-in `wall_clock_enabled` sleeps between ticks, `overrun_policy`, tick/skipped/max-lateness metrics, and trace events. | Independent per-lane threads, hard real-time cadence, hard timeout preemption, and OS jitter control. |
| `thread_pool` | Persistent worker-pool execution for ready invocations. | `max_threads` persistent lane workers, bounded priority queue admission, optional `queue_capacity`, overflow admission with low-priority rejection metrics, cooperative cancellation/timeout-budget observation, non-reentrant serialization, reentrant overlap within the lane bound, worker-id trace attributes, region barrier before downstream work. | OS priority/affinity/RT policy, hard thread-name guarantee, hard timeout preemption, advanced starvation aging. |
| future `isolated_thread` | Dedicated thread per lane or component. | Not supported by schema v1/runtime. | All behavior future. |
| future `manual_step` | Host application manually advances a lane. | Not supported by schema v1/runtime. | All behavior future. |

`topoexec graph plan --format json` includes a `lane_capabilities[]` summary so
tooling can see what each lane type actually implements, which fields are
advisory, and which capabilities remain future extensions.

## Fixed Rate v1

`fixed_rate` defaults to deterministic stepping: each runner iteration is one
tick and no sleeping is inserted. This keeps normal tests reproducible and avoids
wall-clock thresholds unless a graph explicitly opts in.

```yaml
lanes:
  control:
    type: fixed_rate
    period_ms: 10
    wall_clock_enabled: true
    overrun_policy: drop_tick
```

Runtime rules:

- `period_ms` sets the cadence when positive; otherwise `hz` derives the period.
- `tick_budget_ms` overrides the overrun accounting budget without changing the cadence.
- `wall_clock_enabled: false` keeps deterministic simulated ticks.
- `wall_clock_enabled: true` sleeps before later ticks when the next scheduled tick is in the future.
- `overrun_policy` accepts `drop_tick`, `skip_next`, or `catch_up_once`; it is an alpha policy for how the next scheduled wall-clock tick is chosen after lateness, not a hard real-time guarantee.
- The v1 wall-clock scheduler remains single-runtime and cooperative; it does not create independent lane threads.

Trace events:

- `fixed_rate_tick_begin`
- `fixed_rate_tick`
- `fixed_rate_tick_end`
- `fixed_rate_overrun`
- `fixed_rate_skipped_tick`

Metrics:

- `runtime.scheduler.tick_count`
- `runtime.scheduler.tick_overrun_count`
- `runtime.scheduler.skipped_tick_count`
- `runtime.scheduler.tick_jitter_ms`
- `runtime.scheduler.max_lateness_ms`
- `runtime.scheduler.blocked_duration_ms`

## Runtime Priority v1

`execution.priority` is a component/invocation runtime hint. Supported classes
are `high`, `normal`, `low`, and `background`; an omitted value behaves like
`normal`. It is deliberately separate from lane `priority`, `nice_priority`,
`rt_policy`, `rt_priority`, and `cpu_affinity`, which remain OS/platform intents
that TopoExec does not apply today.

Runtime ordering rules:

- Independent compiled regions with no dependency between them are ordered by
  the highest `execution.priority` of the components in that region.
- `thread_pool` queue items are ordered by priority rank, then enqueue order,
  then component id.
- Equal-priority work preserves deterministic enqueue/topology order.
- Priority does not bypass dependency edges, CompositeLoop ownership, epoch
  visibility, publication commit barriers, or `execution.reentrant` limits.
- Lane overflow policy still decides which over-capacity ready invocations are
  dropped or rejected; low/background drops increment
  `runtime.scheduler.low_priority_rejected_count`.
- `runtime.scheduler.starvation_guard_count` exists as an explicit future
  intervention metric. G32 v1 has bounded priority ordering and starvation smoke
  coverage, but no aging intervention that would make this counter non-zero.

## Persistent Thread Pool v1

`thread_pool` owns a run-scoped persistent worker pool. Workers start when the
runtime run starts, wait on a bounded priority queue, and stop/join during runtime
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
- Lane admission orders admitted ready invocations by runtime priority (`high`, `normal`, `low`, `background`), then enqueue order, then component id. This is runtime-level ordering only; it is not OS scheduler priority.
- `thread_name` is applied as a best-effort worker thread name on supported platforms and remains advisory as a portable contract.
- Downstream regions do not run until the current admitted worker work has drained and immediate publications have been committed.

Test coverage:

- `Runtime.ThreadPoolLaneExecutesReentrantInvocationsConcurrently` proves overlap and the `max_threads` upper bound.
- `Runtime.ThreadPoolWorkersPersistAcrossMultipleRuntimeSteps` proves workers remain bounded and reused across runtime steps.
- `Runtime.ThreadPoolLaneSerializesNonReentrantInvocations` proves non-reentrant no-overlap.
- `Runtime.ThreadPoolStopWhileQueueNonEmptyDrainsAdmittedWork` proves stop requests do not deadlock with queued admitted work.
- `Runtime.ThreadPoolLaneQueueCapacityRejectsNewestWhenFull` and `Runtime.ThreadPoolLaneQueueCapacityDropsOldestWhenConfigured` prove explicit lane admission behavior, rejected-count metrics, scheduler-reject health events, and low-priority rejection counting.
- `Runtime.RuntimePriorityOrdersIndependentReadyComponentsAndDoesNotStarveLowPriority` proves runtime priority ordering for independent ready regions while still executing lower-priority work in a bounded example.
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
stops, and joins during runtime cleanup. Component code should prefer
`Invocation::cancel_requested()` or `GraphContext::cancel_requested()` for
long-running work; legacy `Invocation::stop_requested` observes the same token.
`execution.budget_ms`, CompositeLoop `budget_ms`, and `TaskExecutorConfig::task_budget`
are reported after cooperative checkpoints or after work returns. Timeout-based
preemption is not implemented.

Component errors stop the runtime with `SchedulerStopReason::kError`; already-started components still receive reverse-order deactivate cleanup.

## Metrics And Trace

Scheduler metrics are emitted through `RuntimeRunnerResult::runtime_metrics`:

- `runtime.scheduler.tick_count`
- `runtime.scheduler.completed_count`
- `runtime.scheduler.tick_overrun_count`
- `runtime.scheduler.skipped_tick_count`
- `runtime.scheduler.max_lateness_ms`
- `runtime.scheduler.queue_depth`
- `runtime.scheduler.queue_capacity`
- `runtime.scheduler.worker_count`
- `runtime.scheduler.last_callback_duration_ms`
- `runtime.scheduler.blocked_duration_ms`
- `runtime.scheduler.tick_jitter_ms`
- `runtime.scheduler.active_count`
- `runtime.scheduler.in_flight_count`
- `runtime.scheduler.rejected_count`
- `runtime.scheduler.priority_high_count`
- `runtime.scheduler.priority_normal_count`
- `runtime.scheduler.priority_low_count`
- `runtime.scheduler.priority_background_count`
- `runtime.scheduler.low_priority_rejected_count`
- `runtime.scheduler.starvation_guard_count`

For `event_loop` and simulated `fixed_rate`, worker/queue metrics remain zero unless a future implementation adds real queues. Priority counters still report completed invocations by runtime priority class. For `thread_pool`, `worker_count`, `queue_capacity`, `queue_depth`, `active_count`, `in_flight_count`, `completed_count`, `rejected_count`, and `low_priority_rejected_count` describe the maximum admitted or dropped persistent-pool work observed during the run. `starvation_guard_count` is reserved for explicit aging/intervention events; v1 normally reports zero.

Trace events around scheduler and component execution include:

- `scheduler_iteration_begin`
- `scheduler_iteration`
- `scheduler_iteration_end`
- `component_execute_begin`
- `component_execute`
- `component_execute_end`
- `component_cancellation_requested`
- `component_cancellation_observed`
- `component_timeout_budget_exceeded`
- `thread_pool_batch`
- `fixed_rate_tick_begin`
- `fixed_rate_tick`
- `fixed_rate_tick_end`
- `fixed_rate_overrun`
- `fixed_rate_skipped_tick`
- `loop_cancellation_requested`
- `loop_cancellation_observed`

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
- lane `isolation_intent`.

The runtime must not claim OS priority, CPU affinity, hard real-time scheduling, or a portable hard worker-name guarantee until platform-specific enforcement and tests exist.

When advisory fields are set to non-default values, validation still succeeds
but emits machine-readable diagnostics:

- `advisory_lane_field_ignored` for lane `priority`, `cpu_affinity`,
  `nice_priority`, `rt_policy`, `rt_priority`, `isolation_intent`, and
  `wall_clock_enabled` where applicable; `thread_name` also uses this diagnostic
  when it cannot be applied to persistent worker threads on the current runtime
  surface.

Supported component `execution.priority` values are runtime behavior, not advisory. Unknown values fail validation.

These diagnostics are warnings/advisories, not validation failures. They exist to
prevent schema fields from looking implemented merely because they parse.

## Remaining Work

- Independent fixed-rate lane threads and OS jitter controls.
- Aging-based starvation intervention beyond the current bounded priority ordering.
- Hard timeout preemption.
- Platform-specific priority, affinity, and RT helpers.

GitHub Actions has a non-blocking ThreadSanitizer job; keep it green before wider beta concurrency claims.
