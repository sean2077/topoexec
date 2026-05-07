# Quality Gates

TopoExec uses local scripts as the source of truth for code quality. CI,
pre-commit, release preparation, and agents should call the same scripts instead
of reimplementing checks in separate tool-specific configuration.

## Status

The required local gate is:

```bash
./scripts/agent_check.sh
```

It checks whitespace, commit-message policy, CMake configure, `clang-format`,
`clang-tidy`, build, and CTest.

Focused gates are available while iterating:

```bash
./scripts/goal_check.sh format
./scripts/goal_check.sh tidy
./scripts/goal_check.sh quick
./scripts/goal_check.sh compat
```

`compat` is the stable-v0.2 compatibility harness. It checks installed-header
API stability markers and public API inventory, versioning/deprecation docs,
doctor/schema/metrics/trace/live observe JSON fields, golden files, schema v1
contract smoke, live observe NDJSON smoke, and runtime-only downstream package
smoke.

## Local Setup

Install the external tools used by the quality gate:

```bash
clang-format --version
clang-tidy --version
pre-commit --version
```

Then install local hooks:

```bash
pre-commit install
pre-commit install --hook-type commit-msg
```

The hooks run repository-local checks. They do not download third-party hook
repositories, and they do not publish release artifacts.

## CMake Presets

`CMakePresets.json` provides common local configure/build entry points:

```bash
cmake --preset relwithdebinfo
cmake --build --preset relwithdebinfo
cmake --preset debug
cmake --preset asan-ubsan
```

The default scripts still accept `TOPOEXEC_BUILD_DIR` and
`TOPOEXEC_BUILD_TYPE` for agent and CI use.

## clang-tidy Boundary

`.clang-tidy` enables compiler diagnostics, Clang analyzer checks, and broad
`bugprone`, `performance`, and `portability` families, then disables checks that
are noisy for this codebase or would force broad public API/style churn.

The first-pass boundary is intentional:

- no mass `#pragma once` to include-guard rewrite;
- no public enum underlying-type churn solely for lint;
- no semantic-release, tag creation, or package publication as part of quality
  gating;
- no new runtime dependency.

`scripts/tidy_check.sh` reads `compile_commands.json`, configures the default
build directory if it is missing, skips `tests/` translation units by default,
runs production/example/tool translation units in parallel, and applies a
per-file timeout so one slow file reports a concrete failure instead of stalling
the gate. Tests still stay under `clang-format`, build, and CTest coverage.

## Release Boundary

`scripts/release_prepare.sh` consumes the same required gates before creating
candidate artifacts. It remains a preparation tool only: it does not create tags,
push tags, publish packages, or upload public releases.
