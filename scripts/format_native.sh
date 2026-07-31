#!/usr/bin/env bash
set -euo pipefail

# Formats native/ in place per .clang-format. Run this before committing
# changes to native/; CI's clang-format --dry-run will fail otherwise.

: "${CLANG_FORMAT:=clang-format}"

if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
  echo "clang-format not found. Install it, or set CLANG_FORMAT=/path/to/clang-format." >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

shopt -s globstar nullglob
files=("$ROOT_DIR"/native/**/*.c "$ROOT_DIR"/native/**/*.h)

if [ ${#files[@]} -eq 0 ]; then
  echo "No native source files found."
  exit 0
fi

"$CLANG_FORMAT" -i "${files[@]}"
echo "Formatted ${#files[@]} file(s)."
