# Mango

A free, open-source alternative to [Tango](https://www.amanieusystems.com/):
a system that lets 32-bit-only (`armeabi-v7a`) Android apps run on 64-bit-only
(`arm64-v8a`) devices, root required.

Mango has no affiliation with Amanieu Systems and contains none of their
code. It's an independent, from-scratch project.

## Why this exists

Some Android apps never got a 64-bit build: abandoned by their developer,
or just never updated. That was fine as long as every 64-bit chip could
still run 32-bit ARM code in hardware. Newer chips (Snapdragon 8 Gen 3 and
up) dropped that hardware support entirely, so those apps don't run slowly
on new phones, they don't run at all. Mango exists to fix that, for free,
in the open. See `docs/TERMS.md` for exactly what this is (and isn't)
meant for.

## Status

Early and honest about it. This is a passion project, not a finished
product; see `docs/ARCHITECTURE.md` for what's actually implemented today
versus what's still a design sketch, and `docs/CONTRIBUTING.md` if you
want to help push it forward.

## How it works, briefly

Rather than patching every APK individually, Mango implements Android's
own [Native Bridge](https://android.googlesource.com/platform/art/) interface
(the same mechanism Google's NDK Translation and Intel's Houdini use to run
ARM apps on x86 Chromebooks) as a Magisk / KernelSU / KernelSU-Next / APatch
module. Once installed, 32-bit-only apps become launchable device-wide.

The on-device UI is a WebUI built into the module itself (opened from your
root manager app, no separate app to install) for picking an app, checking
it, and installing it. There's also a small Windows/macOS desktop app for
inspecting an APK from a PC.

Android isn't the only place this problem shows up: a 64-bit-only Linux
system can't run a 32-bit ARM binary either, for the same hardware reason.
`linux/` is an early, standalone (no root, no Android) take on that, see
its own README for what's real versus a proof of concept so far.

Full explanation, including why a module + WebUI rather than a standalone
app or per-APK patching, in `docs/ARCHITECTURE.md`.

## Repository layout

```
core/     shared Kotlin: APK inspection, compatibility checks (used by desktop/)
desktop/  Windows/macOS APK-inspection helper app (Compose Desktop)
native/   the ARM32 -> ARM64 translator for Android (C, CMake)
linux/    early standalone ARM32 -> ARM64 support for Linux itself (C, CMake)
module/   the Magisk/KernelSU/KernelSU-Next/APatch module, including the WebUI
docs/     everything below
scripts/  build helper scripts
```

## Docs

- [`docs/USAGE.md`](docs/USAGE.md) — how to use it
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — how it works, and what's real vs. planned
- [`docs/BUILDING.md`](docs/BUILDING.md) — build instructions
- [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md) — how to help
- [`docs/TERMS.md`](docs/TERMS.md) · [`docs/PRIVACY.md`](docs/PRIVACY.md) · [`docs/SECURITY.md`](docs/SECURITY.md) · [`docs/CODE_OF_CONDUCT.md`](docs/CODE_OF_CONDUCT.md)

## License

GPL-3.0, see [`LICENSE`](LICENSE). Free as in freedom, no ads, nothing to
buy, ever.
