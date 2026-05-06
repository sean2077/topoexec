# Python Automation Preview

G62 adds an unstable, default-off Python preview for configuration, testing, and
CLI-like automation. It is a small stdlib-only package over the installed or
build-tree `topoexec` CLI, not a native extension and not a high-throughput
payload path.

## Status

- Package: `topoexec_preview`
- Source tree: `python/topoexec_preview`
- Build option: `TOPOEXEC_BUILD_PYTHON_PREVIEW=ON`
- Package metadata: `TOPOEXEC_HAS_PYTHON_PREVIEW`
- Runtime dependency model: no dependency when the option is off; when enabled,
  the preview uses Python only for tests/automation and shells out to the CLI.
- Default build: off

## Binding decision

G62 deliberately does not add `pybind11`, `Python.h`, or a native extension.
The preview chooses a CLI-backed client because the goal is config/test
automation and deterministic graph inspection, not low-latency component or
payload execution. A future native binding should build on the G61 C API/FFI
surface after ABI, payload ownership, and packaging rules are clearer.

## Supported scope

`TopoExecClient` supports JSON-backed helpers for:

- loading graph YAML from a path or in-memory `GraphDocument` text;
- validating a graph;
- reading the compiled plan;
- running deterministic bounded steps;
- reading runtime metrics;
- reading runtime trace events.

All calls execute the CLI with JSON output and return a `CommandResult` that
captures argv, return code, stdout/stderr, parsed JSON data, and an `ok` helper.
For expected validation failures, pass `check=False` and inspect `result.data`.

## Example

```python
from pathlib import Path

from topoexec_preview import GraphDocument, TopoExecClient

client = TopoExecClient("topoexec")
graph = GraphDocument.from_file(Path("examples/minimal.yaml"))

validation = client.validate(graph)
plan = client.plan(graph)
run = client.run(graph, steps=1)
metrics = client.metrics(graph, steps=1)
trace = client.trace(graph, steps=1)

assert validation.ok
assert plan.data["graph"] == "minimal"
assert run.data["component_count"] >= 3
assert metrics.data["metrics"]
assert trace.data["trace"]
```

## Non-goals

- No native Python extension.
- No pybind11 dependency.
- No high-throughput payload API, image/point-cloud path, or zero-copy bridge.
- No component implementation callbacks in Python.
- No promise that CLI JSON remains the final Python API shape before beta.
- No required Python dependency for `topoexec::runtime`, `topoexec::yaml`, or
  normal C++ embedding.

## Validation

G62 coverage:

- `python_preview_smoke` imports `topoexec_preview`, validates/plans/runs a
  graph, reads metrics and trace, and exercises in-memory YAML materialization
  plus `check=False` validation errors.
- `cmake_python_preview_options_smoke` proves two paths: with the preview off,
  a runtime-only C++ build still configures without YAML/CLI/examples/testing;
  with the preview on, the source and installed Python package can drive the
  CLI.
- `policy_no_core_adapter_deps` continues to audit the runtime/CMake boundary so
  Python preview work does not leak into core runtime dependencies.
