# Release Checklist

Target: next prerelease candidate after the completed G0-G70 stabilization
sweeps.

Do not retag `v0.1.0-alpha`; if a published release is wrong, fix forward with a
new prerelease tag.

Recommended next prerelease:

```text
v0.2.0-alpha.0
```

## Required Local Checks

- [ ] `git status --short` is clean before tagging.
- [ ] `git diff --check` passes.
- [ ] `./scripts/agent_check.sh` passes locally.
- [ ] `./scripts/goal_check.sh format` passes.
- [ ] `./scripts/goal_check.sh tidy` passes.
- [ ] `./scripts/goal_check.sh quick` passes.
- [ ] `./scripts/goal_check.sh docs` passes.
- [ ] `./scripts/goal_check.sh package` passes.
- [ ] `./scripts/goal_check.sh golden` passes.
- [ ] `./scripts/goal_check.sh policy` passes.
- [ ] `./scripts/goal_check.sh release` passes.
- [ ] `cmake -S . -B build-docs -DCMAKE_BUILD_TYPE=Release -DTOPOEXEC_BUILD_DOCS=ON` configures when docs artifacts are part of the release.
- [ ] `cmake --build build-docs --target topoexec_doxygen` generates the C++ API reference when docs artifacts are part of the release.
- [ ] `./scripts/docs_build_site.sh` builds the Pages site locally when docs artifacts are part of the release.
- [ ] `TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer` passes.
- [ ] `docs/43-ci-build-release-tools/release-progression.md` names the intended stage and remaining limitations.
- [ ] `docs/43-ci-build-release-tools/beta-readiness-review.md` is current if the intended stage is beta or release notes use beta language.
- [ ] `docs/43-ci-build-release-tools/v1-readiness-program.md` is current if the intended stage is v1.0 or release notes use stable-v1 language.
- [ ] `CHANGELOG.md` has the release section updated.
- [ ] `docs/43-ci-build-release-tools/versioning.md` matches the intended tag.
- [ ] `scripts/release_prepare.sh --version v0.2.0-alpha.0` completes on the clean candidate commit.

Add focused checks when the release claim depends on the relevant surface:

- [ ] `./scripts/goal_check.sh adapters`
- [ ] `./scripts/goal_check.sh plugins`
- [ ] `./scripts/goal_check.sh python`
- [ ] `./scripts/goal_check.sh ffi`
- [ ] `./scripts/goal_check.sh fuzz`
- [ ] `./scripts/goal_check.sh stress`
- [ ] `./scripts/goal_check.sh bench`
- [ ] GitHub Pages repository settings use **GitHub Actions** as the Pages source before a public docs URL is advertised.

## Golden Drift Surfaces

The release candidate must preserve or intentionally update these goldens:

- [ ] `tests/golden/plan_composite_loop.json`
- [ ] `tests/golden/metrics_minimal.json`
- [ ] `tests/golden/trace_minimal.json`
- [ ] `tests/golden/trace_minimal_chrome.json`
- [ ] `tests/golden/render_minimal.mmd`
- [ ] `tests/golden/schema_dump.json`
- [ ] `tests/golden/doctor.json`

Intentional changes to these files require a changelog and semantic/API/doc note.

## Required CI Checks

- [ ] GitHub Actions CI is green for GCC Debug.
- [ ] GitHub Actions CI is green for GCC RelWithDebInfo.
- [ ] GitHub Actions CI is green for Clang Debug.
- [ ] GitHub Actions CI is green for Clang RelWithDebInfo.
- [ ] GitHub Actions ASAN+UBSAN job is green.
- [ ] GitHub Actions TSAN job result is recorded; it remains non-blocking until release governance changes it.
- [ ] Optional soak evidence is recorded when release risk warrants it, for example
  `TOPOEXEC_STRESS_PROFILE=soak TOPOEXEC_STRESS_DURATION_SECONDS=60 ./scripts/stress_smoke.sh`.

## Artifact Smoke

The preferred candidate-prep entry point is the release runbook:

```bash
./scripts/release_prepare.sh --version v0.2.0-alpha.0
```

It checks policy, runs local gates by default, drafts release notes, creates
source/CPack/schema artifacts, writes `SHA256SUMS`, and prints a human-only
annotated tag command. Use `--dry-run --allow-dirty` only for script smoke and
`--skip-gates` only when skipped evidence is already attached separately.

Manual package checks remain useful when diagnosing artifact failures:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-release -j
ctest --test-dir build-release --output-on-failure
cmake --install build-release --prefix /tmp/topoexec-install
cmake -S tests/cmake/runtime_smoke -B /tmp/topoexec-runtime-smoke \
  -DCMAKE_PREFIX_PATH=/tmp/topoexec-install
cmake --build /tmp/topoexec-runtime-smoke -j
/tmp/topoexec-runtime-smoke/topoexec_runtime_smoke
```

Runtime-only option smoke:

```bash
cmake -S . -B build-runtime-only -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DTOPOEXEC_BUILD_YAML=OFF \
  -DTOPOEXEC_BUILD_CLI=OFF \
  -DTOPOEXEC_BUILD_EXAMPLES=OFF \
  -DTOPOEXEC_BUILD_TESTING=OFF
cmake --build build-runtime-only --target topoexec_runtime -j
cmake --install build-runtime-only --prefix /tmp/topoexec-runtime-only
```

## Known Limitations for Release Notes

- Scheduler runtime priority/admission ordering exists for component
  invocations, but affinity, RT policy, portable hard worker naming guarantees,
  independent fixed-rate lane threads, OS jitter control, advanced starvation
  aging, and hard timeout preemption are not implemented.
- ThreadSanitizer is non-blocking.
- Coverage-guided fuzzing and stress smoke exist, but long fuzz campaigns and
  longer soak runs remain non-blocking release-candidate evidence.
- Benchmark schema v2 and local baseline generation exist, but global timing
  thresholds remain intentionally absent.
- Production ROS 2 packages, production OpenTelemetry/Prometheus, native Python
  bindings, stable C ABI, sandboxed/stable dynamic plugin ecosystems,
  graph-driven plugin discovery, schema v2 implementation/migration tooling,
  full editor/LSP implementation, package-registry publication, and external
  Perfetto adapters remain deferred.
- Package-manager recipes under `packaging/` are drafts, not published ports.
- `scripts/release_prepare.sh` can generate local candidate artifacts and
  checksums, but signed release uploads and annotated tag pushes still require a
  human release step.
- Hierarchical graphs are compile-time namespace expansion only; graph templates
  are strict parameter-substitution snippets only.
- Adapter SDK, OTel/Prometheus/ROS 2, C API, Python, and plugin-loader surfaces
  are previews or boundaries as documented on their pages.
- The robot-cell pilot is a dependency-free in-process case study, not a
  hardware driver, ROS graph, camera SDK integration, exporter integration, or
  external scheduling guarantee.
- Beta wording is allowed only after a human-approved core-runtime beta
  candidate review; adapter/ecosystem beta readiness remains out of scope unless
  separately implemented and verified.

## Tagging

After all required checks pass on the exact candidate commit:

```bash
git tag -a <version> -m "<version>"
git push origin <version>
```
