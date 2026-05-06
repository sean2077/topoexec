# Architecture Guardrails

This document turns the plan's module-boundary rules into reviewable and testable constraints.

## Target boundaries

| Area | Owns | Must not depend on |
| --- | --- | --- |
| `include/topoexec/common` | small value/helper primitives for logging, metrics, trace | runtime graph execution, YAML, CLI, adapters |
| `include/topoexec/runtime` | components, graph model/builder, payloads, channels, scheduler, trigger, runner | YAML parser, CLI implementation, adapter SDKs, private `src/` or `tools/` headers |
| `src` runtime files | implementation for `topoexec::runtime` | YAML parser, CLI presentation, adapter SDKs |
| `src/graph_io.cpp` and `topoexec::yaml` | YAML/schema loading and optional graph I/O | CLI command behavior, adapter SDKs |
| `include/topoexec/adapters` and `topoexec::adapter_sdk` | header-only adapter SDK v0 over public runtime types | YAML, CLI, concrete adapter SDKs, runtime reverse-dependency |
| `tools/topoexec` | CLI presentation and command wiring | new semantics duplicated outside runtime/compiler APIs; low-level channel/scheduler/trigger internals |
| `examples` | runnable app patterns | production adapter dependencies |
| `tests` | unit, semantic, golden, package, policy, and future fuzz/sanitizer checks | hidden production dependencies |

## Enforced today

- Installed package exports `topoexec::core`, `topoexec::runtime`, `topoexec::adapter_sdk`, and `topoexec::yaml` separately.
- `tests/cmake/runtime_smoke` links only `topoexec::runtime` and uses GraphBuilder/RuntimeRunner without YAML or CLI includes.
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
| `topoexec_yaml` | `topoexec_runtime`, YAML parser privately, JSON privately | CLI target or adapter SDKs |
| `topoexec_cli` | `topoexec_yaml`, CLI11/JSON privately | direct low-level runtime internals that duplicate compiler/runtime semantics |

## Include path contract

- Every installed header under `include/` must carry an `API stability:` marker.
- Installed headers must not include private `src/` or `tools/` paths.
- `include/topoexec/common` must not include `topoexec/runtime/*` or YAML/CLI headers.
- `include/topoexec/runtime` must not include YAML/CLI/adapter SDK headers.
- `include/topoexec/adapters` may include public runtime headers, but runtime/common headers must not include adapter headers.
- CLI sources should stay at `RuntimeRunner`, `GraphSpec`, and graph/tool presentation APIs rather than including low-level `channel`, `event_runtime`, `scheduler`, or `trigger_policy` internals.

## Review rules

- Runtime owns publication routing; components use `GraphContext` and do not directly invoke downstream components.
- Scheduler owns execution decisions; trigger engine owns readiness.
- Channel owns capacity, overflow, and backpressure accounting.
- Metrics and trace are observation surfaces, not control flow.
- Concrete adapter-specific dependencies must remain in docs or optional adapter targets; only the dependency-free `topoexec::adapter_sdk` boundary may live in this repo before concrete adapter goals open.
- Any future policy exception must be documented here, justified in `CHANGELOG.md`, and protected by a focused test.
