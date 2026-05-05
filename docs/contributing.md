# Contributing

TopoExec is a small C++20 in-process semantic execution graph runtime. Contributions should preserve the core boundary: `topoexec::runtime` is embeddable and must not depend on YAML, CLI tooling, ROS, Python, OpenTelemetry, Prometheus, or other adapters.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Agent-friendly shortcuts:

```bash
./scripts/goal_check.sh all
./scripts/goal_check.sh quick
./scripts/goal_check.sh package
```

Optional formatting check:

```bash
cmake --build build --target topoexec_format_check
```

## Change policy

- Prefer tests and docs that harden existing runtime semantics before adding new surface area.
- Keep changes small and reversible.
- Public API changes must update `docs/public-api.md`, `docs/versioning.md`, and `CHANGELOG.md` when user-visible.
- Schema changes must update `docs/schema-v1.md`, `schema/topoexec.schema.v1.json`, fixtures/tests, and versioning notes if behavior changes.
- Runtime semantic changes must update `docs/runtime-semantics.md` and targeted tests.
- Metrics or trace field changes must update `docs/metrics.md`, `docs/trace-events.md`, and golden tests.

## Dependency policy

Runtime targets should remain lightweight:

- `topoexec::runtime` must not depend on YAML/CLI/adapters.
- Test-only scripts may use standard system tools already required by the test harness.
- Do not add production dependencies without documenting the reason and target boundary.

## Commit policy

Use the Lore commit protocol from AGENTS.md. Include what was tested and known gaps. Keep commits grouped by goal or subsystem so each commit can be reviewed or reverted independently.
