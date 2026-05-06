# Current Baseline

Date: 2026-05-06 (Asia/Singapore)

Baseline snapshot target:

```text
post-G25 main protected as the starting point for docs/plans/plan2.md (G26+)
```

Baseline commit before G26 documentation/test capture:

```text
b86a586d3a48d84bf4e03ccabde3d061e3073579 记录完成后的发布阶梯与证据要求
```

Release tag relationship:

```text
v0.1.0-alpha points to 201d3e0c75a4334f404085d57282415fa9678fe9.
The post-G25 baseline is v0.1.0-alpha-27-gb86a586 and includes the completed G0-G25 sweep.
No public tag exists yet for the plan2/G26+ baseline.
```

Release decision note:

```text
Recommended next prerelease: v0.2.0-alpha.0.
Reason: the post-G25 tree contains runtime-completeness alpha work beyond a small v0.1.x stabilization patch, while exporter adapters, long fuzz/soak campaigns, and beta readiness remain explicit future work. RuntimeObserver v1, persistent worker-pool v1, fixed-rate wall-clock cadence v1, coverage-guided fuzz smoke, and bounded stress smoke are now part of the plan2 architecture-stabilization line.
Human release approval should still verify CI on the exact tag commit before creating the annotated tag.
```

Environment used for local reproduction:

```text
OS: Ubuntu 24.04.4 LTS, Linux 6.17.0-23-generic x86_64
C++ compiler: g++ 13.3.0 (Ubuntu 13.3.0-6ubuntu2~24.04.1)
CMake: 3.28.3
CTest: 3.28.3
clang-format: /usr/bin/clang-format, Ubuntu clang-format 22.1.3
```

Commands required for this baseline:

```bash
git diff --check
./scripts/agent_check.sh
cmake --build build --target topoexec_format_check
./scripts/goal_check.sh package
./scripts/goal_check.sh golden
./scripts/goal_check.sh stress
./scripts/goal_check.sh bench
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
```

Observed G26 local result:

```text
git diff --check passed through ./scripts/agent_check.sh.
./scripts/agent_check.sh passed: 50/50 CTest tests in the default RelWithDebInfo GCC build.
cmake --build build --target topoexec_format_check passed.
./scripts/goal_check.sh package passed: cmake_package_runtime_smoke and cmake_runtime_only_options_smoke.
./scripts/goal_check.sh golden passed: cli_golden_outputs.
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer passed: 50/50 CTest tests in the ASAN+UBSAN Debug build.
```

Observed G52 local result:

```text
cmake --build build -j passed.
cmake --build build --target topoexec_format_check passed.
ctest -R 'test_stress|stress_graph_smoke|docs_command_smoke|cli_golden_outputs|schema_v1_contract_smoke' passed: 5/5 focused tests.
./scripts/goal_check.sh quick passed.
./scripts/goal_check.sh stress passed: test_stress plus generated stress graph smoke.
TOPOEXEC_STRESS_PROFILE=soak TOPOEXEC_STRESS_SCALE=16 TOPOEXEC_STRESS_STEPS=8 TOPOEXEC_STRESS_DURATION_SECONDS=1 TOPOEXEC_STRESS_MAX_ITERATIONS=2 ./scripts/stress_smoke.sh passed: two bounded iterations.
./scripts/agent_check.sh passed: 58/58 CTest tests in the default RelWithDebInfo GCC build.
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer passed: 58/58 CTest tests in the ASAN+UBSAN Debug build.
git diff --check and python3 -m py_compile tests/stress/check_stress_workloads.py passed.
```

Observed G53 local result:

```text
cmake --build build -j passed.
cmake --build build --target topoexec_format_check passed.
ctest -R 'bench|cli_bench|cli_golden_outputs|schema_v1_contract_smoke|docs_command_smoke' passed: 8/8 focused tests.
./scripts/goal_check.sh quick passed.
./scripts/goal_check.sh bench passed: benchmark CTest smokes plus /tmp local baseline generation without thresholds.
./scripts/agent_check.sh passed: 60/60 CTest tests in the default RelWithDebInfo GCC build.
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer passed: 60/60 CTest tests in the ASAN+UBSAN Debug build.
git diff --check and python3 -m py_compile scripts/bench_baseline.py tests/bench/check_bench_contract.py passed.
```

Observed G54 local result:

```text
cmake --build build -j passed.
cmake --build build --target topoexec_format_check passed.
ctest -R 'cmake_package_runtime_smoke|cmake_runtime_only_options_smoke|cmake_cpack_smoke|package_draft_smoke|cli_golden_outputs|docs_command_smoke' passed: 6/6 focused tests.
./scripts/goal_check.sh package passed: installed runtime/YAML/CLI downstream smokes, runtime-only option smoke, CPack TGZ smoke, and package draft smoke.
./scripts/goal_check.sh quick passed.
./scripts/agent_check.sh passed: 62/62 CTest tests in the default RelWithDebInfo GCC build.
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer passed: 62/62 CTest tests in the ASAN+UBSAN Debug build.
git diff --check and python3 -m py_compile tests/package/check_package_drafts.py passed.
```

Observed G55 local result:

```text
./scripts/goal_check.sh docs passed: recursive docs command smoke plus G55 docs map/section contract.
./scripts/goal_check.sh quick passed.
cmake --build build --target topoexec_format_check passed.
./scripts/agent_check.sh passed: 62/62 CTest tests in the default RelWithDebInfo GCC build.
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer passed: 62/62 CTest tests in the ASAN+UBSAN Debug build.
git diff --check and python3 -m py_compile tests/docs/check_docs.py passed.
```

Observed G56 local result:

```text
ctest -R 'app_|docs_command_smoke' passed: 12/12 focused docs/example tests, including five new reference-app smokes.
./scripts/goal_check.sh docs passed: recursive docs command smoke, including reference-app doc markers.
./scripts/goal_check.sh quick passed.
cmake --build build --target topoexec_format_check passed.
./scripts/agent_check.sh passed: 67/67 CTest tests in the default RelWithDebInfo GCC build.
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer passed: 67/67 CTest tests in the ASAN+UBSAN Debug build.
git diff --check passed.
```

Observed G57 local result:

```text
ctest -R 'test_adapter_sdk|policy_no_core_adapter_deps|cmake_package_runtime_smoke|cmake_runtime_only_options_smoke' passed: 4/4 focused Adapter SDK tests.
./scripts/goal_check.sh package passed: installed/runtime-only Adapter SDK downstream smoke included.
./scripts/goal_check.sh policy passed: adapter SDK target boundary and no-core-adapter-deps policy.
./scripts/goal_check.sh quick passed.
cmake --build build --target topoexec_format_check passed.
./scripts/agent_check.sh passed: 68/68 CTest tests in the default RelWithDebInfo GCC build.
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer passed: 68/68 CTest tests in the ASAN+UBSAN Debug build.
./scripts/goal_check.sh docs and git diff --check passed after adapter docs/ledger updates.
```

Observed G67 local result:

```text
bash -n scripts/release_prepare.sh and python3 -m py_compile tests/release/check_release_prepare.py tests/docs/check_docs.py passed.
./scripts/goal_check.sh release passed: release_prepare_smoke dry-ran release preparation without tagging or creating artifacts.
./scripts/release_prepare.sh --version v0.2.0-alpha.0 --skip-gates --allow-dirty --artifacts-dir build/release-prepare-artifact-smoke --build-dir build-release-candidate-smoke --notes-out build/release-prepare-artifact-smoke/release-notes-v0.2.0-alpha.0.md passed: release notes, source archive, CPack binary/source TGZ, schema artifact, SHA256SUMS, and human-only tag-command draft were generated without publishing.
./scripts/goal_check.sh docs passed: recursive docs command smoke including the release runbook marker.
./scripts/goal_check.sh package passed: installed package, runtime-only option, CPack, and package-draft smokes.
./scripts/goal_check.sh quick passed.
cmake --build build --target topoexec_format_check passed.
./scripts/agent_check.sh passed: 69/69 CTest tests in the default RelWithDebInfo GCC build.
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer passed: 69/69 CTest tests in the ASAN+UBSAN Debug build.
git diff --check passed.
```

Observed G69 local result:

```text
ctest --test-dir build --output-on-failure -R 'app_robot_cell_pilot_runs|docs_command_smoke' passed: robot-cell pilot smoke and docs command marker.
./build/topoexec_app_robot_cell_pilot passed: multiple lanes, async overload/drop, state/delay feedback, BufferPool frame identity, config apply/snapshot, metrics/trace, observer evidence, and invalid-config rejection self-checks.
./scripts/goal_check.sh docs passed: recursive docs command smoke including the robot-cell case-study marker.
./scripts/goal_check.sh quick passed.
cmake --build build --target topoexec_format_check passed.
./scripts/agent_check.sh passed: 70/70 CTest tests in the default RelWithDebInfo GCC build.
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer passed: 70/70 CTest tests in the ASAN+UBSAN Debug build.
git diff --check passed.
```

Golden output surfaces protected after G26:

- `tests/golden/plan_composite_loop.json` — graph plan JSON.
- `tests/golden/metrics_minimal.json` — metrics JSON.
- `tests/golden/trace_minimal.json` — structured trace JSON.
- `tests/golden/trace_minimal_chrome.json` — Chrome trace shape with volatile timing and trace ids normalized.
- `tests/golden/render_minimal.mmd` — Mermaid render output.
- `tests/golden/schema_dump.json` — schema dump JSON.
- `tests/golden/doctor.json` — doctor JSON.

Current branch limitations after the plan2 G69 real-world pilot pass:

- `thread_pool` lanes use persistent worker-pool v1 with bounded runtime-priority admission, cooperative cancellation/timeout-budget observation, queue/rejection/priority metrics, and worker-id trace attributes. CPU affinity, RT policy, portable hard thread-name guarantees, advanced starvation aging, and hard timeout preemption are not implemented.
- `fixed_rate` lane behavior remains deterministic/simulated by default; opt-in wall-clock cadence v1 exists, but independent lane threads, OS jitter control, and hard real-time scheduling are not implemented.
- Async `policy.max_inflight` controls async edge admission; it is separate from optional task executors.
- `TaskExecutor` remains deterministic by default with cooperative pending-task cancellation and post-return task-budget metrics; `ThreadedTaskExecutor` is now an opt-in bounded preview, not a default scheduler lane.
- Metrics/trace/diagnostics exist, including metric schema version 1, trace schema version 1, diagnostic schema version 1, invocation correlation/causation metadata, bounded observer-only health events, RuntimeObserver v1, and Adapter SDK v0. Concrete exporter adapters and a richer health-event v2 contract remain future work.
- Graph input loading is bounded by `GraphInputLimits` with UTF-8 validation, incremental file-size rejection, schema string/count limits, CLI parser-limit overrides, deterministic malformed-input fuzz smoke, and an optional `fuzz_graph_inputs` libFuzzer/standalone corpus target. Longer fuzz campaigns and broader target coverage remain future hardening work.
- Bounded stress smoke now covers generated scheduler/channel graph workloads, `thread_pool` overload, and `ThreadedTaskExecutor` overload. Longer soak runs are opt-in release-candidate evidence, not default slow-path CI or performance claims.
- Benchmark schema v2 now covers graph hashes, compiler/build/CPU/commit metadata, expanded RuntimeRunner graph cases, and a non-installed task-executor benchmark. Local baseline files and threshold comparisons are opt-in per-machine evidence, not default CI gates or global performance claims.
- Installed CMake package smoke now covers runtime-only, Adapter SDK, YAML, imported CLI, CPack TGZ, and package-manager draft surfaces. vcpkg/Conan files are still drafts pending external registry/clean-machine validation, and CPack archives are local release-candidate artifacts rather than signed release artifacts.
- Release automation can prepare candidate notes, local artifacts, checksums, and a human-only annotated tag command, but it does not publish, upload signed release assets, create tags, retag, or replace human release approval.
- Documentation now has an executable cookbook, architecture diagrams, why-not comparisons, and design principles with recursive docs smoke coverage. The docs still describe adapter/exporter surfaces as deferred unless future goals implement them.
- Reference apps now cover low-latency latest/drop, fixed-rate state feedback, request/validator/task completion, CompositeLoop solver convergence/budget overrun, BufferPool copy/shared/loaned metrics, and the G69 robot-cell pilot that composes multiple lanes, async overload, state/delay feedback, config snapshots, metrics/trace, and invalid-config rejection. They remain dependency-free in-process examples; hierarchical graph preview stays deferred until G41 and no external adapter stack is implemented by G69.
- The robot-cell pilot is an embedded case study, not a hardware driver, robot controller, camera integration, ROS graph, exporter integration, or external scheduling guarantee.
- ThreadSanitizer remains non-blocking.
- ROS 2, OpenTelemetry, Prometheus, Python, C API, dynamic plugin loading, and external Perfetto adapters remain deferred and must not be claimed as implemented; G57 provides only a dependency-free SDK boundary.
- Package-manager recipes under `packaging/` are drafts, not published ecosystem packages.
