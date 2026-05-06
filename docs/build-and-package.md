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
- `topoexec::yaml`: optional YAML graph loader target when built.
- `topoexec::topoexec_cli`: optional imported executable target when the CLI is
  built and installed.

The CLI executable is installed as `bin/topoexec` when `TOPOEXEC_BUILD_CLI=ON`.
The installed config also exposes package metadata variables:

- `TOPOEXEC_VERSION`
- `TOPOEXEC_SCHEMA_VERSION`
- `TOPOEXEC_SEMANTIC_CONTRACT_VERSION`
- `TOPOEXEC_HAS_RUNTIME`
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

Release candidates should include:

1. source tarball from a signed/tagged commit;
2. checksum file for the source artifact;
3. release notes from `CHANGELOG.md`;
4. default CTest evidence;
5. runtime-only install/export smoke evidence;
6. YAML and CLI installed package smoke evidence when those options are enabled;
7. CPack source/binary archive smoke evidence;
8. ASAN+UBSAN sanitizer evidence;
9. known limitations for deferred adapters, benchmark thresholds, and non-blocking TSAN.

## Troubleshooting

- If `TOPOEXEC_BUILD_CLI=ON` fails while YAML is disabled, either enable YAML or
  disable CLI.
- If tests are enabled while YAML/CLI/examples are disabled, disable tests for a
  runtime-only build.
- If a sanitized installed static library fails to link in a downstream smoke,
  propagate the same sanitizer link flags to the downstream executable.
- Use `topoexec doctor --format json` in a default build to inspect schema,
  example, and benchmark discovery paths.
