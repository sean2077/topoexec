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
Reason: the post-G25 tree contains runtime-completeness alpha work beyond a small v0.1.x stabilization patch, while observer/exporter APIs, coverage-guided fuzzing, and beta readiness remain explicit future work. Persistent worker-pool v1 and fixed-rate wall-clock cadence v1 are now part of the plan2 architecture-stabilization line and remain experimental.
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

Golden output surfaces protected after G26:

- `tests/golden/plan_composite_loop.json` — graph plan JSON.
- `tests/golden/metrics_minimal.json` — metrics JSON.
- `tests/golden/trace_minimal.json` — structured trace JSON.
- `tests/golden/trace_minimal_chrome.json` — Chrome trace shape with volatile timing and trace ids normalized.
- `tests/golden/render_minimal.mmd` — Mermaid render output.
- `tests/golden/schema_dump.json` — schema dump JSON.
- `tests/golden/doctor.json` — doctor JSON.

Current branch limitations after the plan2 G37 health-event pass:

- `thread_pool` lanes use persistent worker-pool v1 with bounded runtime-priority admission, cooperative cancellation/timeout-budget observation, queue/rejection/priority metrics, and worker-id trace attributes. CPU affinity, RT policy, portable hard thread-name guarantees, advanced starvation aging, and hard timeout preemption are not implemented.
- `fixed_rate` lane behavior remains deterministic/simulated by default; opt-in wall-clock cadence v1 exists, but independent lane threads, OS jitter control, and hard real-time scheduling are not implemented.
- Async `policy.max_inflight` controls async edge admission; it is separate from optional task executors.
- `TaskExecutor` remains deterministic by default with cooperative pending-task cancellation and post-return task-budget metrics; `ThreadedTaskExecutor` is now an opt-in bounded preview, not a default scheduler lane.
- Metrics/trace/diagnostics exist, including invocation correlation/causation metadata and bounded observer-only health events, but stable observer/exporter APIs and metric/trace/health-event v2 contracts are not frozen yet.
- Deterministic fuzz smoke exists; coverage-guided fuzzing remains future work.
- ThreadSanitizer remains non-blocking.
- ROS 2, OpenTelemetry, Prometheus, Python, C API, dynamic plugin loading, and external Perfetto adapters remain deferred and must not be claimed as implemented.
- Package-manager recipes under `packaging/` are drafts, not published ecosystem packages.
