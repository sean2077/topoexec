# Live Observe Performance Policy

G73 turns "low overhead" into a validation target for the live runtime
workbench. The targets are per-machine comparisons against a local baseline, not
portable absolute latency guarantees.

## Budget

| Mode | Target overhead |
| --- | ---: |
| observe disabled | < 0.5% |
| observe summary | < 2% |
| observe detailed | < 5% |
| observe debug | no hard threshold; explicitly intrusive |

The disabled target protects existing `graph run`, `graph metrics`, and `graph
trace` behavior. Summary is the default `graph observe` mode. Detailed and debug
are opt-in investigation modes.

## Measurement policy

- Compare modes on the same machine, same build type, and same graph workload.
- Use repeated runs and report median plus p95 where timing is available.
- Do not enforce cross-machine absolute timing thresholds in CI.
- CI may run smoke/output-contract checks only when the environment is too noisy
  for timing thresholds.
- Thresholds are adjustable through environment variables for local release
  evidence, but docs and defaults remain conservative.

## Benchmark workloads

Checked-in live workloads should cover:

- minimal graph;
- high-frequency channel publish/commit paths;
- trigger stress;
- thread-pool lane behavior;
- CompositeLoop iteration/convergence.

Checked-in live-observe benchmark files:

```text
benchmarks/live_observe_minimal.yaml
benchmarks/live_observe_high_frequency_channels.yaml
benchmarks/live_observe_trigger_stress.yaml
benchmarks/live_observe_thread_pool.yaml
benchmarks/live_observe_composite_loop.yaml
```

## Focused gates

`./scripts/goal_check.sh live` verifies functional and schema behavior:

- observe schema smoke;
- CLI NDJSON smoke;
- assertion pass/fail smoke;
- record artifact smoke;
- replay smoke;
- dashboard static asset smoke;
- observer drop summary smoke.

`./scripts/goal_check.sh live-perf` verifies low-overhead evidence:

- disabled baseline;
- summary overhead;
- detailed overhead;
- high-frequency overflow does not block;
- observer drops are visible;
- no unbounded memory growth in bounded smoke scenarios.

## Local command shape

Example release-evidence loop:

```bash
topoexec graph bench benchmarks/live_observe_high_frequency_channels.yaml \
  --steps 1000 --runs 20 --format json

topoexec graph observe benchmarks/live_observe_high_frequency_channels.yaml \
  --steps 1000 --observe-level summary --record /tmp/topoexec-live-summary

topoexec graph observe benchmarks/live_observe_high_frequency_channels.yaml \
  --steps 1000 --observe-level detailed --record /tmp/topoexec-live-detailed

./scripts/goal_check.sh live-perf
```

`live-perf` must prefer a controlled per-machine baseline. If a local environment
cannot provide stable timing, record the skipped command, reason, replacement
smoke evidence, and whether the gap blocks release in the goal ledger.

The default `live-perf` gate is intentionally smoke/output-contract only. Set
`TOPOEXEC_LIVE_PERF_ENFORCE=1` to make the local per-machine thresholds
blocking for release evidence.

## Hot-path audit checklist

Before claiming the budget, inspect or test that producer paths avoid:

- JSON serialization;
- socket or file I/O;
- UI waits;
- payload body copies;
- unbounded allocations;
- blocking queue pushes;
- contended mutex waits;
- global shared-map lookups;
- global atomic event sequence increments for every event.

Collectors, recorders, assertion tools, and dashboards may use richer data
structures because they run outside the runtime hot path.
