# Concurrency

TopoExec concurrency keeps graph semantics first. `thread_pool` lanes can overlap ready invocations, but compiled region order remains the boundary for downstream visibility.

## Worker Lane MVP

Use a `thread_pool` lane when a component can safely process multiple ready invocations at the same time:

```yaml
lanes:
  pool:
    type: thread_pool
    max_threads: 4
```

Runtime behavior:

- `max_threads` is the worker batch width; `0` means one worker.
- `execution.reentrant: false` serializes invocations for that component.
- `execution.reentrant: true` permits overlap up to the lane worker bound.
- Immediate publications are committed at the worker/component barrier, not recursively from `GraphContext::publish()`.
- Stop requests prevent new worker batches and wait for already-started invocations to drain cooperatively.

Advisory fields such as priority, CPU affinity, RT policy, thread name, and isolation intent are parsed but not enforced by the current runtime.

## Async Admission

Async edges remain deferred-completion edges. They do not execute a task by themselves; they admit a completion event for delivery at a later epoch.

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
- `reject`, `fail_fast`, and `block` reject the new completion.

Async admission metrics use the `runtime.async.*` namespace; channel metrics still report what was actually committed to the runtime channel.
