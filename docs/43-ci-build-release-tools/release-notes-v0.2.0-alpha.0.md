# TopoExec v0.2.0-alpha.0 Release Notes Draft

Status: draft for a human release owner. This file is safe to keep in the repo;
it is not a tag, not a GitHub Release, and not a package-registry publication.
Regenerate exact candidate notes with `scripts/release_prepare.sh` before the
human tag step.

## Highlights

- Stable-v0.2 core runtime semantics are documented and covered by local tests:
  schema v1 validation, deterministic plan/render/run, metrics, trace, and live
  observe output contracts.
- The public README and examples gallery now provide a short onboarding path,
  generated visuals, and curated smokeable examples.
- Local release preparation can build source/CPack/schema artifacts and
  checksums without publishing or tagging.
- Install and downstream CMake package smokes prove `find_package(topoexec
  CONFIG REQUIRED)` consumption for runtime-only embedders.

## Candidate validation checklist

Attach fresh evidence for the exact candidate commit:

```bash
git status --short
git diff --check
./scripts/agent_check.sh
./scripts/goal_check.sh docs
./scripts/goal_check.sh golden
./scripts/goal_check.sh package
./scripts/goal_check.sh release
./scripts/goal_check.sh examples
./scripts/goal_check.sh showcase
./scripts/release_prepare.sh --version v0.2.0-alpha.0
```

Recommended additional local evidence when time permits:

```bash
./scripts/goal_check.sh live
./scripts/goal_check.sh bench
./scripts/goal_check.sh stress
./scripts/goal_check.sh fuzz
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
./scripts/docs_build_site.sh
```

## Explicit non-goals and deferrals

This prerelease must not claim production readiness. The following remain
deferred, preview-only, or outside the current scope unless a later goal opens
them with evidence:

- production OpenTelemetry/Prometheus exporters;
- real ROS 2 package/client-library adapter;
- native Python bindings;
- stable C ABI beyond ABI version 0;
- sandboxed or stable dynamic plugin ecosystem;
- schema v2 implementation and migration tooling;
- full editor extension or LSP implementation;
- package registry publication;
- hard real-time scheduling, affinity, jitter control, or hard timeout
  preemption.

## Human release-owner actions

1. Choose the exact candidate commit.
2. Run the release checklist and attach CI/local evidence.
3. Confirm GitHub Pages deployment state before advertising any public docs URL.
4. Create and push the annotated tag only after approval:

```bash
git tag -a v0.2.0-alpha.0 <candidate-commit> -m "v0.2.0-alpha.0"
git push origin v0.2.0-alpha.0
```

Never delete or recreate a public tag; fix forward with a new prerelease tag.
