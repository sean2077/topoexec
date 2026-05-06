# Testing Strategy

TopoExec uses a small test pyramid that favors semantic runtime coverage over
large fixtures. The default local gate remains:

```bash
./scripts/agent_check.sh
```

This configures, builds, and runs all default CTest tests.

## Test layers

| Layer | Evidence | Command |
| --- | --- | --- |
| Unit | `test_common`, `test_graph`, `test_channel`, `test_state`, `test_runtime` | `ctest --test-dir build --output-on-failure -R 'test_'` |
| Semantic runtime | graph compiler, edge visibility, trigger, scheduler, async, CompositeLoop, state/config tests | `ctest --test-dir build --output-on-failure -R 'test_graph|test_runtime|test_state'` |
| Golden CLI | normalized plan, metrics, trace, and render outputs | `./scripts/goal_check.sh golden` |
| Schema | strict schema contract plus schema/semantic CLI split | `./scripts/goal_check.sh schema` |
| Docs | executable `topoexec-doc-test` tutorial/CLI markers | `./scripts/goal_check.sh docs` |
| Fuzz smoke | deterministic malformed, invalid-UTF-8, oversized, and parser-limit graph input corpus | `./scripts/goal_check.sh fuzz` |
| Package | install/export/downstream `find_package(topoexec)` runtime-only smoke | `./scripts/goal_check.sh package` |
| Sanitizers | ASAN+UBSAN full CTest; TSAN non-blocking CI | `./scripts/goal_check.sh sanitizer` |

## Fuzz smoke

`tests/fuzz/fuzz_graph_inputs.py` generates a deterministic corpus of malformed,
partial, cyclic, nested, invalid-UTF-8, oversized, and mutated YAML graph inputs.
The smoke does not claim coverage-guided fuzzing. It proves the CLI
parser/compiler path rejects hostile inputs without timeouts, crash-like exits,
or sanitizer/crash markers.

Run it directly with:

```bash
ctest --test-dir build --output-on-failure -R fuzz_graph_input_smoke
```

## Sanitizer gates

CMake exposes explicit sanitizer options for GCC/Clang builds:

```bash
cmake -S . -B build-asan-ubsan -DCMAKE_BUILD_TYPE=Debug \
  -DTOPOEXEC_ENABLE_ASAN=ON \
  -DTOPOEXEC_ENABLE_UBSAN=ON
cmake --build build-asan-ubsan -j
ctest --test-dir build-asan-ubsan --output-on-failure
```

The wrapper is shorter:

```bash
TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/sanitizer_check.sh
TOPOEXEC_SANITIZER_MODE=thread ./scripts/sanitizer_check.sh
```

`address-undefined` runs ASAN+UBSAN together. `thread` runs TSAN alone because
TSAN cannot be combined with ASAN/UBSAN in this build. The package smoke passes
sanitizer link flags to its downstream runtime-only app so sanitizer builds still
verify install/export behavior.

## CI policy

- GCC and Clang Debug/RelWithDebInfo matrix jobs run the default gate.
- ASAN+UBSAN is a blocking CI job.
- TSAN remains non-blocking until the runtime concurrency surface is mature
  enough to make it a release blocker.

## Adding tests

When changing runtime semantics, add or update the smallest semantic test that
proves the claim, then update docs/goldens only if public behavior intentionally
changed. New docs commands should include a `topoexec-doc-test` marker so
`docs_command_smoke` keeps the prose executable.
