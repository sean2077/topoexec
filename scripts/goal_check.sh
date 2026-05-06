#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/goal_check.sh [all|quick|golden|schema|package|docs|fuzz|stress|bench|policy|adapters|ffi|python|plugins|release|sanitizer|format|tidy|debug]

Goal-specific validation dispatcher for TopoExec agents.
- all:    required repository gate (scripts/agent_check.sh)
- quick:  configure/build plus focused golden/schema checks
- golden: normalized CLI golden output checks
- schema: schema v1 contract smoke
- package: install/export downstream smoke, runtime-only option smoke, CPack, and package draft checks
- docs:   executable docs command smoke plus docs map contract
- fuzz:   deterministic parser/compiler fuzz smoke plus optional fuzzer target corpus replay
- stress: bounded runtime stress graph smoke plus task-executor overload stress
- bench:  benchmark output-contract smoke plus local baseline generation without thresholds
- policy: architecture/dependency policy smokes
- adapters: optional adapter-preview target/package smokes
- ffi: optional C API/FFI preview target/package smoke
- python: optional CLI-backed Python automation preview smoke
- plugins: optional trusted-native dynamic plugin loader preview smoke
- release: release automation dry-run smoke
- sanitizer: ASAN+UBSAN Debug build and full CTest
- format: clang-format check target
- tidy: clang-tidy check target
- debug:  Debug build + CTest in build-debug-gcc

Set TOPOEXEC_BUILD_DIR to override the default build directory.
EOF
}

MODE="${1:-all}"
BUILD_DIR="${TOPOEXEC_BUILD_DIR:-build}"

configure_build() {
  cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo
  cmake --build "$BUILD_DIR" -j
}

case "$MODE" in
  all)
    ./scripts/agent_check.sh
    ;;
  quick)
    configure_build
    ctest --test-dir "$BUILD_DIR" --output-on-failure -R 'cli_golden_outputs|schema_v1_contract_smoke'
    ;;
  golden)
    configure_build
    ctest --test-dir "$BUILD_DIR" --output-on-failure -R cli_golden_outputs
    ;;
  schema)
    configure_build
    ctest --test-dir "$BUILD_DIR" --output-on-failure -R 'schema_v1_contract_smoke|cli_validate_schema_only_minimal|cli_validate_semantic_minimal'
    ;;
  package)
    configure_build
    ctest --test-dir "$BUILD_DIR" --output-on-failure -R 'cmake_package_runtime_smoke|cmake_runtime_only_options_smoke|cmake_cpack_smoke|package_draft_smoke'
    ;;
  docs)
    configure_build
    ctest --test-dir "$BUILD_DIR" --output-on-failure -R docs_command_smoke
    ;;
  fuzz)
    configure_build
    ctest --test-dir "$BUILD_DIR" --output-on-failure -R fuzz_graph_input_smoke
    TOPOEXEC_FUZZER_ENGINE="${TOPOEXEC_FUZZER_ENGINE:-STANDALONE}" ./scripts/fuzz_smoke.sh
    ;;
  stress)
    configure_build
    ctest --test-dir "$BUILD_DIR" --output-on-failure -R test_stress
    ./scripts/stress_smoke.sh
    ;;
  bench)
    configure_build
    ctest --test-dir "$BUILD_DIR" --output-on-failure -R 'cli_bench_.*|bench_.*'
    TOPOEXEC_BENCH_RUNS="${TOPOEXEC_BENCH_RUNS:-2}" \
      TOPOEXEC_BENCH_STEPS="${TOPOEXEC_BENCH_STEPS:-2}" \
      TOPOEXEC_BENCH_TASKS="${TOPOEXEC_BENCH_TASKS:-8}" \
      TOPOEXEC_BENCH_BASELINE_OUTPUT="${TOPOEXEC_BENCH_BASELINE_OUTPUT:-/tmp/topoexec-bench-baseline.json}" \
      ./scripts/bench_baseline.sh
    ;;
  policy)
    configure_build
    ctest --test-dir "$BUILD_DIR" --output-on-failure -R 'policy_.*'
    ;;
  adapters)
    OTEL_BUILD_DIR="${BUILD_DIR}-otel"
    cmake -S . -B "$OTEL_BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DTOPOEXEC_BUILD_OTEL_ADAPTER=ON \
      -DTOPOEXEC_BUILD_PROMETHEUS_ADAPTER=ON \
      -DTOPOEXEC_BUILD_ROS2_ADAPTER=ON
    cmake --build "$OTEL_BUILD_DIR" -j
    ctest --test-dir "$OTEL_BUILD_DIR" --output-on-failure \
      -R 'test_adapter_sdk|test_otel_adapter|test_prometheus_adapter|test_ros2_adapter|cmake_otel_adapter_options_smoke|cmake_prometheus_adapter_options_smoke|cmake_ros2_adapter_options_smoke|policy_.*'
    ;;
  ffi)
    FFI_BUILD_DIR="${BUILD_DIR}-ffi"
    cmake -S . -B "$FFI_BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTOPOEXEC_BUILD_C_API=ON
    cmake --build "$FFI_BUILD_DIR" -j
    ctest --test-dir "$FFI_BUILD_DIR" --output-on-failure \
      -R 'test_c_api|cmake_c_api_options_smoke|policy_.*'
    ;;
  python)
    PYTHON_BUILD_DIR="${BUILD_DIR}-python-preview"
    cmake -S . -B "$PYTHON_BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTOPOEXEC_BUILD_PYTHON_PREVIEW=ON
    cmake --build "$PYTHON_BUILD_DIR" -j
    ctest --test-dir "$PYTHON_BUILD_DIR" --output-on-failure \
      -R 'python_preview_smoke|cmake_python_preview_options_smoke|policy_.*'
    ;;
  plugins)
    PLUGIN_BUILD_DIR="${BUILD_DIR}-plugins"
    cmake -S . -B "$PLUGIN_BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTOPOEXEC_BUILD_PLUGIN_LOADER=ON
    cmake --build "$PLUGIN_BUILD_DIR" -j
    ctest --test-dir "$PLUGIN_BUILD_DIR" --output-on-failure \
      -R 'test_plugin_loader|cmake_plugin_loader_options_smoke|policy_.*'
    ;;
  release)
    configure_build
    ctest --test-dir "$BUILD_DIR" --output-on-failure -R release_prepare_smoke
    ;;
  sanitizer)
    ./scripts/sanitizer_check.sh
    ;;
  format)
    cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build "$BUILD_DIR" --target topoexec_format_check
    ;;
  tidy)
    cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build "$BUILD_DIR" --target topoexec_tidy_check
    ;;
  debug)
    cmake -S . -B build-debug-gcc -DCMAKE_BUILD_TYPE=Debug
    cmake --build build-debug-gcc -j
    ctest --test-dir build-debug-gcc --output-on-failure
    ;;
  -h|--help|help)
    usage
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac
