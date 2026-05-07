# Trace Events

TopoExec trace events are in-memory runtime spans copied into `RuntimeRunnerResult::trace`. CLI JSON exposes the structured `trace` array and `trace_schema_version` contract.

Run a structured trace:

```bash
topoexec graph trace examples/minimal.yaml --steps 1 --format json
```

Run a Chrome Trace / Perfetto-compatible export:

```bash
topoexec graph trace examples/minimal.yaml --steps 1 --format chrome > topoexec-trace.json
```

The Chrome output uses the standard `traceEvents` array and can be opened in Chrome trace viewers or Perfetto UI. It also carries TopoExec `trace_schema_version` so future exporters can reject incompatible mappings before interpreting tracks.

## Structured JSON Shape

Structured runner/CLI JSON includes top-level `trace_schema_version`. Version `1` events have this shape:

```json
{
  "name": "component_execute",
  "trace_id": "trace-...",
  "phase": "component",
  "component_id": "source",
  "channel_id": "",
  "lane": "main",
  "worker_id": "",
  "epoch_id": "1",
  "transaction_id": "source_to_transform#1",
  "correlation_id": "source_to_transform#1",
  "causation_id": "source_to_transform#1",
  "start_offset_ns": 1234,
  "duration_ns": 9200,
  "attributes": {
    "trigger_kind": "any_input"
  }
}
```

`start_offset_ns` is a monotonic offset from the first recorded event in that run, not wall-clock time. `duration_ns` is zero for point events and positive for scoped spans. The result trace is sorted by `start_offset_ns` with stable tie preservation, so consumers can reconstruct a timeline without reordering.

`phase` is a bounded category derived from the event name: `component`, `channel`, `scheduler`, `loop`, `config`, `health`, or `runtime`. The explicit `component_id`, `channel_id`, `lane`, `worker_id`, `epoch_id`, `transaction_id`, `correlation_id`, and `causation_id` fields duplicate selected attributes into a stable schema; the original `attributes` map keeps event-specific details.

## Event Names

Scheduler:

- `scheduler_iteration`
- `scheduler_iteration_begin`
- `scheduler_iteration_end`

Component execution:

- `component_execute`
- `component_execute_begin`
- `component_execute_end`
- `component_cancellation_requested`
- `component_cancellation_observed`
- `component_timeout_budget_exceeded`
- `thread_pool_batch`

Fixed-rate scheduler:

- `fixed_rate_tick_begin`
- `fixed_rate_tick`
- `fixed_rate_tick_end`
- `fixed_rate_overrun`
- `fixed_rate_skipped_tick`

Channels and publication:

- `channel_publish`
- `channel_commit`
- `state_commit`
- `async_admission`

Config transactions:

- `config_transaction_validate`
- `config_transaction_apply`
- `config_transaction_rollback`
- `config_transaction_commit`

Composite loops:

- `loop_iteration`
- `loop_iteration_begin`
- `loop_iteration_end`
- `loop_converged`
- `loop_budget_overrun`
- `loop_max_iterations_hit`
- `loop_output_discarded`
- `loop_error`
- `loop_cancellation_requested`
- `loop_cancellation_observed`

## Attributes

The runtime includes identifiers where the event source has them:

- Scheduler events include `iteration`; `thread_pool_batch` spans include `component_id`, `lane`, `batch_size`, `worker_count`, `queue_capacity`, and `worker_ids`.
- Fixed-rate tick events include `lane`, `iteration`, `wall_clock_enabled`, and `overrun_policy`; overrun/skipped events add lateness or skipped-tick details.
- Component events include `component_id` and `lane`; `thread_pool` component events also include `worker_id`. Timeout-budget events add `budget_ms` and `duration_ns`. When available, component/channel events also include bounded metadata attributes: `correlation_id`, `causation_id`, `epoch_id`, `transaction_id`, `source_component`, `source_port`, and `trigger_kind`.
- Channel publish events include `channel_id`, `source_component`, `target_component`, and `edge_kind`.
- Channel commit events include `channel_id` and `edge_kind`.
- CompositeLoop iteration/convergence events include `loop_id`, `iteration`,
  and `policy`; solver-style events may also include `residual`,
  `residual_threshold`, and `reason`.
- State commit events include `channel_id` and `edge_kind` when a `state` edge becomes visible at an epoch boundary.
- Async admission events include `channel_id`, `accepted`, and `max_inflight`.
- Config transaction events include `component_id`, `status`, `reason`, or applied component counts where applicable. They are emitted at epoch boundaries before component execution.
- Health events appear as `health_event` trace entries with bounded observer attributes such as `kind`, `source`, `channel_id`/`edge_id`, `component_id`, `lane`, `policy`, `reason`, `depth`, `capacity`, and `occurrence_count`. They are emitted for channel overflow/stale/deadline/high-watermark, task reject, and scheduler reject paths when health events are enabled.
- Loop events include `loop_id` and loop-local `iteration`.

Chrome trace export groups events onto stable tracks by phase plus
lane/component/channel identity. The optional OTel preview maps
`RuntimeTraceEvent` values to in-memory span records with the same schema version,
phase, identifiers, monotonic offsets, duration, and bounded attributes. Future
production exporters or richer Perfetto metadata remain separate from the core
runtime contract and should map from trace schema version `1` instead of
depending on private runtime internals.

## Error fields

Trace JSON remains event-oriented. Runtime errors are exported through
runner/metrics JSON as `runtime_errors[]`, with structured phase/component/code
fields that can be correlated with component trace events by component id, trace
id, or correlation id when present. The OTel preview maps these runtime errors
to bounded log records; it does not turn errors into scheduler control flow.
