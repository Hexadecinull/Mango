#!/usr/bin/env bash
set -euo pipefail

# Cross-compiles native/ with the Android NDK. The result
# (native/build/libmango_translator.so) is what scripts/package_module.sh
# and desktop/'s resources both read from. See docs/BUILDING.md.

: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to your NDK install first.}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/native/build"

cmake -S "$ROOT_DIR/native" -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24

cmake --build "$BUILD_DIR"

SO_PATH="$BUILD_DIR/libmango_translator.so"
if [ ! -f "$SO_PATH" ]; then
  echo "Build didn't produce $SO_PATH, something's wrong." >&2
  exit 1
fi

mkdir -p "$ROOT_DIR/desktop/src/main/resources/translator/arm64-v8a"
cp "$SO_PATH" "$ROOT_DIR/desktop/src/main/resources/translator/arm64-v8a/"

echo "Built $SO_PATH and copied it into desktop/."
echo "Run scripts/package_module.sh next to build the flashable module zip."
