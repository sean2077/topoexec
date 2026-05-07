# Live Observe Events Schema v1

TopoExec live observe events are a local development and CI evidence stream for
`topoexec graph observe`. They are intentionally separate from metrics schema v1,
trace schema v1, and graph schema v1.

```text
observe_schema_version = "1"
```

Live observe output is allowed to reference metric and trace concepts, but it
must not change existing metric, trace, or graph contracts.

## Design boundaries

- Runtime hot paths emit fixed-size numeric records only when live observe is
  enabled.
- Runtime hot paths must not serialize JSON, write files or sockets, wait for a
  UI, allocate event strings, look up shared symbol maps, or copy payload bodies.
- Collectors and tooling own JSON, NDJSON, record artifacts, assertions, and UI
  frames.
- Observer drops and collector failures are observable diagnostics; they do not
  change runtime scheduling, channel delivery, trigger readiness, CompositeLoop
  convergence, or runtime ok/fail semantics.
- Assertion failures may make `graph observe` exit non-zero, but assertion
  evaluation remains outside scheduler/channel/trigger internals.

## Observe levels

| Level | Contract |
| --- | --- |
| `off` | No live observe records are emitted and hot-path timestamp work is skipped. This is the default for `graph run`, `graph metrics`, and `graph trace`. |
| `summary` | Default `graph observe` level. Low-frequency and exceptional events are exact; high-frequency normal events are aggregated, coalesced, or sampled. |
| `detailed` | Component begin/end, exceptional channel/async/loop events, and selected runtime lifecycle events are emitted as exact records where practical. High-frequency publish/trigger streams may still be sampled or filtered. |
| `debug` | Explicit intrusive mode for selected components/channels/events. Bounded payload metadata preview may be enabled; payload bodies are still not streamed by default. |

## Exactness values

Every event, batch, or summary must declare one of these values:

| Value | Meaning |
| --- | --- |
| `exact` | The event represents one runtime fact and no known events of that kind were dropped for the stream/window. |
| `aggregated` | The event summarizes multiple runtime facts over a bounded window. |
| `sampled` | The event is a sample of a larger stream selected by configured sampling. |
| `lossy` | The stream/window has known observer drops or coalescing that prevents full reconstruction. |
| `partial` | The event is intentionally incomplete, for example because the observer was attached after run start or a collector input ended early. |

Dashboards and replay tools must display exactness and observer drop state.

## Internal numeric event record

The runtime-internal record is fixed-size and uses numeric IDs. Public embedder
APIs should treat this as a preview/internal surface until the live observe
contract is promoted.

```cpp
namespace topoexec::runtime_observe {

enum class LiveEventKind : std::uint16_t {
  kRunStarted,
  kRunFinished,
  kSchedulerEpochBegin,
  kSchedulerEpochEnd,
  kComponentBegin,
  kComponentEnd,
  kComponentError,
  kChannelPublishSummary,
  kChannelCommitSummary,
  kChannelDrop,
  kChannelReject,
  kChannelOverwrite,
  kTriggerReadySummary,
  kTriggerSuppressedSummary,
  kAsyncAdmission,
  kAsyncReject,
  kAsyncDrop,
  kLoopIterationBegin,
  kLoopIterationEnd,
  kLoopConverged,
  kLoopBudgetOverrun,
  kLoopMaxIterationsHit,
  kLoopError,
  kHealthEvent,
  kRuntimeError,
  kObserverDropSummary,
};

struct alignas(64) LiveEvent {
  std::uint64_t local_seq;
  std::uint64_t mono_ns;
  std::uint32_t stream_id;
  std::uint32_t epoch_id;
  std::uint32_t kind;
  std::uint32_t flags;
  std::uint32_t lane_id;
  std::uint32_t worker_id;
  std::uint32_t component_id;
  std::uint32_t channel_id;
  std::uint32_t loop_id;
  std::uint32_t policy_id;
  std::uint32_t reason_id;
  std::uint32_t reserved0;
  std::uint64_t value0;
  std::uint64_t value1;
  std::uint64_t value2;
  std::uint64_t value3;
};

}  // namespace topoexec::runtime_observe
```

Constraints for this record:

- no `std::string`, `std::map`, owning `std::vector`, payload owner, or heap-owned
  event body;
- no JSON, logging, socket, file, or UI call from the producer path;
- no blocking queue push or contended mutex wait in the producer path;
- stream-local `local_seq` is preferred over a global event sequence.

## Symbol table event

Collectors translate numeric IDs through a symbol table emitted once per run.

```json
{
  "observe_schema_version": "1",
  "kind": "symbol_table",
  "run_id": "run-20260507-001",
  "components": {"17": "source"},
  "channels": {"42": "source_transform"},
  "lanes": {"0": "main"},
  "loops": {"3": "solver_loop"},
  "policies": {"8": "rate_limit"},
  "reasons": {"5": "min_interval_ms"}
}
```

## NDJSON envelope

Each NDJSON line is one object. The collector assigns `display_seq`; producers
own only `stream_id` and `local_seq`.

```json
{
  "observe_schema_version": "1",
  "run_id": "run-20260507-001",
  "display_seq": 1024,
  "stream_id": "lane:main",
  "local_seq": 88,
  "kind": "component_end",
  "severity": "info",
  "exactness": "exact",
  "mono_ns": 123456789,
  "epoch_id": "7",
  "lane": "main",
  "worker_id": "",
  "component_id": "transform",
  "channel_id": "",
  "loop_id": "",
  "trace_id": "trace-...",
  "transaction_id": "source_transform#7",
  "correlation_id": "source_transform#7",
  "causation_id": "source_transform#7",
  "attributes": {
    "duration_ns": 9200,
    "status": "ok"
  }
}
```

Required top-level fields for normal events are `observe_schema_version`,
`run_id`, `display_seq`, `stream_id`, `local_seq`, `kind`, `severity`,
`exactness`, and `mono_ns`. Optional identity fields should use empty strings
when absent so line-oriented tools can rely on stable keys.

## Event class exactness by level

| Event class | Summary | Detailed | Debug |
| --- | --- | --- | --- |
| `run_started`, `run_finished` | exact | exact | exact |
| `runtime_error`, `component_error` | exact | exact | exact |
| `component_begin`, `component_end` | aggregated latency plus latest | exact | exact |
| `scheduler_epoch_begin`, `scheduler_epoch_end` | aggregated | exact or partial | exact or partial |
| `channel_publish`, `channel_commit` | aggregated | sampled or filtered | exact for selected channels |
| `channel_drop`, `channel_reject`, `channel_overwrite` | exact | exact | exact |
| `trigger_ready`, `trigger_suppressed` | aggregated | sampled or filtered | exact for selected components |
| `async_admission` | aggregated | exact | exact |
| `async_reject`, `async_drop` | exact | exact | exact |
| `loop_iteration` | aggregated plus latest residual | exact | exact |
| `loop_converged`, `loop_error`, `loop_budget_overrun`, `loop_max_iterations_hit` | exact | exact | exact |
| `health_event` | exact within bounded retention | exact | exact |
| `observer_drop_summary` | exact | exact | exact |
| `assertion_fail` | exact | exact | exact |
| `assertion_pass`, `assertion_pending` | aggregated | exact | exact |

## UI frame event

SSE dashboard streams should batch collector output instead of pushing every hot
event as a separate browser message.

```json
{
  "observe_schema_version": "1",
  "kind": "ui_frame",
  "run_id": "run-20260507-001",
  "frame_seq": 120,
  "window_ms": 50,
  "event_count": 1400,
  "dropped_event_count_delta": 0,
  "exactness": "aggregated",
  "events": [
    {"kind": "component_error", "component_id": "filter", "severity": "error"}
  ],
  "summaries": {
    "components": {
      "filter": {
        "execution_count_delta": 10,
        "last_duration_ns": 9000,
        "max_duration_ns": 12000
      }
    },
    "channels": {
      "source_filter": {
        "publish_count_delta": 1000,
        "delivery_count_delta": 998,
        "drop_count_delta": 0,
        "max_depth": 4
      }
    }
  }
}
```

Default UI frame cadence is approximately 50 ms. Raw browser-side event history
must be bounded by default.

## Observer drop summary

Overflow is reported without blocking producers.

```json
{
  "observe_schema_version": "1",
  "kind": "observer_drop_summary",
  "run_id": "run-20260507-001",
  "stream_id": "lane:main",
  "local_seq": 129,
  "display_seq": 2048,
  "severity": "warning",
  "exactness": "lossy",
  "dropped_event_count": 1842,
  "window_start_mono_ns": 120000000,
  "window_end_mono_ns": 170000000,
  "affected_kind": "channel_publish"
}
```

## Assertion events

Live assertions use their own schema version and are emitted by collector/tooling
only.

```json
{"observe_schema_version":"1","kind":"assertion_registered","assertion_schema_version":"1","assertion_id":"no_channel_drops","exactness":"exact"}
{"observe_schema_version":"1","kind":"assertion_pass","assertion_schema_version":"1","assertion_id":"no_runtime_errors","exactness":"exact"}
{"observe_schema_version":"1","kind":"assertion_fail","assertion_schema_version":"1","assertion_id":"sink_receives_by_epoch_3","reason":"eventually condition not satisfied","exactness":"exact"}
{"observe_schema_version":"1","kind":"assertion_pending","assertion_schema_version":"1","assertion_id":"loop_converges","exactness":"exact"}
```

## Payload preview metadata

Default observe output may include payload metadata only:

- `payload_type_id`
- `payload_size_bytes`
- `payload_sequence`
- `payload_timestamp`
- optional bounded hash

Payload body preview requires `debug`, explicit component/channel filters, and a
hard byte limit. Preview must not extend payload ownership or require deep copies.
