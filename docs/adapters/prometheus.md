# Prometheus Exporter Preview

G59 adds a dependency-free Prometheus-style text exposition preview that proves
the runtime metric schema can be consumed by another adapter style without
turning core into an HTTP server or registry.

## Status

- Target: `topoexec_adapters::prometheus`
- Header: `topoexec/adapters/prometheus.hpp`
- Build option: `TOPOEXEC_BUILD_PROMETHEUS_ADAPTER=ON`
- Package metadata: `TOPOEXEC_HAS_PROMETHEUS_ADAPTER`
- Contract version:
  `topoexec::adapters::prometheus::kPrometheusPreviewContractVersion`
- Default build: off

This is a text-rendering preview, not a production scrape service. It does not
start threads, open sockets, run an HTTP server, depend on a Prometheus library,
add graph schema fields, or change runtime scheduling.

## Build and package

```bash
cmake -S . -B build-prometheus -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DTOPOEXEC_BUILD_PROMETHEUS_ADAPTER=ON
cmake --build build-prometheus -j
ctest --test-dir build-prometheus --output-on-failure -R test_prometheus_adapter
```

Downstream consumers request the optional component and link the adapter target:

```cmake
find_package(topoexec CONFIG REQUIRED COMPONENTS prometheus)
target_link_libraries(my_exporter PRIVATE topoexec_adapters::prometheus)
```

## Mapping contract

`topoexec::adapters::prometheus::TextExporterPreview` implements
`RuntimeObserver` for metric callbacks and can also render metrics from an
existing `RuntimeRunnerResult` with `export_result(...)`.

| TopoExec surface | Preview text shape |
| --- | --- |
| Descriptor `counter` | Sanitized metric name with `_total`, `# TYPE ... counter`, and descriptor-allowed labels only. |
| Descriptor `gauge` | Sanitized metric name, `# TYPE ... gauge`, and descriptor-allowed labels only. |
| Descriptor `histogram` | Reserved for future descriptor-native histogram metrics. |
| `MetricRegistry::histogram(name)` summaries | `<name>.count`, `.avg`, `.p50`, `.p95`, and `.p99` render as a Prometheus summary; `.min`, `.max`, and `.avg` also render as separate gauge helpers. |

Example output:

```text
# HELP runtime_component_execution_count_total TopoExec runtime metric runtime.component.execution_count
# TYPE runtime_component_execution_count_total counter
runtime_component_execution_count_total{component_id="source"} 1
# HELP latency_ms TopoExec custom histogram summary latency_ms
# TYPE latency_ms summary
latency_ms_count 2
latency_ms_sum 40
latency_ms{quantile="0.95"} 29
```

## Cardinality rules

The preview uses `runtime_metric_descriptors()` for runtime metrics. Only
descriptor-allowed bounded labels become text labels:

- `component_id`
- `lane`
- `channel_id`

Raw `RuntimeMetricSample::tags` are ignored, so correlation ids, causation ids,
transaction ids, request ids, and trace ids cannot become default Prometheus
labels. A sample that carries a label not allowed by its descriptor is rejected
by the preview.

## Non-goals

- No core/runtime HTTP server.
- No Prometheus registry or external library dependency.
- No scrape endpoint, background thread, batching, or retry.
- No graph schema fields for exporter configuration.
- No production stability promise for text naming before beta.

## Validation

G59 coverage:

- `test_prometheus_adapter` proves counter/gauge text mapping, custom histogram
  summary rendering, high-cardinality tag omission, unexpected-label rejection,
  result export, and observer callback behavior.
- `cmake_prometheus_adapter_options_smoke` configures a runtime-only build with
  `TOPOEXEC_BUILD_PROMETHEUS_ADAPTER=ON`, installs it, and verifies a downstream
  `find_package(topoexec COMPONENTS prometheus)` consumer links
  `topoexec_adapters::prometheus`.
- `policy_no_core_adapter_deps` keeps runtime/core free of adapter dependencies
  and verifies the optional target depends outward through `topoexec::adapter_sdk`.
