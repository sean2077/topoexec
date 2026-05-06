# Examples

TopoExec examples are intentionally small and dependency-free. They teach the
core runtime contracts without implying that production adapter packages,
sandboxed/stable plugin ecosystems, production ROS 2 packages, production
OpenTelemetry/Prometheus, or native Python bindings are implemented. The G58/G59
telemetry targets, G60 ROS 2 target, G61 C API target, G62 Python automation
package, and G63 plugin-loader target are dependency-free, CLI-backed,
trusted-native, or unstable previews, not example app dependencies.

Use this page as the learning path after the README quickstart.

## Validation commands

Build once, then run the example smokes:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure -R 'app_|cli_run_|cli_validate_'
```

For any YAML example you can inspect the graph without executing component code:

```bash
./build/topoexec graph validate examples/minimal.yaml
./build/topoexec graph plan examples/minimal.yaml --format json
./build/topoexec graph render examples/minimal.yaml --format mermaid
```

The runnable YAML examples below use only the CLI demo registry in
`tools/topoexec/main.cpp`; embedded applications should register their own
component factories through `topoexec::ComponentRegistry`.

## Example map

| Category | Example | Primary lesson | Stable smoke |
| --- | --- | --- | --- |
| Minimal | `examples/minimal.yaml`, `examples/apps/minimal_pipeline` | Immediate feed-forward publication becomes visible in the same bounded run step. | `cli_run_minimal`, `app_minimal_pipeline_runs` |
| Low latency | `examples/apps/overload_latest_vs_queue` | `latest` overwrites stale samples; bounded `queue` preserves FIFO until overflow. | `app_overload_latest_vs_queue_runs` |
| Event stream | `examples/apps/overload_latest_vs_queue` queue branch | Event streams need explicit capacity and overflow policy. | `app_overload_latest_vs_queue_runs` |
| Delay feedback | `examples/control_feedback_delay.yaml`, `examples/apps/control_feedback_delay` | Feedback must cross an epoch boundary unless a CompositeLoop owns the SCC. | `cli_validate_control_feedback_delay`, `app_control_feedback_delay_runs` |
| State/config | `examples/state_config_snapshot.yaml` | `state` edges publish the next committed snapshot, so consumers see updates after an epoch boundary. | `cli_run_state_config_snapshot` |
| CompositeLoop | `examples/composite_loop.yaml`, `examples/apps/composite_loop_fixed_point` | Immediate feedback is accepted only when the exact SCC is declared and bounded. | `cli_plan_json_composite`, `app_composite_loop_fixed_point_runs` |
| Async | `examples/apps/async_worker`, `examples/service_pipeline.yaml` | Async delivery is deferred, bounded, and observable; request/task-ready triggers model service-style flow. | `app_async_worker_runs`, `cli_run_service_pipeline` |
| Batch/time sync | `examples/batch_time_sync.yaml` | A time-sync trigger waits for both input streams within the configured slop. | `cli_run_batch_time_sync` |
| Large payload | `examples/large_payload_copy.yaml` | Copy policy is explicit and lintable; use shared/loaned paths for real large data flows. | `cli_lint_large_payload_copy` |
| Diagnostic warnings | `examples/diagnostic_warnings.yaml` | Deep blocking queue policies emit warning diagnostics and can fail strict diagnostics mode. | `cli_validate_warning_diagnostics_do_not_fail`, `cli_validate_strict_diagnostics_fail_warnings` |
| Loaned owner lint | `examples/loaned_view_without_pool_owner.yaml` | `loaned_view` should name a producer/pool owner until pool-return callbacks exist. | `cli_lint_loaned_view_without_pool_owner` |
| Registry / app factories | `examples/apps/cpp_builder_minimal` | Pure C++ apps build graphs and register factories without YAML/CLI dependencies. | `app_cpp_builder_minimal_runs` |
| Boundary adapter pattern | `examples/boundary_adapter_pattern.yaml` | Boundary nodes mark where an app-owned adapter injects or drains data without adding adapter dependencies to core. | `cli_run_boundary_adapter_pattern` |
| Graph templates | `examples/template_source_transform_sink.yaml` | Parameter-substituted snippets expand before validation/runtime so repeated graph shapes stay explicit. | `cli_run_template_source_transform_sink` |
| Reference app v2 | `examples/apps/low_latency_sensor_pipeline` | Source/preprocessor/detector/tracker latest-only path with explicit drop metrics. | `app_low_latency_sensor_pipeline_runs` |
| Reference app v2 | `examples/apps/control_loop_with_state` | Fixed-rate control loop with state snapshot and delay feedback boundaries. | `app_control_loop_with_state_runs` |
| Reference app v2 | `examples/apps/async_request_response` | Request boundary, validator, deterministic task executor, and response boundary without service adapters. | `app_async_request_response_runs` |
| Reference app v2 | `examples/apps/composite_solver` | CompositeLoop solver-iteration residual convergence and loop-budget overrun evidence. | `app_composite_solver_runs` |
| Reference app v2 | `examples/apps/payload_pool_pipeline` | BufferPool, copy/shared/loaned payload metrics, and in-process frame identity. | `app_payload_pool_pipeline_runs` |
| Real-world pilot | `examples/apps/robot_cell_pilot` | Composes multiple lanes, async overload, state/delay feedback, BufferPool frames, config transactions, metrics/trace, and invalid-config rejection without adapters. | `app_robot_cell_pilot_runs` |

## Reference applications v2

G56 adds closer-to-real application slices while preserving the project boundary:
all apps are dependency-free C++20 examples and no production ROS 2,
OpenTelemetry, Prometheus, Python, dynamic plugin, external Perfetto, or shared-memory middleware adapter is
implemented.

Run the focused smoke set:

```bash
ctest --test-dir build --output-on-failure -R 'app_(low_latency_sensor_pipeline|control_loop_with_state|async_request_response|composite_solver|payload_pool_pipeline|robot_cell_pilot)_runs'
```

Command markers keep the standalone binaries executable in docs smoke:

<!-- topoexec-doc-test: ${BUILD_DIR}/topoexec_app_low_latency_sensor_pipeline -->
<!-- topoexec-doc-test: ${BUILD_DIR}/topoexec_app_control_loop_with_state -->
<!-- topoexec-doc-test: ${BUILD_DIR}/topoexec_app_async_request_response -->
<!-- topoexec-doc-test: ${BUILD_DIR}/topoexec_app_composite_solver -->
<!-- topoexec-doc-test: ${BUILD_DIR}/topoexec_app_payload_pool_pipeline -->
<!-- topoexec-doc-test: ${BUILD_DIR}/topoexec_app_robot_cell_pilot -->

Stable outputs:

```text
tracker_latest=track:detection:preprocessed:frame-3
state_snapshot_epoch=2
response_payload=accepted:req-1
budget_overrun_count=1
loaned_address_preserved=true
pilot_value=explicit_feedback_bounded_observable_cpp
```

Hierarchical graph organization is covered by [Hierarchical graphs](hierarchical-graphs.md)
and parser/runtime tests. G41 intentionally implements compile-time namespace
expansion instead of adding a separate runtime-nesting reference app.

## Real-world pilot: robot cell

G69 adds `examples/apps/robot_cell_pilot` as the first composed pilot. Unlike the
single-contract reference apps, it combines event-loop, thread-pool, and
fixed-rate lanes with async frame delivery, bounded overload drops, state/delay
feedback, pool-backed `FrameView` payloads, config transaction/snapshot evidence,
runtime metrics, trace events, and an invalid-config error path. See
[Case Study: Dependency-Free Robot Cell Pilot](case-study-robot-cell.md) for the
architecture and boundaries.

## Walkthroughs

### 1. Minimal pipeline

Graph shape:

```text
source --immediate--> transform --immediate--> sink
```

Run:

```bash
./build/topoexec graph run examples/minimal.yaml --steps 1
```

Expected stable lines:

```text
order: source,transform,sink
runtime_publication_committed: 2
```

Semantic lesson: `publish()` stages output while a component executes, then the
runtime commits the publication before downstream immediate consumers run.

Contrast invalid graph: `examples/invalid_missing_edge_kind.yaml` omits the
required edge kind and is rejected by schema/semantic validation.

### 2. Low-latency latest versus event queue

Graph shape:

```text
fast_source --latest/overwrite--> slow_processor
fast_source --queue/drop_oldest(capacity=2)--> slow_processor
```

Run:

```bash
./build/topoexec_app_overload_latest_vs_queue
```

Expected stable lines:

```text
latest_payloads=frame-3
latest_drop_count=2
queue_payloads=event-2,event-3
queue_drop_count=1
```

Semantic lesson: overload is never implicit. `latest` is suitable for low-latency
state samples where stale data should disappear, while `queue` is suitable for
ordered event streams with bounded backlog.

Contrast invalid graph: a single-thread graph should not rely on unbounded or
silent blocking backpressure; choose an explicit overflow policy.

### 3. Delay feedback

Graph shape:

```text
sensor --immediate--> estimator --immediate--> controller --immediate--> actuator
controller --delay--> estimator
```

Run:

```bash
./build/topoexec graph run examples/control_feedback_delay.yaml --steps 2
```

Expected stable lines:

```text
order: sensor,estimator,controller,actuator,sensor,estimator,estimator,controller
runtime_publication_committed: 8
```

Semantic lesson: a `delay` edge prevents accidental recursive execution by
making feedback visible in the next epoch.

Contrast invalid graph: `examples/invalid_immediate_cycle.yaml` replaces the
safe epoch boundary with a raw immediate cycle and is rejected unless an exact
CompositeLoop declaration owns that SCC.

### 4. State/config snapshot

Graph shape:

```text
config_source --state--> consumer
```

Run:

```bash
./build/topoexec graph run examples/state_config_snapshot.yaml --steps 2
```

Expected stable lines:

```text
order: config_source,config_source,consumer
runtime_publication_committed: 1
```

Semantic lesson: graph-level `config` is parsed, and `state` edge publications
are observed through the next committed snapshot rather than as hidden mutable
globals during the current epoch.

Contrast invalid graph: multiple writers to the same state input remain a future
explicit merge-policy feature; do not model that as silent shared mutation.

### 5. CompositeLoop fixed point

Graph shape:

```text
sensor --immediate--> estimator --immediate--> controller --delay--> actuator
                         ^                         |
                         |-------immediate---------|
```

Run:

```bash
./build/topoexec_app_composite_loop_fixed_point
```

Expected stable lines:

```text
undeclared_loop_rejected=true
loop_iteration_count=2
```

Semantic lesson: immediate feedback is legal only when an exact CompositeLoop
owns the SCC and supplies a bounded loop policy.

Contrast invalid graph: partial loop declarations and undeclared immediate SCCs
are rejected by the graph compiler.

### 6. Async worker and service-style pipeline

App graph shape:

```text
source --immediate--> worker --async(queue capacity=1)--> join
```

YAML service-shape graph:

```text
client_boundary --request--> validator --async/task_ready--> worker_result --immediate--> response_boundary
```

Run:

```bash
./build/topoexec_app_async_worker
./build/topoexec graph run examples/service_pipeline.yaml --steps 2
```

Expected stable lines:

```text
task_ready_epoch=2
async_drop_count=1
order: client_boundary,validator,client_boundary,worker_result,validator,response_boundary
```

Semantic lesson: async edges model deferred completions and `max_inflight`
admission. The service YAML is a boundary pattern, not a network service stack.

Contrast invalid graph: treating async completion as same-epoch immediate data
would hide latency and backpressure; use `task_ready` when downstream work waits
for completion.

### 7. Batch/time sync

Graph shape:

```text
left_sensor  --queue(timestamp_domain=steady)--> sync_join --immediate--> sink
right_sensor --queue(timestamp_domain=steady)--> sync_join
```

Run:

```bash
./build/topoexec graph run examples/batch_time_sync.yaml --steps 1
```

Expected stable lines:

```text
order: left_sensor,right_sensor,sync_join,sink
runtime_publication_committed: 3
```

Semantic lesson: the runtime-owned `time_sync` trigger waits for aligned samples
from both inputs and reports drops when samples fall outside the slop window.

Contrast invalid graph: using `any_input` for sensor fusion would let one side
run without a corresponding sample from the other stream.

### 8. Large payload copy policy

Graph shape:

```text
camera --immediate(copy_policy=copy)--> sink
```

Inspect:

```bash
./build/topoexec graph lint examples/large_payload_copy.yaml
```

Expected stable line:

```text
large_payload_copy
```

Semantic lesson: large data movement is part of the graph contract. The demo
uses a copy policy so lint can warn; real high-volume data paths should evaluate
`shared_view` or `loaned_view` and buffer-pool ownership.

`examples/loaned_view_without_pool_owner.yaml` is the companion lint example:
`loaned_view` preserves in-process buffer identity, but until pool-return
callbacks exist the edge should declare a producer/pool owner instead of relying
on implicit runtime ownership.

Contrast invalid graph: do not claim external shared-memory or zero-copy
middleware integration from this example; those adapters remain deferred.

### 9. Registry and pure C++ builder

Graph shape:

```text
source --immediate--> transform --immediate--> sink
```

Run:

```bash
./build/topoexec_app_cpp_builder_minimal
```

Expected stable lines:

```text
builder_sink=hello:built
builder_order=source,transform,sink
```

Semantic lesson: embedders can construct `GraphSpec` with
`topoexec/runtime/graph_builder.hpp` and register app-defined factories in a
`ComponentRegistry` while linking only the runtime target.

Contrast invalid graph: dynamic plugin loading exists only through the separate
default-off trusted-native G63 preview and package discovery is not implemented
by this example; it demonstrates the stable in-process registry contract.

### 10. Boundary adapter pattern

Graph shape:

```text
external_input --immediate--> app_logic --immediate--> external_output
```

Run:

```bash
./build/topoexec graph run examples/boundary_adapter_pattern.yaml --steps 1
```

Expected stable lines:

```text
order: external_input,app_logic,external_output
runtime_publication_committed: 2
```

Semantic lesson: boundary metadata documents where an application-owned adapter
would translate external I/O into TopoExec payloads. The core runtime remains
adapter-agnostic.

Contrast invalid graph: ROS 2 QoS, Prometheus scraping, OpenTelemetry export,
native Python bindings, and plugin discovery are not active dependencies or
runtime features here.
