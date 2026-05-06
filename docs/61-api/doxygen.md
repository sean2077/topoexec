# Doxygen API Reference

\mainpage TopoExec API Reference

This generated reference is an embedder lookup aid for public headers under
`include/topoexec`. The semantic source of truth remains the Markdown docs:
start with `docs/61-api/api-overview.md`, then read
`docs/61-api/public-api.md` and the runtime semantics docs before building
against low-level runtime internals.

## Stability boundary

- `stable-v0.2` headers are the intended C++ embedder surface for the next
  alpha line.
- Mixed and experimental headers are public for tests, advanced examples, or
  preview extensions, but they may change before beta.
- The C API, adapter SDK, telemetry-shaped previews, ROS 2-shaped preview, and
  plugin loader remain preview surfaces. Doxygen generation does not make them
  production adapters, stable ABI, native Python bindings, or sandboxed plugin
  contracts.

## Generated groups

\defgroup topoexec_runtime_api Runtime API
Primary runtime execution, graph, component, scheduler, channel, trigger,
payload, state/config, diagnostics, metrics, and trace APIs.

\defgroup topoexec_graph_api Graph spec and compiler
C++ graph model, validation, compilation, and builder helpers.

\defgroup topoexec_component_api Components and registry
Component lifecycle hooks, invocation views, registry entries, and graph context
publication APIs.

\defgroup topoexec_channel_api Channels and publication routing
Low-level bounded channel bus and publication router internals. Prefer
`RuntimeRunner`/`GraphContext` for ordinary embedding.

\defgroup topoexec_trigger_api Trigger policy
Declarative trigger readiness internals and trigger-v2 preview helpers.

\defgroup topoexec_scheduler_api Scheduler and cancellation
Scheduler run options/results, lane metrics, and cooperative stop/cancellation
surfaces.

\defgroup topoexec_payload_api Payload and ownership
Payload variants, ownership helpers, and in-process buffer pool helpers.

\defgroup topoexec_state_api State and config
Epoch-boundary runtime state snapshots and config transactions.

\defgroup topoexec_observability_api Metrics, trace, diagnostics, and logging
Runtime metric descriptors, trace events, health events, diagnostics, and common
logging/metrics helpers.

\defgroup topoexec_c_api_preview C API preview
Opaque C handles and ABI-version-0 preview functions. Not a stable ABI promise.

\defgroup topoexec_adapter_preview Adapter SDK preview
Dependency-free adapter boundary helpers and exporter-shaped preview records.

\defgroup topoexec_plugin_preview Plugin loader preview
Trusted-native dynamic plugin loader preview. Not sandboxed, unload-safe, or ABI
stable.

## Local build

Configure the optional docs target and build it:

```bash
cmake -S . -B build-docs -DCMAKE_BUILD_TYPE=Release -DTOPOEXEC_BUILD_DOCS=ON
cmake --build build-docs --target topoexec_doxygen
```

The generated HTML is written to `build-docs/docs/doxygen/html/`. Normal runtime
builds do not require Doxygen because `TOPOEXEC_BUILD_DOCS` defaults to `OFF`.
