# Release Checklist

Target: `v0.1.0-alpha`

The release is ready only when each item is checked against the commit being tagged.

## Required Checks

- [ ] `git status --short` is clean.
- [ ] `git diff --check` passes.
- [ ] `./scripts/agent_check.sh` passes locally.
- [ ] GitHub Actions CI is green for GCC Debug.
- [ ] GitHub Actions CI is green for GCC RelWithDebInfo.
- [ ] GitHub Actions CI is green for Clang Debug.
- [ ] GitHub Actions CI is green for Clang RelWithDebInfo.
- [ ] `cmake_package_runtime_smoke` passes in CI.
- [ ] `CHANGELOG.md` has the release section updated.
- [ ] `docs/versioning.md` matches the intended tag.
- [ ] `docs/schema-v1.md`, `docs/runtime-semantics.md`, `docs/metrics.md`, and `docs/trace-events.md` describe current behavior.
- [ ] Runnable app READMEs match current app output.

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
/tmp/topoexec-runtime-smoke/runtime_smoke
```

## Known Limitations To Keep In Release Notes

- Worker-pool lanes are schema-visible but not implemented as threaded scheduling.
- Async max-inflight is deferred; current async backpressure is bounded channel capacity and overflow policy.
- Sanitizer CI is not required for `v0.1.0-alpha`, but should be added before beta.
- ROS 2, OpenTelemetry, Prometheus, Python, and external Perfetto adapters are deferred.

## Tagging

After all required checks pass:

```bash
git tag -a v0.1.0-alpha -m "v0.1.0-alpha"
git push origin v0.1.0-alpha
```

Do not retag a public release. If the release candidate is wrong after publication, fix forward with a new prerelease tag.
