# Versioning Policy

TopoExec uses semantic versioning for release tags and CMake package versions.

Current package version:

```text
0.1.0
```

Current release target:

```text
v0.1.0-alpha
```

## Stability Levels

`0.x` releases are pre-1.0 releases. The project may still refine public C++ APIs, but releases should not silently change documented schema v1 runtime semantics.

### Stable Within v0.1.x

- Schema v1 accepted field names, enum values, defaults, and validation behavior.
- Edge visibility semantics for `immediate`, `delay`, `state`, and `async`.
- Non-recursive `GraphContext::publish()` staging.
- CompositeLoop ownership requirements and fixed-point runtime metrics.
- CMake package target names: `topoexec::core`, `topoexec::runtime`, and `topoexec::yaml`.
- CLI JSON field names documented in [metrics.md](metrics.md) and [trace-events.md](trace-events.md).

### Subject To Change Before 1.0

- C++ component lifecycle return types.
- Typed input helper APIs.
- Error propagation surface from component execution to runtime result.
- Scheduler lane concurrency behavior.
- Worker-pool and async max-inflight policy.

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
