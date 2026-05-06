# Build, Package, and Distribution

TopoExec is packaged as CMake targets with a runtime-first boundary. The default
build includes YAML loading, CLI tools, examples, and tests; runtime-only
embedders can switch those surfaces off.

## Build from source

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Install and consume

```bash
cmake --install build --prefix /tmp/topoexec-install
cmake -S tests/cmake/runtime_smoke -B /tmp/topoexec-runtime-smoke \
  -DCMAKE_PREFIX_PATH=/tmp/topoexec-install
cmake --build /tmp/topoexec-runtime-smoke -j
/tmp/topoexec-runtime-smoke/topoexec_runtime_smoke
```

A downstream runtime-only app should link:

```cmake
find_package(topoexec CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE topoexec::runtime)
```

YAML-loading consumers should request the component so the package config can
discover parser/JSON dependencies:

```cmake
find_package(topoexec CONFIG REQUIRED COMPONENTS yaml)
target_link_libraries(my_graph_tool PRIVATE topoexec::yaml)
```

CLI package checks can request the imported executable component:

```cmake
find_package(topoexec CONFIG REQUIRED COMPONENTS cli)
add_custom_target(check_topoexec_cli
  COMMAND $<TARGET_FILE:topoexec::topoexec_cli> doctor --format json)
```

The installed package exports:

- `topoexec::core`: header-only common baseline.
- `topoexec::runtime`: embeddable runtime library.
- `topoexec::adapter_sdk`: dependency-free adapter SDK v0 boundary.
- `topoexec::yaml`: optional YAML graph loader target when built.
- `topoexec::topoexec_cli`: optional imported executable target when the CLI is
  built and installed.
- `topoexec_adapters::otel`: optional dependency-free OTel exporter preview
  target when `TOPOEXEC_BUILD_OTEL_ADAPTER=ON`.
- `topoexec_adapters::prometheus`: optional dependency-free Prometheus text
  exporter preview target when `TOPOEXEC_BUILD_PROMETHEUS_ADAPTER=ON`.
- `topoexec_adapters::ros2`: optional dependency-free ROS 2 fake-boundary
  preview target when `TOPOEXEC_BUILD_ROS2_ADAPTER=ON`.

The CLI executable is installed as `bin/topoexec` when `TOPOEXEC_BUILD_CLI=ON`.
The installed config also exposes package metadata variables:

- `TOPOEXEC_VERSION`
- `TOPOEXEC_SCHEMA_VERSION`
- `TOPOEXEC_SEMANTIC_CONTRACT_VERSION`
- `TOPOEXEC_HAS_RUNTIME`
- `TOPOEXEC_HAS_ADAPTER_SDK`
- `TOPOEXEC_HAS_OTEL_ADAPTER`
- `TOPOEXEC_HAS_PROMETHEUS_ADAPTER`
- `TOPOEXEC_HAS_ROS2_ADAPTER`
- `TOPOEXEC_HAS_YAML`
- `TOPOEXEC_HAS_CLI`
- `TOPOEXEC_HAS_EXAMPLES`

## Build options

| Option | Default | Purpose |
| --- | --- | --- |
| `TOPOEXEC_BUILD_YAML` | `ON` | Build `topoexec::yaml` and require `yaml-cpp`. |
| `TOPOEXEC_BUILD_CLI` | `ON` | Build the CLI; requires YAML and `CLI11`. |
| `TOPOEXEC_BUILD_EXAMPLES` | `ON` | Build runnable example applications; requires YAML for YAML-backed apps. |
| `TOPOEXEC_BUILD_TESTING` | `ON` | Build CTest suite; currently requires YAML, CLI, and examples. |
| `TOPOEXEC_BUILD_FUZZERS` | `OFF` | Build optional graph-input fuzz targets; requires YAML. |
| `TOPOEXEC_BUILD_OTEL_ADAPTER` | `OFF` | Build and export the optional dependency-free OTel exporter preview target. |
| `TOPOEXEC_BUILD_PROMETHEUS_ADAPTER` | `OFF` | Build and export the optional dependency-free Prometheus text exporter preview target. |
| `TOPOEXEC_BUILD_ROS2_ADAPTER` | `OFF` | Build and export the optional dependency-free ROS 2 fake-boundary preview target. |
| `TOPOEXEC_FUZZER_ENGINE` | `AUTO` | Fuzzer engine when fuzzers are enabled: `AUTO`, `LIBFUZZER`, or `STANDALONE`. |
| `TOPOEXEC_ENABLE_ASAN` | `OFF` | Add AddressSanitizer instrumentation for GCC/Clang builds. |
| `TOPOEXEC_ENABLE_UBSAN` | `OFF` | Add UndefinedBehaviorSanitizer instrumentation for GCC/Clang builds. |
| `TOPOEXEC_ENABLE_TSAN` | `OFF` | Add ThreadSanitizer instrumentation; cannot be combined with ASAN/UBSAN. |

Runtime-only configure smoke:

```bash
cmake -S . -B build-runtime-only -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DTOPOEXEC_BUILD_YAML=OFF \
  -DTOPOEXEC_BUILD_CLI=OFF \
  -DTOPOEXEC_BUILD_EXAMPLES=OFF \
  -DTOPOEXEC_BUILD_TESTING=OFF
cmake --build build-runtime-only --target topoexec_runtime -j
cmake --install build-runtime-only --prefix /tmp/topoexec-runtime-only
```

This path is covered by `cmake_runtime_only_options_smoke`.

Optional OTel preview target:

```bash
cmake -S . -B build-otel -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DTOPOEXEC_BUILD_OTEL_ADAPTER=ON
cmake --build build-otel -j
ctest --test-dir build-otel --output-on-failure -R test_otel_adapter
```

Installed consumers request the optional package component and link the adapter
namespace target:

```cmake
find_package(topoexec CONFIG REQUIRED COMPONENTS otel)
target_link_libraries(my_exporter PRIVATE topoexec_adapters::otel)
```

This preview target maps existing runtime metrics, trace, health, and errors to
in-memory OTel-shaped records. It does not link an external telemetry SDK and is
covered by `cmake_otel_adapter_options_smoke`.

Optional Prometheus preview target:

```bash
cmake -S . -B build-prometheus -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DTOPOEXEC_BUILD_PROMETHEUS_ADAPTER=ON
cmake --build build-prometheus -j
ctest --test-dir build-prometheus --output-on-failure -R test_prometheus_adapter
```

Installed consumers request the optional package component and link the adapter
namespace target:

```cmake
find_package(topoexec CONFIG REQUIRED COMPONENTS prometheus)
target_link_libraries(my_exporter PRIVATE topoexec_adapters::prometheus)
```

This preview target renders runtime metric descriptors and custom histogram
summaries as text exposition. It does not start an HTTP server or link a
Prometheus library and is covered by `cmake_prometheus_adapter_options_smoke`.

Optional ROS 2 preview target:

```bash
cmake -S . -B build-ros2 -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DTOPOEXEC_BUILD_ROS2_ADAPTER=ON
cmake --build build-ros2 -j
ctest --test-dir build-ros2 --output-on-failure -R test_ros2_adapter
```

Installed consumers request the optional package component and link the adapter
namespace target:

```cmake
find_package(topoexec CONFIG REQUIRED COMPONENTS ros2)
target_link_libraries(my_ros_adapter PRIVATE topoexec_adapters::ros2)
```

This preview target maps topics, services, actions, and QoS into adapter-owned
endpoint descriptors and fake boundary bridges. It does not link ROS packages,
create nodes/executors, or add schema fields, and is covered by
`cmake_ros2_adapter_options_smoke`.

Optional fuzzer smoke:

```bash
TOPOEXEC_FUZZER_ENGINE=STANDALONE ./scripts/fuzz_smoke.sh
```

Use `TOPOEXEC_FUZZER_ENGINE=LIBFUZZER` with `CXX=clang++` for the libFuzzer
instrumented target.

Optional stress/soak smoke:

```bash
./scripts/goal_check.sh stress
TOPOEXEC_STRESS_PROFILE=soak TOPOEXEC_STRESS_DURATION_SECONDS=60 ./scripts/stress_smoke.sh
```

The first command is a bounded smoke. The soak profile repeats bounded-step
stress graph suites only for the caller-selected duration/iteration limits.

Optional benchmark baseline:

```bash
./scripts/goal_check.sh bench
./scripts/bench_baseline.sh
```

The focused goal check validates benchmark output contracts and writes a short
temporary baseline without thresholds. The baseline script can generate a local
ignored baseline file and optionally compare against a user-selected per-machine
threshold.

Packaging smoke:

```bash
./scripts/goal_check.sh package
```

This installs the current build and verifies downstream runtime-only,
`topoexec::yaml`, and imported CLI consumption without requiring a source-tree
clone. It also checks the runtime-only option build/install path, CPack TGZ
generation, and package-manager draft files.

## Dependency policy

- Runtime code does not depend on YAML, CLI11, ROS, Python, OpenTelemetry, or
  Prometheus.
- YAML loading requires `yaml-cpp` and `nlohmann_json`.
- CLI builds require `CLI11` and `nlohmann_json`.
- Fuzzer targets are off by default and require no runtime dependency; libFuzzer
  instrumentation requires Clang.
- Stress and soak scripts use the built CLI plus CTest/Python only; they add no
  runtime dependency.
- Benchmark scripts use the built CLI, a non-installed test benchmark binary,
  and Python only; timing thresholds are never mandatory in CI.
- Tests require GTest; if unavailable, the test build fetches it through CMake
  `FetchContent`.

## Package-manager drafts

Draft notes live under:

- `packaging/vcpkg/README.md`
- `packaging/vcpkg/vcpkg.json`
- `packaging/vcpkg/portfile.cmake`
- `packaging/conan/README.md`
- `packaging/conan/conanfile.py`

They are intentionally not published package recipes yet. Keep them aligned with
the CMake options above and do not add package-manager-specific dependencies to
the core runtime.

## CPack drafts

The build defines TGZ source and binary package generators:

```bash
cpack -G TGZ --config build/CPackConfig.cmake
cpack -G TGZ --config build/CPackSourceConfig.cmake
```

`cmake_cpack_smoke` checks that both packages can be generated. These archives
are local release-candidate artifacts, not a substitute for signed source
archives and checksums in the final release process.

## Release artifacts

Use the release runbook for candidate artifact preparation:

```bash
./scripts/release_prepare.sh --version v0.2.0-alpha.0
```

The script runs the release gates by default, creates the source archive from
`git archive HEAD`, runs CPack for binary/source TGZ artifacts, copies the schema
artifact, writes `SHA256SUMS`, and emits only a human-approved annotated tag
command. It does not tag, retag, publish, or upload a public release.

Release candidates should include:

1. source tarball from the exact candidate commit or signed/tagged commit;
2. checksum file for generated artifacts;
3. release notes from `CHANGELOG.md`;
4. schema artifact;
5. default CTest evidence;
6. runtime-only install/export smoke evidence;
7. YAML and CLI installed package smoke evidence when those options are enabled;
8. CPack source/binary archive smoke evidence;
9. ASAN+UBSAN sanitizer evidence;
10. known limitations for deferred adapters, benchmark thresholds, and non-blocking TSAN.

## Troubleshooting

- If `TOPOEXEC_BUILD_CLI=ON` fails while YAML is disabled, either enable YAML or
  disable CLI.
- If tests are enabled while YAML/CLI/examples are disabled, disable tests for a
  runtime-only build.
- If a sanitized installed static library fails to link in a downstream smoke,
  propagate the same sanitizer link flags to the downstream executable.
- Use `topoexec doctor --format json` in a default build to inspect schema,
  example, and benchmark discovery paths.
