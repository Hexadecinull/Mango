# Building Mango

Three things get built here: the shared Kotlin engine, the desktop app,
and the native translator (which gets packaged into the module, alongside
the WebUI, which needs no build step at all). This doc hasn't been run
through a real CI pipeline yet as this repo comes together, so if a step
is wrong for your toolchain version, please open an issue or a PR fixing
it.

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

This won't catch everything (real device quirks, real `pm`/`unzip` output),
but it's enough to iterate on the UI without a device in the loop. Real
testing still needs an actual rooted device; see `docs/USAGE.md`.

## Building the desktop app (`:desktop`)

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

## CI

`.github/workflows/build.yml` runs the Gradle builds above (not the
native/module packaging yet, since that needs a real NDK install in CI;
see the workflow file for what's actually wired up today versus what's
still a TODO).

Every run also uploads its build output as a workflow artifact, so you
don't need to build locally just to try something out: the `core` jar,
its test report, a runnable (unpackaged) Linux build of the desktop app,
and, if the native job succeeded, `libmango_translator.so`. Find them on
the specific workflow run's summary page, under "Artifacts".
