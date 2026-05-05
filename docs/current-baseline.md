# Current Baseline

Date: 2026-05-05

Baseline snapshot target:

```text
current main after v0.1.0-alpha
```

Current implementation commit:

```text
b4886c2 feat(runtime): 补齐并发执行语义边界
```

Release tag relationship:

```text
v0.1.0-alpha points to 201d3e0 Prepare alpha release evidence.
Current main is ahead of v0.1.0-alpha and includes post-alpha thread_pool and async max_inflight work.
```

Environment used for local reproduction:

```text
OS: Ubuntu 24.04.4 LTS, Linux 6.17.0-22-generic x86_64
C++ compiler: g++ 13.3.0 (Ubuntu 13.3.0-6ubuntu2~24.04.1)
Clang: clang++ not installed locally; covered by GitHub Actions
CMake: 3.28.3
CTest: 3.28.3
clang-format: available via /usr/bin/clang-format
```

Commands reproduced locally for this baseline:

```bash
git diff --check
./scripts/agent_check.sh
```

Observed result:

```text
29/29 CTest tests passed in the default local RelWithDebInfo GCC run.
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

GitHub Actions evidence for current main:

```text
CI run 25355571811 on commit b4886c2 completed successfully:
- gcc / Debug
- gcc / RelWithDebInfo
- clang / Debug
- clang / RelWithDebInfo
- clang / Debug / TSAN (non-blocking job, successful in this run)
```

Current branch limitations after the post-alpha scheduler/async pass:

- `thread_pool` lanes have bounded MVP execution, but priority, affinity, RT policy, persistent worker naming, and timeout preemption are not implemented.
- Async `policy.max_inflight` is enforced for deferred completions, but it is not a general async task/future executor.
- Non-blocking ThreadSanitizer CI is wired and passed for current main; local verification still uses `scripts/agent_check.sh` because local `clang++` is unavailable.
- ROS 2, OpenTelemetry, Prometheus, Python, and external Perfetto adapters are deferred.
