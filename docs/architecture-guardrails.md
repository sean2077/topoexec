# Architecture Guardrails

This document turns the plan's module-boundary rules into reviewable and testable constraints.

## Target boundaries

| Area | Owns | Must not depend on |
| --- | --- | --- |
| `include/topoexec/common` | small value/helper primitives for logging, metrics, trace | runtime graph execution, YAML, CLI, adapters |
| `include/topoexec/runtime` | components, graph model/builder, payloads, channels, scheduler, trigger, runner | YAML parser, CLI implementation, adapter SDKs, private `src/` or `tools/` headers |
| `src` runtime files | implementation for `topoexec::runtime` | YAML parser, CLI presentation, adapter SDKs, dynamic-loader APIs |
| `src/graph_io.cpp` and `topoexec::yaml` | YAML/schema loading and optional graph I/O | CLI command behavior, adapter SDKs |
| `include/topoexec/adapters`, `topoexec::adapter_sdk`, and optional `topoexec_adapters::*` targets | adapter SDK v0 plus dependency-free preview mappings over public runtime types | YAML, CLI, runtime reverse-dependency, external SDKs unless an explicit optional adapter goal adds and tests them |
| `include/topoexec/plugins`, `src/plugin_loader.cpp`, and optional `topoexec::plugin_loader` | trusted-native dynamic plugin loader preview over public runtime registry APIs | YAML, CLI, adapters, Python, schema-driven discovery, sandbox claims, stable ABI claims, or runtime reverse-dependency |
| `tools/topoexec` | CLI presentation and command wiring | new semantics duplicated outside runtime/compiler APIs; low-level channel/scheduler/trigger internals |
| `examples` | runnable app patterns | production adapter dependencies |
| `tests` and `python/topoexec_preview` | tests plus optional stdlib-only CLI-backed Python automation preview | hidden production dependencies, native Python extension hooks, or core/runtime reverse dependencies |

## Enforced today

- Installed package exports `topoexec::core`, `topoexec::runtime`, `topoexec::adapter_sdk`, optional `topoexec::c_api`, optional `topoexec::plugin_loader`, and `topoexec::yaml` separately.
- `tests/cmake/runtime_smoke` links only `topoexec::runtime` and uses GraphBuilder/RuntimeRunner without YAML or CLI includes.
- `tests/cmake/c_api_options_smoke.cmake` configures a runtime-only build
  with `TOPOEXEC_BUILD_C_API=ON`, installs it, and proves downstream C source
  consumption of `topoexec::c_api` without YAML, CLI, adapters, Python, or
  dynamic plugins.
- `tests/cmake/python_preview_options_smoke.cmake` proves the G62 Python
  preview stays default-off: with it disabled, a runtime-only C++ build has no
  Python/CLI/YAML requirement; with it enabled, source and installed
  `topoexec_preview` drive the CLI for JSON automation.
- `tests/cmake/plugin_loader_options_smoke.cmake` proves the G63 plugin loader
  preview stays default-off: with it disabled, a runtime-only C++ build has no
  dynamic-loader requirement; with it enabled, sample plugins, installed
  `topoexec::plugin_loader`, and package metadata are explicit.
- `tests/cmake/otel_adapter_options_smoke.cmake` configures a runtime-only
  build with `TOPOEXEC_BUILD_OTEL_ADAPTER=ON`, installs it, and proves
  downstream `topoexec_adapters::otel` consumption without the CLI.
- `tests/cmake/prometheus_adapter_options_smoke.cmake` configures a runtime-only
  build with `TOPOEXEC_BUILD_PROMETHEUS_ADAPTER=ON`, installs it, and proves
  downstream `topoexec_adapters::prometheus` consumption without the CLI or an
  HTTP server.
- `tests/cmake/ros2_adapter_options_smoke.cmake` configures a runtime-only
  build with `TOPOEXEC_BUILD_ROS2_ADAPTER=ON`, installs it, and proves
  downstream `topoexec_adapters::ros2` consumption without the CLI or ROS
  packages.
- `tests/policy/check_no_adapter_deps.py` audits installed headers, runtime source files, CLI includes, adapter tokens, private include paths, and CMake target links.
- `policy_no_core_adapter_deps` verifies the current tree.
- `policy_architecture_self_test` plants fake dependency violations and proves the policy checker catches them.
- CLI golden tests catch output drift in plan JSON, metrics JSON, trace JSON, Chrome trace, Mermaid render, schema dump JSON, and doctor JSON.
- Schema contract smoke catches schema enum/strictness drift and validates representative fixtures through the runtime validator.

## CMake dependency contract

| Target | Allowed links | Forbidden links |
| --- | --- | --- |
| `topoexec_core` | interface include path and C++20 feature only | YAML, CLI, adapter SDKs, runtime implementation |
| `topoexec_runtime` | `topoexec_core` | `topoexec_yaml`, `topoexec_adapter_sdk`, `CLI11`, `PkgConfig::YAML_CPP`, `nlohmann_json`, adapter SDKs |
| `topoexec_adapter_sdk` | `topoexec_runtime` | `topoexec_yaml`, `CLI11`, `PkgConfig::YAML_CPP`, `nlohmann_json`, concrete adapter SDKs |
| `topoexec_c_api` | `topoexec_runtime` | `topoexec_yaml`, `topoexec_adapter_sdk`, `CLI11`, `PkgConfig::YAML_CPP`, `nlohmann_json`, adapters, Python/dynamic plugin runtimes |
| `topoexec_plugin_loader` | `topoexec_runtime`, platform dynamic-loader library privately | `topoexec_yaml`, `topoexec_adapter_sdk`, `CLI11`, `PkgConfig::YAML_CPP`, `nlohmann_json`, Python, adapters, schema-driven discovery |
| `topoexec_adapters_otel` | `topoexec_adapter_sdk` | direct `topoexec_runtime`, `topoexec_yaml`, `CLI11`, `PkgConfig::YAML_CPP`, `nlohmann_json`, external telemetry SDKs |
| `topoexec_adapters_prometheus` | `topoexec_adapter_sdk` | direct `topoexec_runtime`, `topoexec_yaml`, `CLI11`, `PkgConfig::YAML_CPP`, `nlohmann_json`, HTTP server libraries, external Prometheus SDKs |
| `topoexec_adapters_ros2` | `topoexec_adapter_sdk` | direct `topoexec_runtime`, `topoexec_yaml`, `CLI11`, `PkgConfig::YAML_CPP`, `nlohmann_json`, ROS package discovery, ROS client libraries |
| `topoexec_yaml` | `topoexec_runtime`, YAML parser privately, JSON privately | CLI target or adapter SDKs |
| `topoexec_cli` | `topoexec_yaml`, CLI11/JSON privately | direct low-level runtime internals that duplicate compiler/runtime semantics |

## Include path contract

- Every installed header under `include/` must carry an `API stability:` marker.
- Installed headers must not include private `src/` or `tools/` paths.
- `include/topoexec/common` must not include `topoexec/runtime/*` or YAML/CLI headers.
- `include/topoexec/runtime` must not include YAML/CLI/adapter SDK/plugin-loader headers or dynamic-loader APIs.
- `include/topoexec/adapters` may include public runtime headers, but runtime/common headers must not include adapter headers.
- CLI sources should stay at `RuntimeRunner`, `GraphSpec`, and graph/tool presentation APIs rather than including low-level `channel`, `event_runtime`, `scheduler`, or `trigger_policy` internals.

## Review rules

- Runtime owns publication routing; components use `GraphContext` and do not directly invoke downstream components.
- Scheduler owns execution decisions; trigger engine owns readiness.
- Channel owns capacity, overflow, and backpressure accounting.
- Metrics and trace are observation surfaces, not control flow.
- Concrete adapter-specific dependencies must remain in docs or optional adapter
  targets. The G58 OTel, G59 Prometheus, and G60 ROS 2 previews remain
  dependency-free; production telemetry SDKs/servers or real ROS packages still
  require an explicit dependency decision and focused boundary tests.
- The G63 plugin loader is an optional trusted-native preview target only. It
  may use dynamic-loader APIs internally, but runtime/core must not include or
  link those APIs, and no docs should imply sandboxing, graph discovery, or a
  stable plugin ABI.
- Any future policy exception must be documented here, justified in `CHANGELOG.md`, and protected by a focused test.
