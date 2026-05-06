# Release Checklist

Target: next prerelease candidate after the completed post-G25 goal sweep and the
G26 plan2 baseline.

Do not retag `v0.1.0-alpha`; if a published release is wrong, fix forward with a
new prerelease tag.

Recommended next prerelease after G26:

```text
v0.2.0-alpha.0
```

## Required local checks

- [ ] `git status --short` is clean before tagging.
- [ ] `git diff --check` passes.
- [ ] `./scripts/agent_check.sh` passes locally.
- [ ] `cmake --build build --target topoexec_format_check` passes.
- [ ] `./scripts/goal_check.sh package` passes.
- [ ] `./scripts/goal_check.sh golden` passes.
- [ ] `./scripts/goal_check.sh docs` passes.
- [ ] `./scripts/goal_check.sh adapters` passes.
- [ ] `./scripts/goal_check.sh release` passes.
- [ ] `./scripts/goal_check.sh stress` passes.
- [ ] `./scripts/goal_check.sh bench` passes.
- [ ] `TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer` passes.
- [ ] `cmake_runtime_only_options_smoke` passes as part of CTest/package smoke, including runtime-only adapter SDK downstream consumption.
- [ ] `cmake_package_runtime_smoke` passes as part of CTest/package smoke, including installed `topoexec::adapter_sdk` consumption.
- [ ] `cmake_cpack_smoke` passes as part of package smoke.
- [ ] `package_draft_smoke` passes as part of package smoke.
- [ ] `docs/release-progression.md` names the intended stage and remaining limitations.
- [ ] `docs/beta-readiness-review.md` is current if the intended stage is beta or
  release notes use beta language.
- [ ] `CHANGELOG.md` has the release section updated.
- [ ] `docs/versioning.md` matches the intended tag.
- [ ] `scripts/release_prepare.sh --version v0.2.0-alpha.0` completes on the
  clean candidate commit or any intentional `--skip-gates` rehearsal is clearly
  labeled as not final release evidence.

Current local evidence for G26:

```text
./scripts/agent_check.sh: passed, 50/50 CTest tests.
cmake --build build --target topoexec_format_check: passed.
./scripts/goal_check.sh package: passed, 2/2 package tests.
./scripts/goal_check.sh golden: passed, cli_golden_outputs.
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer: passed, 50/50 ASAN+UBSAN CTest tests.
```

Current local evidence after G67 release-automation wiring:

```text
./scripts/goal_check.sh release: passed release_prepare_smoke.
./scripts/release_prepare.sh --version v0.2.0-alpha.0 --skip-gates --allow-dirty --artifacts-dir build/release-prepare-artifact-smoke --build-dir build-release-candidate-smoke --notes-out build/release-prepare-artifact-smoke/release-notes-v0.2.0-alpha.0.md: passed artifact rehearsal without publishing.
./scripts/goal_check.sh docs: passed release-runbook doc marker.
./scripts/goal_check.sh package: passed package/CPack smokes.
./scripts/goal_check.sh quick: passed golden/schema smokes.
cmake --build build --target topoexec_format_check: passed.
./scripts/agent_check.sh: passed, 69/69 CTest tests.
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer: passed, 69/69 ASAN+UBSAN CTest tests.
git diff --check: passed.
```

## Golden drift surfaces

The release candidate must preserve or intentionally update these goldens:

- [ ] `tests/golden/plan_composite_loop.json`
- [ ] `tests/golden/metrics_minimal.json`
- [ ] `tests/golden/trace_minimal.json`
- [ ] `tests/golden/trace_minimal_chrome.json`
- [ ] `tests/golden/render_minimal.mmd`
- [ ] `tests/golden/schema_dump.json`
- [ ] `tests/golden/doctor.json`

Intentional changes to these files require a changelog and semantic/API/doc note.

## Required CI checks

- [ ] GitHub Actions CI is green for GCC Debug.
- [ ] GitHub Actions CI is green for GCC RelWithDebInfo.
- [ ] GitHub Actions CI is green for Clang Debug.
- [ ] GitHub Actions CI is green for Clang RelWithDebInfo.
- [ ] GitHub Actions ASAN+UBSAN job is green.
- [ ] GitHub Actions TSAN job result is recorded; it remains non-blocking until beta.
- [ ] Optional soak evidence is recorded when release risk warrants it, for example
  `TOPOEXEC_STRESS_PROFILE=soak TOPOEXEC_STRESS_DURATION_SECONDS=60 ./scripts/stress_smoke.sh`.

## Artifact smoke

The preferred candidate-prep entry point is the release runbook:

```bash
./scripts/release_prepare.sh --version v0.2.0-alpha.0
```

It checks policy, runs local gates by default, drafts release notes, creates
source/CPack/schema artifacts, writes `SHA256SUMS`, and prints a human-only
annotated tag command. Use `--dry-run --allow-dirty` only for script smoke and
`--skip-gates` only when the skipped evidence is already attached separately.

Manual package checks remain useful when diagnosing artifact failures:

Build from a clean checkout:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-release -j
ctest --test-dir build-release --output-on-failure
```

Install and consume the package:

```bash
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

## Known limitations for release notes

- Scheduler runtime priority/admission ordering exists for component invocations, but affinity, RT policy, portable hard worker
  naming guarantees, independent fixed-rate lane threads, OS jitter control, advanced starvation aging, and
  hard timeout preemption are still not implemented.
- ThreadSanitizer is non-blocking.
- Coverage-guided fuzzing and stress smoke exist, but long fuzz campaigns and
  longer soak runs remain non-blocking release-candidate evidence.
- Benchmark schema v2 and local baseline generation exist, but global timing
  thresholds remain intentionally absent; use only opt-in per-machine
  comparisons.
- Production ROS 2 packages, production OpenTelemetry/Prometheus, Python, stable C ABI, dynamic plugin
  loading, and external Perfetto adapters remain deferred.
- Package-manager recipes under `packaging/` are drafts, not published ports.
- `scripts/release_prepare.sh` can generate local candidate artifacts and
  checksums, but signed release uploads and annotated tag pushes still require a
  human release step.
- G41 hierarchical graphs are compile-time namespace expansion only; they do not
  implement runtime nesting, `graph_ref` templates, or external adapter stacks.
- G42 graph templates are strict parameter-substitution snippets only; they do
  not implement arbitrary expressions, includes, conditionals, loops, or runtime
  template interpretation.
- G57 Adapter SDK v0 is a dependency-free boundary and G58/G59 telemetry targets
  are only mapping/text previews, G60 is only a fake-boundary preview, and G61 is only an unstable C API
  preview; concrete ROS 2 client-library packages, production OTel/Prometheus,
  Python, Perfetto, stable C ABI, and plugin adapters remain deferred.
- G69 robot-cell pilot is a dependency-free in-process case study; it is not a
  hardware driver, ROS graph, camera SDK integration, exporter integration, or
  external scheduling guarantee.
- G70 authorizes only a human-approved core-runtime beta candidate review.
  Adapter/ecosystem beta readiness, hard real-time scheduling, signed release
  uploads, and package-registry publication remain out of scope unless separately
  implemented and verified.
- G35 trigger-v2 policies are additive declarative previews. They do not provide
  arbitrary trigger scripts, wall-clock debounce timers, external watermark
  coordination, or schema-v2 trigger expressions.

## Tagging

After all required checks pass on the exact candidate commit:

```bash
git tag -a <version> -m "<version>"
git push origin <version>
```
