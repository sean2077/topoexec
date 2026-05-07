# Defensive Input Handling

TopoExec validates graph inputs before runtime execution and keeps hostile or
accidental large inputs bounded. This page records the current defensive limits
and the behavior expected by tests.

## Parser limits

The YAML loader rejects inputs before allocation-heavy runtime work when these
limits are exceeded:

| Limit | Value | Applies to |
| --- | ---: | --- |
| Graph text size | 1 MiB | `load_graph_text()` and files read through `load_graph_file()` |
| Lanes | 256 | `lanes` mapping entries |
| Components | 4096 | `components` sequence entries |
| Edges | 8192 | `edges` sequence entries |
| CompositeLoops | 1024 | `composite_loops` sequence entries |
| Identifier length | 128 bytes | graph name, lane ids, component ids, edge ids, loop ids |
| Config nesting depth | 8 | graph/component config maps and sequences |
| Config scalar/serialized value | 4096 bytes | config scalar values and serialized nested values |
| Non-config string length | 4096 bytes | component types, endpoints, policy names, descriptors, and other YAML strings |

Limit errors are explicit `std::invalid_argument` messages that name the graph
path and limit, for example `runtime graph.components count exceeds limit 4096`.
Inputs must be valid UTF-8 text; invalid byte sequences fail before YAML parsing
with a bounded diagnostic instead of being treated as opaque binary.

The public `GraphInputLimits` struct exposes these defaults through
`default_graph_input_limits()`. Embedders can call `load_graph_text(text,
limits)` or `load_graph_file(path, limits)` to tighten limits for tests,
editors, or CI. File loading reads incrementally and fails once the configured
byte limit would be exceeded, so oversized files do not need to be materialized
fully before rejection.

## CLI overrides

Graph-reading CLI commands accept the same local parser-limit overrides:

```bash
./build/topoexec graph validate examples/minimal.yaml \
  --max-graph-input-bytes 65536 \
  --max-components 512 \
  --max-edges 1024 \
  --format json
```

Overrides are intentionally per-invocation; TopoExec does not persist them in
project state. The defaults remain safe for normal examples, while CI/editor
jobs can lower them to protect interactive workflows. `topoexec doctor --format
json` reports the default `graph_input_limits` contract.

## Numeric validation

The parser uses bounded integer conversions and semantic validation rejects
negative capacities, durations, trigger windows, and loop budgets where they are
not meaningful. Capacity and `max_inflight` errors are reported during graph
validation before runtime execution.

## Fuzz and malformed input

`fuzz_graph_input_smoke` runs a deterministic corpus of malformed, cyclic,
partial, nested, invalid-UTF-8, oversized, and mutated YAML inputs. The optional
`fuzz_graph_inputs` target can run the checked-in corpus in standalone mode or
as a libFuzzer target. Both fail on timeouts, crash-like exit codes, or
sanitizer/crash markers. Run the local gate with `./scripts/goal_check.sh fuzz`;
see [Coverage-guided fuzzing](fuzzing.md) for libFuzzer and regression-corpus
commands.

## Trust boundary

TopoExec YAML declares graph structure. It is not an untrusted code execution
engine: the loader parses data, validates semantics, and returns a `GraphSpec`;
component instantiation still comes from an embedder-provided registry. Dynamic
plugin loading remains a separate default-off trusted-native preview; plugin
discovery, shared-library paths, signing/allowlists, sandboxing, and ABI
compatibility stay outside graph YAML and must remain explicit before any
broader plugin ecosystem can be enabled by default.

## CLI output paths

Current CLI commands write to stdout/stderr only. Users can redirect output with
their shell, but the CLI does not accept arbitrary output-file paths today. If a
future command writes files, it must validate path traversal and overwrite policy
before writing.

## Blocking overflow policy

`overflow: block` is treated as a bounded policy, not as permission for hidden
unbounded recursion. Low-level channel calls return `would block producer` rather
than blocking forever in the single-thread path. Runtime async admission treats
`block` like a rejection when `max_inflight` is exhausted, so rejected completions
are not committed. Graph authors should prefer explicit `drop_*`, `reject`, or
`fail_fast` unless a future scheduler lane documents safe blocking behavior.

## Error-message hygiene

Diagnostics should report graph paths, ids, policy names, and limits. They should
not dump environment variables, credentials, or unrelated filesystem state.
`topoexec doctor --format json` reports only tool/schema/example/benchmark
locations needed for local troubleshooting.
