# Goal Status

Last updated: 2026-05-07

This is the current goal ledger. It records live state and high-signal release
facts only; detailed per-goal command transcripts belong in CI artifacts,
release-prep evidence, or git history.

## Current State

| Area | State |
| --- | --- |
| Historical goal sweeps | G0-G25 and G26-G70 are complete. |
| Active implementation goal | None. Next-stage ultragoal G75-G85 and G99 final validation are complete. |
| Required repository gate | `scripts/agent_check.sh` before declaring repo changes complete. |
| Focused docs gate | `scripts/goal_check.sh docs`. |
| Focused live gates | `scripts/goal_check.sh live` and `scripts/goal_check.sh live-perf`. |
| Focused G74 gates | `scripts/goal_check.sh examples` and `scripts/goal_check.sh showcase`. |
| Blockers | G81/G82x/G83 ecosystem, integration, package-publication, schema-v2, and v1.0 readiness deferrals remain active until adoption signals and human owner decisions resolve them. |

## Completed Goal: G75-release-adoption-readiness

| Field | Value |
| --- | --- |
| Priority | P0 |
| Status | complete |
| Scope | Safe local release/adoption readiness only: public metadata consistency, Apache-2.0 license wording, `0.2.0` package/CMake metadata for the `v0.2.0-alpha.0` candidate line, release/adoption documentation, tracked prerelease notes draft, install/downstream onboarding guidance, CHANGELOG, and goal ledgers. |
| Allowed files | `README.md`, `CMakeLists.txt`, package draft files, package/golden tests, `CHANGELOG.md`, `docs/01-quickstart/`, `docs/43-ci-build-release-tools/`, `docs/README.md`, and goal ledgers. |
| Acceptance | README and package drafts match Apache-2.0; CMake/package draft version metadata aligns to `0.2.0`; docs explain clone/build/install/find_package/run-example/report-issue path; release notes for `v0.2.0-alpha.0` are ready for human release-owner review; no tag, publication, production-readiness, or public Pages URL claim is introduced. |
| Validation | `./scripts/goal_check.sh docs`, `./scripts/goal_check.sh golden`, `./scripts/goal_check.sh package`, `./scripts/goal_check.sh release`, and `git diff --check` passed on 2026-05-07. |
| Blocker protocol | No active code blocker. Human release owner must still choose the exact candidate commit, run full release gates, verify GitHub Pages deployment before advertising a public URL, and create/push any tag manually. |

## Completed Goal: G76-production-like-dogfood-pilot

| Field | Value |
| --- | --- |
| Priority | P0/P1 mixed |
| Status | complete |
| Scope | Synthetic production-like dogfood only: curated YAML example, live assertion file, generated example assets, benchmark graph, focused dogfood gate, README/examples index updates, case-study docs, CHANGELOG, and goal ledgers. |
| Allowed files | `examples/90-dogfood-pilot/`, `docs/assets/generated/examples/dogfood-robot-cell/`, `benchmarks/dogfood_robot_cell.yaml`, `scripts/dogfood_pilot_smoke.sh`, `scripts/goal_check.sh`, `tests/bench/check_bench_contract.py`, README/examples/docs pages, CHANGELOG, and goal ledgers. |
| Acceptance | The pilot validates, runs for a bounded long smoke, emits metrics/trace/Chrome trace/live observe, passes live assertions, replays recorded artifacts, and participates in benchmark contract smoke. Expected async/channel drops are visible and explained; runtime errors, assertion failures, and observer drops are not expected. The pilot remains synthetic and dependency-free. |
| Validation | `./scripts/goal_check.sh dogfood` passed with `channel_drop_count=89`, `trace_event_count=1068`, `observe_records=834`, runtime ok, assertions ok, and zero observer drops. `./scripts/goal_check.sh examples`, `showcase`, `bench`, and `git diff --check` also passed on 2026-05-07. |
| Blocker protocol | No active blocker. Hardware, real ROS 2, external ML runtime, production telemetry, remote dashboard/control, and hard-real-time claims remain deferred unless a future goal opens them with a product/API decision. |

## Completed Goal: G77-compatibility-harness

| Field | Value |
| --- | --- |
| Priority | P1 |
| Status | complete |
| Scope | Stable-v0.2 compatibility harnesses only: installed-header API marker/inventory checks, public API/versioning/deprecation docs, CLI JSON field checks for doctor/schema/metrics/trace/live observe, golden-file presence, schema/live/package smoke composition, quality-gate docs, CHANGELOG, and goal ledgers. |
| Allowed files | `tests/compat/check_compatibility_contract.py`, `scripts/goal_check.sh`, `docs/61-api/public-api.md`, `docs/41-development-tools/quality-gates.md`, `CHANGELOG.md`, and goal ledgers. |
| Acceptance | The focused `compat` gate proves stable-v0.2 surfaces without freezing preview/deferred APIs: installed headers stay inventoried, JSON contract fields stay present, schema v1/goldens/live observe/runtime-only package smokes compose successfully, and API change rules remain visible. |
| Validation | `./scripts/goal_check.sh compat` passed on 2026-05-07, including `cli_golden_outputs`, `schema_v1_contract_smoke`, `cmake_package_runtime_smoke`, and `cli_observe_ndjson_minimal`; `git diff --check` passed. |
| Blocker protocol | No active blocker. If a compatibility change would remove/rename stable-v0.2 API, schema, metric, trace, diagnostic, or CLI JSON fields, use the API change checklist and open a blocker/migration note before implementation. |

## Completed Goal: G78-distribution-packaging-hardening

| Field | Value |
| --- | --- |
| Priority | P1 |
| Status | complete |
| Scope | Local distribution and package hardening: package matrix docs, package matrix alignment check, package draft version/license docs, package/release gate wiring, build/package docs, docs map, CHANGELOG, and goal ledgers. |
| Allowed files | `docs/43-ci-build-release-tools/package-matrix.md`, `docs/43-ci-build-release-tools/build-and-package.md`, `docs/README.md`, `packaging/*/README.md`, `tests/package/check_package_matrix.py`, `scripts/goal_check.sh`, CHANGELOG, and goal ledgers. |
| Acceptance | Runtime-only, default, YAML/CLI, CPack/source archive, release-candidate, vcpkg draft, and Conan draft package forms are documented; package metadata is aligned to `0.2.0`/Apache-2.0; CMake options and package config variables are checked; registry publication remains deferred. |
| Validation | `./scripts/goal_check.sh package` passed including package matrix check, runtime-only option smoke, downstream install smoke, CPack smoke, and package draft smoke. `./scripts/goal_check.sh release` and `git diff --check` also passed on 2026-05-07. |
| Blocker protocol | No active blocker. Publishing to vcpkg/Conan/PyPI/system package registries requires a future human release-owner decision with exact artifacts, checksums, credentials, and clean-machine evidence. |

## Completed Goal: G79-reliability-soak-perf-regression

| Field | Value |
| --- | --- |
| Priority | P1 |
| Status | complete |
| Scope | Bounded reliability program only: test-tier docs, soak-lite script, reliability policy check/gate, fuzz corpus/perf/sanitizer/failure artifact conventions, testing docs map, CHANGELOG, and goal ledgers. |
| Allowed files | `docs/24-testing/reliability-program.md`, `docs/24-testing/testing-strategy.md`, `scripts/soak_lite_smoke.sh`, `tests/reliability/check_reliability_program.py`, `scripts/goal_check.sh`, `docs/README.md`, CHANGELOG, and goal ledgers. |
| Acceptance | Reliability tiers are explicit; soak-lite is bounded by duration, iteration, scale, steps, and timeout; benchmark thresholds remain opt-in per-machine; fuzz corpus and ASAN+UBSAN/TSAN policy are documented; failure artifacts use `/tmp/topoexec-<gate>-*`; no unbounded required gate is added. |
| Validation | `./scripts/goal_check.sh reliability` passed, including policy check, stress/fuzz/bench CTest smokes, and soak-lite profile with 3 iterations. Focused `stress`, `fuzz`, `bench`, and `git diff --check` also passed on 2026-05-07. |
| Blocker protocol | No active blocker. Longer soak, global perf thresholds, long fuzz campaigns, or TSAN-blocking promotion require explicit owner/release governance and environment-specific evidence. |

## Completed Goal: G80-external-feedback-adoption-loop

| Field | Value |
| --- | --- |
| Priority | P1/P2 mixed |
| Status | complete |
| Scope | Docs/templates/checks only: first-user path, debug-pack guidance, label taxonomy, triage states, issue-template prompts, adoption focused gate, docs map, CHANGELOG, and goal ledgers. |
| Allowed files | `.github/ISSUE_TEMPLATE/*.md`, `docs/44-coding-standards/feedback-and-triage.md`, `tests/docs/check_adoption_feedback.py`, `scripts/goal_check.sh`, `docs/README.md`, CHANGELOG, and goal ledgers. |
| Acceptance | Maintainers can triage early adopter reports using a documented first-user path, debug pack, labels, and states; templates ask for commit/tag, environment, commands, minimal graph/snippet, JSON/stdout/stderr, and focused gates; no production deployment, registry publication, or support SLA claim is added. |
| Validation | `./scripts/goal_check.sh adoption` passed, including adoption feedback check, `community_readiness_smoke`, `docs_command_smoke`, and `cmake_package_runtime_smoke`. `./scripts/goal_check.sh docs` and `git diff --check` also passed on 2026-05-07. |
| Blocker protocol | No active blocker. Real GitHub label creation, external user follow-up, support commitments, or release publication remain human maintainer/release-owner actions. |

## Completed Goal: G81-ecosystem-decision-gate

| Field | Value |
| --- | --- |
| Priority | P2 decision |
| Status | complete |
| Scope | Decision artifact only: compare G82a-e options, recommend a future track, record blocker/deferral, add focused check/gate, update docs map, CHANGELOG, and goal ledgers. |
| Allowed files | `docs/31-planning-roadmap/ecosystem-decision-gate.md`, `docs/31-planning-roadmap/goals/blockers/g81-ecosystem-track-selection.md`, `tests/docs/check_ecosystem_gate.py`, `scripts/goal_check.sh`, `docs/README.md`, CHANGELOG, and goal ledgers. |
| Acceptance | G82a-e are compared; all implementation remains deferred; recommendation is to prefer G82e package registry publication only after a human release owner and adoption signal exist; policy checks prove no production adapter/binding/editor/registry scope was added. |
| Validation | `./scripts/goal_check.sh ecosystem`, `./scripts/goal_check.sh policy`, `./scripts/goal_check.sh docs`, and `git diff --check` passed on 2026-05-07. |
| Blocker protocol | Active blocker: `docs/31-planning-roadmap/goals/blockers/g81-ecosystem-track-selection.md`. Do not implement G82a-e until a human owner resolves that blocker with adoption evidence and exact scope. |

## Completed Goal: G82x-g83-conditional-tracks-ledger

| Field | Value |
| --- | --- |
| Priority | P2/P3 conditional |
| Status | complete |
| Scope | Conditional documentation/ledger only: per-track blockers for G82a-e and G83, conditional tracks ledger, focused check/gate, docs map, CHANGELOG, and goal ledgers. |
| Allowed files | `docs/31-planning-roadmap/conditional-tracks-ledger.md`, `docs/31-planning-roadmap/goals/blockers/g82*.md`, `docs/31-planning-roadmap/goals/blockers/g83-schema-v2-migration.md`, `tests/docs/check_conditional_tracks.py`, `scripts/goal_check.sh`, `docs/README.md`, CHANGELOG, and goal ledgers. |
| Acceptance | Production telemetry exporter, native Python binding, real ROS 2 adapter, editor/LSP, package registry publication, and schema v2/migration implementation each have explicit blocked/deferred entry criteria and safe independent work; no implementation/dependency scope is opened. |
| Validation | `./scripts/goal_check.sh conditional`, `./scripts/goal_check.sh policy`, `./scripts/goal_check.sh schema`, and `git diff --check` passed on 2026-05-07. |
| Blocker protocol | Active blockers: `g82a-production-observability-exporter.md`, `g82b-native-python-binding.md`, `g82c-real-ros2-adapter.md`, `g82d-editor-lsp.md`, `g82e-package-registry-publication.md`, and `g83-schema-v2-migration.md`. Resolve the relevant blocker before opening implementation. |

## Completed Goal: G84-core-runtime-beta-candidate-readiness

| Field | Value |
| --- | --- |
| Priority | P1 release decision |
| Status | complete |
| Scope | Readiness review only: G75-G80 evidence synthesis, core-runtime beta candidate boundaries, beta gate checklist, beta-release notes boundaries, explicit non-beta-ready deferrals, docs map wiring, focused beta check/gate, CHANGELOG, and goal ledgers. |
| Allowed files | `docs/43-ci-build-release-tools/core-runtime-beta-candidate-g84.md`, `docs/43-ci-build-release-tools/beta-readiness-review.md`, `tests/release/check_beta_candidate_readiness.py`, `scripts/goal_check.sh`, `docs/README.md`, CHANGELOG, and goal ledgers. |
| Acceptance | TopoExec has a local core-runtime beta candidate review path that summarizes G75-G80 evidence while making clear that no tag, upload, production readiness, adapter/ecosystem beta readiness, package registry publication, or v1.0 stable-compatibility claim is made without a human release owner. |
| Validation | `./scripts/goal_check.sh beta` passed, including beta-readiness policy checks plus `release_prepare_smoke`, `docs_command_smoke`, and `cli_golden_outputs`. `./scripts/goal_check.sh release`, `./scripts/goal_check.sh compat`, and `git diff --check` also passed on 2026-05-07. |
| Blocker protocol | No new implementation blocker. A human release owner must choose the exact candidate commit, run the full beta-candidate gate, accept remaining deferrals, and perform any beta tag or publication manually. |

## Completed Goal: G85-v1-readiness-deferral

| Field | Value |
| --- | --- |
| Priority | P1 release decision |
| Status | complete |
| Scope | Criteria and deferral documentation only: v1 readiness program, post-beta adoption criteria, stable-surface expectations, scheduler/runtime limitation policy, package/adoption maturity criteria, owner decision path, focused v1 gate, release-doc wiring, CHANGELOG, and goal ledgers. |
| Allowed files | `docs/43-ci-build-release-tools/v1-readiness-program.md`, `docs/43-ci-build-release-tools/release-progression.md`, `docs/43-ci-build-release-tools/release-checklist.md`, `tests/release/check_v1_readiness_deferral.py`, `scripts/goal_check.sh`, `docs/README.md`, CHANGELOG, and goal ledgers. |
| Acceptance | TopoExec explicitly remains not v1.0 ready; future v1.0 requires post-beta adoption evidence, stable API/schema/CLI/metric/trace/diagnostic/package surfaces, scheduler/runtime limitation classification, package/adoption maturity, and a human release-owner decision. No tag, publication, stable-v1 language, or implementation scope is introduced. |
| Validation | `./scripts/goal_check.sh v1`, `./scripts/goal_check.sh docs`, and `git diff --check` passed on 2026-05-07. |
| Blocker protocol | Active deferral, not an implementation blocker: v1.0 remains blocked on post-beta adoption evidence, G81/G82/G83 decisions, package-publication decision or source/archive-only statement, and owner acceptance of stable surfaces plus runtime limitations. |

## Completed Goal: G99-final-validation-ultragoal-evidence

| Field | Value |
| --- | --- |
| Priority | P0 validation |
| Status | complete |
| Scope | Final validation and durable evidence for the next-stage ultragoal: required full repository gate, focused goal gates, sanitizer/stress/fuzz/bench/live coverage, diff whitespace check, ultragoal status reconciliation, CHANGELOG, and goal ledgers. |
| Allowed files | `.omx/ultragoal/`, `CHANGELOG.md`, `docs/31-planning-roadmap/goals/backlog.md`, and `docs/31-planning-roadmap/goals/status.md`. |
| Acceptance | All next-stage executable goals are checkpointed complete; no pending implementation goal remains; required repository gate and focused checks pass on the final tree; validation gaps are recorded instead of hidden. |
| Validation | `.omx/ultragoal/evidence/g99-final-validation.log` records passing `./scripts/agent_check.sh`, focused `docs`, `golden`, `package`, `release`, `compat`, `dogfood`, `reliability`, `adoption`, `ecosystem`, `conditional`, `beta`, `v1`, `examples`, `showcase`, `live`, `bench`, `stress`, `fuzz`, sanitizer, `git diff --check`, and `omx ultragoal status` checks on 2026-05-07. A final post-ledger `./scripts/agent_check.sh`, `./scripts/goal_check.sh docs`, and `git diff --check` also passed. |
| Blocker protocol | No G99 blocker. Remaining blockers are intentional product/release-owner decisions for G81/G82/G83, package publication, schema v2, and v1.0 readiness. |

## Completed Goal: G74-examples-showcase-and-readme-refresh

| Field | Value |
| --- | --- |
| Priority | P0/P1 mixed |
| Status | complete |
| Scope | Examples hierarchy, executable YAML examples, example metadata, generated README/example visual assets, example index, README refresh, showcase/docs tooling, focused examples/showcase gates, docs/CHANGELOG/goal ledgers. Runtime semantics should remain unchanged unless a change has clear developer/documentation value and its boundary is documented. |
| Allowed files | `README.md`, `examples/`, `docs/assets/`, `scripts/render_example_assets.py`, `scripts/update_examples_index.py`, `scripts/check_readme_assets.sh`, `scripts/goal_check.sh`, `docs/41-development-tools/`, `docs/README.md`, `CHANGELOG.md`, and this goal ledger/backlog. |
| Acceptance | README communicates TopoExec value within one screen; Quick Start uses real build/validate/render/run commands; at least 8 curated examples cover minimal graph, branching, triggers, async, CompositeLoop/loop, observability, validation diagnostics, testing/golden-friendly use, and benchmark/performance; each public example has README/config/commands/expected output/graph asset references; assets are generated or validated from real examples; examples/showcase gates pass; project status remains honest beta/pre-production without production-readiness exaggeration. |
| Validation | Baseline and final `./scripts/agent_check.sh`, focused `docs`, `golden`, new `examples` and `showcase` gates, generated asset/index checks, README/example link or asset checks, and `git diff --check`; additional live/bench gates where touched or reused by showcase assets. |
| Blocker protocol | No product/API blocker is active. If example taxonomy, README positioning, or visualization generation requires a product/API decision, write `docs/31-planning-roadmap/goals/blockers/g74-*.md`, recommend one option, and continue only with safe independent work. |

### G74 M0 baseline evidence

Baseline captured on 2026-05-07 from the current working tree before G74
examples/showcase edits. The only tracked change present at baseline was the
requested added plan file
`docs/32-plans/topoexec_g74_examples_readme_showcase_plan_zh.md`.
Detailed logs are kept in the local OMX evidence artifact
`.omx/ultragoal/evidence/g74-m0-baseline.log`.

| Command or inventory | Result |
| --- | --- |
| `git status --short` | pass; reported the requested added G74 plan file. |
| README/examples inventory | README had 72 lines; examples had 15 top-level YAML files and 12 C++ app example directories; no generated docs/assets files existed. |
| `./scripts/agent_check.sh` | pass; configure, format, tidy, build, and 86/86 CTest passed. |
| `./scripts/goal_check.sh docs` | pass; `docs_command_smoke` passed. |
| `./scripts/goal_check.sh golden` | pass; `cli_golden_outputs` passed. |
| `git diff --check` | pass. |

### G74 M1-M6 implementation evidence

| Area | Evidence |
| --- | --- |
| Examples taxonomy and metadata | Added curated directories `examples/00-getting-started/` through `examples/80-realistic-mini-scenario/`, dependency-free `example.json` metadata, `examples/metadata.schema.json`, and `examples/_templates/example-readme.md`. |
| Curated example matrix | Added 9 documented, smokeable examples covering minimal pipeline, branching/fan-out/join, trigger policies, async bounded inflight, CompositeLoop fixed-point solver, metrics/trace/live observe, validation diagnostics plus golden-friendly output, benchmark, and synthetic robot-cell mini scenario. |
| Generated assets and index | Added `scripts/update_examples_index.py` and `scripts/render_example_assets.py`; generated `docs/assets/generated/examples/*` graph Mermaid/SVG/summary assets plus README hero/showcase assets from real CLI output. |
| README refresh | Rebuilt README around one-sentence positioning, generated hero/showcase visuals, Quick Start, expected output, core capabilities table, examples gallery, docs links, embedding notes, and honest beta/pre-production boundaries. |
| Focused gates | Added `scripts/examples_smoke.py`, `scripts/check_readme_assets.sh`, `./scripts/goal_check.sh examples`, and `./scripts/goal_check.sh showcase` to prevent metadata, assets, README links, and quick-start drift. |
| Validation | `./scripts/goal_check.sh examples`, `./scripts/goal_check.sh showcase`, `./scripts/goal_check.sh docs`, `./scripts/goal_check.sh golden`, `python3 scripts/update_examples_index.py --check`, `python3 scripts/render_example_assets.py --topoexec build/topoexec --check`, and `git diff --check` passed during M1-M6. |


### G74 M7 final validation evidence

Final validation on 2026-05-07 completed after G74 example, asset, README, tooling, docs, and ledger changes. No unsupported local G74 gates remain.

| Command | Result |
| --- | --- |
| `./scripts/agent_check.sh` | pass; configure, format, tidy, build, and 86/86 CTest passed. |
| `./scripts/goal_check.sh docs` | pass; `docs_command_smoke` passed. |
| `./scripts/goal_check.sh golden` | pass; `cli_golden_outputs` passed. |
| `./scripts/goal_check.sh examples` | pass; generated index/assets were current and 31 metadata commands across 9 curated examples behaved as expected. |
| `./scripts/goal_check.sh showcase` | pass; README/generated assets, Markdown links, status phrases, and quick-start validate/render/run smoke passed. |
| `./scripts/goal_check.sh bench` | pass; benchmark output contract and local baseline generation passed. |
| `git diff --check` | pass. |

## Completed Goal: G71-post-alpha-hardening-docs-pages

| Field | Value |
| --- | --- |
| Priority | P0/P1 mixed |
| Status | complete |
| Scope | Runtime semantics, tests, docs, optional docs-only Doxygen support, GitHub Pages docs workflow, benchmark/release evidence, and goal ledgers. |
| Acceptance | Previous-tick wake/update behavior, `overflow: block` alpha semantics, bounded trigger pending behavior, condition timestamp handling, async `max_inflight`, and CompositeLoop output visibility are tested and documented; benchmark evidence exists; optional Doxygen and Pages docs builds exist; release/readme/changelog/goal docs are aligned. |
| Validation | Baseline and final `cmake`/`ctest`/format/agent gates, sanitizer/stress/fuzz/bench smokes where locally supported, plus docs-site and Doxygen targets after those surfaces are added. |
| Blocker protocol | No product/API blockers were opened. GitHub Pages repository settings remain an external owner action before a public URL can be advertised. |

## Completed Goal: G73-low-overhead-live-runtime-validation

| Field | Value |
| --- | --- |
| Priority | P0/P1 mixed |
| Status | complete |
| Scope | Low-overhead live runtime validation workbench: live observe schema/docs, fixed-size bounded runtime event transport, runtime instrumentation, `graph observe`, live assertions, record/replay artifacts, local SSE dashboard, focused live/live-perf gates, and release/roadmap docs. |
| Allowed files | Runtime headers and sources under `include/topoexec/runtime/` and `src/`; CLI sources under `tools/topoexec/`; tests under `tests/`; benchmark examples under `benchmarks/`; local tooling under `tools/topoexec_live_*`; validation scripts under `scripts/`; docs/README/CHANGELOG/goal ledgers. |
| Acceptance | `graph observe` emits `observe_schema_version=1` NDJSON; observe is disabled by default; enabled hot paths are bounded, non-blocking, allocation-light, and free of JSON/file/socket/UI/payload-body work; observer overflow reports drop summaries without changing runtime semantics; live assertions pass/fail/pending outside graph schema v1; record artifacts replay; local dashboard is observe-only; existing metrics/trace/golden/schema/docs contracts remain stable; live and live-perf focused gates exist. |
| Validation | Baseline and final `cmake`/`ctest`/format/agent gates, focused `docs`, `golden`, `bench`, `live`, and `live-perf` gates, plus stress/fuzz/sanitizer where locally supported and `git diff --check`. |
| Blocker protocol | No product/API blocker is active. If low-overhead transport, assertion DSL, or dashboard scope requires a product/API decision, write `docs/31-planning-roadmap/goals/blockers/g73-*.md`, recommend one option, and continue only with safe independent work. |

### G73 M0 baseline evidence

Baseline captured on 2026-05-07 from the current working tree before G73 runtime
behavior edits. The only tracked change present at baseline was the requested
added plan file
`docs/32-plans/topoexec_g73_low_overhead_live_runtime_validation_plan_zh.md`.
Detailed logs are intentionally kept outside this ledger in the local OMX
evidence artifact `.omx/ultragoal/evidence/g73-m0-baseline.log`.

| Command | Result |
| --- | --- |
| `git status --short` | pass; reported the requested added G73 plan file. |
| `cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo` | pass |
| `cmake --build build -j` | pass |
| `ctest --test-dir build --output-on-failure` | pass; 80/80 tests passed. |
| `cmake --build build --target topoexec_format_check` | pass |
| `./scripts/agent_check.sh` | pass; includes configure, format, tidy, build, and 80/80 tests. |
| `./scripts/goal_check.sh docs` | pass; `docs_command_smoke` passed. |
| `./scripts/goal_check.sh golden` | pass; `cli_golden_outputs` passed. |
| `./scripts/goal_check.sh bench` | pass; bench focused smoke passed and wrote `/tmp/topoexec-bench-baseline.json`. |

### G73 M1-M8 implementation evidence

| Area | Evidence |
| --- | --- |
| Live observe schema/docs | Added `docs/62-schemas-protocols/live-observe-events.md`, `docs/41-development-tools/live-runtime-validation.md`, and `docs/24-testing/live-observe-performance.md`; docs map links these pages as schema/tooling/performance references. |
| Low-overhead transport | Added fixed-size `runtime_observe::LiveEvent`, bounded non-blocking `LiveEventRingBuffer`, `LiveEventStream`, and `LiveObserveSession` under `include/topoexec/runtime/` and `src/live_observe.cpp`; `TOPOEXEC_ENABLE_LIVE_OBSERVE=OFF` build smoke passed during M2. |
| Runtime instrumentation | `RuntimeRunnerOptions::live_observe` defaults off; enabled runs emit lifecycle/component/channel/async/loop/health/error/drop-summary events into `RuntimeRunnerResult::live_events`; observer overflow reports drops without changing `RuntimeRunnerResult::ok`, metrics, trace, or health contracts. |
| `graph observe` CLI | Added `topoexec graph observe` with `observe_schema_version=1` NDJSON and `json-summary`, deterministic symbol table/run id, run validation/plan/final-summary records, collector-side include/exclude/sample filters, debug-only payload preview guard, and observer-drop fail option. |
| Live assertions | Added CLI/tooling-layer assertion YAML schema v1 with pass/fail/pending/result events and exit code 3 for failing assertions unless `--no-fail-on-assertion-fail` is set; assertions are not graph schema v1 fields and do not run inside runtime internals. |
| Record/replay artifacts | `--record DIR` writes replayable local artifacts including manifest, graph, normalized graph, plan, Mermaid render, observe NDJSON/summary, assertions/result, metrics, trace, Chrome trace, health, and dashboard HTML; `tools/topoexec_live_server.py replay --smoke` validates artifacts. |
| Local dashboard | Added static dashboard assets and `tools/topoexec_live_server.py` with local `127.0.0.1` SSE `/events`, `/snapshot`, `/bundle`, one-time token URLs, bounded raw events, smoke mode, and no runtime control endpoint. |
| Focused gates/performance | Added `scripts/live_smoke.sh`, `scripts/live_perf_check.py`, `./scripts/goal_check.sh live`, `./scripts/goal_check.sh live-perf`, and live-observe benchmark cases for minimal, high-frequency channels, trigger stress, thread-pool, and CompositeLoop workloads. |

### G73 M8 focused-gate evidence

| Command | Result |
| --- | --- |
| `./scripts/goal_check.sh live` | pass; CTest live smokes plus observe schema, assertion, record, replay, dashboard, filter, and drop-summary smoke passed. |
| `./scripts/goal_check.sh live-perf` | pass; default non-enforcing per-machine smoke reported disabled/summary/detailed/debug medians, runtime_ok=true, and overflow drop visibility. |
| `./scripts/goal_check.sh bench` | pass; benchmark contract and local baseline generation include the new live-observe workloads. |
| `cmake --build build --target topoexec_format_check` | pass. |

### G73 M9 docs and release alignment evidence

Docs, README, CHANGELOG, runtime architecture, runtime invariants, testing strategy,
performance policy, and docs map now describe live observe as an output-only
observability/test-validation tool. Deferred/non-goal scope remains explicit:
no runtime control, pause/resume/step, fault injection, remote multi-user UI,
WebSocket control, payload-body streaming, production telemetry exporter, schema
v2 assertion embedding, native Python binding, or hard real-time guarantee.
Final G73 completion remains gated on the M10 validation suite below.

### G73 M10 final validation evidence

Final validation on 2026-05-07 completed after G73 runtime, CLI, tooling, docs, benchmark, and ledger changes. No unsupported local gates remain.

| Command | Result |
| --- | --- |
| `./scripts/agent_check.sh` | pass; configure, format, tidy, build, and 86/86 CTest passed. |
| `./scripts/goal_check.sh docs` | pass; `docs_command_smoke` passed. |
| `./scripts/goal_check.sh golden` | pass; `cli_golden_outputs` passed. |
| `./scripts/goal_check.sh live` | pass; CTest live smokes plus observe schema, filter, assertion, record, replay, dashboard, and drop-summary smoke passed. |
| `./scripts/goal_check.sh bench` | pass; benchmark contract passed and local baseline generation wrote `/tmp/topoexec-bench-baseline.json`. |
| `./scripts/goal_check.sh live-perf` | pass; default per-machine smoke reported runtime_ok=true, observer overflow visibility, and summary median overhead within the opt-in 2% target on this run. |
| `./scripts/goal_check.sh stress` | pass; `test_stress` and generated stress smoke passed. |
| `./scripts/goal_check.sh fuzz` | pass; parser/compiler fuzz CTest, standalone fuzz smoke, and optional fuzzer target corpus replay passed. |
| `./scripts/goal_check.sh sanitizer` | pass; ASAN+UBSAN build and 86/86 CTest passed. |
| `git diff --check` | pass. |
| `omx ultragoal status` | pass after checkpoint; expected 11/11 complete. |

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
