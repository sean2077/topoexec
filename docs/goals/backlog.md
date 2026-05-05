# Goal Backlog

This backlog is derived from `docs/plan3.md`. It is ordered for agents that are asked to implement the plan without a narrower goal.

## P0 Infrastructure

| ID | Priority | Status | Scope | Acceptance | Validation |
| --- | --- | --- | --- | --- | --- |
| A1 | P0 | complete | `docs/current-baseline.md`, `README.md`, `CHANGELOG.md`, `docs/release-checklist.md` | Baseline records current commit, local and CI evidence, CTest count, limitations, and `v0.1.0-alpha` tag relationship. | `./scripts/agent_check.sh` |
| A2 | P0 | complete | `AGENTS.md`, `docs/goals/README.md`, `docs/goals/backlog.md`, `docs/goals/status.md` | Goal format includes scope, allowed files, acceptance, validation, and blocker protocol; adapters and new major CLI commands are gated until core goals finish. | `./scripts/agent_check.sh` |
| A3 | P0 | complete | `.github/workflows/ci.yml`, `CMakeLists.txt`, `scripts/`, `README.md`, `docs/release-checklist.md` | CI covers GCC/Clang Debug/RelWithDebInfo, sanitizer status is explicit, package smoke proves downstream use, and local quality commands are documented. | `./scripts/agent_check.sh`; optional `cmake --build build --target topoexec_format_check` |

## P0 Core Goals

| ID | Priority | Status | Scope | Acceptance | Validation |
| --- | --- | --- | --- | --- | --- |
| 001 | P0 | complete | `docs/current-baseline.md`, `README.md`, `CHANGELOG.md`, `docs/release-checklist.md` | Docs agree on current status of thread_pool, async max_inflight, sanitizer CI, adapters; current commit and CTest count recorded; no runtime code changes. | `./scripts/agent_check.sh` |
| 002 | P0 | complete | `docs/public-api.md`, public headers under `include/topoexec/runtime`, examples if needed | Stable, experimental, and internal APIs categorized; examples include only stable headers unless marked advanced; no behavior change. | `./scripts/agent_check.sh` |
| 003 | P0 | complete | `tests/test_runtime.cpp`, runtime files only if tests expose bugs | Immediate, delay, state, and async visibility are tested across epochs; metrics checked for staged/committed counts; no new CLI feature. | `./scripts/agent_check.sh` |
| 004 | P0 | complete | `tests/test_runtime.cpp`, `include/topoexec/runtime/component.hpp`, `src/runtime_runner.cpp` | Configure, activate, execute, and deactivate failures covered; runner errors include phase and component id; cleanup/deactivate order deterministic. | `./scripts/agent_check.sh` |
| 005 | P0 | complete | `tests/test_graph.cpp`, `src/graph.cpp` only if needed | Fixed-seed random DAG/cycle tests; exact SCC CompositeLoop rules tested; overlapping cycles handled. | `./scripts/agent_check.sh` |
| 006 | P0 | complete | `docs/scheduler.md`, `docs/concurrency.md`, `tests/test_runtime.cpp` | Event-loop, fixed-rate, and thread-pool lanes defined precisely; tests prove non-reentrant serialization and reentrant overlap; README limitations updated if needed. | `./scripts/agent_check.sh` |
| 007 | P0 | complete | async edge code, tests, and docs | Tests cover max_inflight accept/reject/drop policies; metrics match docs; shutdown behavior documented. | `./scripts/agent_check.sh` |
| 008 | P0 | complete | `docs/payloads.md`, `include/topoexec/runtime/payload.hpp`, runtime or payload tests | Typed access tests cover wrong type and missing port; copy/shared/loaned/move-only semantics documented and tested; large copy rejection visible. | `./scripts/agent_check.sh` |

## P1 Goals

| ID | Priority | Status | Scope | Acceptance | Validation |
| --- | --- | --- | --- | --- | --- |
| 009 | P1 | not-started | scheduler docs/runtime/tests | Persistent worker pool is designed with explicit deferral or implemented with leak/teardown tests; README does not overclaim. | `./scripts/agent_check.sh` |
| 010 | P1 | not-started | fixed-rate scheduler docs/runtime/tests | Fixed-rate simulation is deterministic; overrun metrics tested; wall-clock mode documented if implemented. | `./scripts/agent_check.sh` |
| 011 | P1 | not-started | trigger policy docs/runtime/tests | Time-sync `sync_slop` behavior implemented/tested; timestamp domain rules documented; out-of-window drops counted. | `./scripts/agent_check.sh` |
| 012 | P1 | not-started | trigger policy docs/runtime/tests | Batch size and batch window tested with fake clock; partial flush behavior documented. | `./scripts/agent_check.sh` |
| 013 | P1 | not-started | CompositeLoop docs/runtime/tests | Convergence policy is no longer stringly typed where public; converged/max/budget/error covered; loop trace spans include iteration. | `./scripts/agent_check.sh` |
| 014 | P1 | not-started | metrics docs/tests/CLI JSON tests | Metrics JSON golden-like tests; docs match exact field names; durations tested as present/non-negative. | `./scripts/agent_check.sh` |
| 015 | P1 | not-started | trace docs/tests/examples | Trace demonstrates minimal DAG and thread_pool concurrency; Chrome trace output remains valid; docs show inspection path. | `./scripts/agent_check.sh` |
| 016 | P1 | not-started | benchmark docs/runtime/CLI tests | Benchmarks cover immediate chain, latest vs queue, large payload, and thread_pool cases; machine-readable output stays stable. | `./scripts/agent_check.sh` |
| 017 | P1 | not-started | CMake packaging and CI | Runtime package smoke proves no YAML/CLI dependency; YAML package smoke proves YAML path; install tree inspected in CI. | `./scripts/agent_check.sh` |
| 018 | P1 | not-started | channel/backpressure docs/runtime/tests | Watermark/stale/deadline health events designed or implemented; metrics surfaced; no unbounded health queue. | `./scripts/agent_check.sh` |

## P2/P3 Goals

| ID | Priority | Status | Scope | Acceptance | Validation |
| --- | --- | --- | --- | --- | --- |
| 019 | P2 | not-started | hierarchical graph docs | Hierarchical subgraph design covers runtime model and API impact; no implementation unless impact is clear. | docs review plus `./scripts/agent_check.sh` |
| 020 | P2 | not-started | buffer/payload docs/runtime/tests | In-process BufferPool or LoanedFrame prototype; no-copy identity tests; no external SHM dependency. | `./scripts/agent_check.sh` |
| 021 | P2 | not-started | explain docs/runtime/CLI tests | `explain` can show why something ran, dropped, or stopped, derived from runtime metrics/trace. | `./scripts/agent_check.sh` |
| 022 | P2 | not-started | lint docs/runtime/CLI tests | Error/warning/info severity model; machine-readable lint output; warning rules documented. | `./scripts/agent_check.sh` |
| 023 | P2 | not-started | `docs/adapters.md` | Adapter target conventions and runtime hooks documented; no adapter code required. | docs review plus `./scripts/agent_check.sh` |
| 024 | P3 | not-started | optional ROS 2 adapter docs/spike only after core stability | Design or minimal optional build only after runtime/API stable; no core dependency. | explicit approval plus adapter-specific checks |
| 025 | P3 | not-started | optional Prometheus/OTel docs/spike only after core stability | Optional exporter target; no core dependency; metrics docs remain source of truth. | explicit approval plus adapter-specific checks |
