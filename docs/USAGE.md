# Using Mango

Mango is one thing to install: a Magisk/KernelSU/KernelSU-Next/APatch
module. It does the actual 32-bit-to-64-bit translation work, and it
carries its own WebUI (opened from your root manager app) for picking an
app, checking it, and installing it. There's no separate app to install
alongside it. See `docs/ARCHITECTURE.md` if you want the reasoning behind
that, including why this replaced an earlier plan for a standalone
Android app.

This describes the intended end-state workflow. Until `native/` actually
works (see its current status in `docs/ARCHITECTURE.md`), the WebUI can
walk through these steps and tell you whether a given APK *would* work,
without being able to make it run yet.

## Requirements

- A rooted Android device, arm64-v8a, with Magisk, KernelSU, KernelSU-Next,
  or APatch.
- Enough storage for the translator and any ahead-of-time translated
  caches Mango creates per app.

## Option 1: the WebUI

1. Grab the Mango module (a `.zip`) from the project's releases, and flash
   it in your root manager app, then reboot.
2. Open your root manager app, find Mango in the module list, and open its
   WebUI (this is a normal feature of Magisk v28.1+, KernelSU, KernelSU-Next,
   and APatch, not something extra to install).
3. The WebUI shows your device's ABIs and whether the module is active. If
   something looks off here, nothing past this point will work, see
   Troubleshooting below.
4. Pick an app, either from the list of installed apps or by typing a path
   to an APK file, and tap **Check compatibility**. The installed-apps
   list is checked through Android's own package manager and always
   works; a raw APK path needs `unzip` on-device, which isn't part of
   stock Android and not every device has it, the WebUI will tell you
   clearly if that's what went wrong.
5. Mango tells you whether the app already runs natively, needs the bridge
   (and can proceed), or is blocked (and why).
6. If it can proceed, tap **Install**. This runs `pm install -r` on the
   APK, no separate confirmation dialog, because the WebUI already has the
   root a normal app would have had to ask for separately.
7. Launch the app as usual. The first launch of a given app may be slower
   than later ones if Mango caches a translation ahead of time.
8. Turn on **Show command log (advanced)** any time you want to see the
   actual shell commands the WebUI is running, rather than trusting the
   summary.

## Option 2: Desktop app (Windows, macOS)

Useful if you'd rather prepare things on a PC and push the result to your
phone, or if you just want to inspect an APK without touching your device.
No root is needed on the desktop side; root is only needed on the phone,
and only for the module + WebUI above, the desktop app doesn't need or ask
for it. On Linux, `scripts/package_module.sh` and friends already do this
from the command line; see `docs/BUILDING.md`.

1. Open an APK file in the desktop app; it runs the same ABI checks
   (`core/`'s `CompatibilityChecker`, the same logic the WebUI mirrors)
   and shows you the result.
2. "Check connected device" queries a device over `adb` (must be on your
   PATH, and the device authorized) for its real ABI list and whether
   Mango's native bridge is active, then combines that with the APK check
   above for a real verdict instead of just "what's in the APK".
3. "Push module to device" runs `adb push` on `build/mango-module.zip`
   (`scripts/package_module.sh`'s output) to the device's Downloads
   folder. Flashing it is still a manual step in your root manager app
   (Option 1, step 1); the desktop app doesn't flash anything itself.

## Option 3: Do it entirely by hand

1. Grab the Mango module (a `.zip`) from the project's releases, and flash
   it yourself, then reboot.
2. Install the 32-bit-only APK the normal way (`adb install`, a file
   manager, whatever you'd normally use).
3. Launch it.

## Troubleshooting

- **The WebUI won't open, or shows "no root bridge found"**: your root
  manager doesn't support module WebUIs. Try the standalone
  [KsuWebUI](https://github.com/5ec1cff/KsuWebUIStandalone) app, which
  renders module WebUIs for Magisk, KernelSU, and APatch.
- **"No unzip on this device" when checking a raw APK path**: expected on
  some devices, `unzip` isn't part of stock Android, see
  `module/README.md`'s "How it checks an app's ABI". Use the
  installed-apps dropdown instead, that path doesn't need it.
- **App won't install at all**: check `docs/ARCHITECTURE.md`'s note on
  `ro.product.cpu.abilist`; the module needs to have applied correctly and
  the device needs a reboot after flashing it.
- **App installs but crashes on launch**: likely a translation gap for
  something that app does. File an issue with the app's name and what you
  see in the command log, see `docs/CONTRIBUTING.md`.
- **Everything is slow**: expected for now. See the roadmap in
  `docs/ARCHITECTURE.md`; performance work comes after correctness.
