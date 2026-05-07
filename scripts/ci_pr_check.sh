#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${TOPOEXEC_BUILD_DIR:-"$ROOT_DIR/build-pr-fast"}"
BUILD_TYPE="${TOPOEXEC_BUILD_TYPE:-RelWithDebInfo}"
TEST_REGEX="${TOPOEXEC_PR_TEST_REGEX:-cli_golden_outputs|schema_v1_contract_smoke|cmake_package_runtime_smoke|docs_command_smoke}"

cd "$ROOT_DIR"

git diff --check
python3 scripts/check_commit_messages.py
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" --target topoexec_format_check
cmake --build "$BUILD_DIR" -j --target \
  topoexec_cli \
  topoexec_app_cpp_builder_minimal \
  topoexec_app_low_latency_sensor_pipeline \
  topoexec_app_control_loop_with_state \
  topoexec_app_async_request_response \
  topoexec_app_composite_solver \
  topoexec_app_payload_pool_pipeline \
  topoexec_app_robot_cell_pilot
ctest --test-dir "$BUILD_DIR" --output-on-failure -R "$TEST_REGEX"
