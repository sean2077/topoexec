#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${TOPOEXEC_SANITIZER_MODE:-address-undefined}"
BUILD_TYPE="${TOPOEXEC_BUILD_TYPE:-Debug}"

case "$MODE" in
  address-undefined|asan-ubsan)
    BUILD_DIR="${TOPOEXEC_BUILD_DIR:-$ROOT_DIR/build-asan-ubsan}"
    SANITIZER_ARGS=(-DTOPOEXEC_ENABLE_ASAN=ON -DTOPOEXEC_ENABLE_UBSAN=ON)
    ;;
  thread|tsan)
    BUILD_DIR="${TOPOEXEC_BUILD_DIR:-$ROOT_DIR/build-tsan}"
    SANITIZER_ARGS=(-DTOPOEXEC_ENABLE_TSAN=ON)
    ;;
  *)
    echo "unknown TOPOEXEC_SANITIZER_MODE: $MODE" >&2
    echo "expected address-undefined or thread" >&2
    exit 2
    ;;
esac

cd "$ROOT_DIR"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" "${SANITIZER_ARGS[@]}"
cmake --build "$BUILD_DIR" -j
ctest --test-dir "$BUILD_DIR" --output-on-failure
