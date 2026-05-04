# Adapter Boundaries

Adapters are intentionally deferred until the core runtime and API stabilize. This page records extension boundaries so core code does not absorb adapter-specific assumptions.

## Core Boundary

The core/runtime layer owns:

- `GraphSpec` and validation.
- Component lifecycle and invocation.
- Channel policy, payload ownership, and publication routing.
- Runtime metrics and trace spans.
- `RuntimeRunnerResult` as the in-process observation surface.

The core/runtime layer must not depend on ROS, OpenTelemetry, Prometheus, Python, Perfetto SDKs, or service-specific client libraries.

## Candidate Future Adapters

- ROS 2: topics/services/actions mapped at TopoExec graph boundaries.
- OpenTelemetry/Prometheus: export existing metrics and trace spans.
- Python: configuration, tests, and scripting first; not the high-performance data path initially.
- Perfetto: richer trace export beyond Chrome-compatible JSON.

Adapters should consume stable runtime APIs instead of adding adapter-specific fields to schema v1 unless a field is useful generically.
