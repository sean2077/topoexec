# Versioning Policy

TopoExec uses semantic versioning for release tags and CMake package versions.

Current package version:

```text
0.1.0
```

Current release target:

```text
next prerelease candidate; see docs/release-progression.md
```

Current runtime semantic contract:

```text
0.2
```

## Stability Levels

`0.x` releases are pre-1.0 releases. The project may still refine public C++ APIs, but releases should not silently change documented schema v1 runtime semantics.

## Schema Version vs Semantic Contract Version

- `schema_version` is a graph input field. It defines the accepted YAML/JSON shape, strict fields, enum values, defaults, and validation envelope.
- `semantic_contract_version` is runtime/tool metadata. It defines what accepted graphs mean during execution: epoch boundaries, transactions, staged publication, commit visibility, trigger readiness, channel capacity, loop output commit, async deferral, and state/config snapshots.
- Schema-v1 additive fields are allowed only when the existing runtime meaning stays compatible.
- A semantic change can require a contract-version update even if the schema shape does not change.
- A schema bump can be required when old graph files would be rejected or would mean something different.

The current semantic contract is documented in [semantic-contract.md](semantic-contract.md) and exposed through `topoexec doctor` plus the schema dump annotation `x-topoexec-semantic_contract_version`.

### Stable-v0.2 Embedder Surface

The next prerelease line should treat these as stable-v0.2 source surfaces:

- Schema v1 accepted field names, enum values, defaults, and validation behavior.
- Edge visibility semantics for `immediate`, `delay`, `state`, and `async`.
- Non-recursive `GraphContext::publish()` staging.
- Component/registry/builder/runtime-runner headers marked `API stability: stable-v0.2`.
- Runtime result metrics/trace/error field meanings exposed through `RuntimeRunnerResult`.
- Runtime metric descriptor names, kinds, units, allowed labels, and `metric_schema_version`.
- Graph diagnostic descriptor codes, severities, categories, suggested fixes, and `diagnostics_schema_version`.
- Graph parser limit field names/defaults exposed through `GraphInputLimits` and CLI parser-limit options.
- CompositeLoop ownership requirements and fixed-point runtime metrics.
- CMake package target names: `topoexec::core`, `topoexec::runtime`, and `topoexec::yaml`.
- CLI JSON field names documented in [metrics.md](metrics.md), [trace-events.md](trace-events.md), schema tooling docs, and G26 goldens.

### Subject To Change Before 1.0

- C++ component lifecycle return types.
- Typed input helper APIs.
- Error propagation surface from component execution to runtime result.
- Scheduler lane concurrency behavior beyond the current documented MVP.
- Worker-pool and async max-inflight policy.
- Trigger v2 preview fields beyond the current declarative
  `watermark`/`condition`/`debounce`/`rate_limit` behavior.
- Experimental headers listed in [public-api.md](public-api.md).

Use [api-change-checklist.md](api-change-checklist.md) before modifying installed headers, schema fields, or CLI JSON surfaces.

## Deprecation and Removal Policy

Pre-1.0 releases may still break source or semantic compatibility, but not
silently. Apply this policy before opening a beta candidate:

- `stable-v0.2` C++ headers, stable schema v1 fields, semantic-contract claims,
  metric/trace/diagnostic descriptor names, and stable CLI JSON fields should
  move through a documented deprecation note before removal or rename when a
  compatibility path is practical.
- Deprecation notes belong in `CHANGELOG.md` and the owning reference page
  (`public-api.md`, schema/CLI docs, metrics/trace/diagnostics docs, or release
  notes). They should name the replacement and the earliest prerelease line where
  removal is expected.
- Experimental headers, adapter SDK v0 preview helpers, benchmark fields, direct
  scheduler/channel internals, lifecycle/config transaction metadata, examples,
  and human-readable CLI text may change faster, but changes still need a
  changelog note when users could depend on them.
- Security, correctness, or unsound-semantics fixes may remove or tighten a
  surface immediately. The release note must call out why the ordinary
  deprecation window was skipped.
- Binary compatibility is not guaranteed before `1.0.0`; downstream embedders
  should rebuild on every upgrade even when source compatibility is preserved.

## Schema Compatibility

Schema v1 is strict and compatibility-preserving:

- Additive fields are allowed only when they do not change v1 meaning.
- Unknown fields remain invalid.
- Breaking semantic changes require a schema version bump.
- New adapter-specific fields should not be added to core schema v1 unless they are useful without that adapter.

## Release Tags

Use annotated tags for public releases:

```bash
git tag -a v0.1.0-alpha -m "v0.1.0-alpha"
git push origin v0.1.0-alpha
```

Do not tag a release until [release-checklist.md](release-checklist.md) is complete for the intended target.
The current recommended next prerelease target is `v0.2.0-alpha.0`; prepare it
with [release-runbook.md](release-runbook.md) and `scripts/release_prepare.sh`
before any human-approved annotated tag is created.
A beta tag must also include the [beta readiness review](beta-readiness-review.md)
evidence and must keep deferred adapter/ecosystem surfaces explicit.

## Progression Notes

Use [release-progression.md](release-progression.md) to decide whether the next
tag is a `v0.1.x` stabilization alpha, `v0.2.0-alpha` runtime-completeness alpha,
or a later preview. Release stage names must reflect implemented behavior, not
only design docs. Adapter implementation tags should not be claimed while ROS 2,
OpenTelemetry, Prometheus, Python, C API, dynamic plugin loading, and external
Perfetto integrations remain docs-only/deferred.
