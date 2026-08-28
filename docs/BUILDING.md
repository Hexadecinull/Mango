# Building Mango

Four things get built here: the shared Kotlin engine, the desktop app,
the Android native translator (which gets packaged into the module,
alongside the WebUI, which needs no build step at all), and the early
standalone Linux support. This doc hasn't been run through a real CI
pipeline yet as this repo comes together, so if a step is wrong for your
toolchain version, please open an issue or a PR fixing it.

## Prerequisites

- JDK 21 (JDK 17 also works for the Gradle/Kotlin/Compose side, but the
  version catalog assumes a recent JDK; adjust `gradle.properties` if not).
- Android NDK (a recent LTS version) and CMake, for `native/`. You don't
  need the full Android SDK/platform tools for anything in this repo
  anymore, just the NDK, since there's no Android app module; Android
  Studio's SDK Manager can still install just the NDK if that's easiest.
- `adb`, useful for testing manually (`adb install`, `adb shell`).

Exact Gradle/Kotlin/Compose Multiplatform versions are pinned in
`gradle/libs.versions.toml`, with a note on when they were last verified.
This kind of tooling moves fast; if a build fails on version resolution,
check that file's comment first.

## Building `:core`

Pure Kotlin/JVM, no Android or native dependencies. This is the easiest
piece to build and test in isolation:

```
./gradlew :core:build
./gradlew :core:test
```

## The WebUI

No build step. `module/webroot/` is plain HTML/CSS/JS; edit it, then
package the module (below) and reflash, or for faster iteration on
layout/logic that doesn't need real root, serve it locally and stub out
the bridge in the browser console:

```
cd module/webroot
python3 -m http.server 8000
```

Then open `http://localhost:8000` and, in devtools, define a fake bridge
before the page's own script runs (paste this in the console, then reload
with it still defined, or add it as a browser snippet that runs on load):

```js
window.ksu = {
  exec: async (cmd) => ({ errno: 0, stdout: '' }), // fill in fake output as needed
  toast: (msg) => console.log('[toast]', msg),
};
```

This won't catch everything (real device quirks, real `pm`/`unzip` output,
and APatch's `exec()` returning a bare string instead of the object shape
above, see `docs/ARCHITECTURE.md`), but it's enough to iterate on the UI
without a device in the loop. Real testing still needs an actual rooted
device; see `docs/USAGE.md`.

`module/webroot/app.test.js` covers the pure-logic pieces (result
normalization, compatibility checks) without a browser or a device at
all: `node --test module/webroot/app.test.js`.

## Building the desktop app (`:desktop`)

Windows/macOS only, a small helper for inspecting an APK from a PC before
pushing it or the module to a device; not where Linux support lives (see
below).

```
./gradlew :desktop:run
```

To produce a native installer/distributable for your OS:

```
./gradlew :desktop:packageDistributionForCurrentOS
```

(This task name comes from the Compose Multiplatform Gradle plugin; check
`org.jetbrains.compose` docs if it's changed by the time you read this.)

## Building the native translator (`native/`)

This is a standalone CMake project, deliberately not wired into the
Gradle build, because it's cross-compiled with the NDK toolchain rather
than a JVM toolchain, and because you generally want to build it once and
reuse the prebuilt `.so`, not rebuild it on every app build.

```
cd native
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24
cmake --build build
```

The resulting `native/build/libmango_translator.so` is what
`scripts/package_module.sh` bundles into the module, and what
`scripts/build_native.sh` also copies into
`desktop/src/main/resources/translator/arm64-v8a/` (the desktop app
bundles it purely to prepare module builds, it never executes it itself).

There's a helper script for the cmake invocation in
`scripts/build_native.sh`. See `native/README.md` for what's actually
implemented right now versus stubbed; check `docs/ARCHITECTURE.md` for why
this is the hard, unfinished part of the project.

## Building standalone Linux support (`linux/`)

Another standalone CMake project, like `native/`, but built with your
host's own toolchain, no NDK involved:

```
cmake -S linux -B linux/build
cmake --build linux/build
./linux/build/mango_linux_selfcheck
```

or without CMake at all, same idea as `native/README.md`'s alternative:

```
cc -std=c11 -Wall -Wextra -Werror -Inative/include \
  native/src/decoder.c native/src/interp.c linux/src/selfcheck.c \
  -o /tmp/mango_linux_selfcheck
```

See `linux/README.md` before expecting much: `mango_linux_selfcheck`
really runs (it proves the interpreter core builds outside Android/NDK),
but `mango_linux_loader` is currently a stub, not a working translator.

## Building the module (including the WebUI)

The module lives in `module/`, WebUI included. It's plain files, not a
Gradle project:

```
scripts/package_module.sh
```

This copies `libmango_translator.so` (built above) into
`module/system/lib64/`, zips up `module/` with the correct structure (no
parent folder inside the zip, see `module/README.md`), and writes the
result to `build/mango-module.zip`. Flash it in your root manager app, or
push it with:

```
adb push build/mango-module.zip /sdcard/Download/
```

## Running tests

```
./gradlew :core:test
```

`native/tests/` has its own small test harness; see `native/README.md`
for how to build and run it, since it isn't part of the Gradle build.
`linux/`'s `mango_linux_selfcheck` (above) is a much smaller sibling of
that same harness, for the standalone build.

## CI

`.github/workflows/build.yml` builds `:core`, the (now Windows) desktop
app, the native Android translator, the packaged module end to end, and
`linux/`'s standalone build. The native, module, and linux jobs are all
marked `continue-on-error`, since each is still an early proof of concept
and none should block an unrelated PR; see the workflow file for exactly
what's wired up.

Every run also uploads its build output as a workflow artifact, so you
don't need to build locally just to try something out: the `core` jar,
its test report, a runnable (unpackaged) Windows build of the desktop
app, `libmango_translator.so` if the native job succeeded, the flashable
`mango-module.zip` if the module job succeeded, and the `linux/` binaries.
Find them on the specific workflow run's summary page, under "Artifacts".
