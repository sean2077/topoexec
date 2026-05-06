# Testing Strategy

TopoExec uses a small test pyramid that favors semantic runtime coverage over
large fixtures. The default local gate remains:

```bash
./scripts/agent_check.sh
```

This configures, builds, and runs all default CTest tests.

## Test layers

| Layer | Evidence | Command |
| --- | --- | --- |
| Unit | `test_common`, `test_graph`, `test_channel`, `test_state`, `test_runtime` | `ctest --test-dir build --output-on-failure -R 'test_'` |
| Semantic runtime | graph compiler, edge visibility, trigger, scheduler, async, CompositeLoop, state/config tests | `ctest --test-dir build --output-on-failure -R 'test_graph|test_runtime|test_state'` |
| Golden CLI | normalized plan, metrics, trace, and render outputs | `./scripts/goal_check.sh golden` |
| Schema | strict schema contract plus schema/semantic CLI split | `./scripts/goal_check.sh schema` |
| Docs | recursive `topoexec-doc-test` markers plus docs learning-map/section contract | `./scripts/goal_check.sh docs` |
| Example apps | dependency-free reference apps plus the G69 robot-cell pilot covering latest/drop, fixed-rate state feedback, task completion, CompositeLoop, payload ownership, multi-lane feedback, config snapshots, metrics/trace, and invalid-config rejection | `ctest --test-dir build --output-on-failure -R 'app_'` |
| Fuzz smoke | deterministic malformed, invalid-UTF-8, oversized, parser-limit corpus plus optional fuzzer target corpus replay | `./scripts/goal_check.sh fuzz` |
| Stress smoke | generated scheduler/channel graph workloads plus task-executor/thread-pool overload stress | `./scripts/goal_check.sh stress` |
| Benchmark smoke | RuntimeRunner benchmark cases, task-executor benchmark output, schema v2 metadata, and optional local baseline generation | `./scripts/goal_check.sh bench` |
| Package | install/export/downstream `find_package(topoexec)` runtime-only, adapter SDK, YAML, and CLI smokes | `./scripts/goal_check.sh package` |
| Adapter previews | optional adapter targets, package exports, and dependency-boundary policy | `./scripts/goal_check.sh adapters` |
| Release prep | non-publishing release_prepare dry run, release notes draft, and human-only tag-command contract | `./scripts/goal_check.sh release` |
| Sanitizers | ASAN+UBSAN full CTest; TSAN non-blocking CI | `./scripts/goal_check.sh sanitizer` |

## Fuzz smoke

`tests/fuzz/fuzz_graph_inputs.py` generates a deterministic corpus of malformed,
partial, cyclic, nested, invalid-UTF-8, oversized, and mutated YAML graph inputs.
The optional `fuzz_graph_inputs` target can also replay
`tests/fuzz/corpus/graph_inputs` in standalone mode or run as a libFuzzer target
when built with Clang. Together they prove the CLI/parser/compiler path rejects
hostile inputs without timeouts, crash-like exits, or sanitizer/crash markers,
and they provide a place to store minimized crash regressions.

Run it directly with:

```bash
ctest --test-dir build --output-on-failure -R fuzz_graph_input_smoke
TOPOEXEC_FUZZER_ENGINE=STANDALONE ./scripts/fuzz_smoke.sh
```

See [Coverage-guided fuzzing](fuzzing.md) for libFuzzer commands and corpus
rules.

## Stress and soak smoke

`test_stress` exercises opt-in `ThreadedTaskExecutor` overload and a bursty
`thread_pool` graph with expected rejections and bounded queue-depth assertions.
`stress_graph_smoke` generates high fan-out, high fan-in, long-chain, mixed
immediate/delay/state/async, and bounded thread-pool graph workloads and runs
them through `topoexec graph metrics`.

Run the focused gate with:

```bash
./scripts/goal_check.sh stress
```

Longer soak runs are opt-in and bounded by caller-selected steps, duration, and
iteration limits:

```bash
TOPOEXEC_STRESS_PROFILE=soak TOPOEXEC_STRESS_DURATION_SECONDS=60 ./scripts/stress_smoke.sh
```

See [Stress and soak testing](stress-testing.md) for workload details and
configuration. Stress success is confidence evidence, not a performance or
real-time guarantee.

## Benchmark smoke

`bench_contract_smoke` runs all checked-in benchmark YAML cases through
`topoexec graph bench --format json` and asserts schema v2 metadata, graph hash,
per-run samples, p50/p95/p99 summaries, throughput fields, and empty errors.
`bench_task_executor_smoke` covers the non-installed task-executor benchmark
binary. `./scripts/goal_check.sh bench` also writes a short local baseline to
`/tmp/topoexec-bench-baseline.json` without applying a timing threshold.

Longer comparisons are opt-in and per-machine:

```bash
TOPOEXEC_BENCH_BASELINE_IN=benchmarks/local-baseline.json \
TOPOEXEC_BENCH_THRESHOLD_PERCENT=15 \
./scripts/bench_baseline.sh
```

See [Performance baselines](performance-baselines.md) for interpretation and
policy. Benchmark success proves output-contract and workload health, not a
portable performance guarantee.

## Docs smoke

`docs_command_smoke` runs every `topoexec-doc-test` marker under `docs/` and
verifies the G55/G69/G70 docs map: getting-started, concepts, the robot-cell
case study, runtime semantics, API reference, schema, cookbook, adapters,
testing/release pages, beta-readiness review, architecture diagrams, why-not
comparisons, and design principles. It also executes the G56 reference-app
binaries and the G69 pilot app listed from `docs/examples.md`. Add a marker for
commands that should remain executable, and update `tests/docs/check_docs.py`
only when the documentation contract intentionally changes.

## Release prep smoke

`release_prepare_smoke` runs `scripts/release_prepare.sh --dry-run --allow-dirty`
with the recommended `v0.2.0-alpha.0` prerelease target. It proves the script
can validate release docs/changelog policy, draft notes, and write the
human-only annotated tag command without creating artifacts, tags, or uploads.
Use the focused gate for script changes:

```bash
./scripts/goal_check.sh release
```

Artifact creation is covered by candidate rehearsals through
`scripts/release_prepare.sh --skip-gates` or by a full clean-tree release prep
when a human-approved candidate commit is ready.

## Sanitizer gates

CMake exposes explicit sanitizer options for GCC/Clang builds:

```bash
cmake -S . -B build-asan-ubsan -DCMAKE_BUILD_TYPE=Debug \
  -DTOPOEXEC_ENABLE_ASAN=ON \
  -DTOPOEXEC_ENABLE_UBSAN=ON
cmake --build build-asan-ubsan -j
ctest --test-dir build-asan-ubsan --output-on-failure
```

The wrapper is shorter:

```bash
TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/sanitizer_check.sh
TOPOEXEC_SANITIZER_MODE=thread ./scripts/sanitizer_check.sh
```

`address-undefined` runs ASAN+UBSAN together. `thread` runs TSAN alone because
TSAN cannot be combined with ASAN/UBSAN in this build. The package smoke passes
sanitizer link flags to its downstream runtime-only app so sanitizer builds still
verify install/export behavior.

## CI policy

- GCC and Clang Debug/RelWithDebInfo matrix jobs run the default gate.
- ASAN+UBSAN is a blocking CI job.
- TSAN remains non-blocking until the runtime concurrency surface is mature
  enough to make it a release blocker. The selected stress smoke tests run in
  TSAN because they are normal CTest entries.

## Adding tests

When changing runtime semantics, add or update the smallest semantic test that
proves the claim, then update docs/goldens only if public behavior intentionally
changed. New docs commands should include a `topoexec-doc-test` marker so
`docs_command_smoke` keeps the prose executable.
