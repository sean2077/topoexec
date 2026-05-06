# Release Progression

This page tracks how the completed `docs/plans/plan.md` sweep and the new
`docs/plans/plan2.md` goals map to release stages. It is a planning ledger, not
a tag announcement.

## Current candidate state

As of the post-G25 baseline commit `b86a586d3a48d84bf4e03ccabde3d061e3073579`,
the repository has:

- G0-G25 complete and archived as the previous plan sweep.
- 50/50 default CTest tests passing in the previous baseline evidence.
- ASAN+UBSAN Debug CTest passing in the previous baseline evidence.
- Runtime-only configure/build/install smoke coverage with YAML, CLI, examples,
  and tests disabled.
- Adapter and ROS 2 previews documented only; no adapter SDK dependency in core.
- Defensive parser limits plus deterministic fuzz smoke.
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
- The tag must still be an alpha because wall-clock fixed-rate lane v1,
  observer/exporter APIs, coverage-guided fuzzing, and beta readiness remain
  incomplete. Persistent worker-pool v1 is implemented in the plan2 line but is
  still an experimental alpha scheduler surface.

## Version ladder

| Stage | Status | Evidence | Remaining before tagging that stage |
| --- | --- | --- | --- |
| `v0.1.1-alpha` | Still possible, but no longer the recommended label | Completed post-MVP stabilization evidence exists. | Use only if the release intentionally excludes broader runtime-completeness messaging. |
| `v0.2.0-alpha.0` | Recommended next prerelease candidate after G26 | G0-G25 complete; G26 baseline/golden surfaces protect post-G25 outputs. | Verify CI on exact tag commit; run local release checklist; prepare notes/checksums. |
| `v0.3.0-alpha` | Preview-doc ready, implementation deferred | Adapter contracts, stub layout, and ROS 2 design are complete without core dependency pollution. | Add stable observer/exporter API and optional adapter targets before claiming adapter implementation readiness. |
| `v0.5.0-beta` | Not ready | ASAN+UBSAN and fuzz smoke exist; docs/examples are mature. | Blocking TSAN decision, coverage-guided fuzz/property tests, API/deprecation hardening, and external release artifact rehearsals. |
| `v1.0.0` | Not ready | Core semantic direction is clear. | Stable schema/API/metrics names, mature packages, adapter boundary stability, and no known MVP-only scheduler limitations. |

## Goal completion rollup

- `docs/plans/plan.md` G0-G25: complete and archived.
- `docs/plans/plan2.md` G26: release-candidate baseline protection.
- `docs/plans/plan2.md` G27+ remain active future work unless marked complete in
  `docs/goals/status.md`.

Deferred capabilities remain documented as limitations rather than hidden TODOs:

- ROS 2, OpenTelemetry, Prometheus, Python, Perfetto, C API, dynamic plugin
  loaders, and package-manager publication are preview/deferred surfaces.
- TSAN remains non-blocking until concurrency signal is stable.
- Coverage-guided fuzzing and stress/soak expansion remain beta hardening work.
- Scheduler priority/admission ordering, affinity/RT policy, wall-clock
  fixed-rate scheduling, and hard timeout preemption remain future work.

## Required release evidence for next tag

Before tagging any next prerelease:

```bash
git status --short
git diff --check
./scripts/agent_check.sh
cmake --build build --target topoexec_format_check
./scripts/goal_check.sh package
./scripts/goal_check.sh golden
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
```

Then confirm GitHub Actions are green for:

- GCC Debug and RelWithDebInfo;
- Clang Debug and RelWithDebInfo;
- ASAN+UBSAN blocking sanitizer job;
- TSAN non-blocking job result recorded in release notes.

## Artifact checklist

A release artifact rehearsal should produce:

1. source tarball from the exact annotated tag;
2. checksum file;
3. release notes from `CHANGELOG.md`;
4. default CTest summary;
5. runtime-only package smoke summary;
6. ASAN+UBSAN summary;
7. golden output summary for plan, metrics, trace, Chrome trace, render, schema dump, and doctor JSON;
8. known limitations copied into release notes.
