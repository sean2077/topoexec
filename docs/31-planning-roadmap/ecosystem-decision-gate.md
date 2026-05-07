# Ecosystem Direction Decision Gate

Status: G81 decision artifact. No G82 integration track is open yet. This page
compares possible ecosystem directions against the evidence from G75-G80 and
keeps implementation blocked until adoption signals justify one track.

## Entry evidence available now

| Evidence | Status |
| --- | --- |
| Release/adoption readiness | Local `v0.2.0-alpha.0` candidate docs, package metadata, and release-prep dry-run are ready; no tag has been published by a human owner. |
| Dogfood pilot | Synthetic YAML pilot plus C++ robot-cell app exercise runtime semantics without external dependencies. |
| Compatibility harness | Stable-v0.2 header/API, schema, CLI JSON, metrics, trace, live observe, and package smokes exist. |
| Packaging hardening | Runtime-only/default/CPack/vcpkg/Conan draft package matrix exists; registry publication remains deferred. |
| Reliability/adoption loop | Bounded reliability tiers and feedback/triage docs exist; no real external adoption data is present yet. |

## Candidate G82 tracks

| Track | Candidate | Upside | Main blocker | Current decision |
| --- | --- | --- | --- | --- |
| G82a | Production observability exporter | Converts metrics/trace/live observe into real telemetry integration. | Requires exporter dependency/transport/API decision and evidence that users need it. | Deferred. |
| G82b | Native Python binding | Improves scripting and notebooks. | Requires ownership/performance/API stability decision; current CLI-backed preview may be enough. | Deferred. |
| G82c | Real ROS 2 adapter | Attractive robotics adoption path. | Requires ROS dependency/version/executor/lifecycle boundary and hardware-like validation plan. | Deferred. |
| G82d | Editor/LSP | Better graph-authoring UX. | Requires proof JSON Schema + diagnostics are insufficient and a maintenance plan. | Deferred. |
| G82e | Package registry publication | Improves install/adoption with least runtime-scope risk. | Requires human release owner, exact release artifacts, credentials, and clean-machine evidence. | Recommended future first track after a human `v0.2.0-alpha.0` release decision. |

## Recommendation

Do not implement any G82 track in this goal. If adoption evidence appears after
G75-G80, prefer **G82e package registry publication** first because it improves
access without adding runtime dependencies or widening API semantics. Keep G82a-d
behind explicit user demand or concrete downstream integration issues.

## Decision criteria for opening a track

Open at most one track when all of these are true:

1. a human owner accepts the scope and credentials/dependencies involved;
2. at least one adoption signal names the need, such as install friction,
   telemetry integration need, ROS integration request, Python automation limit,
   or editor authoring friction;
3. the track has scope, allowed files, non-goals, acceptance, validation, and
   blocker handling before implementation;
4. the track preserves G73 live observe hot-path invariants and G74 generated
   showcase anti-rot rules;
5. status/backlog/changelog/release docs make preview/deferred claims explicit.

## Safe independent work before any track

- keep examples, package, reliability, and adoption gates green;
- collect real issue/PR feedback through the G80 triage workflow;
- improve docs and package smoke evidence without publishing;
- write blocker notes for any dependency/API decision that would otherwise be
  guessed.
