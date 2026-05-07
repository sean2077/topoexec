# Goal Status

Last updated: 2026-05-07

This is the current goal ledger. It records live state, completed ranges,
current evidence, and active deferrals only. Detailed command transcripts,
milestone logs, and local evidence paths belong in CI artifacts, release-prep
bundles, or git history.

## Current State

| Area | State |
| --- | --- |
| Historical goal sweeps | G0-G25, G26-G70, G71, G73-G95, and G99 are complete. |
| Active implementation goal | None. |
| Required repository gate | `scripts/agent_check.sh` before declaring repo changes complete. |
| Focused docs gate | `scripts/goal_check.sh docs`. |
| High-signal focused gates | `golden`, `schema`, `examples`, `showcase`, `package`, `policy`, `live`, `bench`, `live-perf`, `stress`, `fuzz`, and `sanitizer` as relevant to the changed surface. |
| Blockers | G81/G82/G83 ecosystem, integration, package-publication, schema-v2, v1.0, and hard-real-time decisions remain deferred until adoption signals and human owner decisions resolve them. |

## Completed Goals

| Range | Outcome | Current evidence |
| --- | --- | --- |
| G0-G25 | Earlier runtime/API stabilization board completed. | Runtime/API docs, release docs, tests, and git history. |
| G26-G70 | Public API, semantic contract, scheduler, task executor, graph compiler, hierarchy/templates, lifecycle/config, CompositeLoop, metrics/trace/diagnostics, packaging, previews, release automation, community readiness, robot-cell pilot, and beta-readiness work completed. | Current architecture, user-guide, API, testing, integration, release, and schema docs. |
| G71 | Post-alpha hardening completed: previous-tick behavior, alpha overflow semantics, trigger pending bounds, async accounting, CompositeLoop visibility, benchmark expansion, optional Doxygen, and Pages workflow. | Runtime semantics/invariants, testing and release docs, API docs, and focused docs/golden/bench/sanitizer/stress/fuzz validation. |
| G73 | Low-overhead live runtime validation completed: `graph observe`, live assertions, record/replay, local dashboard, and live performance gates. | Live observe schema/tooling/performance docs, runtime invariants, and focused live/live-perf/bench validation. |
| G74 | Examples, showcase assets, and README refresh completed with curated public examples and anti-rot checks. | README, examples guide/index, generated showcase assets, and focused examples/showcase/docs/golden validation. |
| G75-G80 | Release/adoption readiness, dogfood pilot, compatibility harness, package hardening, reliability program, and feedback loop completed. | Release/adoption/package/testing docs, issue templates, CHANGELOG, and focused release/package/compat/reliability/adoption validation. |
| G81-G83 | Ecosystem and conditional-track decision records completed; implementation tracks remain blocked/deferred. | `docs/31-planning-roadmap/ecosystem-decision-gate.md`, `conditional-tracks-ledger.md`, and blocker notes under `goals/blockers/`. |
| G84-G85 | Core-runtime beta-candidate readiness and v1 readiness deferral completed. | Beta readiness review, v1 readiness program, release progression/checklist, and focused beta/v1 validation. |
| G86-G95 | Trust-trial maintainability sweeps completed across runtime correctness, concurrency/lifetime, CLI/schema/YAML errors, performance, coverage, API/package, reliability, public consistency, and release-candidate stabilization. | Current docs, tests, CHANGELOG, focused gates, and full repository validation. |
| G99 | Final validation and ledger reconciliation completed for G75-G95. | Required repository gate, focused docs/golden/schema/examples/showcase/package/policy/live/bench/live-perf/stress/fuzz/sanitizer gates, diff check, and ultragoal status reconciliation were completed on 2026-05-07. |

## Active Deferrals

| Topic | Status | Decision needed before opening |
| --- | --- | --- |
| Schema v2 implementation and migration CLI | Deferred | Reviewed v2 loader/migration design and compatibility policy. |
| Full editor extension or LSP server | Deferred | Evidence that JSON Schema plus CLI diagnostic JSON no longer covers editor UX needs. |
| Production OpenTelemetry or Prometheus exporters | Deferred | Exporter dependency, transport boundary, deployment owner, and release policy. |
| Real ROS 2 package/client-library adapter | Deferred | Stable core/runtime/API boundary and approved ROS dependency/package decision. |
| Stable C ABI beyond ABI version 0 | Deferred | FFI ownership, error model, and compatibility guarantees. |
| Native Python bindings | Deferred | Evidence that CLI-backed automation is insufficient plus binding ownership and packaging plan. |
| Sandboxed or stable plugin ecosystem | Deferred | Sandbox/unload/API policy and distribution owner. |
| Package registry publication | Deferred | Human release owner, exact artifacts/tags/checksums, credentials, and clean-machine evidence. |
| Hard real-time scheduling, affinity, or preemption | Deferred | OS policy, timing guarantees, and test strategy. |
| v1.0 readiness | Deferred | Post-beta adoption evidence, stable-surface decisions, package/adoption maturity, and human release-owner acceptance. |

## Canonical Evidence Surfaces

| Need | Canonical doc or check |
| --- | --- |
| Architecture and runtime behavior | `docs/21-architecture/runtime-architecture.md`, `runtime-semantics.md`, `runtime-invariants.md`, and `semantic-contract.md` |
| Codebase ownership | `docs/22-codebase/codebase-map.md` |
| Public API stability | `docs/61-api/public-api.md`, `api-change-checklist.md`, and `c-api.md` |
| Schema and wire contracts | `docs/33-specs-rfcs/schema-v1.md`, `schema-v2-notes.md`, and `docs/62-schemas-protocols/` |
| Testing and release gates | `docs/24-testing/testing-strategy.md`, `docs/43-ci-build-release-tools/current-baseline.md`, `release-checklist.md`, and `release-runbook.md` |
| Beta readiness and deferrals | `docs/43-ci-build-release-tools/beta-readiness-review.md`, `release-progression.md`, and `v1-readiness-program.md` |
| Current backlog | `docs/31-planning-roadmap/goals/backlog.md` |

## Update Rule

When a future goal starts, completes, blocks, or is intentionally deferred,
update this page and `backlog.md` with compact current state only: scope,
outcome, blocker location, and validation gate names. Do not reintroduce
milestone evidence sections, baseline command tables, local OMX evidence paths,
full command transcripts, or migration-record zones into the primary docs.
