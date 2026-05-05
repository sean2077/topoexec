# Trace Events

TopoExec trace events are in-memory runtime spans copied into `RuntimeRunnerResult::trace`. The legacy `trace_events` array remains a compatibility list of event names, but new integrations should use the structured `trace` array.

Run a structured trace:

```bash
topoexec graph trace examples/minimal.yaml --steps 1 --format json
```

Run a Chrome Trace / Perfetto-compatible export:

```bash
topoexec graph trace examples/minimal.yaml --steps 1 --format chrome > topoexec-trace.json
```

The Chrome output uses the standard `traceEvents` array and can be opened in Chrome trace viewers or Perfetto UI.

## Structured JSON Shape

Each structured event has this shape:

```json
{
  "name": "component_execute",
  "trace_id": "trace-...",
  "start_offset_ns": 1234,
  "duration_ns": 9200,
  "attributes": {
    "component_id": "source",
    "lane": "main"
  }
}
```

`start_offset_ns` is monotonic offset from the first recorded event in that run, not wall-clock time. `duration_ns` is currently zero for point events and positive for scoped spans.

## Event Names

Scheduler:

- `scheduler_iteration`
- `scheduler_iteration_begin`
- `scheduler_iteration_end`

Component execution:

- `component_execute`
- `component_execute_begin`
- `component_execute_end`

Channels and publication:

- `channel_publish`
- `channel_commit`
- `async_admission`

Composite loops:

- `loop_iteration`
- `loop_iteration_begin`
- `loop_iteration_end`

## Attributes

The runtime includes identifiers where the event source has them:

- Scheduler events include `iteration`.
- Component events include `component_id` and `lane`.
- Channel publish events include `channel_id`, `source_component`, `target_component`, and `edge_kind`.
- Channel commit events include `channel_id` and `edge_kind`.
- Async admission events include `channel_id`, `accepted`, and `max_inflight`.
- Loop events include `loop_id` and loop-local `iteration`.

Future adapters may add OpenTelemetry, Prometheus, or richer Perfetto metadata, but those adapters are separate from the core runtime contract.

## Error fields

Trace JSON remains event-oriented. Runtime errors are exported through runner/metrics JSON as `runtime_errors[]`, with structured phase/component/code fields that can be correlated with component trace events by component id and future trace id fields.
