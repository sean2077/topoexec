#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${TOPOEXEC_FUZZ_BUILD_DIR:-build-fuzz}"
ENGINE="${TOPOEXEC_FUZZER_ENGINE:-AUTO}"
BUILD_TYPE="${TOPOEXEC_BUILD_TYPE:-Debug}"

cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DTOPOEXEC_BUILD_FUZZERS=ON \
  -DTOPOEXEC_FUZZER_ENGINE="$ENGINE"
cmake --build "$BUILD_DIR" --target fuzz_graph_inputs -j
ctest --test-dir "$BUILD_DIR" --output-on-failure -R fuzz_graph_input_target_smoke
