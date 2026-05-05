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

## Stability Levels

`0.x` releases are pre-1.0 releases. The project may still refine public C++ APIs, but releases should not silently change documented schema v1 runtime semantics.

### Stable-v0.2 Embedder Surface

The next prerelease line should treat these as stable-v0.2 source surfaces:

- Schema v1 accepted field names, enum values, defaults, and validation behavior.
- Edge visibility semantics for `immediate`, `delay`, `state`, and `async`.
- Non-recursive `GraphContext::publish()` staging.
- Component/registry/builder/runtime-runner headers marked `API stability: stable-v0.2`.
- Runtime result metrics/trace/error field meanings exposed through `RuntimeRunnerResult`.
- CompositeLoop ownership requirements and fixed-point runtime metrics.
- CMake package target names: `topoexec::core`, `topoexec::runtime`, and `topoexec::yaml`.
- CLI JSON field names documented in [metrics.md](metrics.md), [trace-events.md](trace-events.md), schema tooling docs, and G26 goldens.

### Subject To Change Before 1.0

- C++ component lifecycle return types.
- Typed input helper APIs.
- Error propagation surface from component execution to runtime result.
- Scheduler lane concurrency behavior beyond the current documented MVP.
- Worker-pool and async max-inflight policy.
- Experimental headers listed in [public-api.md](public-api.md).

Use [api-change-checklist.md](api-change-checklist.md) before modifying installed headers, schema fields, or CLI JSON surfaces.

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

## Progression Notes

Use [release-progression.md](release-progression.md) to decide whether the next
tag is a `v0.1.x` stabilization alpha, `v0.2.0-alpha` runtime-completeness alpha,
or a later preview. Release stage names must reflect implemented behavior, not
only design docs. Adapter implementation tags should not be claimed while ROS 2,
OpenTelemetry, Prometheus, Python, C API, dynamic plugin loading, and external
Perfetto integrations remain docs-only/deferred.
