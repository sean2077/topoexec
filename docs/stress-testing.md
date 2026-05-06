# Stress and Soak Testing

TopoExec stress tests are bounded confidence checks for scheduler, channel, and
optional task-executor surfaces. They are not benchmark claims and do not set
latency or throughput thresholds.

## Default stress smoke

The default smoke is short enough for normal CTest and `./scripts/agent_check.sh`:

```bash
./scripts/goal_check.sh stress
```

It runs two surfaces:

1. `test_stress`, a C++ stress target that overloads the opt-in
   `ThreadedTaskExecutor` and a `thread_pool` graph using a burst source. The
   assertions require bounded queue depth, expected scheduler/task rejections,
   completion of admitted work, and no runtime errors.
2. `stress_graph_smoke`, a generated-graph CLI workload suite covering:
   - high fan-out;
   - high fan-in through a join tree;
   - long immediate chains;
   - mixed immediate, delay, state, and async edges;
   - bounded `thread_pool` fan-out.

The generated graph suite asserts:

- `ok: true` from `topoexec graph metrics`;
- no `errors` or `runtime_errors`;
- scheduler queue depth never exceeds queue capacity;
- expected drop/reject counters match the workload contract.

## Opt-in soak mode

Longer soak runs are deliberately outside the slow default CI path. Use the
wrapper and choose bounds explicitly:

```bash
TOPOEXEC_STRESS_PROFILE=soak \
TOPOEXEC_STRESS_SCALE=64 \
TOPOEXEC_STRESS_STEPS=200 \
TOPOEXEC_STRESS_DURATION_SECONDS=60 \
./scripts/stress_smoke.sh
```

Each graph execution still has bounded steps. `TOPOEXEC_STRESS_DURATION_SECONDS`
only repeats complete bounded suites until the duration or max-iteration bound is
reached. Set `TOPOEXEC_STRESS_MAX_ITERATIONS` when you want an additional
iteration cap lower than the duration would allow.

## Direct script use

For local debugging, call the Python checker directly:

```bash
python3 tests/stress/check_stress_workloads.py \
  --topoexec build/topoexec \
  --profile smoke \
  --scale 24 \
  --steps 8
```

The script prints a JSON summary listing the profile, scale, step count,
iteration count, elapsed time, and workload names.

## Sanitizer policy

Because `test_stress` and `stress_graph_smoke` are normal CTest tests, the
existing non-blocking ThreadSanitizer CI job runs the selected stress surfaces in
the TSAN build. ASAN+UBSAN also runs them through the default sanitizer CTest.

TSAN remains non-blocking until the concurrency signal is stable enough for a
beta release blocker. Record TSAN failures as concurrency evidence; do not treat
stress success as proof of real-time behavior, absence of all races, or stable
performance.
