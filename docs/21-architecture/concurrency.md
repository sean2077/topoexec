# Concurrency

TopoExec concurrency keeps graph semantics first. Worker overlap is allowed only inside explicit lane and component policy bounds, and it never bypasses compiled region barriers or publication commit rules.

## Mental Model

```text
GraphContext::publish()
  -> runtime publication router
  -> edge-kind staging
  -> commit boundary
  -> downstream trigger readiness
```

No lane is allowed to turn `publish()` into direct same-call-stack downstream execution.

## Thread Pool Lane

Use a `thread_pool` lane when a component can safely process multiple ready invocations at the same time:

```yaml
lanes:
  pool:
    type: thread_pool
    max_threads: 4
components:
  - id: worker
    execution:
      lane: pool
      reentrant: true
```

Runtime behavior:

- `max_threads` is the run-scoped persistent worker count and active worker width; `0` means one worker.
- `queue_capacity` optionally bounds pending ready invocations behind active workers.
- `overflow` handles over-capacity ready invocations before work is submitted to workers: `drop_oldest`/`overwrite` keep newest work, `drop_newest`/`reject`/`reject_new`/`block` keep oldest work in the non-blocking runtime, and `fail_fast` stops the run.
- `execution.reentrant: false` serializes invocations for that component.
- `execution.reentrant: true` permits overlap up to the lane worker bound.
- Ready invocations admitted to the lane are queued by runtime priority (`high`, `normal`, `low`, `background`), then enqueue order, then component id.
- Immediate publications are committed at the worker/component barrier, not recursively from `GraphContext::publish()`.
- Stop/cancel requests prevent new scheduler iterations/submissions and wait for already-admitted invocations to drain cooperatively before workers stop and join. Components can observe requests with `Invocation::cancel_requested()` or `GraphContext::cancel_requested()`.
- `thread_name` is best-effort for persistent worker threads on supported platforms and remains advisory as a portable guarantee.

Advisory lane fields such as lane priority, CPU affinity, RT policy, thread-name portability guarantees, and isolation intent are parsed but not fully enforced by the current runtime. Unsupported policy should be documented as advisory rather than silently claimed.

Validation emits advisory diagnostics when lane-only scheduler fields are set, and plan JSON
reports the lane capability summary. Treat runtime priority and OS priority as
separate concepts: `execution.priority` is implemented as runtime ordering and
accepts `background`, `low`, `normal`, or `high`, while
`nice_priority`/`rt_policy`/`cpu_affinity` are OS hints that TopoExec does not
apply today.

## Fixed Rate Lane

`fixed_rate` is accepted by schema v1 and defaults to deterministic runner ticks.
It reports simulated overrun and positive jitter when an iteration exceeds `hz`,
`period_ms`, or `tick_budget_ms`. When `wall_clock_enabled: true`, the runtime
inserts cooperative sleeps before later ticks according to `period_ms` or `hz`
and records skipped/max-lateness metrics according to `overrun_policy`. This is
an opt-in wall-clock cadence smoke, not hard real-time scheduling or independent
per-lane threading.

## Async Admission

Async edges are deferred-completion edges. They do not execute arbitrary tasks by themselves; they admit a completion event for delivery at a later epoch.

`policy.max_inflight` limits outstanding deferred completions before channel capacity is considered:

```yaml
edges:
  - id: worker_join
    kind: async
    from: worker.done
    to: join.ready
    policy:
      mode: queue
      capacity: 8
      max_inflight: 2
      overflow: drop_oldest
      copy_policy: shared_view
```

Overflow behavior:

- `drop_oldest` and `overwrite` drop the oldest pending async completion and accept the new one.
- `drop_newest` rejects the new completion and records it as dropped.
- `reject`, `fail_fast`, and `block` reject the new completion in the current non-blocking runtime.

Async admission metrics use the `runtime.async.*` namespace; channel metrics report only completions that were actually committed to the runtime channel.

## What Is Still Deferred

- Full service/future response API and default runtime-owned task pools; `ThreadedTaskExecutor` exists only as an opt-in bounded preview helper.
- OS priority, affinity, and hard real-time policy enforcement.
- Independent fixed-rate lane threads and OS jitter control.
- Advanced starvation aging or OS-backed priority enforcement beyond runtime priority ordering.
- Hard timeout preemption for long-running component code.
- Blocking overflow behavior on the default non-blocking runtime path.
