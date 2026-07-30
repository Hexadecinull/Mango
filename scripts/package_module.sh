#!/usr/bin/env bash
set -euo pipefail

# Packages module/ (including its WebUI) into a flashable zip. Needs
# libmango_translator.so to already exist, see scripts/build_native.sh.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SO_PATH="$ROOT_DIR/native/build/libmango_translator.so"

if [ ! -f "$SO_PATH" ]; then
  echo "Run scripts/build_native.sh first, no translator .so found." >&2
  exit 1
fi

mkdir -p "$ROOT_DIR/module/system/lib64"
cp "$SO_PATH" "$ROOT_DIR/module/system/lib64/"

mkdir -p "$ROOT_DIR/build"
OUT="$ROOT_DIR/build/mango-module.zip"
rm -f "$OUT"

# Zip module/'s contents at the top level, not the module/ folder itself.
(cd "$ROOT_DIR/module" && zip -r -X "$OUT" . -x "README.md")

echo "Wrote $OUT"
