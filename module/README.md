# module/

A Magisk/KernelSU/KernelSU-Next/APatch module. Not a Gradle project, just
files; see `docs/BUILDING.md` for how it gets packaged and
`docs/ARCHITECTURE.md` ("Option A") for why this is the primary way Mango
integrates with the system, rather than patching each APK individually.

## Layout

```
module.prop      metadata (id, name, version)
system.prop      sets ro.dalvik.vm.native.bridge, loaded via resetprop
post-fs-data.sh  early boot check, logs which root solution it's running on
service.sh       placeholder for later (cache warm-up, etc.)
customize.sh     install-time messages, arch check
uninstall.sh     placeholder, nothing persistent to clean up yet
system/lib64/    libmango_translator.so goes here (copied in by
                 scripts/package_module.sh, not committed to git)
webroot/         the WebUI: pick an app, check it, install it. See below.
```

## The WebUI

`webroot/index.html` is Mango's actual on-device UI, opened from within
your root manager app (Magisk, KernelSU, KernelSU-Next, or APatch), not a
separate app. This replaced an earlier plan to ship a standalone Android
app; see `docs/ARCHITECTURE.md` for why. It's plain HTML/CSS/JS, no build
step, no npm, nothing to compile: edit the files, reflash or resync the
module, reload the page.

It talks to root through the `ksu` object your manager's WebView injects
(`ksu.exec(cmd)` to run shell commands, `ksu.toast(msg)` for feedback).
This convention comes from KernelSU and was adopted by Magisk in v28.1+;
it hasn't been confirmed identical on every manager yet, see
`docs/ARCHITECTURE.md` and `docs/SECURITY.md`.

## Multi-root-manager support

Magisk, KernelSU, KernelSU-Next, and APatch all install modules from
`/data/adb/modules/` with the same `module.prop`/`post-fs-data.sh`/
`service.sh`/`system.prop` behavior, so one module works across all four
without special-casing, with one caveat:

KernelSU and APatch need a separate metamodule (such as meta-overlayfs)
installed for `/system` mounting to work at all; Magisk has that built in.
Without one, `system.prop` and the scripts still run, but the translator
`.so` under `system/lib64/` won't actually get mounted. `customize.sh`
prints a note about this when it detects a KernelSU-family environment.

This has only been checked against the documented behavior of each
project, not tested across all four on real devices yet. If you hit a
compatibility issue on a specific root solution, please file it, see
`docs/CONTRIBUTING.md`.
