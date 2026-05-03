# Runtime Semantics

TopoExec is a single-process, in-process semantic graph runtime. A graph is compiled before execution, and the compiled plan is the runtime source of truth for component order, CompositeLoop ownership, and immediate-cycle validation.

This document defines the schema v1 execution contract. Some runtime behaviors are still being implemented; those gaps should stay visible in tests, examples, and release notes rather than being hidden behind compatibility aliases or optimistic examples.

## Time And Commit Model

An epoch is one bounded runtime step, usually one event-loop iteration or one simulated tick in tests.

A transaction is the set of component executions and staged publications processed under one scheduler decision boundary inside an epoch.

A commit is the point where staged publications become visible according to their edge kind. `GraphContext::publish()` records output through the runtime-owned publication path; it must not call downstream component `execute()` directly.

Runtime execution is bounded by steps, duration, stop token, or idle detection. `run_until_idle` still uses the configured step bound as a safety limit, but stops early once a full event-loop iteration executes no components.

## Edge Visibility

`immediate` edges are same-transaction dependencies. They participate in immediate dependency SCC analysis, and a nontrivial immediate SCC is rejected unless it exactly matches one declared `composite_loops[]` entry. In a DAG, immediate publications may become visible to downstream components during the same epoch according to compiled region order.

`delay` edges break immediate feedback at compile time: they do not participate in immediate SCC rejection. At runtime, publications on delay edges are staged by the runtime-owned publication router and committed at the next epoch boundary. Use this for previous-frame feedback, sample-and-hold control, or any feedback path that must not recursively execute in the same transaction.

`state` edges do not participate in immediate SCC rejection. Runtime state-edge publications are staged during the current epoch and committed at the next epoch boundary, so readers continue to see the previously committed snapshot during the publishing epoch. Use this for blackboard-like state, configuration snapshots, and slow-to-fast crossings. Multi-writer conflict policy and richer state-store APIs are later work.

`async` edges do not participate in immediate SCC rejection. Runtime async-edge publications are deferred to the next epoch boundary, which prevents same-call-stack or same-transaction recursive execution. Use this for worker completion, future-ready events, diagnostics, and lower-priority notifications. Components with `task_ready`, `future_ready`, or `request` event sources receive matching invocation event kinds once their input event is observed. Richer max-inflight worker policy is later work.

## Channel Policy

Runtime channels are bounded. `latest` with `overwrite` and `capacity=1` is the low-latency default. `queue` with explicit capacity is for ordered event or command streams. `latched` keeps the last committed value for late readers. `previous_tick` exposes only the prior epoch's value. `barrier` with capacity N waits until N queued messages are available before delivering the synchronized batch.

Overflow behavior must be explicit. Dropped, overwritten, blocked, or rejected publications must be observable through channel metrics. Blocking overflow is not appropriate on a single-thread event-loop hot path unless the graph explicitly opts into a blocking runtime mode.

Copy policy is part of the runtime contract. `copy` owns a copied payload and rejects large payloads that cannot be copied safely. `shared_view` shares immutable payload storage. `loaned_view` currently aliases shared immutable storage until a real loaned-buffer API lands. `move_only` is accepted only for `readers: single`; multi-reader move-only edges are invalid.

## Trigger Readiness

Components implement one `execute()` method. Readiness belongs to the trigger engine, not to component code.

`manual` triggers are explicitly scheduled. `timer` / `on_tick` triggers fire from timer event sources. `any_input` fires when at least one configured input has an update. `all_inputs` waits until every configured input has a message, then consumes one from each input in deterministic input order. `time_sync` currently uses the same required-input readiness as `all_inputs` and marks the invocation as `kTimeSync`; timestamp slop enforcement is a later timestamp-model refinement. `batch` waits until its configured batch size is available. `request` and `task_ready` are input-driven and produce matching invocation kinds. Coalescing merges pending updates into one invocation, and `min_interval_ms` suppresses repeated invocations inside the interval.

An `Invocation` carries event kind, trigger kind, ready input names, payloads by port, batch payloads when relevant, timing fields, sequence, budget, lane, priority, and stop-token access.

## CompositeLoop Ownership

A `composite_loops[]` entry owns an immediate cyclic SCC only when its `components` set exactly matches that SCC. Partial declarations and decorative loop declarations are invalid.

At runtime, a CompositeLoop compiled region owns its internal component scheduling. For `loop_policy.type: fixed_point`, the current runtime executes internal components in deterministic compiled order up to `max_iterations` and emits loop iteration metrics. `loop_policy.convergence: single_pass` stops after the first iteration and records convergence. `budget_ms` stops the loop when elapsed loop time exceeds the budget and records a budget overrun. Publications from loop-internal components to components outside the loop are staged and committed only when the CompositeLoop region finishes.

## Example Graph Shapes

### DAG Pipeline

```text
source --immediate--> transform --immediate--> sink
```

The immediate graph is acyclic. The compiler produces component regions and a deterministic topological `region_order`. Runtime execution can make `source` output visible to `transform`, then `transform` output visible to `sink`, without recursive calls from `publish()`.

### Delay Feedback

```text
sensor --immediate--> estimator --immediate--> controller --immediate--> actuator
controller --delay--> estimator
```

The feedback edge is not part of the immediate dependency graph. The compiler accepts the graph without a CompositeLoop. The controller correction becomes visible to the estimator only at the next delay boundary, so the current transaction remains acyclic.

### CompositeLoop Feedback

```text
estimator --immediate--> controller
controller --immediate--> estimator
```

Without a matching `composite_loops[]` entry, this graph is invalid. With `components: [estimator, controller]` and a `loop_policy`, the compiler condenses the SCC into one CompositeLoop region and the runtime executes that region through the loop owner.
