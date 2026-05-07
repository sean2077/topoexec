# Low-Overhead Live Runtime Validation

The local-first runtime validation workbench centers on `topoexec graph
observe`. The goal is not a GUI editor or production observability platform.
The workbench gives developers a low-disturbance event stream, live assertions,
record/replay artifacts, and a local dashboard for reproducing runtime behavior.

## Boundary

Live observation is observe-only:

- no runtime control endpoint;
- no pause/resume/step/fault injection in the MVP;
- no WebSocket control channel;
- no Electron/Tauri/Qt desktop application;
- no React/Vite/Node default dependency;
- no production OpenTelemetry or Prometheus exporter;
- no schema v2 or graph schema v1 assertion embedding;
- no native Python binding;
- no payload body streaming by default.

TopoExec remains a compact C++20 in-process runtime. Python and static dashboard
tooling may consume CLI output, but Python/UI/SSE code must not become a
`topoexec::runtime` dependency.

## Data flow

```text
Runtime hot path
  -> fixed-size numeric LiveEvent
  -> bounded non-blocking per-stream queues
  -> collector / assertion engine / recorder
  -> NDJSON, record artifacts, and local SSE dashboard
  -> offline replay and CI evidence
```

The runtime producer path is intentionally small: write a fixed-size event,
perform a non-blocking push, and increment a drop counter on overflow. Symbol
resolution, JSON, file writes, assertions, and UI batching happen outside runtime
hot paths.

## `graph observe`

Current command shape:

```bash
topoexec graph observe examples/minimal.yaml \
  --steps 100 \
  --observe-level summary \
  --format ndjson \
  --assert tests/live/minimal_pass.assert.yaml \
  --record artifacts/live/minimal
```

Execution options:

- `--steps N`
- `--duration-ms N`
- `--until-idle`

Observe options:

- `--observe-level off|summary|detailed|debug`
- `--event-buffer-capacity N`
- `--ui-frame-ms N`
- `--include-component ID`
- `--include-channel ID`
- `--include-event KIND`
- `--exclude-event KIND`
- `--sample-event KIND:RATIO`
- `--payload-preview-bytes N`
- `--record DIR`

Assertion options:

- `--assert FILE`
- `--fail-on-assertion-fail`
- `--fail-on-observer-drop`

Output options:

- `--format ndjson|json-summary`

`--include-*`, `--exclude-event`, and `--sample-event` filter collector output;
they do not change runtime execution. `--payload-preview-bytes` is accepted only
with `--observe-level debug` and remains a bounded metadata preview surface, not
payload-body streaming.

## Output sequence

A normal NDJSON run should emit these record classes:

1. `symbol_table`
2. `graph_validated`
3. `plan_ready`
4. live observe events
5. assertion events when an assertion file is provided
6. `assertion_result` when assertions are evaluated
7. `final_summary`

All lines use `observe_schema_version = "1"`; assertion records also identify
`assertion_schema_version = "1"`.

## Exit codes

| Code | Meaning |
| ---: | --- |
| 0 | Runtime completed and live assertions completed successfully. |
| 1 | Runtime execution failed. |
| 2 | Graph validation, parse, or CLI configuration failed. |
| 3 | A live assertion failed. |
| 4 | The observe collector failed. |

Observer drops do not fail by default. `--fail-on-observer-drop` turns observed
drops into a non-zero observe command result without changing runtime semantics.

## Live assertion DSL

Assertions are YAML tooling input, not graph schema v1 fields.

```yaml
assertion_schema_version: "1"
assertions:
  - id: no_runtime_errors
    type: counter_equals
    source: runtime_errors
    value: 0

  - id: no_channel_drops
    type: metric_equals
    metric: runtime.channel.drop_count
    value: 0

  - id: sink_receives_by_epoch_3
    type: eventually
    event: component_end
    where:
      component_id: sink
      status: ok
    within_epochs: 3
```

Supported MVP assertion types:

- `always`
- `never`
- `eventually`
- `within_epochs`
- `within_events`
- `counter_equals`
- `metric_equals`
- `metric_lte`
- `metric_gte`
- `sequence`

Unsupported by design:

- arbitrary scripts;
- Python or JavaScript expression evaluation;
- user-defined functions;
- graph schema inline assertions;
- runtime hook assertions.

## Record artifact

`--record DIR` writes a replayable local artifact. Files are relative to the run
artifact directory and may be omitted only when the corresponding feature is not
available; omissions must be reflected in `manifest.json`.

```text
manifest.json
graph.yaml
graph.normalized.json
plan.json
render.mmd
observe.ndjson
observe.summary.json
assertions.yaml
assertion_result.json
metrics.final.json
trace.final.json
trace.chrome.json
health.final.json
dashboard.html
```

Manifest skeleton:

```json
{
  "artifact_schema_version": "1",
  "run_id": "run-20260507-001",
  "graph_name": "minimal",
  "observe_schema_version": "1",
  "assertion_schema_version": "1",
  "observe_level": "summary",
  "recorded_at": "2026-05-07T00:00:00Z",
  "files": {
    "observe": "observe.ndjson",
    "plan": "plan.json",
    "metrics": "metrics.final.json",
    "trace": "trace.final.json",
    "chrome_trace": "trace.chrome.json",
    "assertions": "assertions.yaml",
    "assertion_result": "assertion_result.json"
  },
  "summary": {
    "runtime_ok": true,
    "assertions_ok": true,
    "observer_dropped_event_count": 0
  }
}
```

Artifacts may contain graph configuration and runtime metadata. Treat them as
local evidence unless a human has reviewed them for publication.

## Local dashboard

The MVP dashboard is a local static UI served by `tools/topoexec_live_server.py`.
Defaults:

- bind `127.0.0.1`;
- generate a one-time token in the local URL;
- serve static assets without a CDN;
- expose `/events` SSE and `/snapshot` read-only endpoints;
- batch UI frames, defaulting to roughly 50 ms;
- cap raw event retention, defaulting to latest 1000;
- no runtime control endpoint.

Panels:

- Overview: runtime status, current epoch, observe level, exactness, dropped
  events, assertion pass/fail/pending, runtime errors, health.
- Topology: rendered graph, active component highlight, channel/trigger/loop
  badges.
- Timeline: component and worker lanes, loop spans, error markers, bounded
  latest history.
- Channels: publish/delivery/drop/reject/overwrite/depth/capacity/max depth.
- Triggers: ready/suppressed/coalesced/pending-drop and reason summaries.
- CompositeLoops: iteration count, residual, stop reason, output discard,
  budget/max-iteration/error.
- Assertions: pass/fail/pending, evidence, related events.
- Health / Errors: runtime errors, health events, observer drops, collector
  errors.
- Raw Events: bounded, filterable latest event view.

## Invariants

Live observe must not affect component order, trigger readiness, channel delivery,
async admission/release, CompositeLoop convergence/output visibility, scheduler
stop reason, runtime ok/fail result, metrics result contract, or trace result
contract. The runtime-invariant table links this statement to tests as the
implementation lands.
