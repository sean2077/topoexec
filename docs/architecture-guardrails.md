# Architecture Guardrails

This document turns the plan's module-boundary rules into reviewable constraints.

## Target boundaries

| Area | Owns | Must not depend on |
| --- | --- | --- |
| `include/topoexec/common` | small value/helper primitives for logging, metrics, trace | runtime graph execution, YAML, CLI, adapters |
| `include/topoexec/runtime` | components, graph model/builder, payloads, channels, scheduler, trigger, runner | YAML parser, CLI implementation, adapter SDKs |
| `src` | implementation for runtime/yaml libraries | tool-only behavior leaking into runtime |
| `tools/topoexec` | CLI presentation and command wiring | new semantics duplicated outside runtime/compiler APIs |
| `examples` | runnable app patterns | production adapter dependencies |
| `tests` | unit, semantic, golden, package, and future fuzz/sanitizer checks | hidden production dependencies |

## Enforced today

- Installed package exports `topoexec::core`, `topoexec::runtime`, and `topoexec::yaml` separately.
- `tests/cmake/runtime_smoke` links only `topoexec::runtime` and uses GraphBuilder/RuntimeRunner without YAML or CLI includes.
- CLI golden tests catch output drift in plan JSON, metrics JSON, trace JSON, and Mermaid render.
- Schema contract smoke catches schema enum/strictness drift and validates representative fixtures through the runtime validator.

## Review rules

- Runtime owns publication routing; components use `GraphContext` and do not directly invoke downstream components.
- Scheduler owns execution decisions; trigger engine owns readiness.
- Channel owns capacity, overflow, and backpressure accounting.
- Metrics and trace are observation surfaces, not control flow.
- Adapter-specific concepts must remain in docs or optional adapter targets until the core adapter boundary is explicit.
