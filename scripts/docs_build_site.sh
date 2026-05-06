#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${TOPOEXEC_DOCS_BUILD_DIR:-$ROOT_DIR/build-docs}"
SITE_DIR="${TOPOEXEC_SITE_DIR:-$ROOT_DIR/site}"
PYTHON_BIN="${PYTHON:-python3}"

cd "$ROOT_DIR"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DTOPOEXEC_BUILD_DOCS=ON
TARGET_HELP="$BUILD_DIR/docs-targets.txt"
cmake --build "$BUILD_DIR" --target help >"$TARGET_HELP"
if ! grep -q '^... topoexec_doxygen$' "$TARGET_HELP"; then
  echo "topoexec_doxygen target is unavailable; install Doxygen or disable the API copy step" >&2
  exit 1
fi
cmake --build "$BUILD_DIR" --target topoexec_doxygen
"$PYTHON_BIN" -m mkdocs build --strict --site-dir "$SITE_DIR"
mkdir -p "$SITE_DIR/api"
cp -a "$BUILD_DIR/docs/doxygen/html/." "$SITE_DIR/api/"
echo "wrote docs site: $SITE_DIR"
