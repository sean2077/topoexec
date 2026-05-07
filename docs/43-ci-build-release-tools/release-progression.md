# Release Progression

This page maps the current repository state to release labels. It is a release
planning aid, not a tag announcement.

## Current Candidate State

The project has completed the historical G0-G70 runtime/API/docs/release
stabilization sweeps. The detailed plan files for those sweeps were deleted as
completed process artifacts; current release truth now lives in:

- [`current-baseline.md`](current-baseline.md)
- [`release-checklist.md`](release-checklist.md)
- [`release-runbook.md`](release-runbook.md)
- [`beta-readiness-review.md`](beta-readiness-review.md)
- [`v1-readiness-program.md`](v1-readiness-program.md)
- [`../31-planning-roadmap/goals/status.md`](../31-planning-roadmap/goals/status.md)
- [`../31-planning-roadmap/goals/backlog.md`](../31-planning-roadmap/goals/backlog.md)

The next public tag should be chosen by a human release step after CI for the
exact candidate commit is green.

## Version Ladder

| Stage | Status | Evidence | Remaining before tagging that stage |
| --- | --- | --- | --- |
| `v0.1.1-alpha` | Still possible, but no longer the recommended label. | Post-MVP stabilization evidence exists. | Use only if the release intentionally excludes broader runtime-completeness messaging. |
| `v0.2.0-alpha.0` | Recommended next prerelease candidate. | Runtime/API semantic contracts, package smokes, docs map, release automation, adapter/FFI/Python/plugin previews, robot-cell pilot, and beta-readiness audit exist. | Verify CI on exact tag commit, run the local release checklist, attach release-prep artifacts, and get human tag approval. |
| `v0.3.0-alpha` | Possible future adapter-preview expansion. | RuntimeObserver v1 and dependency-free preview adapter mappings exist. | Add concrete production exporter/adapter targets before claiming adapter implementation readiness. |
| `v0.5.0-beta` | Conditional core-runtime review only. | ASAN+UBSAN, fuzz smoke, bounded stress smoke, docs/examples, API/deprecation policy, release prep, community readiness, pilot app, and beta audit exist. | Human release owner must accept deferrals, run gates on the exact candidate commit, decide TSAN/soak/fuzz scope, and avoid adapter/ecosystem beta claims. |
| `v1.0.0` | Not ready. | Core semantic direction is clear, and [`v1-readiness-program.md`](v1-readiness-program.md) defines the deferred criteria. | post-beta adoption evidence, frozen stable surfaces, mature packages, package-publication decision, resolved/deferred G81-G83 blockers, and owner-accepted scheduler/runtime limitation policy. |

## Explicit Deferrals

These are limitations, not hidden TODOs:

- production ROS 2 packages;
- production OpenTelemetry/Prometheus exporters;
- native Python bindings;
- Perfetto or other external trace exporters;
- stable C ABI beyond ABI version 0;
- sandboxed/stable plugin ecosystem and graph-driven plugin discovery;
- schema v2 implementation and migration tooling;
- full editor/LSP extension implementation;
- package-manager registry publication;
- hard real-time scheduling, affinity, OS jitter control, or hard timeout
  preemption.

## Required Release Evidence

Before tagging any next prerelease:

```bash
git status --short
git diff --check
./scripts/agent_check.sh
cmake --build build --target topoexec_format_check
./scripts/goal_check.sh package
./scripts/goal_check.sh golden
./scripts/goal_check.sh docs
./scripts/goal_check.sh policy
./scripts/goal_check.sh release
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
./scripts/release_prepare.sh --version v0.2.0-alpha.0
```

Run adapter, plugin, Python, stress, bench, fuzz, and soak checks when the tag
message or release scope depends on those surfaces.
