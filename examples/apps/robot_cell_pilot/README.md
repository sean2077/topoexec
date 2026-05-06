# Robot Cell Pilot

`topoexec_app_robot_cell_pilot` is a dependency-free, in-process robotics-like
inspection cell. It is the G69 pilot app: realistic enough to combine perception,
planning, control, overload, state/config, payload ownership, metrics, and trace
surfaces, but still only links `topoexec_runtime`.

## Architecture

The app models a small pick/inspect cell:

```text
camera --async/loaned frame--> detector --immediate target--> planner --immediate plan--> controller --immediate command--> actuator
                                        |                         ^
                                        +--state obstacle---------+
controller --delay last_command-----------------------------------+
config_tuner --config transaction--> controller
```

Lanes are explicit:

- `acquisition`: event-loop source lane for the camera boundary.
- `perception`: bounded `thread_pool` lane for detector work.
- `control`: `fixed_rate` lane for planner/controller work.
- `supervision`: event-loop lane for config tuning and actuator output.

## Why TopoExec helps

- Explicit feedback: controller output returns to planning through a `delay` edge,
  while detector obstacle state crosses a `state` edge. Same-epoch recursion is
  avoided without hiding feedback.
- Bounded overload: the camera publishes a two-frame burst every epoch, but the
  async detector edge admits only one in-flight frame and drops stale frames.
- Observable ownership: camera frames are loaned from `BufferPool` and delivered
  through a `loaned_view` async edge; the detector verifies the payload address is
  preserved.
- C++ embedding: the graph is built with `GraphBuilder` and app-owned component
  factories; no YAML, CLI, ROS, Python, OTel, Prometheus, or plugin adapter is
  required.

## Expected metrics

Run:

```bash
./build/topoexec_app_robot_cell_pilot
```

Expected stable lines include:

```text
pilot_value=explicit_feedback_bounded_observable_cpp
detector_latest_frame=4
payload_pool_detached_count=6
payload_address_preserved=true
async_publication_count=6
bounded_overload_drop_count=5
state_feedback_epoch=3
delay_feedback_epoch=3
config_apply=controller.speed=0.40
controller_snapshot=speed=0.40;commands=2
error_path=invalid_config_rejected
```

`runtime_metric_samples` and `trace_event_count` are intentionally printed as
counts rather than exact goldens; they prove the app exposed observability without
freezing volatile trace detail.

## Failure and overload scenario

The normal run verifies bounded overload by dropping older burst frames and still
commanding the latest detected frame. The same binary then runs an error scenario
where `config_tuner` stages an invalid controller speed. Runtime config
validation rejects the transaction at an epoch boundary, and the app prints
`error_path=invalid_config_rejected` instead of silently continuing with unsafe
control parameters.
