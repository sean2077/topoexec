# TopoExec Examples

This directory contains dependency-free YAML graphs and C++ applications that
teach the core TopoExec runtime contracts. See the full walkthrough in
[`docs/examples.md`](../docs/examples.md).

## Quick smoke

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure -R 'app_|cli_run_|cli_validate_'
```

## YAML graphs

| File | Purpose | Command |
| --- | --- | --- |
| `minimal.yaml` | Immediate source-transform-sink pipeline. | `./build/topoexec graph run examples/minimal.yaml --steps 1` |
| `control_feedback_delay.yaml` | Feedback delayed to the next epoch. | `./build/topoexec graph run examples/control_feedback_delay.yaml --steps 2` |
| `composite_loop.yaml` | Declared immediate feedback SCC. | `./build/topoexec graph plan examples/composite_loop.yaml --format json` |
| `large_payload_copy.yaml` | Lintable large-payload copy policy. | `./build/topoexec graph lint examples/large_payload_copy.yaml` |
| `diagnostic_warnings.yaml` | Warning diagnostics and strict-diagnostics behavior. | `./build/topoexec graph validate examples/diagnostic_warnings.yaml --strict-diagnostics --format json` |
| `state_config_snapshot.yaml` | State/config snapshot visibility. | `./build/topoexec graph run examples/state_config_snapshot.yaml --steps 2` |
| `batch_time_sync.yaml` | Two-input time-sync trigger. | `./build/topoexec graph run examples/batch_time_sync.yaml --steps 1` |
| `service_pipeline.yaml` | Request, async, and task-ready pipeline shape. | `./build/topoexec graph run examples/service_pipeline.yaml --steps 2` |
| `boundary_adapter_pattern.yaml` | App-owned external I/O boundary pattern. | `./build/topoexec graph run examples/boundary_adapter_pattern.yaml --steps 1` |
| `invalid_*.yaml` | Negative validation fixtures. | `./build/topoexec graph validate examples/invalid_immediate_cycle.yaml` |

## C++ apps

- `apps/minimal_pipeline`: immediate pipeline with runtime metrics.
- `apps/overload_latest_vs_queue`: low-latency latest versus event queue overload.
- `apps/control_feedback_delay`: previous-tick feedback.
- `apps/composite_loop_fixed_point`: explicit CompositeLoop ownership.
- `apps/async_worker`: async completion and bounded backlog.
- `apps/cpp_builder_minimal`: pure C++ builder and app-defined registry path.
- `apps/low_latency_sensor_pipeline`: source/preprocessor/detector/tracker latest-only path.
- `apps/control_loop_with_state`: fixed-rate control loop with state and delay boundaries.
- `apps/async_request_response`: request, validator, task-executor, and response boundary path.
- `apps/composite_solver`: CompositeLoop convergence and budget-overrun evidence.
- `apps/payload_pool_pipeline`: BufferPool copy/shared/loaned payload metrics.

These examples do not implement ROS 2, OpenTelemetry, Prometheus, Python, or
external Perfetto adapters. Boundary examples show where such adapters can attach
once the core API is stable. The `hierarchical_graph_preview` candidate remains
deferred until G41 adds a hierarchy/subgraph contract.
