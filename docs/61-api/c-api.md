# C API / FFI Preview

This unstable, default-off C API preview is for future FFI consumers. It is a
small design/prototype surface for Python/Rust/C embedding paths, not an ABI
freeze and not a dynamic plugin system.

## Status

- Target: `topoexec::c_api`
- Header: `topoexec/c_api/topoexec.h`
- Build option: `TOPOEXEC_BUILD_C_API=ON`
- Package metadata: `TOPOEXEC_HAS_C_API`
- C macro: `TOPOEXEC_C_API_VERSION == 0`
- Default build: off

The preview is source-level evidence only. Binary compatibility is not promised
before 1.0, and the ABI version is intentionally `0`.

## Design decisions

| Topic | Preview decision |
| --- | --- |
| Handles | Opaque C handles: `topoexec_runtime_t`, `topoexec_graph_builder_t`, and `topoexec_result_t`. |
| Lifecycle | Every create function has an explicit destroy function; callers own returned handles. |
| Errors | Functions return `TOPOEXEC_STATUS_OK`/`TOPOEXEC_STATUS_ERROR`; last-error strings are owned by the related handle and valid until the next mutating call or destroy. |
| Payloads | High-throughput payload handles are deferred. The preview only creates an internal no-op component graph and reads result metrics. |
| Graph builder | Minimal builder supports graph create, event-loop lane add, and no-op component add. It is enough to prove create/run/destroy and metric iteration. |
| Results | `topoexec_result_ok`, error count/string access, metric count, and `topoexec_result_metric_at` expose read-only run evidence. |
| Versioning | `TOPOEXEC_C_API_VERSION` starts at `0`; any ABI-affecting change before a real ABI promise may change names or layout. |
| Dependencies | `topoexec::c_api` links only `topoexec::runtime`; it does not link YAML, CLI, adapters, Python, or plugin loaders. |

## Example

```c
#include "topoexec/c_api/topoexec.h"

int main(void) {
  topoexec_runtime_t* runtime = topoexec_runtime_create();
  topoexec_graph_builder_t* graph = topoexec_graph_builder_create("ffi_preview");
  topoexec_graph_builder_add_event_loop_lane(graph, "main");
  topoexec_graph_builder_add_noop_component(graph, "noop", "main");
  topoexec_result_t* result = topoexec_runtime_run(runtime, graph, 1u);

  int ok = topoexec_result_ok(result);

  topoexec_result_destroy(result);
  topoexec_graph_builder_destroy(graph);
  topoexec_runtime_destroy(runtime);
  return ok ? 0 : 1;
}
```

## Ownership rules

- `topoexec_runtime_create` and `topoexec_graph_builder_create` return owned
  handles or `NULL`.
- `topoexec_runtime_run` returns an owned result handle or `NULL`.
- `const char*` values returned by error/metric accessors are borrowed from the
  owning handle and must not be freed by callers.
- Destroying a handle invalidates all borrowed strings from that handle.
- Passing `NULL` to destroy functions is allowed.

## Non-goals

- No stable ABI promise.
- No dynamic plugins or C callbacks for component implementation.
- No Python binding or package generation.
- No payload zero-copy/high-throughput FFI path.
- No YAML loading or CLI wrappers in the C API target.
- No cross-language ownership of `RuntimePayload` internals.

## Validation

Coverage:

- `test_c_api` creates runtime/builder/result handles, runs a no-op graph,
  iterates runtime metrics, and checks error-string behavior without throwing
  through the C API.
- `cmake_c_api_options_smoke` configures a runtime-only build with
  `TOPOEXEC_BUILD_C_API=ON`, installs it, and verifies a downstream C source can
  `find_package(topoexec COMPONENTS c_api)`, link `topoexec::c_api`, run a graph,
  and iterate metrics.
- `policy_no_core_adapter_deps` checks the C API target links runtime without
  YAML/CLI/adapter dependencies.
