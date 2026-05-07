# Case Study: Dependency-Free Robot Cell Pilot

TopoExec uses an in-process robotics-like inspection/control cell as the first
real-world pilot. It deliberately avoids ROS 2, camera SDKs, native Python
bindings, OpenTelemetry, Prometheus, dynamic plugins, and external shared-memory
middleware. The point is
to show that the core C++ runtime can model a realistic embedded graph before any
adapter layer exists.

## Scenario

A camera emits small frame bursts, a detector consumes the latest admitted frame,
a planner combines detections with retained obstacle state and delayed command
feedback, a controller emits actuator commands, and a config tuner stages a safe
controller-speed update.

```text
camera --async/loaned frame--> detector --target--> planner --plan--> controller --command--> actuator
                               detector --state obstacle--------------^        |
                               controller --delay last_command-----------------+
                               config_tuner --epoch config update--> controller
```

## Runtime contracts exercised

| Contract | Pilot evidence |
| --- | --- |
| Multiple lanes | `acquisition`, `perception`, `control`, and `supervision` lanes are declared in C++ with event-loop, thread-pool, and fixed-rate semantics. |
| Async edge | Camera frames cross `camera_detector_async`, so detector work is deferred to a later epoch. |
| Bounded overload | The camera publishes two frames per epoch while the async edge admits one in-flight frame and drops stale burst members. |
| Payload pool | Frames are loaned from `BufferPool`, detached into `FrameView`, and consumed through a `loaned_view` edge with address preservation verified. |
| State/delay feedback | Detector obstacle state uses a `state` edge; controller command feedback uses a `delay` edge. The planner observes both in epoch 3. |
| Config snapshot | `config_tuner` stages `controller.speed=0.40`; the runtime validates/applies the transaction at the epoch boundary and snapshots controller state after the run. |
| Metrics and trace | The app asserts non-empty runtime metrics, trace events, observer metrics, observer traces, and a `config_transaction_apply` trace event. |
| Error path | A second run stages invalid controller config and expects runtime validation to reject it. |

## Run and expected output

```bash
./build/topoexec_app_robot_cell_pilot
```

<!-- topoexec-doc-test: ${BUILD_DIR}/topoexec_app_robot_cell_pilot -->

Expected stable lines:

```text
pilot=robot_cell_pilot
pilot_value=explicit_feedback_bounded_observable_cpp
detector_latest_frame=4
payload_address_preserved=true
state_feedback_epoch=3
delay_feedback_epoch=3
error_path=invalid_config_rejected
```

## YAML dogfood pilot

The YAML-first synthetic dogfood pilot exercises the same release/adoption
story through the public CLI and examples metadata pipeline:

```bash
./build/topoexec graph validate examples/90-dogfood-pilot/dogfood_robot_cell.yaml
./build/topoexec graph observe examples/90-dogfood-pilot/dogfood_robot_cell.yaml --steps 20 \
  --observe-level summary \
  --assert examples/90-dogfood-pilot/assertions.yaml \
  --format ndjson
./build/topoexec graph bench examples/90-dogfood-pilot/dogfood_robot_cell.yaml --steps 3 --runs 2 --format json
```

Generated topology, metrics, and trace cards live under
`docs/assets/generated/examples/dogfood-robot-cell/`. The focused local gate is:

```bash
./scripts/goal_check.sh dogfood
```

The dogfood YAML is intentionally synthetic and dependency-free. Expected
async/channel drops are allowed when they are visible in metrics and explained by
bounded `max_inflight` and `drop_oldest` policy; runtime errors, assertion
failures, and observer drops are not expected in the default smoke.

## Why this is not just another demo

Earlier reference apps isolate one semantic at a time. This pilot composes them:
app-owned C++ factories, multiple lane types, async admission, pool-backed large
payloads, feedback isolation, config transactions, snapshots, metrics, trace, and
a real failure path all run in one binary. That combination is the evidence that
TopoExec can be embedded as a semantic runtime in a real application slice without
pulling in an adapter stack.

## Boundaries

This pilot is still dependency-free and in-process. It does not implement robot
hardware I/O, ROS 2 executors, camera drivers, OpenTelemetry/Prometheus exporters,
external Perfetto integration, native Python bindings, stable C ABI usage, or
dynamic plugin loading. A separate default-off trusted-native loader preview
exists, but this pilot intentionally stays on explicit in-process registration
and does not claim plugin package discovery, sandboxing, or stable ABI support.
