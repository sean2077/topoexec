# Coverage-Guided Fuzzing

TopoExec keeps the default test gate deterministic, but graph input fuzzing now
has an optional target that can run with libFuzzer or as a standalone corpus
replay executable.

## What is covered

`fuzz_graph_inputs` feeds arbitrary bytes through the YAML graph loader,
defensive parser limits, graph structure validator/compiler, plan JSON renderer,
Mermaid renderer, and lifecycle-order helper when a graph validates. This covers
the current parser/compiler path for:

- YAML loader failures and UTF-8 rejection;
- schema-v1 shape and strict-field checks;
- endpoint and trigger-policy parsing through graph validation;
- immediate-cycle and CompositeLoop compile paths.

It does not instantiate components or run external adapters.

## Local smoke

Use the portable standalone engine when libFuzzer is unavailable:

```bash
TOPOEXEC_FUZZER_ENGINE=STANDALONE ./scripts/fuzz_smoke.sh
```

This configures `build-fuzz`, builds `fuzz_graph_inputs`, and replays the seed
corpus under `tests/fuzz/corpus/graph_inputs` through CTest.

When Clang/libFuzzer is available, run a coverage-guided smoke:

```bash
CXX=clang++ TOPOEXEC_FUZZER_ENGINE=LIBFUZZER ./scripts/fuzz_smoke.sh
```

The CMake option is explicit and off by default:

```bash
cmake -S . -B build-fuzz \
  -DTOPOEXEC_BUILD_FUZZERS=ON \
  -DTOPOEXEC_FUZZER_ENGINE=LIBFUZZER
cmake --build build-fuzz --target fuzz_graph_inputs -j
ctest --test-dir build-fuzz --output-on-failure -R fuzz_graph_input_target_smoke
```

`TOPOEXEC_FUZZER_ENGINE=AUTO` selects libFuzzer on Clang and standalone replay on
other compilers.

## Corpus regressions

If a crash is found, minimize it with the fuzzer toolchain, then commit the
smallest reproducer under `tests/fuzz/corpus/graph_inputs/` with a descriptive
name such as `crash_2026_05_06_invalid_anchor.yaml`. Corpus files may be invalid
YAML or invalid UTF-8; write binary seeds with a script when needed.

Keep seeds small, deterministic, and dependency-free. Do not add private data,
credentials, host-specific paths, or large generated corpora to the repository.

## CI policy

Default CI remains `scripts/agent_check.sh`. The libFuzzer smoke is a separate
Clang job so fuzzer instrumentation does not change the normal package/runtime
builds. Longer fuzz campaigns should run outside the default PR gate and commit
only minimized regression seeds.
