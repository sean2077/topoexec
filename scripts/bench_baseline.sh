#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${TOPOEXEC_BUILD_DIR:-"$ROOT_DIR/build"}"
BUILD_TYPE="${TOPOEXEC_BUILD_TYPE:-RelWithDebInfo}"
OUTPUT="${TOPOEXEC_BENCH_BASELINE_OUTPUT:-"$ROOT_DIR/benchmarks/local-baseline.json"}"

cd "$ROOT_DIR"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" --target topoexec_cli topoexec_bench_task_executor -j

args=(
  --topoexec "$BUILD_DIR/topoexec"
  --source-dir "$ROOT_DIR"
  --task-executor-bench "$BUILD_DIR/topoexec_bench_task_executor"
  --output "$OUTPUT"
)

if [[ -n "${TOPOEXEC_BENCH_RUNS:-}" ]]; then
  args+=(--runs "$TOPOEXEC_BENCH_RUNS")
fi
if [[ -n "${TOPOEXEC_BENCH_STEPS:-}" ]]; then
  args+=(--steps "$TOPOEXEC_BENCH_STEPS")
fi
if [[ -n "${TOPOEXEC_BENCH_TASKS:-}" ]]; then
  args+=(--task-executor-tasks "$TOPOEXEC_BENCH_TASKS")
fi
if [[ -n "${TOPOEXEC_BENCH_BASELINE_IN:-}" ]]; then
  args+=(--baseline-in "$TOPOEXEC_BENCH_BASELINE_IN")
fi
if [[ -n "${TOPOEXEC_BENCH_THRESHOLD_PERCENT:-}" ]]; then
  args+=(--threshold-percent "$TOPOEXEC_BENCH_THRESHOLD_PERCENT")
fi

python3 scripts/bench_baseline.py "${args[@]}"
