# Release Checklist

Target: `v0.1.0-alpha` tag plus current post-alpha `main`

The release is ready only when each item is checked against the commit being tagged. Post-alpha baseline refreshes should record the current `main` commit separately and must not retag a published release.

## Required Checks

- [x] `git status --short` is clean before tagging.
- [x] `git diff --check` passes.
- [x] `./scripts/agent_check.sh` passes locally.
- [x] Optional `cmake --build build --target topoexec_format_check` is available for local formatting checks.
- [x] GitHub Actions CI is green for GCC Debug.
- [x] GitHub Actions CI is green for GCC RelWithDebInfo.
- [x] GitHub Actions CI is green for Clang Debug.
- [x] GitHub Actions CI is green for Clang RelWithDebInfo.
- [x] `cmake_package_runtime_smoke` passes in CI.
- [x] `CHANGELOG.md` has the release section updated.
- [x] `docs/versioning.md` matches the intended tag.
- [x] `docs/schema-v1.md`, `docs/runtime-semantics.md`, `docs/metrics.md`, and `docs/trace-events.md` describe current behavior.
- [x] Runnable app READMEs match current app output.

Evidence:

- Current post-alpha main: `b4886c2 feat(runtime): 补齐并发执行语义边界`.
- Current local `./scripts/agent_check.sh`: 33/33 CTest tests passed after normalized golden/schema gates were added on top of `64892c9`.
- Current GitHub Actions run `25355571811`: GCC/Clang Debug/RelWithDebInfo passed, and the non-blocking clang Debug TSAN job also passed.
- Optional local format gate: `cmake --build build --target topoexec_format_check` passed.
- Local `./scripts/agent_check.sh`: 33/33 CTest tests passed, including `cli_golden_outputs` and `schema_v1_contract_smoke`.
- Local Debug GCC check: 33/33 CTest tests passed with `TOPOEXEC_BUILD_TYPE=Debug`.
- GitHub Actions run `25331487554`: GCC/Clang Debug/RelWithDebInfo all passed for implementation commit `3b5d7c0`.
- Isolated release smoke build/install/downstream package executable passed.

## Artifact Smoke

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

## Known Limitations To Keep In Release Notes

- Worker-pool lanes have bounded MVP execution, but priority, affinity, RT policy, persistent worker naming, and timeout preemption are not implemented.
- Async `policy.max_inflight` is enforced for deferred completions, but it is not a general async task/future executor.
- Non-blocking ThreadSanitizer CI is wired and passed on current post-alpha `main`; keep it non-blocking until sanitizer signal is stable enough for a beta gate.
- ROS 2, OpenTelemetry, Prometheus, Python, and external Perfetto adapters are deferred.

## Tagging

After all required checks pass:

```bash
git tag -a v0.1.0-alpha -m "v0.1.0-alpha"
git push origin v0.1.0-alpha
```

Do not retag a public release. If the release candidate is wrong after publication, fix forward with a new prerelease tag.
