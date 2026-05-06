# Release Progression

This page tracks how the completed `docs/plans/plan.md` sweep and the new
`docs/plans/plan2.md` goals map to release stages. It is a planning ledger, not
a tag announcement.

## Current candidate state

The post-G25 baseline commit `b86a586d3a48d84bf4e03ccabde3d061e3073579`
started the current release-candidate line. As of the plan2 line through G70
plus the later G35 trigger-v2 preview, G63 plugin-loader preview, G64
schema-v2 exploration, and G65 editor/schema UX, the repository has:

- G0-G25 complete and archived as the previous plan sweep.
- 50/50 default CTest tests passing in the previous baseline evidence.
- ASAN+UBSAN Debug CTest passing in the previous baseline evidence.
- Runtime-only configure/build/install smoke coverage with YAML, CLI, examples,
  and tests disabled.
- Dependency-free Adapter SDK v0 plus G58/G59 dependency-free telemetry mapping
  previews, the G60 ROS 2 fake-boundary preview, the G61 unstable C
  API/FFI preview, the G62 CLI-backed Python automation preview, and the G63
  trusted-native plugin loader preview; no concrete ROS 2 client-library package,
  production OpenTelemetry/Prometheus, stable C ABI, native Python binding,
  sandboxed/stable plugin ecosystem, graph-driven plugin discovery, or external
  Perfetto adapter is implemented.
- Defensive parser limits plus deterministic fuzz smoke.
- Bounded stress smoke for generated scheduler/channel workloads, thread-pool
  overload, and task-executor overload.
- Benchmark schema v2 output-contract coverage with expanded graph cases,
  task-executor benchmark smoke, and local baseline generation without mandatory
  timing thresholds.
- Installed package smokes cover runtime-only, YAML, and CLI consumption from an
  install prefix; CPack TGZ and package-manager draft checks exist.
- Documentation system v2 adds an executable cookbook, architecture diagrams,
  why-not comparisons, design principles, and recursive docs contract checks.
- Release automation prepares a candidate without publishing: `scripts/release_prepare.sh`
  can run gates, draft notes, generate source/CPack/schema artifacts, write
  checksums, and print a human-only annotated tag command.
- G69 adds a dependency-free robot-cell pilot case study that composes multiple
  lanes, async overload, state/delay feedback, BufferPool frames, config
  snapshots, metrics/trace, and invalid-config rejection without adapter claims.
- G70 adds a beta-readiness review, beta-candidate gate checklist, pre-1.0
  deprecation policy, and explicit deferred-scope ledger. It supports only a
  human-approved core-runtime beta candidate review, not adapter/ecosystem beta
  readiness.
- G64 adds schema-v2 notes that classify additive-v1 vs breaking-v2 candidates
  and keep v2 loader/migration tooling deferred until design review.
- G65 adds editor/schema UX guidance and smokes over installed schema discovery
  plus diagnostic JSON fields, without adding an editor extension or LSP server.
- G35 adds declarative trigger-v2 preview policies for watermark late-drop,
  condition readiness, debounce coalescing, and rate limiting without arbitrary
  scripting or schema-v2 expression language claims.
- G26 adds golden coverage for Chrome trace shape, schema dump JSON, and doctor
  JSON in addition to plan/metrics/trace/render outputs.

The next public tag should be chosen by a human release step after CI for the
exact tag candidate commit is green.

## Release decision note after G26

Recommended next prerelease tag:

```text
v0.2.0-alpha.0
```

Rationale:

- The completed G0-G25 work is broader than a narrow `v0.1.x` patch and includes
  scheduler, async admission, channel/backpressure, trigger, payload, loop,
  state/config, observability, benchmark, packaging, and defensive-input work.
- The new G26 baseline protects that state before deeper plan2 runtime/API work.
- The tag must still be an alpha because production exporter adapters, long
  fuzz/soak campaigns, and beta readiness remain incomplete. RuntimeObserver v1
  and the G58/G59 telemetry preview mappings are now available for in-process
  adapters.
  Persistent worker-pool v1, fixed-rate wall-clock cadence v1, coverage-guided
  fuzz smoke, and bounded stress smoke are implemented in the plan2 line, while
  scheduler concurrency surfaces remain experimental alpha surfaces.

## Version ladder

| Stage | Status | Evidence | Remaining before tagging that stage |
| --- | --- | --- | --- |
| `v0.1.1-alpha` | Still possible, but no longer the recommended label | Completed post-MVP stabilization evidence exists. | Use only if the release intentionally excludes broader runtime-completeness messaging. |
| `v0.2.0-alpha.0` | Recommended next prerelease candidate after G26 | G0-G25 complete; G26 baseline/golden surfaces protect post-G25 outputs; plan2 now adds package-consumption, Adapter SDK boundary, OTel/Prometheus preview mappings, C API preview, Python automation preview, trusted-native plugin-loader preview, schema-v2 decision notes, editor/schema UX docs and smokes, CPack smoke, G67 release-prep automation, G69 pilot-app evidence, and the G70 beta-readiness audit. | Verify CI on exact tag commit; run local release checklist; attach release-prep artifacts; human approves the annotated tag. |
| `v0.3.0-alpha` | Preview-doc ready, partial observer/exporter API implemented | RuntimeObserver v1, adapter contracts, OTel/Prometheus preview targets, ROS 2 fake-boundary preview, C API/FFI preview, Python automation preview, trusted-native plugin-loader preview, and stub layout are complete without core dependency pollution. | Add concrete production exporter/adapter targets before claiming adapter implementation readiness. |
| `v0.5.0-beta` | Conditional core-runtime review only; not automatically tag-ready | ASAN+UBSAN, fuzz smoke, bounded stress smoke, docs/examples, API/deprecation policy, G64 schema-v2 boundary, G65 editor/schema UX guide, G67 release prep, G69 pilot, and G70 audit exist. | Human release owner must accept deferrals, run gates on exact candidate commit, attach release-prep artifacts, decide TSAN/soak/fuzz scope, and avoid adapter/ecosystem beta claims. |
| `v1.0.0` | Not ready | Core semantic direction is clear. | Stable schema/API/metrics names, mature packages, adapter boundary stability, and no known MVP-only scheduler limitations. |

## Goal completion rollup

- `docs/plans/plan.md` G0-G25: complete and archived.
- `docs/plans/plan2.md` G26: release-candidate baseline protection.
- `docs/plans/plan2.md` G27+ remain active future work unless marked complete in
  `docs/goals/status.md`.

Deferred capabilities remain documented as limitations rather than hidden TODOs:

- Production ROS 2 packages, production OpenTelemetry/Prometheus, native Python
  bindings, Perfetto, stable C ABI, sandboxed/stable plugin ecosystems,
  graph-driven plugin discovery, schema v2 implementation/migration tooling, full
  editor/LSP extension implementation, and package-manager publication are
  preview/deferred surfaces.
- TSAN remains non-blocking until concurrency signal is stable.
- Coverage-guided fuzzing, bounded stress smoke, and benchmark schema v2 baseline
  tooling exist; longer fuzz campaigns, soak runs, and opt-in per-machine
  benchmark comparisons remain non-blocking beta hardening evidence.
- Scheduler runtime priority/admission ordering exists for component invocations, while affinity/RT policy, independent
  fixed-rate lane threads, OS jitter control, advanced starvation aging, and hard timeout preemption remain
  future work.

## Required release evidence for next tag

Before tagging any next prerelease:

```bash
git status --short
git diff --check
./scripts/agent_check.sh
cmake --build build --target topoexec_format_check
./scripts/goal_check.sh package
./scripts/goal_check.sh golden
./scripts/goal_check.sh docs
./scripts/goal_check.sh adapters
./scripts/goal_check.sh plugins
./scripts/goal_check.sh release
./scripts/goal_check.sh stress
./scripts/goal_check.sh bench
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
./scripts/release_prepare.sh --version v0.2.0-alpha.0
```

Then confirm GitHub Actions are green for:

- GCC Debug and RelWithDebInfo;
- Clang Debug and RelWithDebInfo;
- ASAN+UBSAN blocking sanitizer job;
- TSAN non-blocking job result recorded in release notes.

## Artifact checklist

A release artifact rehearsal should produce:

1. source tarball from the exact candidate commit or annotated tag;
2. checksum file;
3. release notes from `CHANGELOG.md`;
4. schema artifact copied from `schema/topoexec.schema.v1.json`;
5. default CTest summary;
6. runtime-only package smoke summary;
7. ASAN+UBSAN summary;
8. golden output summary for plan, metrics, trace, Chrome trace, render, schema dump, and doctor JSON;
9. docs command/map smoke summary;
10. release-prep smoke summary;
11. stress smoke summary, plus optional soak summary when run;
12. benchmark output-contract summary and any optional local baseline comparison;
13. CPack source/binary smoke summary and package-draft review status;
14. beta-readiness review and explicit deferred-scope acceptance when targeting a
    beta stage;
15. known limitations copied into release notes.
