# API Change Checklist

Use this checklist for any change that touches installed headers, public C++ types, CLI JSON, schema fields, CMake targets, or adapter-preview boundaries.

## Classification

- [ ] Identify every touched public header under `include/`.
- [ ] Classify each touched surface as `stable-v0.2`, `mixed`, `experimental`, or internal-only.
- [ ] Confirm internal-only declarations are not installed under `include/`.
- [ ] Decide whether the change is additive, behavior-preserving, behavior-changing, or breaking.

## Required updates

- [ ] Update `docs/public-api.md` when a header/type/function/class stability level changes.
- [ ] Update `docs/versioning.md` for schema, semantic-contract, release-target, or compatibility-policy changes.
- [ ] Update `CHANGELOG.md` for public behavior/API/docs changes.
- [ ] Update schema docs and JSON schema if a schema field changes.
- [ ] Update metrics/trace/diagnostics docs and goldens if CLI JSON changes.
- [ ] Add or update a downstream smoke when embedder usage changes.

## Compatibility gates

- [ ] Stable-v0.2 fields/functions keep existing names and meanings, or the breaking change is explicitly documented with migration notes.
- [ ] Runtime-only downstream consumers still link only `topoexec::runtime` and do not need YAML, CLI, or adapter targets.
- [ ] Core/runtime headers do not include ROS 2, OpenTelemetry, Prometheus, Python, Perfetto, dynamic-loader APIs, plugin-loader headers, or other adapter-only headers.
- [ ] CLI JSON removals/renames have a changelog and versioning note; additive fields are preferred.
- [ ] Schema v1 changes do not silently alter existing graph meaning.

## Validation

Run the smallest focused check that proves the change, then the required full gate:

```bash
./scripts/goal_check.sh package
./scripts/goal_check.sh golden
./scripts/agent_check.sh
```

Add `./scripts/goal_check.sh schema`, `policy`, `docs`, or `sanitizer` when the change touches those surfaces.
