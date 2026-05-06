#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLANG_FORMAT_BIN="${CLANG_FORMAT:-}"
if [[ -z "$CLANG_FORMAT_BIN" ]]; then
  for candidate in clang-format-22 clang-format; do
    if command -v "$candidate" >/dev/null 2>&1; then
      CLANG_FORMAT_BIN="$candidate"
      break
    fi
  done
fi

if ! command -v "$CLANG_FORMAT_BIN" >/dev/null 2>&1; then
  echo "clang-format not found; install clang-format-22 or set CLANG_FORMAT" >&2
  exit 127
fi

mapfile -t FILES < <(
  git -C "$ROOT_DIR" ls-files \
    '*.cc' '*.cpp' '*.cxx' '*.h' '*.hh' '*.hpp' '*.hxx'
)

if [[ ${#FILES[@]} -eq 0 ]]; then
  echo "No C++ files tracked by git."
  exit 0
fi

cd "$ROOT_DIR"
"$CLANG_FORMAT_BIN" --dry-run --Werror "${FILES[@]}"
