# Adapter Stub Examples

This directory is a preview layout only. Files here are not built, do not include
external SDK headers, and do not implement ROS, production OpenTelemetry,
Prometheus, Perfetto, Python, C API, or dynamic plugin loading. The built G58
OTel preview lives in `include/topoexec/adapters/otel.hpp` and remains a
dependency-free in-memory mapping target, not a network exporter.

Use these notes to keep future adapter work outside `topoexec::runtime`.

## Boundary bridge sketch

```text
class MyBoundaryBridge {
  poll_external_inputs() -> RuntimePayload
  publish_boundary_output(RuntimePayload)
}
```

The bridge owns external I/O. TopoExec still owns internal edge policy, trigger
readiness, channel capacity, and payload visibility.

See `examples/boundary_adapter_pattern.yaml` for the dependency-free graph shape.

## Result exporter sketch

```text
class MyMetricsExporter {
  on_result(const RuntimeRunnerResult& result)
}
```

The exporter reads metrics/trace/errors from `RuntimeRunnerResult`. Export
failure should be adapter health, not a runtime scheduling decision.
For a compileable dependency-free version of this pattern, see
`topoexec::adapters::otel::ExporterPreview`.

## Plugin registry sketch

```text
void register_components(ComponentRegistry& registry) {
  registry.register_component({"app.Component"}, [] { return make_component(); });
}
```

Explicit in-process registration is the current stable path. Dynamic plugin
loading is a future optional package with separate ABI/versioning policy.
