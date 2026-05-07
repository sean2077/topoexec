# Robot-cell sensor fusion mini scenario

## What this example demonstrates

A small robot-cell-inspired event/data pipeline with time sync, detector stage, async planner, and output boundary.

## Graph structure

Primary graph: [`sensor_fusion_mini.yaml`](sensor_fusion_mini.yaml).

- Category: Realistic Scenario
- Difficulty: intermediate
- Tags: realistic, robot-cell, async

## How to run

```bash
./build/topoexec graph validate examples/80-realistic-mini-scenario/sensor_fusion_mini.yaml
./build/topoexec graph run examples/80-realistic-mini-scenario/sensor_fusion_mini.yaml --steps 3
./build/topoexec graph observe examples/80-realistic-mini-scenario/sensor_fusion_mini.yaml --steps 3 --observe-level summary --format ndjson
```

## Expected result

The graph remains a local synthetic scenario: no ROS, hardware, or production deployment claim is made.

## What to inspect

- `example.json` for the commands, generated asset expectations, and CI smoke flags.
- Generated graph asset under `docs/assets/generated/examples/sensor-fusion-mini/graph.mmd` after running the asset pipeline.
- Runtime metrics, trace, observe, or diagnostics output when the command list includes those surfaces.

## Related docs

- [`docs/11-user-guide/case-study-robot-cell.md`](../../docs/11-user-guide/case-study-robot-cell.md)
- [`docs/41-development-tools/live-runtime-validation.md`](../../docs/41-development-tools/live-runtime-validation.md)
