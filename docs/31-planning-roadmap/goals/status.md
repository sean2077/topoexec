# Goal Status

Last updated: 2026-05-06

This is the current goal ledger. It records live state and high-signal release
facts only; detailed per-goal command transcripts belong in CI artifacts,
release-prep evidence, or git history.

## Current State

| Area | State |
| --- | --- |
| Historical goal sweeps | G0-G25 and G26-G70 are complete. |
| Active implementation goal | None. G71-post-alpha-hardening-docs-pages is complete. |
| Required repository gate | `scripts/agent_check.sh` before declaring repo changes complete. |
| Focused docs gate | `scripts/goal_check.sh docs`. |
| Blockers | None active. |

## Active Goal: G71-post-alpha-hardening-docs-pages

| Field | Value |
| --- | --- |
| Priority | P0/P1 mixed |
| Status | complete |
| Scope | Runtime semantics, tests, docs, optional docs-only Doxygen support, GitHub Pages docs workflow, benchmark/release evidence, and goal ledgers. |
| Acceptance | Previous-tick wake/update behavior, `overflow: block` alpha semantics, bounded trigger pending behavior, condition timestamp handling, async `max_inflight`, and CompositeLoop output visibility are tested and documented; benchmark evidence exists; optional Doxygen and Pages docs builds exist; release/readme/changelog/goal docs are aligned. |
| Validation | Baseline and final `cmake`/`ctest`/format/agent gates, sanitizer/stress/fuzz/bench smokes where locally supported, plus docs-site and Doxygen targets after those surfaces are added. |
| Blocker protocol | No product/API blockers were opened. GitHub Pages repository settings remain an external owner action before a public URL can be advertised. |

### G71 M0 baseline evidence

Baseline captured on 2026-05-06 from the current working tree before runtime
behavior edits. The only tracked change present at baseline was the requested
post-alpha plan file `docs/32-plans/topoexec_post_alpha_iteration_plan.md`.
Detailed logs are intentionally kept outside this ledger in the local OMX
evidence artifact `.omx/ultragoal/evidence/g71-m0-baseline.log`.

| Command | Result |
| --- | --- |
| `git status --short` | pass; reported the requested added plan file. |
| `cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo` | pass |
| `cmake --build build -j` | pass |
| `ctest --test-dir build --output-on-failure` | pass; 80/80 tests passed. |
| `cmake --build build --target topoexec_format_check` | pass |
| `./scripts/agent_check.sh` | pass; includes configure, format, tidy, build, and 80/80 tests. |
| `./scripts/sanitizer_check.sh` | pass; ASAN/UBSAN build and 80/80 tests passed. |
| `./scripts/stress_smoke.sh` | pass |
| `./scripts/fuzz_smoke.sh` | pass |
| `./scripts/bench_baseline.sh` | pass; wrote local, machine-specific `benchmarks/local-baseline.json`. |

### G71 M1 semantic hardening evidence

| Area | Evidence |
| --- | --- |
| Previous-tick update/wake invariant | `Channel.PreviousTickNotifiesWaitersExactlyOnceWhenPendingValueBecomesVisible` documents and verifies that staging does not advance `update_sequence`, while epoch visibility does exactly once and wakes waiters. |
| `overflow: block` alpha semantics | Existing `Channel.QueueBlockReturnsWouldBlockWithoutDroppingExistingPayload` remains the contract: non-blocking would-block rejection, no existing payload drop. |
| Bounded trigger pending | `Runtime.RateLimitTriggerBoundsSuppressedPendingMessages` covers derived pending bounds and `runtime.trigger.pending_drop_count`. |
| Condition timestamp head-of-line handling | `Runtime.ConditionTimestampTriggerDropsMissingTimestampHeadItem` verifies missing-timestamp heads are dropped and later timestamped messages are not permanently blocked. |
| Async `max_inflight` accounting | Async max-inflight tests now assert in-flight counts stay within policy; `Runtime.CompositeLoopDiscardedAsyncOutputsReleaseInflightAccounting` verifies discarded staged async loop outputs release accounting. |
| CompositeLoop output visibility | `Runtime.CompositeLoopSolverIterationCanCommitPartialOutputsWhenPolicyAllows` and existing discard/fail/budget/cancel tests cover commit/discard behavior. |
| Validation | `cmake --build build -j`; `ctest --test-dir build --output-on-failure`; `cmake --build build --target topoexec_format_check`; `./scripts/goal_check.sh docs`; `./scripts/goal_check.sh golden`; `git diff --check` passed after M1 changes. |

### G71 M2 performance baseline and hot-path evidence

| Area | Evidence |
| --- | --- |
| Benchmark matrix | Added `benchmarks/channel_modes.yaml` for latest/queue/latched/previous-tick/barrier modes and `benchmarks/trigger_policies.yaml` for any/all/time-sync/batch/watermark/condition/debounce/rate-limit paths. |
| Benchmark contract | Updated `scripts/bench_baseline.py`, `tests/bench/check_bench_contract.py`, `benchmarks/README.md`, `docs/24-testing/performance-baselines.md`, and doctor golden discovery to include the new cases. |
| Hot-path indexing | `EventRuntime` now builds run-scoped component and region indexes; `RuntimeRunner` now builds run-scoped instance/spec indexes before lifecycle restore/reset and runtime handoff. Deterministic output order is preserved by using ordered maps and compiled order. |
| Message-copy reduction | Trigger ready collection uses move-from-queue paths for any/coalesced/batch collection while keeping non-consuming readiness probes copy-safe. |
| Validation | `./scripts/goal_check.sh bench` passed and wrote `/tmp/topoexec-bench-baseline.json` without timing thresholds. |

### G71 M3 documentation/release/CI alignment evidence

| Area | Evidence |
| --- | --- |
| Runtime docs | Channel, trigger, async/concurrency, CompositeLoop, runtime semantics, metrics, and runtime-invariant docs now match previous-tick, alpha `overflow: block`, bounded trigger pending, condition timestamp, async, and loop-output behavior. |
| Release docs | Updated `CHANGELOG.md`, `docs/43-ci-build-release-tools/current-baseline.md`, `release-checklist.md`, `release-runbook.md`, and `build-and-package.md` for G71 semantics, benchmark, Doxygen, and Pages surfaces. |
| README/API docs | Root `README.md`, `docs/README.md`, `docs/61-api/api-overview.md`, and `docs/61-api/public-api.md` link or describe optional generated API/site docs without advertising a deployed Pages URL. |
| Preview CI | `.github/workflows/ci.yml` adds a preview-option smoke matrix for adapters, FFI, Python preview, and plugin-loader surfaces while keeping them default-off runtime dependencies. |
| Validation | `./scripts/goal_check.sh docs`, `./scripts/goal_check.sh golden`, and `git diff --check` passed after doc/golden updates. Preview focused checks `adapters`, `ffi`, `python`, and `plugins` passed locally. |

### G71 M4 Doxygen API reference evidence

| Area | Evidence |
| --- | --- |
| Optional build surface | Added default-off `TOPOEXEC_BUILD_DOCS` and `topoexec_doxygen` target only when Doxygen is found. Normal runtime configure/build paths do not require Doxygen. |
| Inputs/output | `docs/Doxyfile.in` generates HTML from public headers under `include/topoexec/{runtime,common,adapters,c_api,plugins}` and `docs/61-api/doxygen.md` into `build-docs/docs/doxygen/html/`. |
| Stability disclaimers | `docs/61-api/doxygen.md` states generated docs are a lookup aid, Markdown docs remain semantic source of truth, and preview APIs remain preview. |
| Header comments | Touched public channel/scheduler/trigger headers have minimal `@brief` / `@ingroup` comments for the changed low-level APIs and metrics. |
| Validation | `cmake -S . -B build-docs -DCMAKE_BUILD_TYPE=Release -DTOPOEXEC_BUILD_DOCS=ON` and `cmake --build build-docs --target topoexec_doxygen` passed locally. |

### G71 M5 GitHub Pages docs-site evidence

| Area | Evidence |
| --- | --- |
| Static site config | Added `mkdocs.yml` using the built-in ReadTheDocs theme in strict mode over the existing `docs/` tree. |
| Local script | Added `scripts/docs_build_site.sh` to configure/build Doxygen, build MkDocs, and copy generated API HTML into `site/api/`. |
| Pages workflow | Added `.github/workflows/pages.yml` with GitHub Pages Actions upload/deploy, docs-only Doxygen/MkDocs dependencies, and no public URL claim before deployment succeeds. |
| Docs | Added `docs/45-doc-standards/github-pages.md` with local/CI build, repository setting, validation, and non-replacement boundaries. |
| Validation | `PYTHON=.cache/mkdocs-venv/bin/python ./scripts/docs_build_site.sh` passed locally and wrote ignored `site/`; workflow users should set Pages source to GitHub Actions before advertising a URL. |

### G71 M6 final validation evidence

Final validation on 2026-05-06 completed after G71 code, docs, benchmark,
Doxygen, Pages, and release-ledger changes.

| Command | Result |
| --- | --- |
| `./scripts/agent_check.sh` | pass; configure, format, tidy, build, and 80/80 CTest passed. |
| `ctest --test-dir build --output-on-failure` | pass; 80/80 tests passed during the final build/test sweep. |
| `cmake --build build --target topoexec_format_check` | pass through `agent_check.sh`. |
| `./scripts/goal_check.sh docs` | pass; `docs_command_smoke` passed. |
| `cmake --build build-docs --target topoexec_doxygen` | pass; generated Doxygen HTML under `build-docs/docs/doxygen/html/`. |
| `PYTHON=.cache/mkdocs-venv/bin/python ./scripts/docs_build_site.sh` | pass; built the MkDocs site and copied Doxygen to ignored `site/api/`. |
| `./scripts/sanitizer_check.sh` | pass; ASAN+UBSAN build and 80/80 CTest passed. |
| `./scripts/stress_smoke.sh` | pass; smoke profile completed generated scheduler/channel workloads. |
| `./scripts/fuzz_smoke.sh` | pass; standalone fuzz target corpus smoke passed. |
| `./scripts/bench_baseline.sh` | pass; wrote ignored local `benchmarks/local-baseline.json` without timing thresholds. |
| `./scripts/goal_check.sh adapters && ./scripts/goal_check.sh ffi && ./scripts/goal_check.sh python && ./scripts/goal_check.sh plugins` | pass; preview option builds/tests passed locally. |
| `git diff --check` | pass. |

## Canonical Evidence Surfaces

| Need | Canonical doc or check |
| --- | --- |
| Architecture and runtime behavior | `docs/21-architecture/runtime-architecture.md`, `docs/21-architecture/runtime-semantics.md`, `docs/21-architecture/runtime-invariants.md`, `docs/21-architecture/semantic-contract.md` |
| Codebase ownership | `docs/22-codebase/codebase-map.md` |
| Public API stability | `docs/61-api/public-api.md`, `docs/61-api/api-change-checklist.md`, `docs/61-api/c-api.md` |
| Schema and wire contracts | `docs/33-specs-rfcs/schema-v1.md`, `docs/33-specs-rfcs/schema-v2-notes.md`, `docs/62-schemas-protocols/metrics.md`, `docs/62-schemas-protocols/trace-events.md`, `docs/62-schemas-protocols/diagnostics.md` |
| Testing and release gates | `docs/24-testing/testing-strategy.md`, `docs/43-ci-build-release-tools/current-baseline.md`, `docs/43-ci-build-release-tools/release-checklist.md`, `docs/43-ci-build-release-tools/release-runbook.md` |
| Beta readiness and deferrals | `docs/43-ci-build-release-tools/beta-readiness-review.md`, `docs/43-ci-build-release-tools/release-progression.md` |
| Current backlog | `docs/31-planning-roadmap/goals/backlog.md` |
| Deleted planning/process artifacts | `docs/94-doc-migrations/2026-05-process-ledger-cleanup.md` |

## Completed Rollup

| Range | Outcome | Replacement evidence |
| --- | --- | --- |
| G0-G25 | Completed the earlier runtime/API stabilization board. | Current runtime/API docs, release docs, tests, and git history. |
| G26-G34 | Completed public API, semantic contract, scheduler, worker-pool, fixed-rate, priority, cancellation, and task-executor stabilization. | API, semantic, scheduler, concurrency, async, metrics, trace, and runtime docs plus tests. |
| G35-G45 | Completed trigger, metadata, channel, payload, graph compiler, hierarchy, templates, lifecycle, config, and CompositeLoop work. | User guides, architecture docs, schema docs, runtime tests, graph tests, and goldens. |
| G46-G54 | Completed observer, metric, trace, diagnostic, defensive-input, fuzz, stress, benchmark, and packaging surfaces. | Schema/protocol docs, testing docs, package docs, release docs, and focused goal checks. |
| G55-G70 | Completed docs system, examples, adapter/FFI/Python/plugin previews, schema-v2 boundary, editor/schema UX, architecture policy, release automation, community readiness, robot-cell pilot, and beta-readiness review. | Docs tree, examples, integration docs, release docs, policy checks, and beta review. |

## Deferred Scope

The following remain deferred unless the user opens a goal with explicit scope:

- schema v2 implementation and migration tooling;
- full editor extension or LSP server;
- production OpenTelemetry/Prometheus exporters;
- real ROS 2 package/client-library adapter;
- stable C ABI beyond ABI version 0;
- native Python bindings;
- sandboxed/stable plugin ecosystem and graph-driven plugin discovery;
- package-registry publication;
- hard real-time scheduling, CPU affinity guarantees, or hard preemption.

## Update Rule

When a future goal starts, completes, blocks, or is intentionally deferred,
update this page and `backlog.md` with the new state. Do not reintroduce large
generated task boards or paste full command transcripts into this ledger.
