# Package Matrix

Status: local packaging hardening surface for the `v0.2.0-alpha.0` candidate
line. This matrix documents what can be built and installed locally; it does not
publish a vcpkg port, Conan recipe, PyPI package, system package, or GitHub
Release artifact.

## Supported local package forms

| Form | Command / target | Evidence gate | Publication status |
| --- | --- | --- | --- |
| Runtime-only CMake install | `TOPOEXEC_BUILD_YAML=OFF`, `TOPOEXEC_BUILD_CLI=OFF`, `TOPOEXEC_BUILD_EXAMPLES=OFF`, `TOPOEXEC_BUILD_TESTING=OFF` | `cmake_runtime_only_options_smoke` | Local only |
| Default source build | `cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo` | `./scripts/agent_check.sh` | Local only |
| Installed runtime consumer | `find_package(topoexec CONFIG REQUIRED)` + `topoexec::runtime` | `cmake_package_runtime_smoke` | Local only |
| YAML/CLI consumer | `find_package(topoexec CONFIG REQUIRED COMPONENTS yaml cli)` | package smoke + CLI golden checks | Local only |
| CPack binary/source TGZ | `cpack -G TGZ --config build/CPackConfig.cmake` and source config | `cmake_cpack_smoke` | Local artifact only |
| Release candidate bundle | `./scripts/release_prepare.sh --version v0.2.0-alpha.0` | `./scripts/goal_check.sh release` for dry-run smoke; full script on a clean exact commit | Human release-owner action only |
| vcpkg draft | `packaging/vcpkg/` | `package_draft_smoke` | Draft, not submitted |
| Conan draft | `packaging/conan/` | `package_draft_smoke` | Draft, not uploaded |

## Dependency boundary by option

| Surface | Runtime dependency impact | Package target |
| --- | --- | --- |
| `topoexec::runtime` | C++20 library only; no YAML, CLI, adapter, Python, or plugin-loader dependency. | Always installed |
| `topoexec::yaml` | Adds `yaml-cpp` and `nlohmann_json` only when YAML is enabled/requested. | Optional default-on target |
| `topoexec::topoexec_cli` | Adds `CLI11` and YAML/JSON tooling only when CLI is enabled. | Optional default-on executable target |
| `topoexec::adapter_sdk` | Header-only dependency-free boundary over runtime observer/result concepts. | Always installed |
| `topoexec::c_api` | Optional ABI-version-0 preview, default off. | `TOPOEXEC_BUILD_C_API=ON` |
| `topoexec::plugin_loader` | Optional trusted-native dynamic-loader preview, default off. | `TOPOEXEC_BUILD_PLUGIN_LOADER=ON` |
| `topoexec_adapters::otel` / `prometheus` / `ros2` | Optional dependency-free preview mappings/fake boundary only; no production SDKs. | Adapter options default off |
| `topoexec_preview` | Optional stdlib-only CLI-backed Python automation preview, not native bindings. | `TOPOEXEC_BUILD_PYTHON_PREVIEW=ON` |

The CMake option names that define the local package matrix are:

```text
TOPOEXEC_BUILD_YAML
TOPOEXEC_BUILD_CLI
TOPOEXEC_BUILD_EXAMPLES
TOPOEXEC_BUILD_TESTING
TOPOEXEC_BUILD_C_API
TOPOEXEC_BUILD_PYTHON_PREVIEW
TOPOEXEC_BUILD_PLUGIN_LOADER
TOPOEXEC_BUILD_OTEL_ADAPTER
TOPOEXEC_BUILD_PROMETHEUS_ADAPTER
TOPOEXEC_BUILD_ROS2_ADAPTER
```

## Required local checks

For package/distribution changes, run:

```bash
./scripts/goal_check.sh package
./scripts/goal_check.sh release
./scripts/goal_check.sh compat
```

Before a human release-owner tag, also run the release runbook on the exact
candidate commit. Local dirty-worktree dry runs are only rehearsal evidence.

## Registry publication blockers

Package registry publication remains deferred until a human release owner opens
that scope with exact artifacts and credentials. Before publishing any registry
entry, record:

1. exact release tag and immutable archive URL;
2. checksum updates for vcpkg/Conan/package manager manifests;
3. clean-machine install evidence for runtime-only, YAML, CLI, and optional
   preview profiles;
4. owner decision on whether CLI/Python preview ship in library recipes or
   separate tool packages;
5. rollback/fix-forward policy if a package is wrong.
