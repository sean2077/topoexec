# OTel Exporter Preview

G58 adds a dependency-free exporter preview that validates the observer API and
metric/trace schema mapping without linking an external telemetry SDK.

## Status

- Target: `topoexec_adapters::otel`
- Header: `topoexec/adapters/otel.hpp`
- Build option: `TOPOEXEC_BUILD_OTEL_ADAPTER=ON`
- Package metadata: `TOPOEXEC_HAS_OTEL_ADAPTER`
- Contract version: `topoexec::adapters::otel::kOtelPreviewContractVersion`
- Default build: off

This is an in-memory mapping preview, not a production network exporter. It does
not start threads, open sockets, depend on an OTel SDK, add schema fields, or
change runtime scheduling.

## Build and package

```bash
cmake -S . -B build-otel -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DTOPOEXEC_BUILD_OTEL_ADAPTER=ON
cmake --build build-otel -j
ctest --test-dir build-otel --output-on-failure -R test_otel_adapter
```

Downstream consumers request the optional component and link the adapter target:

```cmake
find_package(topoexec CONFIG REQUIRED COMPONENTS otel)
target_link_libraries(my_exporter PRIVATE topoexec_adapters::otel)
```

The default runtime target remains dependency-free:

```cmake
find_package(topoexec CONFIG REQUIRED)
target_link_libraries(my_runtime_app PRIVATE topoexec::runtime)
```

## Mapping contract

`topoexec::adapters::otel::ExporterPreview` implements `RuntimeObserver` and can
also export an existing `RuntimeRunnerResult` with `export_result(...)`.

| TopoExec surface | Preview record | Mapping |
| --- | --- | --- |
| `RuntimeMetricSample` + `RuntimeMetricDescriptor` | `MetricRecord` | Descriptor `kind` maps to counter / observable gauge / histogram. Descriptor `unit` and `stability` are copied. Only descriptor-allowed labels (`component_id`, `lane`, `channel_id`) become attributes. |
| `RuntimeTraceEvent` | `SpanRecord` | Event name, trace id, phase, monotonic offsets, duration, and bounded schema fields become span attributes. |
| `RuntimeError` | `LogRecord` | Fatal errors become `error`; non-fatal errors become `warning`, with phase/component/lane/code/trace attributes. |
| `HealthEvent` | `LogRecord` | Health events become warning logs with bounded source, component, lane, channel/edge, policy, reason, depth/capacity, and occurrence attributes. |
| `RuntimeRunnerResult` | `LogRecord` summary plus optional record export | `on_result(...)` records a bounded run summary. `export_result(...)` replays result metrics, trace, health, and errors into preview records. |

## Cardinality rules

The preview uses `runtime_metric_descriptors()` as the source of truth. It does
not convert correlation ids, causation ids, transaction ids, request ids, or raw
`RuntimeMetricSample::tags` into metric labels. If tags are present, the preview
records only `topoexec.ignored_tag_count` so exporter authors can detect that a
sample carried non-schema tag data without creating label explosion.

## Non-goals

- No external SDK dependency.
- No background exporter thread, batching, retry, or network transport.
- No core/runtime dependency on adapter headers.
- No graph schema fields for exporter configuration.
- No production stability promise for preview record structs before beta.

## Validation

G58 coverage:

- `test_otel_adapter` proves descriptor-to-metric, trace-to-span,
  error/health-to-log, result export, and observer callback behavior.
- `cmake_otel_adapter_options_smoke` configures a runtime-only build with
  `TOPOEXEC_BUILD_OTEL_ADAPTER=ON`, installs it, and verifies a downstream
  `find_package(topoexec COMPONENTS otel)` consumer links `topoexec_adapters::otel`.
- `policy_no_core_adapter_deps` keeps runtime/core free of adapter dependencies
  and verifies the optional target depends outward through `topoexec::adapter_sdk`.
