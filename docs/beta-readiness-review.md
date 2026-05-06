# Beta Readiness Review

Date: 2026-05-06

G70 is a review gate, not a tag operation. The current tree has enough evidence
for an honest **core-runtime beta candidate review**, but a human release owner
must still choose the tag, run release preparation on the exact candidate commit,
and copy the explicit deferrals into release notes. This is not an adapter,
hardware, package-registry, or 1.0 readiness claim.

## Verdict

| Question | Verdict |
| --- | --- |
| Are all plan2 P0/P1 goals complete or explicitly deferred? | Yes. G26-G65, G66-G67, and G69-G70 are complete; remaining G68 is P2 community work and must stay explicit if not done before beta. Schema v2 implementation remains deferred beyond the G64 design note, and a full editor/LSP extension remains beyond the G65 guide. |
| Can TopoExec honestly prepare a core-runtime beta candidate? | Yes, if release notes keep the deferred features and alpha/experimental surfaces explicit and CI/release-prep evidence is attached for the exact candidate commit. |
| Can TopoExec claim adapter/ecosystem beta readiness? | No. G58/G59 add dependency-free telemetry mapping/text previews and G60 adds only a dependency-free ROS 2 fake-boundary preview; concrete ROS 2 client-library packages, production OpenTelemetry/Prometheus, stable C ABI, native Python bindings, sandboxed/stable plugin ecosystems, graph-driven plugin discovery, external Perfetto, and package-registry surfaces remain deferred or draft-only. |
| Can TopoExec claim hard real-time or external scheduling guarantees? | No. Fixed-rate/thread-pool behavior is cooperative and observable; OS RT policy, CPU affinity guarantees, independent lane threads, hard preemption, and advanced starvation aging remain future work. |

Recommended public line remains `v0.2.0-alpha.0` until a human explicitly opens a
beta tag. If a beta is opened, describe it as a **core runtime beta**, not an
adapter or ecosystem beta.

## Prompt-to-artifact checklist

| G70 requirement | Evidence | Result |
| --- | --- | --- |
| Public API matrix updated | `docs/public-api.md` classifies targets, installed headers, stable/mixed/experimental surfaces, Adapter SDK v0, examples, and CLI JSON compatibility. | Covered |
| Deprecation policy | `docs/public-api.md` and `docs/versioning.md` now define pre-1.0 deprecation expectations for stable-v0.2, mixed, experimental, schema, and CLI JSON surfaces. | Covered |
| No accidental unstable/internal headers installed | `tests/policy/check_no_adapter_deps.py`, `policy_no_core_adapter_deps`, `policy_architecture_self_test`, and `docs/architecture-guardrails.md`. | Covered |
| Runtime invariants updated | `docs/runtime-invariants.md` maps edge visibility, feedback, scheduler, async, metrics, trace, config transactions, observers, parser limits, and architecture-boundary invariants to tests/smokes. | Covered |
| Scheduler/thread/task/channel semantics documented | `docs/scheduler.md`, `docs/concurrency.md`, `docs/async-tasks.md`, `docs/channels.md`, `docs/runtime-semantics.md`. | Covered |
| Known limitations honest | `docs/current-baseline.md`, `docs/release-checklist.md`, `docs/release-progression.md`, README known limitations, and this review. | Covered |
| Unit and semantic tests | `tests/test_common.cpp`, `tests/test_graph.cpp`, `tests/test_channel.cpp`, `tests/test_state.cpp`, `tests/test_runtime.cpp`, `tests/test_stress.cpp`, and `./scripts/agent_check.sh`. | Covered |
| Golden tests | `tests/golden/*` and `./scripts/goal_check.sh golden` / `quick`. | Covered |
| Docs tests | `tests/docs/check_docs.py`, `docs_command_smoke`, and required docs map entries. | Covered |
| Package tests | `tests/cmake/package_smoke.cmake`, runtime-only option smoke, CPack smoke, package draft smoke, and `./scripts/goal_check.sh package`. | Covered |
| Sanitizer tests | `./scripts/goal_check.sh sanitizer` runs ASAN+UBSAN; TSAN remains non-blocking policy evidence, not hidden. | Covered with explicit TSAN limitation |
| Fuzz tests | `tests/fuzz/fuzz_graph_inputs.py`, optional `fuzz_graph_inputs`, and `./scripts/goal_check.sh fuzz`. | Covered as smoke; longer campaigns optional |
| Stress tests | `tests/test_stress.cpp`, `tests/stress/check_stress_workloads.py`, `scripts/stress_smoke.sh`, and `./scripts/goal_check.sh stress`. | Covered as bounded smoke; longer soak optional |
| Metric schema | `docs/metrics.md`, `include/topoexec/runtime/metric_schema.hpp`, metric descriptors, validation tests, and metrics goldens. | Covered |
| Trace schema | `docs/trace-events.md`, `RuntimeTraceEvent`, trace schema version, trace/Chrome goldens, and runtime tests. | Covered |
| Diagnostics registry | `docs/diagnostics.md`, `include/topoexec/runtime/diagnostics.hpp`, `src/diagnostics.cpp`, graph tests, and CLI strict diagnostics smokes. | Covered |
| Getting started / cookbook / API overview | `docs/getting-started.md`, `docs/cookbook.md`, `docs/api-overview.md`, and docs map smoke. | Covered |
| Release notes and changelog | `CHANGELOG.md`, `docs/release-checklist.md`, `docs/release-runbook.md`, `docs/release-progression.md`, `scripts/release_prepare.sh`. | Covered |
| Adapters/plugin previews only unless tested | `docs/adapters.md`, `docs/adapters/otel.md`, `docs/adapters/prometheus.md`, `docs/adapters/ros2.md`, `docs/plugin-loader.md`, adapter SDK/telemetry preview headers, Python/plugin preview smokes, adapter package smokes, and architecture policy. | Covered; OTel/Prometheus/ROS 2/Python/plugin-loader are dependency-free, CLI-backed, or trusted-native previews; concrete production adapters/native bindings/sandboxed plugins deferred |
| No core dependency pollution | CMake target boundaries plus policy checks keep runtime free of YAML/CLI/adapter dependencies. | Covered |
| Install smoke / package-manager drafts / artifact smoke | `./scripts/goal_check.sh package`, `packaging/vcpkg/*`, `packaging/conan/*`, CPack smoke, and release-prep artifact rehearsal. | Covered; package registries unpublished |
| Benchmark output stable / no overclaims | `docs/performance-baselines.md`, `benchmarks/*.yaml`, `tests/bench/check_bench_contract.py`, and `./scripts/goal_check.sh bench`. | Covered; thresholds opt-in per machine |
| Parser limits / no dynamic plugin default | `GraphInputLimits`, defensive-input docs/tests/fuzz smoke, architecture guardrails, plugin-loader docs, and plugin option smoke. | Covered; plugin loading stays default-off and explicit |
| Schema v2 boundary | `docs/schema-v2-notes.md`, `docs/schema-v1.md`, `docs/versioning.md`, `schema_v1_contract_smoke`, and docs map smoke. | Covered as design boundary only; no v2 loader or migration CLI |
| Editor/schema UX | `docs/editor-schema.md`, installed CLI schema discovery package smoke, `editor_schema_ux_smoke`, and docs map smoke. | Covered as setup guide and JSON diagnostics; no editor extension or LSP server |
| Hierarchical graph contract | `docs/hierarchical-graphs.md`, schema-v1 docs, graph/runtime tests, plan JSON hierarchy metadata, and invariant coverage. | Covered as compile-time namespace expansion, not runtime nesting |
| Graph template contract | `docs/graph-templates.md`, schema-v1 docs, template example YAML, graph/schema tests, and docs smoke. | Covered as strict parameter substitution, not runtime interpretation |
| CompositeLoop solver-style policy contract | `docs/composite-loops.md`, schema-v1 docs, runtime/graph tests, metrics/trace docs, and invariant coverage. | Covered as bounded in-process residual reporting and partial-output policy, not external solver plugins |
| Goal ledger | `docs/goals/backlog.md`, `docs/goals/status.md`, and this review. | Covered |

## Required beta-candidate gate

Before any beta tag, attach fresh evidence for the exact candidate commit:

```bash
git status --short
git diff --check
./scripts/agent_check.sh
cmake --build build --target topoexec_format_check
./scripts/goal_check.sh quick
./scripts/goal_check.sh docs
./scripts/goal_check.sh package
./scripts/goal_check.sh policy
./scripts/goal_check.sh python
./scripts/goal_check.sh plugins
./scripts/goal_check.sh fuzz
./scripts/goal_check.sh stress
./scripts/goal_check.sh bench
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
./scripts/release_prepare.sh --version <beta-tag>
```

Optional but recommended for a beta tag:

```bash
TOPOEXEC_STRESS_PROFILE=soak TOPOEXEC_STRESS_DURATION_SECONDS=60 ./scripts/stress_smoke.sh
TOPOEXEC_FUZZER_ENGINE=STANDALONE ./scripts/fuzz_smoke.sh
```

TSAN remains non-blocking unless release governance changes it. If TSAN is not
blocking for a beta tag, record that decision in release notes.

## Explicitly deferred from beta scope unless completed first

- Production OpenTelemetry SDK/network exporter and Prometheus HTTP/scrape
  service. G58/G59 cover only dependency-free mapping/text previews.
- Real ROS 2 client-library package beyond the G60 fake-boundary preview.
- Stable C ABI beyond the G61 ABI-version-0 preview, native Python bindings beyond the G62 CLI-backed automation preview, sandboxed/stable plugin ecosystems beyond the G63 trusted-native loader preview, actual schema v2 implementation/migration tooling beyond the G64 design note, and a full editor extension or LSP server beyond the G65 setup guide.
- Community/contribution readiness work (G68) if the beta is aimed at broad external contributors rather than core-runtime evaluators.

## Human release decision

A beta tag is allowed only if the release owner accepts the remaining deferred
scope and the exact candidate commit passes the required beta-candidate gate. If
that acceptance is not explicit, keep the next public tag on the documented alpha
line.
