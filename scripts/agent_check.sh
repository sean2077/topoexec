#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${TOPOEXEC_BUILD_DIR:-"$ROOT_DIR/build"}"
BUILD_TYPE="${TOPOEXEC_BUILD_TYPE:-RelWithDebInfo}"

cd "$ROOT_DIR"

git diff --check
python3 scripts/check_commit_messages.py
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" -j
ctest --test-dir "$BUILD_DIR" --output-on-failure
