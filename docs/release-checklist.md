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
- [ ] `TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer` passes.
- [ ] `cmake_runtime_only_options_smoke` passes as part of CTest/package smoke.
- [ ] `cmake_package_runtime_smoke` passes as part of CTest/package smoke.
- [ ] `docs/release-progression.md` names the intended stage and remaining limitations.
- [ ] `CHANGELOG.md` has the release section updated.
- [ ] `docs/versioning.md` matches the intended tag.

Current local evidence for G26:

```text
./scripts/agent_check.sh: passed, 50/50 CTest tests.
cmake --build build --target topoexec_format_check: passed.
./scripts/goal_check.sh package: passed, 2/2 package tests.
./scripts/goal_check.sh golden: passed, cli_golden_outputs.
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer: passed, 50/50 ASAN+UBSAN CTest tests.
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

## Artifact smoke

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

- Scheduler priority, affinity, RT policy, persistent worker lifecycle/naming,
  wall-clock fixed-rate scheduling, and timeout preemption are still not implemented.
- ThreadSanitizer is non-blocking.
- Deterministic fuzz smoke exists, but coverage-guided fuzzing is future beta work.
- ROS 2, OpenTelemetry, Prometheus, Python, C API, dynamic plugin loading, and
  external Perfetto adapters remain deferred.
- Package-manager recipes under `packaging/` are drafts, not published ports.

## Tagging

After all required checks pass on the exact candidate commit:

```bash
git tag -a <version> -m "<version>"
git push origin <version>
```
