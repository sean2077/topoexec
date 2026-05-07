# G85 v1.0 Readiness Program

Status: deferred criteria only. This page does not declare TopoExec ready for
`v1.0.0`, does not tag, does not publish, and does not weaken the current
beta/pre-production wording.

## Verdict

TopoExec is **not v1.0 ready**. The current safe public line remains
`v0.2.0-alpha.0` unless a human release owner opens a core-runtime beta review.
A future `v1.0.0` tag requires post-beta adoption evidence, stable-surface
freeze decisions, package maturity, and explicit owner acceptance of any
remaining runtime limitations.

## Entry criteria after beta adoption

A v1.0 readiness review can start only after all of these are true:

1. A human release owner has shipped or deliberately skipped a core-runtime beta
   with documented reasons.
2. At least one post-beta adoption cycle exists, including issue/feedback
   triage, debug-pack evidence, and a maintainer response record.
3. The synthetic dogfood pilot and at least one downstream install/package smoke
   are rerun against the exact candidate commit.
4. Compatibility evidence shows no unplanned breaking changes to stable-v0.2
   C++ headers, schema v1 semantics, CLI JSON, metrics, trace events,
   diagnostics, or package config outputs.
5. All active G81/G82/G83 blockers are either resolved, intentionally kept out
   of v1.0 scope, or converted into explicit post-v1 roadmap items by a human
   owner.

## Stable-surface expectations

Before a v1.0 tag, maintainers must decide which surfaces are frozen for the
first stable line:

| Surface | v1.0 expectation |
| --- | --- |
| C++ runtime API | Stable headers are inventoried and API change checklist exceptions are closed. |
| Graph schema | Schema v1 accepted fields, defaults, enum values, validation behavior, and semantic meaning are compatibility-preserving. |
| CLI JSON | `doctor`, `schema`, `metrics`, `trace`, `observe`, and diagnostics JSON keep documented stable fields or provide a migration note. |
| Metrics/trace/diagnostics | Names, schema versions, cardinality policy, and required fields are frozen or versioned. |
| CMake package | Runtime-only and default package consumption remain clean-machine reproducible. |
| Examples/docs | Quick Start, dogfood, cookbook, API overview, release notes, and troubleshooting match the exact candidate commit. |

Preview surfaces such as production adapters, native Python bindings, plugin
loader behavior, C ABI beyond version 0, editor/LSP, and schema v2 are not
silently promoted by a v1.0 tag. They need their own owner decision if included.

## Scheduler and runtime limitation policy

A v1.0 review must classify every known scheduler/runtime limitation as one of:

- fixed and covered by tests;
- documented as an intentional stable limitation;
- renamed/repositioned so it is not overclaimed;
- deferred with a blocker and excluded from v1.0 release notes.

The following cannot be hidden under a v1.0 claim:

- hard real-time scheduling, CPU affinity, OS jitter control, or hard timeout
  preemption;
- independent lane-thread guarantees beyond the implemented task executor and
  scheduler contracts;
- production telemetry/network exporters without approved dependency and
  transport boundaries;
- ROS 2 client-library behavior, native bindings, editor/LSP, or schema v2
  migration tooling unless those blockers are explicitly resolved.

## Package and adoption maturity criteria

A v1.0 candidate needs stronger evidence than the current local prerelease line:

- clean-machine source build and install evidence for supported compilers;
- runtime-only and default downstream `find_package` consumption;
- CPack/source archive rehearsal with checksums;
- package registry publication decision, or an explicit statement that v1.0 is
  source/archive-only;
- documented known limitations and support expectations;
- at least one completed external or realistic adopter feedback loop, even if
  the loop concludes that scope must remain narrower.

## Owner decision path

A human release owner must create a short decision record before any `v1.0.0`
tag. The record should include:

1. exact candidate commit;
2. completed validation commands and artifacts;
3. accepted stable surfaces and excluded preview surfaces;
4. resolved or deferred blocker list;
5. package publication decision;
6. adoption feedback summary;
7. release-notes wording that says what is stable and what is not.

Agents may prepare docs and checks, but they must not create or publish a v1.0
tag without explicit human release-owner action.

## Required v1.0 readiness gate

Run these on the exact candidate commit before a v1.0 decision:

```bash
git status --short
git diff --check
./scripts/agent_check.sh
./scripts/goal_check.sh compat
./scripts/goal_check.sh package
./scripts/goal_check.sh release
./scripts/goal_check.sh dogfood
./scripts/goal_check.sh reliability
./scripts/goal_check.sh adoption
./scripts/goal_check.sh ecosystem
./scripts/goal_check.sh conditional
./scripts/goal_check.sh beta
./scripts/goal_check.sh v1
./scripts/goal_check.sh live
./scripts/goal_check.sh bench
./scripts/goal_check.sh stress
./scripts/goal_check.sh fuzz
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
./scripts/release_prepare.sh --version v1.0.0
```

Any skipped gate must be listed in the human decision record with a reason and
replacement evidence.

## Current deferral list

As of 2026-05-07, v1.0 is deferred by:

- missing post-beta adoption evidence;
- unresolved G81/G82/G83 ecosystem and schema blockers;
- package-registry publication undecided;
- preview adapter/binding/plugin/editor/schema-v2 surfaces not stable;
- scheduler/runtime limitation classification not yet owner-accepted for a
  stable release.
