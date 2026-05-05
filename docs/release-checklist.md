# Release Checklist

Target: next prerelease candidate after the completed post-MVP goal sweep.

This checklist is for the commit being tagged. Do not retag `v0.1.0-alpha`; if a
published release is wrong, fix forward with a new prerelease tag.

## Required local checks

- [ ] `git status --short` is clean before tagging.
- [ ] `git diff --check` passes.
- [ ] `./scripts/agent_check.sh` passes locally.
- [ ] `cmake --build build --target topoexec_format_check` passes.
- [ ] `TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/sanitizer_check.sh` passes.
- [ ] `cmake_runtime_only_options_smoke` passes as part of CTest.
- [ ] `cmake_package_runtime_smoke` passes as part of CTest.
- [ ] `docs/release-progression.md` names the intended stage and remaining limitations.
- [ ] `CHANGELOG.md` has the release section updated.
- [ ] `docs/versioning.md` matches the intended tag.

Current local evidence from the completed goal sweep:

- Last implementation goal commit before this release-ledger update: `fd23c2d`.
- Default local gate: 50/50 CTest tests passed through `./scripts/agent_check.sh`.
- Local ASAN+UBSAN gate: 50/50 CTest tests passed through `scripts/sanitizer_check.sh`.
- Local format gate: `topoexec_format_check` passed.
- Runtime-only configure/build/install smoke passed with YAML, CLI, examples, and tests disabled.
- Adapter SDK policy smoke passed; no core/build adapter SDK tokens were detected.

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

- Scheduler priority, affinity, RT policy, persistent worker naming, and timeout
  preemption are still not implemented.
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
