# Goal Status

Last updated: 2026-05-05

## Current Plan Source

- Architecture plan: `docs/plans/plan.md`
- Backlog: `docs/goals/backlog.md`
- Required repository gate: `scripts/agent_check.sh`

## Active / Recent Goals

| ID | Status | Evidence | Notes |
| --- | --- | --- | --- |
| G0 | complete | `tests/golden/check_cli_golden.py`, `tests/golden/*.json`, `tests/golden/render_minimal.mmd`, `cli_golden_outputs` CTest, `docs/current-baseline.md`. | Normalizes volatile duration/trace fields while preserving semantic plan/metrics/trace/render drift detection. |
| G1 | complete | `docs/public-api.md` and `tests/cmake/runtime_smoke/main.cpp`. | Installed downstream smoke links only `topoexec::runtime` and uses GraphBuilder, typed payload access, ComponentRegistry, and RuntimeRunner. |
| G5 | complete | `schema/topoexec.schema.v1.json`, `docs/schema-v1.md`, `cli_validate_schema_only_minimal`, `cli_validate_semantic_minimal`, and `schema_v1_contract_smoke`. | JSON Schema is a generation/documentation contract; semantic SCC/port/trigger rules remain enforced by C++ validation. |
| G2 | complete | `RuntimeRunnerResult::runtime_errors`, `ExecutionSpec::on_error`, runtime lifecycle tests, thread_pool execute-failure test, and `docs/runtime-semantics.md`. | Existing `errors` strings remain compatible; structured errors expose phase/component/code/fatal and only `fail_fast` is implemented. |
| G4 | complete | `GraphValidationResult::diagnostics`, `GraphCompileResult::diagnostics`, CLI validate JSON diagnostics, `Graph.NonFailFastExecutionPolicyIsParsedButRejected`, and plan JSON. | Diagnostics expose code/severity/message/path/involved ids/suggested fix while keeping legacy errors. |
| G3 | complete | `docs/runtime-invariants.md` maps all 20 invariants to existing runtime, graph, channel, CLI, and golden tests. | No duplicate test file was added because the invariant coverage already exists in CI; the mapping is now explicit. |
| G6 | complete | `Runtime.ThreadPoolLaneExecutesReentrantInvocationsConcurrently`, `Runtime.ThreadPoolLaneSerializesNonReentrantInvocations`, `Runtime.ThreadPoolLaneQueueCapacityRejectsNewestWhenFull`, `Runtime.ThreadPoolLaneQueueCapacityDropsOldestWhenConfigured`, `Runtime.FixedRateSimulatedLaneReportsOverrunMetric`, `Graph.ParsesAndValidatesSchedulerLaneAdmissionFields`, `docs/scheduler.md`, and `docs/concurrency.md`. | Bounded-batch thread_pool v1 and simulated fixed_rate metrics are covered; persistent named workers and wall-clock sleep cadence remain explicit future work, not alpha claims. |
| G7 | complete | `Runtime.TaskExecutorCompletesDeterministicTasksInOrder`, `Runtime.TaskExecutorRejectsAndCancelsBoundedBacklog`, `Runtime.TaskExecutorReportsFailureAndGraphContextPublishesCompletion`, `docs/async-tasks.md`, and runtime package smoke. | Async task runtime is optional and deterministic with bounded backlog; threaded executor pools remain future work. |
| G8 | complete | `Channel.QueueDrainMaxBatchPreservesRemainingMessages`, `Channel.SnapshotDoesNotConsumeQueuedMessages`, `Channel.QueueMultiReaderMaintainsPerReaderCursor`, `Channel.DeadlineMissIsMarkedOnLateConsume`, `Channel.LifespanDropsStaleMessageBeforeDelivery`, runtime channel health metrics, `docs/channels.md`, and `docs/metrics.md`. | Channel/backpressure v1 is explicit and bounded; health/backpressure is observable through metrics and degradation reasons without recursive upstream execution. |
| G9 | complete | `Payload.OpaqueCustomPayloadPreservesSchemaAddressAndSummary`, `Channel.BufferPoolReusesReleasedFramesAndReportsMetrics`, `Channel.LoanedViewPreservesLoanedFrameBufferWithoutCopying`, `docs/payloads.md`, and `docs/memory.md`. | Custom type-erased payloads and in-process BufferPool metrics are available; external shared-memory zero-copy remains out of scope. |
| G10 | complete | `Runtime.RequestTriggerUsesRequestInvocationKind`, `Runtime.RequestTriggerDropsTimedOutPendingMessage`, `Runtime.FutureReadyEventSourceUsesFutureReadyEventKind`, `Runtime.TimeSyncDropsOldestOutOfSlopSampleUntilInputsAlign`, `Runtime.BatchTriggerFlushesPartialBatchAfterWindowExpires`, `docs/triggers.md`, and trigger metrics golden output. | Trigger engine owns readiness, timeout drop, batch flush, time-sync drop, and local correlation metadata; watermark/condition triggers remain future extensions. |
| G11 | complete | `Runtime.CompositeLoopRegionOwnsInternalFixedPointIterations`, `Runtime.CompositeLoopConvergenceStopsBeforeMaxIterations`, `Runtime.CompositeLoopBudgetOverrunStopsLoopAndReportsMetric`, `Runtime.CompositeLoopInternalFailureStopsLoopAndSuppressesExternalCommit`, `docs/composite-loops.md`, and loop metrics/trace docs. | CompositeLoop regions are bounded, observable, and prevent half-updated external output commits on internal failure; solver-style typed convergence remains future work. |
| G12 | complete | `Runtime.StateEdgeKeepsCommittedSnapshotIsolatedUntilNextEpoch`, `Runtime.ComponentConfigUpdatesApplyOnEpochBoundary`, `StateStore.*`, `ConfigSnapshotStore.ComponentConfigUpdatesRespectEpochBoundary`, `Graph.ParsesGraphLevelConfigSnapshot`, and `docs/state.md`. | State edges and optional blackboard/config stores are snapshot-based and epoch-boundary committed; single-writer blackboard semantics are enforced until an explicit merge policy exists. |
| G13 | complete | `Common.MetricsSnapshotIncludesCountersGaugesAndHistograms`, `Graph.DiagnosticRegistryExposesStableCodesAndFixes`, `docs/metrics.md`, `docs/trace-events.md`, and `docs/diagnostics.md`. | Observability now has scriptable metrics/trace JSON, Chrome trace, histogram percentile summaries, state/config metrics, and a stable diagnostic descriptor registry. |
| G14 | complete | `benchmarks/*.yaml`, `benchmarks/README.md`, `docs/performance-baselines.md`, `cli_bench_json_minimal`, and `cli_bench_json_immediate_chain`. | Benchmark output is scriptable and richer, but CI remains correctness-only and docs explicitly avoid cross-machine performance claims. |
| G15 | complete | `cli_doctor_json`, `cli_schema_dump_json`, `cli_schema_check_minimal_json`, README CLI docs, and schema docs. | CLI now has scriptable schema and doctor entry points while keeping replay/record/format-graph deferred until runtime event logs and formatter policy are stable. |
| G16 | complete | `docs/examples.md`, `examples/README.md`, `examples/state_config_snapshot.yaml`, `examples/batch_time_sync.yaml`, `examples/service_pipeline.yaml`, `examples/boundary_adapter_pattern.yaml`, `cli_validate_*` example smokes, `cli_run_*` example smokes, and existing app smokes. | Examples cover the plan categories without adding adapter dependencies; service and boundary examples are core boundary patterns, not external service/ROS implementations. |
| G17 | complete | `docs/README.md`, `docs/getting-started.md`, `docs/concepts.md`, `docs/graph-spec.md`, `docs/components.md`, `docs/lifecycle.md`, `docs/cli.md`, `tests/docs/check_docs.py`, and `docs_command_smoke`. | Docs now provide a tutorial/reference/design/architecture learning path; compileable snippets are represented by app/package smokes and selected tutorial/CLI commands are executable doc tests. |
| G18 | complete | `tests/fuzz/fuzz_graph_inputs.py`, `fuzz_graph_input_smoke`, `TOPOEXEC_ENABLE_ASAN`, `TOPOEXEC_ENABLE_UBSAN`, `TOPOEXEC_ENABLE_TSAN`, `scripts/sanitizer_check.sh`, `.github/workflows/ci.yml`, and `docs/testing-strategy.md`. | Malformed graph inputs are exercised as a deterministic no-crash fuzz smoke; ASAN+UBSAN is a blocking CI gate and TSAN remains non-blocking until concurrency maturity. |
| G19 | complete | `TOPOEXEC_BUILD_YAML`, `TOPOEXEC_BUILD_CLI`, `TOPOEXEC_BUILD_EXAMPLES`, `cmake_runtime_only_options_smoke`, `docs/build-and-package.md`, `packaging/vcpkg/README.md`, and `packaging/conan/README.md`. | Runtime-only configure/build/install works with YAML, CLI, examples, and tests disabled; package-manager notes remain drafts and do not add runtime dependencies. |
| G20 | complete | `docs/adapters.md`, `examples/adapters/README.md`, `tests/policy/check_no_adapter_deps.py`, `policy_no_core_adapter_deps`, and `scripts/goal_check.sh policy`. | Adapter preview defines result sink, observer, boundary bridge, factory-provider, schema policy, and future `topoexec_adapters` target boundaries without adding adapter SDK dependencies. |
| G21 | complete | `docs/adapters/ros2.md`, `docs/adapters.md`, and `policy_no_core_adapter_deps`. | ROS 2 remains a deferred separate adapter package; the design separates ROS QoS/executor/lifecycle concerns from internal TopoExec edge policy and runtime scheduling. |
| G22 | complete | `scripts/goal_check.sh`, `docs/agent-goals.md`, `.github/PULL_REQUEST_TEMPLATE.md`, `.github/ISSUE_TEMPLATE/*`, and `docs/contributing.md`. | Agents and reviewers have goal-specific validation, handoff, PR, issue, and contribution surfaces. |
| G23 | complete | `docs/architecture-guardrails.md`, `docs/public-api.md`, and runtime-only package smoke. | Module boundaries and dependency constraints are explicit and partially enforced by install smoke. |

## Current Stage

The repository has moved beyond the initial P0 baseline/API/schema lock. The next safe implementation stage is to finish the partial P0/P1 runtime hardening goals in this order:

1. G24 defensive input handling;
2. then G25 release progression work.

## Validation Evidence

Most recent targeted checks in this working tree:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure -R 'cli_validate_schema_only_minimal|cli_validate_semantic_minimal|schema_v1_contract_smoke|cli_golden_outputs'
ctest --test-dir build --output-on-failure -R 'schema_v1_contract_smoke|cli_golden_outputs|cmake_package_runtime_smoke'
ctest --test-dir build --output-on-failure -R 'test_state|test_runtime|test_graph|cli_golden_outputs|schema_v1_contract_smoke'
ctest --test-dir build --output-on-failure -R 'test_common|test_graph|cli_golden_outputs'
ctest --test-dir build --output-on-failure -R 'cli_bench_json_minimal|cli_bench_json_immediate_chain'
ctest --test-dir build --output-on-failure -R 'cli_doctor_json|cli_schema_dump_json|cli_schema_check_minimal_json'
ctest --test-dir build --output-on-failure -R 'cli_validate_state_config_snapshot|cli_validate_batch_time_sync|cli_validate_service_pipeline|cli_validate_boundary_adapter_pattern|cli_run_state_config_snapshot|cli_run_batch_time_sync|cli_run_service_pipeline|cli_run_boundary_adapter_pattern|schema_v1_contract_smoke'
ctest --test-dir build --output-on-failure -R 'docs_command_smoke'
ctest --test-dir build --output-on-failure -R 'fuzz_graph_input_smoke'
ctest --test-dir build --output-on-failure -R 'cmake_package_runtime_smoke|cmake_runtime_only_options_smoke'
ctest --test-dir build --output-on-failure -R 'policy_no_core_adapter_deps'
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/sanitizer_check.sh
```

Run `./scripts/agent_check.sh` before declaring a repo-changing stage complete.

## Blockers

No active blockers.
