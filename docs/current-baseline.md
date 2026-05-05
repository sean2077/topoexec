# Current Baseline

Date: 2026-05-05

Baseline release target:

```text
v0.1.0-alpha
```

Runtime implementation commit used for release-candidate CI:

```text
3b5d7c0 Stabilize alpha runtime API contracts
```

Environment used for local reproduction:

```text
OS: Ubuntu 24.04 environment
C++ compiler: g++ 13.3.0
Clang: not installed locally; covered by GitHub Actions
CMake: 3.28.3
CTest: 3.28.3
clang-format: available via /usr/bin/clang-format
```

Commands reproduced locally before tagging:

```bash
git diff --check
./scripts/agent_check.sh
TOPOEXEC_BUILD_DIR=/tmp/topoexec-gcc-debug TOPOEXEC_BUILD_TYPE=Debug ./scripts/agent_check.sh
```

Observed result:

```text
29/29 CTest tests passed in RelWithDebInfo and Debug GCC local runs.
```

Release artifact smoke:

```bash
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD_DIR" -j
ctest --test-dir "$BUILD_DIR" --output-on-failure
cmake --install "$BUILD_DIR" --prefix "$INSTALL_DIR"
cmake -S tests/cmake/runtime_smoke -B "$RUNTIME_SMOKE_DIR" -DCMAKE_PREFIX_PATH="$INSTALL_DIR"
cmake --build "$RUNTIME_SMOKE_DIR" -j
"$RUNTIME_SMOKE_DIR/topoexec_runtime_smoke"
```

Observed result:

```text
29/29 CTest tests passed; downstream topoexec::runtime package smoke executable exited 0.
```

GitHub Actions evidence:

```text
CI run 25331487554 on commit 3b5d7c0 completed successfully:
- gcc / Debug
- gcc / RelWithDebInfo
- clang / Debug
- clang / RelWithDebInfo
```

Current branch limitations after the post-alpha scheduler/async pass:

- `thread_pool` lanes have bounded MVP execution, but priority, affinity, RT policy, persistent worker naming, and timeout preemption are not implemented.
- Async `policy.max_inflight` is enforced for deferred completions, but it is not a general async task/future executor.
- Non-blocking ThreadSanitizer CI is wired for GitHub Actions after the worker-lane MVP; local verification still uses `scripts/agent_check.sh`.
- ROS 2, OpenTelemetry, Prometheus, Python, and external Perfetto adapters are deferred.
