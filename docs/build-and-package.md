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

The installed package exports:

- `topoexec::core`: header-only common baseline.
- `topoexec::runtime`: embeddable runtime library.
- `topoexec::yaml`: optional YAML graph loader target when built.

The CLI executable is installed as `bin/topoexec` when `TOPOEXEC_BUILD_CLI=ON`.

## Build options

| Option | Default | Purpose |
| --- | --- | --- |
| `TOPOEXEC_BUILD_YAML` | `ON` | Build `topoexec::yaml` and require `yaml-cpp`. |
| `TOPOEXEC_BUILD_CLI` | `ON` | Build the CLI; requires YAML and `CLI11`. |
| `TOPOEXEC_BUILD_EXAMPLES` | `ON` | Build runnable example applications; requires YAML for YAML-backed apps. |
| `TOPOEXEC_BUILD_TESTING` | `ON` | Build CTest suite; currently requires YAML, CLI, and examples. |
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

## Dependency policy

- Runtime code does not depend on YAML, CLI11, ROS, Python, OpenTelemetry, or
  Prometheus.
- YAML loading requires `yaml-cpp` and `nlohmann_json`.
- CLI builds require `CLI11` and `nlohmann_json`.
- Tests require GTest; if unavailable, the test build fetches it through CMake
  `FetchContent`.

## Package-manager drafts

Draft notes live under:

- `packaging/vcpkg/README.md`
- `packaging/conan/README.md`

They are intentionally not published package recipes yet. Keep them aligned with
the CMake options above and do not add package-manager-specific dependencies to
the core runtime.

## Release artifacts

Release candidates should include:

1. source tarball from a signed/tagged commit;
2. checksum file for the source artifact;
3. release notes from `CHANGELOG.md`;
4. default CTest evidence;
5. runtime-only install/export smoke evidence;
6. ASAN+UBSAN sanitizer evidence;
7. known limitations for deferred adapters and non-blocking TSAN.

## Troubleshooting

- If `TOPOEXEC_BUILD_CLI=ON` fails while YAML is disabled, either enable YAML or
  disable CLI.
- If tests are enabled while YAML/CLI/examples are disabled, disable tests for a
  runtime-only build.
- If a sanitized installed static library fails to link in a downstream smoke,
  propagate the same sanitizer link flags to the downstream executable.
- Use `topoexec doctor --format json` in a default build to inspect schema,
  example, and benchmark discovery paths.
