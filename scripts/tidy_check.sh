#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${TOPOEXEC_BUILD_DIR:-"$ROOT_DIR/build"}"
BUILD_TYPE="${TOPOEXEC_BUILD_TYPE:-RelWithDebInfo}"
CLANG_TIDY_BIN="${CLANG_TIDY:-}"
TIMEOUT_BIN="${TIMEOUT:-timeout}"
TIMEOUT_SECONDS="${TOPOEXEC_TIDY_TIMEOUT_SECONDS:-240}"
JOBS="${TOPOEXEC_TIDY_JOBS:-4}"

if [[ -z "$CLANG_TIDY_BIN" ]]; then
  for candidate in clang-tidy-22 clang-tidy; do
    if command -v "$candidate" >/dev/null 2>&1; then
      CLANG_TIDY_BIN="$candidate"
      break
    fi
  done
fi

if ! command -v "$CLANG_TIDY_BIN" >/dev/null 2>&1; then
  echo "clang-tidy not found; install clang-tidy-22 or set CLANG_TIDY" >&2
  exit 127
fi
if ! command -v "$TIMEOUT_BIN" >/dev/null 2>&1; then
  echo "timeout not found; install coreutils or set TIMEOUT" >&2
  exit 127
fi

cd "$ROOT_DIR"

if [[ ! -f "$BUILD_DIR/compile_commands.json" ]]; then
  cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
fi

"$CLANG_TIDY_BIN" --verify-config >/dev/null

mapfile -t FILES < <(
  python3 - "$ROOT_DIR" "$BUILD_DIR/compile_commands.json" "$@" <<'PY'
from __future__ import annotations

import json
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()
compile_commands = Path(sys.argv[2])
selected = {Path(arg).resolve() for arg in sys.argv[3:]}

with compile_commands.open(encoding="utf-8") as handle:
    entries = json.load(handle)

known_files = sorted({Path(entry["file"]).resolve() for entry in entries})
if selected:
    files = [path for path in known_files if path in selected]
else:
    files = known_files

for path in files:
    try:
        relative = path.relative_to(root)
    except ValueError:
        continue
    if (
        ".git" in relative.parts
        or "_deps" in relative.parts
        or (relative.parts and relative.parts[0].startswith("build"))
        or relative.parts[:1] == ("tests",)
    ):
        continue
    print(path)
PY
)

if [[ ${#FILES[@]} -eq 0 ]]; then
  echo "No clang-tidy translation units selected."
  exit 0
fi

export BUILD_DIR CLANG_TIDY_BIN TIMEOUT_BIN TIMEOUT_SECONDS
printf '%s\0' "${FILES[@]}" |
  xargs -0 -r -n1 -P "$JOBS" bash -c '
    "$TIMEOUT_BIN" "$TIMEOUT_SECONDS" "$CLANG_TIDY_BIN" -p "$BUILD_DIR" "$1" --quiet
    status=$?
    if [[ $status -ne 0 ]]; then
      echo "clang-tidy failed for $1" >&2
    fi
    exit "$status"
  ' _
