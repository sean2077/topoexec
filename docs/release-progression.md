# Release Progression

This page tracks how the long-running `docs/plans/plan.md` goals map to release
stages. It is a planning ledger, not a tag announcement.

## Current candidate state

As of the completed G24 implementation commit `fd23c2d`, the repository has:

- 50/50 default CTest tests passing locally through `./scripts/agent_check.sh`.
- ASAN+UBSAN Debug CTest passing locally through `scripts/sanitizer_check.sh`.
- Non-blocking TSAN wired in CI.
- Runtime-only configure/build/install smoke with YAML, CLI, examples, and tests
  disabled.
- Adapter and ROS 2 previews documented only; no adapter SDK dependency in core.
- Defensive parser limits plus deterministic fuzz smoke.

The next public tag should be chosen by a human release step after CI for the tag
candidate commit is green.

## Version ladder

| Stage | Status | Evidence | Remaining before tagging that stage |
| --- | --- | --- | --- |
| `v0.1.1-alpha` | Ready as a stabilization candidate | Baseline gaps, invariants, docs, examples, package smoke, schema/tooling, and defensive input handling are complete. | Verify CI on exact tag commit; prepare release notes/checksums. |
| `v0.2.0-alpha` | Mostly ready as runtime-completeness alpha | Scheduler, channel/backpressure, trigger, payload/memory, CompositeLoop, state/config, observability, benchmark, and CLI goals are complete. | Decide whether persistent worker naming/wall-clock fixed-rate cadence remain explicit limitations or move to a later alpha. |
| `v0.3.0-alpha` | Preview-doc ready, implementation deferred | Adapter contracts, stub layout, and ROS 2 design are complete without core dependency pollution. | Add stable observer/exporter API and optional adapter targets before claiming adapter implementation readiness. |
| `v0.5.0-beta` | Not ready | ASAN+UBSAN and fuzz smoke exist; docs/examples are mature. | Blocking TSAN decision, coverage-guided fuzz/property tests, deprecation policy hardening, and external release artifact rehearsals. |
| `v1.0.0` | Not ready | Core semantic direction is clear. | Stable schema/API/metrics names, mature packages, adapter boundary stability, and no known MVP-only scheduler limitations. |

## Goal completion rollup

The plan goals tracked in `docs/goals/backlog.md` are complete through G25 once
this release progression update lands. Deferred capabilities remain documented as
limitations rather than hidden TODOs:

- ROS 2, OpenTelemetry, Prometheus, Python, Perfetto, C API, and dynamic plugin
  loaders are preview/deferred surfaces, not implemented adapters.
- TSAN remains non-blocking until concurrency signal is stable.
- Coverage-guided fuzzing and property-test expansion remain beta hardening work.
- Scheduler priority/affinity/RT policy and timeout preemption remain future work.

## Required release evidence for next tag

Before tagging any next prerelease:

```bash
git status --short
git diff --check
./scripts/agent_check.sh
cmake --build build --target topoexec_format_check
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/sanitizer_check.sh
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
7. known limitations copied into release notes.
