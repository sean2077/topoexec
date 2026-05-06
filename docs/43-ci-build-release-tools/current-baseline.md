# Current Baseline

Date: 2026-05-06 (Asia/Singapore)

This page records the compact baseline facts needed for release and maintenance.
It intentionally does not preserve full per-goal command transcripts; those are
available from git history, CI logs, and release-prep artifacts.

## Baseline Relationship

| Item | Value |
| --- | --- |
| Last public tag | `v0.1.0-alpha` |
| Tag target | `201d3e0c75a4334f404085d57282415fa9678fe9` |
| Post-G25 baseline commit | `b86a586d3a48d84bf4e03ccabde3d061e3073579` |
| Completed local goal sweeps | G0-G25 and G26-G70 |
| Recommended next prerelease line | `v0.2.0-alpha.0` until a human opens a beta tag |

## Release Decision Note

The next prerelease should stay on the alpha line unless a human release owner
accepts the explicit beta deferrals in
[`beta-readiness-review.md`](beta-readiness-review.md). The current repository
contains broad runtime/API/package/docs hardening, but it still must not claim:

- production OpenTelemetry or Prometheus exporters;
- a real ROS 2 client-library package;
- stable C ABI, native Python bindings, or sandboxed/stable plugin ecosystem;
- schema v2 loader or migration tooling;
- full editor extension or LSP server;
- package-registry publication;
- hard real-time scheduling, CPU affinity guarantees, or hard preemption.

## Local Reproduction Environment

The latest local reproduction evidence in this docs cleanup used:

```text
OS: Ubuntu 24.04.4 LTS, Linux 6.17.0-23-generic x86_64
C++ compiler: g++ 13.3.0
CMake/CTest: 3.28.3
clang-format: Ubuntu clang-format 22.1.3
```

Treat this as environment context, not a portability guarantee.

## Required Maintenance Gate

Before declaring repository changes complete:

```bash
git diff --check
./scripts/goal_check.sh format
./scripts/goal_check.sh tidy
./scripts/goal_check.sh docs
./scripts/agent_check.sh
```

Use focused checks as needed for touched areas:

```bash
./scripts/goal_check.sh quick
./scripts/goal_check.sh package
./scripts/goal_check.sh golden
./scripts/goal_check.sh policy
./scripts/goal_check.sh adapters
./scripts/goal_check.sh plugins
./scripts/goal_check.sh release
./scripts/goal_check.sh stress
./scripts/goal_check.sh bench
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
cmake -S . -B build-docs -DCMAKE_BUILD_TYPE=Release -DTOPOEXEC_BUILD_DOCS=ON
cmake --build build-docs --target topoexec_doxygen
./scripts/docs_build_site.sh
```

## G71 Post-alpha Evidence

The 2026-05-06 G71 local sweep records post-alpha semantic and documentation
hardening: previous-tick visibility wake behavior, alpha `overflow: block`
would-block semantics, bounded trigger pending queues, condition timestamp
head-item handling, async/CompositeLoop output accounting, expanded benchmark
coverage, optional Doxygen API docs, and Pages site wiring. Keep generated
benchmark and docs-site artifacts out of git unless a release owner explicitly
publishes them.

## Baseline Maintenance Rule

Keep this file short. If a future release needs detailed evidence, attach the
fresh command output to the release artifact bundle or CI run instead of pasting
long historical logs here.
