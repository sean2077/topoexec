# Reliability, Soak, and Performance Regression Program

Status: G79 bounded local reliability program for the `v0.2.0-alpha.0` candidate
line. It layers existing tests into tiers and keeps longer evidence opt-in so CI
and agent runs do not become flaky or unbounded.

## Test tiers

| Tier | Purpose | Required command |
| --- | --- | --- |
| T0 required gate | Formatting, commit-message policy, build, clang-tidy, and default CTest. | `./scripts/agent_check.sh` |
| T1 compatibility/release | Stable-v0.2 contract, package install/downstream, docs, release-prep dry run. | `./scripts/goal_check.sh compat`, `package`, `docs`, `release` |
| T2 runtime robustness | Bounded stress, deterministic fuzz smoke, benchmark contract, live observe smoke. | `./scripts/goal_check.sh stress`, `fuzz`, `bench`, `live` |
| T3 sanitizer | ASAN+UBSAN full CTest. TSAN remains non-blocking policy evidence. | `./scripts/goal_check.sh sanitizer` |
| T4 opt-in soak/perf | Bounded soak-lite or longer human-requested runs; timing thresholds are per-machine. | `./scripts/soak_lite_smoke.sh`, opt-in benchmark/live-perf thresholds |

## Bounded soak-lite

The default soak-lite command repeats the generated stress workload suite with
small explicit limits:

```bash
./scripts/soak_lite_smoke.sh
```

It sets `TOPOEXEC_STRESS_PROFILE=soak`, a short duration, a maximum iteration
count, and small scale/step values. Longer soaks must set explicit duration,
iteration, scale, and timeout values and should attach output as local release
or CI evidence instead of editing docs with machine-specific timings.

## Performance regression policy

`./scripts/goal_check.sh bench` proves benchmark JSON shape, graph hashes,
samples, p50/p95/p99 summaries, and local baseline generation. It does not apply
a global timing threshold. Timing comparisons are opt-in and per-machine:

```bash
TOPOEXEC_BENCH_BASELINE_IN=benchmarks/local-baseline.json \
TOPOEXEC_BENCH_THRESHOLD_PERCENT=15 \
./scripts/bench_baseline.sh
```

If a future goal adds thresholds, it must document hardware, compiler, build
type, run count, accepted variance, and failure artifact location.

## Fuzz corpus and sanitizer policy

Deterministic fuzz smoke owns the checked-in corpus under
`tests/fuzz/corpus/graph_inputs/`. New minimized parser/compiler crashes should
be added there with an issue or changelog note. Coverage-guided fuzz campaigns
remain optional release evidence, not a default local gate.

ASAN+UBSAN is the blocking sanitizer gate. ThreadSanitizer remains non-blocking
until release governance changes it because current concurrency previews and
platform scheduler behavior can make TSAN policy noisy. Do not hide TSAN gaps;
record whether it was run and whether failures are blocking in release notes.

## Failure artifact convention

When a robustness gate fails, capture the smallest durable artifact set:

```text
/tmp/topoexec-<gate>-*.json
/tmp/topoexec-<gate>-*.txt
/tmp/topoexec-<gate>-*.ndjson
/tmp/topoexec-<gate>-*.log
```

For release candidates, copy relevant artifacts into the release-prep bundle or
CI job artifacts. Do not commit machine-specific benchmark baselines unless a
future release owner explicitly asks for a portable baseline policy.

## Non-goals

- no hard real-time guarantee;
- no unbounded soak or long fuzz campaign in the required local gate;
- no global performance threshold across machines;
- no production telemetry exporter or dashboard control plane;
- no hidden sanitizer exceptions.
