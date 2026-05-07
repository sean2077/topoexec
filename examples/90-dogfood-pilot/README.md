# Synthetic Robot-Cell Dogfood Pilot

This example is a deterministic, dependency-free pilot for release/adoption
validation. It is inspired by a robot-cell inspection/control slice, but it is
not connected to hardware, ROS 2, camera SDKs, ML runtimes, production telemetry
exporters, or hard-real-time scheduling.

## What this example demonstrates

- two synthetic input boundaries synchronized with `time_sync`;
- an estimator/planner/controller path with an `async` bounded-inflight edge;
- explicit delayed feedback from controller correction back to the estimator;
- state-edge audit output visible on the next epoch boundary;
- metrics, trace, live observe, live assertions, and benchmark smoke using the
  same graph.

## Graph structure

```text
camera_event ----\
                 fusion -> estimator --async--> planner_worker -> controller -> actuator
force_event  ----/                         ^             |              |
                                           |             |              +--state--> audit_log
                                           +---delay-----+
```

Generated assets live under
`docs/assets/generated/examples/dogfood-robot-cell/` after running the showcase
asset generator.

## How to run

```bash
./build/topoexec graph validate examples/90-dogfood-pilot/dogfood_robot_cell.yaml
./build/topoexec graph run examples/90-dogfood-pilot/dogfood_robot_cell.yaml --steps 20
./build/topoexec graph metrics examples/90-dogfood-pilot/dogfood_robot_cell.yaml --steps 20 --format json
./build/topoexec graph trace examples/90-dogfood-pilot/dogfood_robot_cell.yaml --steps 20 --format json
./build/topoexec graph observe examples/90-dogfood-pilot/dogfood_robot_cell.yaml --steps 20 \
  --observe-level summary \
  --assert examples/90-dogfood-pilot/assertions.yaml \
  --format ndjson
./build/topoexec graph bench examples/90-dogfood-pilot/dogfood_robot_cell.yaml --steps 3 --runs 2 --format json
```

## Expected result

The validate/run/metrics/trace/observe/bench commands should exit `0`. The live
assertions require zero runtime errors, actuator completion within a bounded
number of observe events, and zero observer drops for the default smoke size.

## What to inspect

- plan/render output for the explicit async, delay, and state boundaries;
- metrics for channel publish/delivery/drop counts and async accounting;
- trace output for component order and epoch boundaries;
- observe NDJSON for live assertion records and runtime/component events;
- benchmark JSON for local machine-specific timing metadata only.

## Boundaries

This is synthetic dogfooding evidence, not a production deployment. It does not
claim production ROS 2 integration, real camera/force hardware, native Python,
production OpenTelemetry/Prometheus, external Perfetto, schema v2, package
registry publication, or hard real-time behavior.
