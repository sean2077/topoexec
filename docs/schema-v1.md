# Schema Version 1

Schema v1 describes a single-process TopoExec runtime graph. The loader is strict: unknown fields are rejected at the root and inside known sections.

Runtime visibility rules for edge kinds, epochs, transactions, commits, triggers, and CompositeLoop ownership are defined in [runtime-semantics.md](runtime-semantics.md).

## Root

Required fields:

- `schema_version: 1`
- `graph`
- `lanes`
- `components`
- `edges`

Optional fields:

- `composite_loops`

Allowed root fields are exactly `schema_version`, `graph`, `lanes`, `components`, `edges`, and `composite_loops`.

## graph

```yaml
graph:
  name: minimal
  kind: runnable
  clock:
    runtime_domain: steady
    event_domain: steady
```

Fields:

- `name` required string.
- `kind` optional string, default `runnable`; allowed values are `runnable` and `internal_test`.
- `clock.runtime_domain` optional string, default `steady`; only `steady` is currently accepted.
- `clock.event_domain` optional string, default `steady`; allowed values are `steady`, `system`, `device`, and `external`.

## lanes

`lanes` is a mapping from lane id to lane configuration:

```yaml
lanes:
  main:
    type: event_loop
    hz: 100
    max_callback_ms: 5
```

Fields:

- `type` required string; allowed values are `event_loop`, `fixed_rate`, and `thread_pool`.
- `hz` optional number, default `0`.
- `priority` optional string, default empty.
- `max_callback_ms` optional integer, default `0`.
- `max_threads` optional integer, default `0`; must be non-negative. For `thread_pool`, this is the active worker batch width (`0` means one worker).
- `queue_capacity` optional integer, default `0`; must be non-negative. For `thread_pool`, positive values bound pending ready invocations after the active batch; `0` preserves the current ready set without adding a persistent queue.
- `overflow` optional string, default `reject`; allowed values are `overwrite`, `drop_oldest`, `drop_newest`, `reject`, `reject_new`, `fail_fast`, and `block`. For `thread_pool`, `drop_oldest`/`overwrite` discard oldest ready invocations before execution, `drop_newest`/`reject`/`reject_new`/`block` skip newest ready invocations in the non-blocking runtime, and `fail_fast` stops the run.
- `wall_clock_enabled` optional boolean, default `false`; parsed for future wall-clock fixed-rate mode and currently advisory.
- `period_ms` optional integer, default `0`; fixed-rate period override for simulated overrun accounting.
- `tick_budget_ms` optional integer, default `0`; explicit per-iteration budget for simulated overrun accounting.
- `thread_name` optional string.
- `cpu_affinity` optional integer array.
- `nice_priority` optional integer, default `0`.
- `rt_policy` optional string, default `none`.
- `rt_priority` optional integer, default `0`.
- `isolation_intent` optional string, default `none`.

Runtime support note: `event_loop` is the deterministic default. `fixed_rate` is simulated by bounded runtime ticks and reports overrun/jitter metrics from `hz`, `period_ms`, or `tick_budget_ms`; real sleeping cadence is still deferred. `thread_pool` uses bounded worker batches for ready invocations with explicit queue admission metrics; see [scheduler.md](scheduler.md) and [concurrency.md](concurrency.md).

## components

`components` is a sequence. Each component must have an id, type, event sources, trigger policy, and execution lane.

```yaml
components:
  - id: transform
    type: topoexec.transforms.Identity
    event_sources:
      - type: message
        inputs: [in]
    trigger_policy:
      type: any_input
      inputs: [in]
    execution:
      lane: main
    boundary:
      role: processing
    config:
      gain: 1
```

Component fields:

- `id` required string; must be unique.
- `type` required string.
- `event_sources` optional sequence, default `[{type: manual}]`.
- `trigger_policy` optional mapping, default `{type: manual}`.
- `execution` required mapping.
- `depends_on` optional string array; lifecycle dependencies must form a DAG.
- `boundary` optional mapping.
- `config` optional mapping of component-specific values.

### event_sources[]

Allowed fields:

- `id` optional string.
- `type` optional string, default `manual`; allowed values are `manual`, `message`, `timer`, `request`, `action_goal`, `action_cancel`, `task_ready`, and `future_ready`.
- `inputs` optional string array.
- `input` optional string shorthand for one input.
- `period_ms` optional integer; required positive value for `timer`.

`message` sources require at least one input.

### trigger_policy

Allowed fields:

- `type` optional string, default `manual`; allowed values are `manual`, `on_event`, `any_input`, `all_inputs`, `time_sync`, `batch`, `request`, and `task_ready`.
- `inputs` optional string array.
- `input` optional string shorthand for one input.
- `batch_size` optional non-negative integer.
- `batch_window_ms` optional non-negative integer.
- `sync_slop_ms` optional non-negative integer.
- `min_interval_ms` optional non-negative integer.
- `max_latency_ms` optional non-negative integer.
- `coalesce` optional boolean, default `false`.

Input-driven trigger policies require incoming edges for every listed input. `batch` requires `batch_size` or `batch_window_ms`.

### execution

Allowed fields:

- `lane` required string; must reference a lane id.
- `reentrant` optional boolean, default `false`.
- `priority` optional string, default `normal`.
- `budget_ms` optional integer, default `0`.
- `on_error` optional string, default `fail_fast`; declared values are `fail_fast`, `continue`, and `isolate`, but only `fail_fast` is implemented in schema v1 today. Other values parse but semantic validation rejects them rather than silently emulating a policy, using diagnostic code `unsupported_error_policy`.

### boundary

Allowed fields:

- `role` optional string, default `processing`; allowed values are `processing`, `input`, `output`, and `input_output`.
- `descriptor` optional string.

For registry-backed `runnable` graphs, validation requires at least one input boundary and one output boundary.

## edges

`edges` is a sequence. Every edge must declare an explicit kind.

```yaml
edges:
  - id: source_to_transform
    kind: immediate
    from: source.out
    to: transform.in
    policy:
      mode: queue
      capacity: 4
      overflow: drop_oldest
      copy_policy: shared_view
```

Edge fields:

- `id` required string; must be unique.
- `kind` required string; allowed values are `immediate`, `delay`, `state`, and `async`.
- `from` required endpoint string, usually `component.output`.
- `to` required endpoint string, usually `component.input`.
- `policy` optional mapping.

Only `immediate` edges participate in immediate SCC analysis. Immediate cycles are invalid unless they exactly match one `composite_loops[]` entry. `delay`, `state`, and `async` edges break same-transaction feedback and become visible at a later epoch boundary.

### policy

Allowed fields:

- `mode` optional string, default `latest`; allowed values are `latest`, `queue`, `ring_buffer`, `latched`, `barrier`, and `previous_tick`.
- `capacity` optional positive integer, default `1`.
- `overflow` optional string, default `overwrite`; allowed values are `overwrite`, `drop_oldest`, `drop_newest`, `block`, `fail_fast`, and `reject`.
- `lifespan_ms` optional integer, default `0`.
- `deadline_ms` optional integer, default `0`.
- `max_inflight` optional non-negative integer, default `0`; applies only to `async` edges and limits deferred completions before channel capacity.
- `preserve_order` optional boolean, default `true`.
- `allow_drop` optional boolean, default `true`.
- `emit_health_events` optional boolean, default `true`.
- `timestamp_domain` optional string, default `steady`; allowed values are `steady`, `system`, `device`, and `external`.
- `copy_policy` optional string, default `copy`; allowed values are `copy`, `shared_view`, `loaned_view`, and `move_only`.
- `owner` optional string, default `runtime`; allowed values are `producer`, `runtime`, and `consumer`.
- `readers` optional string, default `single`; allowed values are `single` and `multi`.

Latest-style modes (`latest`, `latched`, `previous_tick`) cannot use `drop_newest` or `block`. `move_only` requires `readers: single`. State edges currently reject multiple writers to the same target endpoint. `max_inflight` is invalid on non-`async` edges.

## composite_loops

`composite_loops` is optional. Each entry must exactly match one immediate cyclic SCC.

```yaml
composite_loops:
  - id: estimator_controller_loop
    components: [estimator, controller]
    loop_policy:
      type: fixed_point
      max_iterations: 3
      budget_ms: 5
```

Fields:

- `id` required string.
- `components` required non-empty string array.
- `loop_policy` required mapping.

Loop policy fields:

- `type` required string; allowed values are `fixed_point`, `transaction`, `coalesced_event`, and `async_task`.
- `budget_ms` optional non-negative integer.
- `max_iterations` optional non-negative integer.
- `max_inflight` optional non-negative integer.
- `drop_policy` optional string.
- `min_interval_ms` optional non-negative integer.
- `convergence` optional string.

Current runtime execution is strongest for `fixed_point`. `loop_policy.max_inflight` is reserved for loop-level async policies and is separate from edge-level async `policy.max_inflight`.

## Valid Minimal Example

```yaml
schema_version: 1
graph: {name: minimal, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - id: source
    type: topoexec.boundary.Input
    boundary: {role: input}
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: sink
    type: topoexec.boundary.Output
    boundary: {role: output}
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
edges:
  - id: source_sink
    kind: immediate
    from: source.out
    to: sink.in
    policy: {mode: latest, copy_policy: shared_view}
```

## Invalid Examples

Missing edge kind:

```yaml
edges:
  - id: source_sink
    from: source.out
    to: sink.in
```

Immediate cycle without a matching CompositeLoop:

```yaml
edges:
  - {id: ab, kind: immediate, from: a.out, to: b.in}
  - {id: ba, kind: immediate, from: b.out, to: a.in}
```

Both cases are covered by CLI validation fixtures under `examples/invalid_*.yaml`.

## Machine-Readable Schema

A machine-readable Draft 2020-12 JSON Schema is checked in at [`../schema/topoexec.schema.v1.json`](../schema/topoexec.schema.v1.json). It mirrors the strict loader field set and enum surface documented here. The `schema_v1_contract_smoke` CTest parses that schema and validates representative checked-in graph fixtures through the runtime validator so schema documentation and executable validation do not silently drift.

The JSON Schema is a documentation and generation contract today; semantic rules such as SCC ownership, registry-backed port compatibility, multi-state-writer rejection, and trigger/input compatibility remain enforced by the C++ validator.

CLI validation exposes the same split:

```bash
topoexec graph validate examples/minimal.yaml --schema-only --format json
topoexec graph validate examples/minimal.yaml --semantic --format json
```

`--schema-only` checks the strict loader contract (required fields, known fields, basic scalar shapes). `--semantic` is the default and additionally runs the compiler/validator checks.

## Versioning

Schema v1 is strict and compatibility-preserving. Additive fields require a schema update only when v1 validation or runtime meaning would change. Breaking semantic changes should bump the schema version rather than silently changing v1 behavior.
