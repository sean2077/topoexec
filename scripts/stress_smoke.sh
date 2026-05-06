#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${TOPOEXEC_BUILD_DIR:-"$ROOT_DIR/build"}"
BUILD_TYPE="${TOPOEXEC_BUILD_TYPE:-RelWithDebInfo}"
PROFILE="${TOPOEXEC_STRESS_PROFILE:-smoke}"

cd "$ROOT_DIR"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" --target topoexec_cli -j

args=(
  --topoexec "$BUILD_DIR/topoexec"
  --profile "$PROFILE"
)

if [[ -n "${TOPOEXEC_STRESS_SCALE:-}" ]]; then
  args+=(--scale "$TOPOEXEC_STRESS_SCALE")
fi
if [[ -n "${TOPOEXEC_STRESS_STEPS:-}" ]]; then
  args+=(--steps "$TOPOEXEC_STRESS_STEPS")
fi
if [[ -n "${TOPOEXEC_STRESS_DURATION_SECONDS:-}" ]]; then
  args+=(--duration-seconds "$TOPOEXEC_STRESS_DURATION_SECONDS")
fi
if [[ -n "${TOPOEXEC_STRESS_MAX_ITERATIONS:-}" ]]; then
  args+=(--max-iterations "$TOPOEXEC_STRESS_MAX_ITERATIONS")
fi
if [[ -n "${TOPOEXEC_STRESS_TIMEOUT_SECONDS:-}" ]]; then
  args+=(--timeout-seconds "$TOPOEXEC_STRESS_TIMEOUT_SECONDS")
fi

python3 tests/stress/check_stress_workloads.py "${args[@]}"
