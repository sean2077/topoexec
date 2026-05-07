# Versioning Policy

TopoExec uses semantic versioning for release tags and CMake package versions.

Current package version:

```text
0.2.0
```

Current release target:

```text
next prerelease candidate; see docs/43-ci-build-release-tools/release-progression.md
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
- `subgraphs[]` is an additive schema-v1 field because it expands to the same
  flat `GraphSpec` semantics before validation and runtime execution.
- `templates[]` and `template_instances[]` are additive schema-v1 fields because
  they are strict parameter-substitution inputs that disappear before runtime.
- CompositeLoop `solver_iteration`, `residual_threshold`, and `partial_success`
  are additive schema-v1 fields because they extend the existing explicit loop
  owner without weakening immediate-SCC validation or adding runtime plugins.
- A semantic change can require a contract-version update even if the schema shape does not change.
- A schema bump can be required when old graph files would be rejected or would mean something different.
- [Schema v2 notes](../33-specs-rfcs/schema-v2-notes.md) classify candidate future fields so v1
  does not absorb breaking behavior, graph-driven plugin/package discovery,
  adapter-specific transport config, or arbitrary expression languages.

The current semantic contract is documented in [semantic-contract.md](../21-architecture/semantic-contract.md) and exposed through `topoexec doctor` plus the schema dump annotation `x-topoexec-semantic_contract_version`.

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
- Compile-time `subgraphs[]` namespace expansion metadata in `GraphHierarchyEntry`.
- CompositeLoop ownership requirements and fixed-point runtime metrics.
- CMake package target names: `topoexec::core`, `topoexec::runtime`, and `topoexec::yaml`.
- CLI JSON field names documented in [metrics.md](../62-schemas-protocols/metrics.md), [trace-events.md](../62-schemas-protocols/trace-events.md), schema tooling docs, and golden outputs.

### Subject To Change Before 1.0

- C++ component lifecycle return types.
- Typed input helper APIs.
- Error propagation surface from component execution to runtime result.
- Scheduler lane concurrency behavior beyond the current documented MVP.
- Worker-pool and async max-inflight policy.
- Trigger v2 preview fields beyond the current declarative
  `watermark`/`condition`/`debounce`/`rate_limit` behavior.
- CompositeLoop solver-style residual reporting and partial-success policy.
- OTel/Prometheus exporter preview targets, in-memory/text record shapes, and
  mapping options.
- Trusted-native plugin loader preview target, manifest fields, error codes,
  unload option, and plugin ABI/version policy.
- Schema v2 candidate sketches in [schema-v2-notes.md](../33-specs-rfcs/schema-v2-notes.md);
  no v2 loader or migration CLI exists yet.
- Experimental headers listed in [public-api.md](../61-api/public-api.md).

Use [api-change-checklist.md](../61-api/api-change-checklist.md) before modifying installed headers, schema fields, or CLI JSON surfaces.

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
  scheduler/channel internals, lifecycle/config transaction metadata, telemetry
  preview record/text shapes/options, examples, and human-readable CLI text may
  change faster, but changes still need a
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
- Candidate v2 features must be classified against [schema-v2-notes.md](../33-specs-rfcs/schema-v2-notes.md) before changing v1.

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
only design docs. Adapter implementation tags should not be claimed while real ROS 2
client-library packages, production OpenTelemetry/Prometheus, native Python
bindings, stable C ABI, sandboxed/stable dynamic plugin ecosystems, graph-driven
plugin discovery, schema v2 implementation/migration tooling, and external
Perfetto integrations remain docs-only/deferred.
Telemetry targets, the ROS 2 preview target, the C API target, the Python
automation package, and the plugin loader target are only dependency-free,
CLI-backed, ABI-version-0, or trusted-native previews.
