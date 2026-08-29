# Architecture

This document is deliberately honest about what's solved, what's a design
sketch, and what's genuinely unsolved. If a section reads as tentative,
that's on purpose, not a placeholder someone forgot to fill in.

## The problem, precisely

Some Android apps only ship an `armeabi-v7a` (32-bit ARM) native library
slice and never got a 64-bit build. That was fine while every 64-bit chip
still had hardware support for running 32-bit ARM (AArch32) code. It stops
being fine on chips that dropped AArch32 hardware support entirely, which
is now happening (Snapdragon 8 Gen 3 and newer are the well-known case).
On those phones, a 32-bit-only app doesn't run slowly, it doesn't run at
all: the CPU cannot execute AArch32 instructions.

Tango (Amanieu Systems) solves this today via a *dynamic binary
translator*: it rewrites AArch32/Thumb-2 machine code into equivalent
AArch64 machine code, either ahead of time or on first use, and handles
everything around that (syscalls, signals, threading) so the translated
program behaves like it's still running natively. It's licensed to OEMs
and baked into specific ROMs (Xiaomi China ROMs, some OnePlus builds).
Mango's goal is the same idea, available to anyone, on any rooted device.
Mango has no affiliation with Amanieu Systems and contains none of their
code; see `docs/TERMS.md` for the project's intended scope.

## Two ways to plug a translator into Android, and which one we're using

There are two structurally different places to hook a translator into
Android, and they lead to different product shapes. Worth being explicit
about this before writing any more code.

### Option A: system-level, via AOSP's Native Bridge

Android already has a public, built-in mechanism for exactly this problem:
`libnativebridge` (in AOSP's `art/` tree). It exists so a device can load a
native library compiled for a "foreign" ISA into an otherwise-native
process, by routing that one library's loading and JNI resolution through
a translator instead of the normal linker. Google's own NDK Translation
project, and Intel's older Houdini, both implement this same interface to
run ARM apps on x86 Chromebooks and emulator images: they register as the
device's native bridge implementation (historically via the
`ro.dalvik.vm.native.bridge` system property) and the ART runtime
transparently defers to them whenever it needs to load a library whose ISA
doesn't match the process's native one.

This is almost certainly close to how Tango itself integrates at the OS
level too (their own docs describe a distinct "OS integration" component
alongside the translator).

For Mango this means: implement the `NativeBridgeCallbacks` interface,
ship it as a Magisk module that sets the relevant system properties
persistently (a module `system.prop` file, loaded via Magisk's `resetprop`
mechanism, systemlessly, no partition writes) and installs the translator,
and every 32-bit-only app on the device becomes launchable
automatically, no per-APK patching required, because the OS itself already
knows how to ask a native bridge for help. Also worth noting: Android's
package installer normally refuses to install (or extracts nothing usable
from) an APK that doesn't declare a supported ABI, so getting a 32-bit-only
APK installable at all on such a device also depends on
`ro.product.cpu.abilist` (and related props) listing `armeabi-v7a`, which
is itself a system-level property, not something you can fix from inside
one APK either.

### Option B: per-APK, self-contained patching

Patch a specific APK to carry its own translator and a stub loader, so it
works without any system-wide change. This is closer to the ReVanced/
Magisk-Manager-style workflow originally sketched for this project, and
it's more self-contained (uninstall the app, everything's gone). But it's
fighting the platform instead of using the hook Android already provides
for this exact scenario, and getting a self-modified APK's native library
loading to transparently redirect into a translator, from inside app
sandboxing, without any of the native bridge plumbing, is a substantially
harder and less-trodden path.

### What Mango does

Primarily Option A. The Magisk module (native bridge implementation +
system property setup) is the real deliverable; it's what actually mirrors
how Tango, Houdini, and NDK Translation solve this problem, and it's the
path with an existing, documented hook to build against instead of one we'd
have to invent.

The on-device UI is a WebUI shipped inside the module itself
(`module/webroot/`), not a separate Android app, see "Why a WebUI, not a
standalone app" below. It still gives the "pick an app, check it, install
it" workflow from the original idea. On top of Option A, that mostly means:
making sure the module is installed, checking the target APK will actually
be accepted for install and launch given the current device configuration,
and surfacing clear errors when it won't (missing ABI declaration,
incompatible `minSdkVersion`, and so on) rather than deep binary surgery on
every APK. Option B may still be worth revisiting later for specific apps
the native bridge path can't handle well, but it's not the starting point.

### Why a WebUI, not a standalone app

The first version of this project sketched a separate Android app (Compose
UI, its own root grant, installing itself via a normal APK). That got
replaced with a WebUI living inside the module for a concrete reason, not
just style: the WebUI runs inside a manager app (Magisk, KernelSU,
KernelSU-Next, or APatch) that already has root, so there's no separate
permission grant, no separate APK to install and keep in sync with the
module, and one artifact instead of two.

It also simplifies the two things the UI actually needs to do. Both
KernelSU and, as of **Magisk v28.1**, Magisk itself support a
`webroot/index.html` module UI with a JS bridge (a global `ksu` object,
`ksu.exec(cmd)` to run a root shell command, `ksu.toast(msg)` for
feedback); a community template
([`ksu-webui-module-template`](https://github.com/barsikus007/ksu-webui-module-template))
already targets KernelSU, Magisk, and APatch from one `webroot/`, and a
standalone renderer app, **KsuWebUI**, exists specifically to fill the gap
on managers without native WebUI support. Given that, "check if an APK has
an arm64-v8a slice" becomes a one-line shell command
(`unzip -l app.apk | grep lib/`) instead of Kotlin ZIP-parsing code, and
"install this app" becomes `pm install -r <path>` instead of a
FileProvider/`ACTION_VIEW`/user-confirmation dance, because the WebUI
already has the root a plain app would have had to ask for.

One thing not independently confirmed here: whether Magisk's native
v28.1+ WebUI exposes the *exact same* `ksu` JS interface as KernelSU's, as
opposed to something merely compatible. The cross-manager template and
KsuWebUI both existing is a good sign this is solved in practice, but it's
worth testing on a real Magisk device rather than assuming; see
`docs/SECURITY.md` and `module/README.md`.

APatch is now confirmed to diverge, not just theoretically: its
`exec(cmd)` resolves to a bare stdout string rather than KernelSU/Magisk's
`{errno, stdout, stderr}` object (checked against APatch's
`WebViewInterface` source), which used to crash the WebUI on first load.
`module/webroot/app.js`'s `run()` normalizes both shapes now; see
`module/README.md`.

`desktop/` (and the `core/` module it depends on) stayed as a normal
Compose Desktop app, now scoped to Windows/macOS specifically: it doesn't
run with root the way the on-device UI does, so none of the reasoning
above changes anything about it, it's still a separate app for local APK
inspection from a PC, now also able to query a connected device over
`adb` and push a packaged module to it, see `docs/USAGE.md`. Linux gets
first-class standalone support instead, via `linux/`, described below.

## Repository layout

```
core/     shared Kotlin: APK inspection, install-compatibility checks,
          signing helpers, orchestration. No Android or UI dependencies.
          Used by desktop/; the WebUI reimplements the same checks in JS,
          since it can't call into the JVM, see "Why a WebUI" above.
desktop/  Compose Desktop app, Windows/macOS. Same checks, useful for
          inspecting an APK or preparing the module/translator build
          before pushing to a device over adb. Needs no root itself.
native/   the Android translator: the native bridge implementation and
          the actual AArch32/Thumb-2 -> AArch64 translation engine.
          C/C++, CMake, cross-compiled with the NDK. This is the hard,
          unfinished part.
linux/    an early, standalone (no root, no Android, no native bridge)
          take on the same translation problem for Linux itself. Reuses
          native/'s decoder+interpreter core; see its own README for
          exactly what's real (loading and running static, non-PIE ARM32
          code) versus not (anything that makes a syscall) yet.
module/   the Magisk/KernelSU/KernelSU-Next/APatch module. module/webroot/
          is the on-device UI, see above.
```

## Standalone Linux support

Tango itself has more than the native-bridge integration described above:
its own docs mention a "standalone mode" that does full process-level
dynamic binary translation, the same shape of thing QEMU's `linux-user`
does for other ISA pairs. That mode doesn't depend on Android or ART at
all, which means the identical hardware problem (a 64-bit-only chip that
can't run AArch32 code) shows up just as much on a 64-bit-only Linux
system as it does on Android, and can be solved the same way: a normal
process, not a system hook.

`linux/` is Mango's early take on that. It reuses `native/`'s decoder,
interpreter, and now ELF32 parser (`native/src/elf32.c`, shared with the
Android shim) completely unchanged, and adds what's genuinely different:
an ELF loader instead of ART's native-bridge hook, and syscall thunking
instead of JNI resolution, since a bare Linux process has no runtime to
defer to when the guest traps into the kernel. Loading and running static,
non-PIE ARM32 code is real now, verified end to end against a hand-built,
`readelf`-checked executable, see `linux/README.md`; syscall thunking
(so a genuinely real binary, not a hand-built one, can run to completion)
is the part that's still just a sketch.

## Why the translation engine is genuinely hard

A few concrete reasons this isn't a mechanical find-and-replace over
instruction encodings:

- **Variable-length, dual instruction sets.** AArch32 code can switch
  between 32-bit ARM and 16/32-bit Thumb-2 encoding at runtime (the low bit
  of a branch target address selects the mode). The decoder has to track
  that, not just decode a fixed-width stream.
- **Widespread conditional execution.** Most AArch32 instructions can be
  individually predicated (`ADDEQ`, `MOVNE`, and so on), and Thumb-2 adds
  IT-blocks that predicate up to four following instructions. AArch64
  dropped general predication in favor of a handful of conditional-select
  instructions (`CSEL`, `CSINC`, ...) and branches. There's no 1:1
  instruction mapping for most of these; they have to be lowered into a
  branch, or a conditional-select sequence, changing the instruction count
  and control flow shape.
- **Register file mismatch.** AAPCS32 has 16 general-purpose registers
  (r0-r15, with r13=SP, r14=LR, r15=PC exposed as a GPR) and passes the
  first 4 integer/pointer arguments in r0-r3. AAPCS64 has 31 (x0-x30, with
  SP and PC no longer general-purpose registers, LR is just x30) and passes
  the first 8 in x0-x7. Translated code needs a register allocation
  strategy that maps the guest's architectural register file onto the
  host's, including the fact that AArch32 code can read its own PC as data,
  which AArch64 code can't do the same way.
- **Floating point / SIMD.** VFP and NEON map reasonably well onto
  AArch64's FP/SIMD unit conceptually, but not instruction-for-instruction;
  this needs its own translation rules and is a common source of subtle
  correctness bugs in every project that's done this (see prior art below).
- **System and library calls.** Because the translator runs the translated
  code *inside* an already-64-bit process (via native bridge), it should
  not need to emulate 32-bit Linux syscalls the way a full process-level
  emulator (Tango's standalone mode, QEMU-user) does. Instead, calls the
  guest code makes into libc, libm, liblog, GLES, and similar should be
  relinked ("thunked") to the process's real 64-bit implementations of
  those libraries. This is less work than full syscall translation, but it
  means the translator needs accurate per-function thunks for every system
  API surface an app might touch, and a fallback for the rest.
- **Self-modifying / JIT-generated guest code.** Rare in typical apps, but
  not impossible (some games embed their own script VMs or JIT compilers).
  Needs invalidation of any cached translation when guest memory it was
  derived from changes.
- **JIT cache and warm-up cost.** Translating on every call is too slow;
  caching translated blocks (keyed by guest address) is necessary, and an
  ahead-of-time "pre-translate on install" pass (like Tango's
  pre-translator) helps startup time at the cost of install-time work and
  storage.

None of this is exotic. It's the same list of problems QEMU's TCG, Box64,
FEX-Emu, and Apple's Rosetta 2 all had to solve for their own source/target
ISA pairs. That's genuinely useful: the *shape* of the problem and a lot of
the hard-won lessons are public, even though none of them target this exact
AArch32-guest-inside-AArch64-host-process scenario.

## Prior art worth learning from

| Project | Translates | Approach | License | Notes |
|---|---|---|---|---|
| QEMU (`linux-user`, TCG) | many ISA pairs, including ARM-on-ARM64 | full process-level dynamic binary translation | mostly GPL-2.0, some LGPL, mixed per-file | Battle-tested, general-purpose, not performance-focused. `qemu-arm` on an aarch64 host is the closest existing thing to "run 32-bit ARM code on 64-bit ARM," though as a whole-process emulator, not an in-process native bridge. |
| Box64 / Box86 | x86(_64) on other 64-bit hosts | dynamic recompilation + library call forwarding ("thunking") to host libs | MIT | The thunking idea (forward library calls to real host libraries instead of emulating them) is exactly what Mango needs for libc/GLES calls. |
| FEX-Emu | x86/x86-64 on ARM64 | custom IR, JIT with code caching, extensive thunking | check current repo | Useful reference for a clean, cache-friendly JIT pipeline design and a real-world thunking system with guest/host packers. |
| Apple Rosetta 2 | x86-64 on Apple Silicon | mostly ahead-of-time translation at install time, JIT fallback | proprietary | No code to borrow, but the "translate once at install, cache it" strategy is directly relevant to Android's APK install flow. |
| AOSP `libnativebridge` + NDK Translation | ARM-on-x86 (opposite direction from Mango) | in-process native bridge, exactly the hook described above | AOSP: Apache-2.0 | This is the interface to implement, not a translator to copy; NDK Translation targets the opposite ISA pair, but the surrounding plumbing is exactly Option A above. |

Important: license compatibility isn't automatic just because something is
"open source." Mango is GPL-3.0. Code under GPL-2.0-*only*, or under a
license the FSF doesn't consider GPL-3-compatible, cannot simply be copied
in; check each specific file's license before reusing any code, not just
the project's headline license. Studying an approach and reimplementing it
independently is always fine; copying code is a per-file legal question.

## Realistic roadmap

This is a multi-year, community-scale effort in every project that's
attempted something in this space. Rough phases, each one a real milestone
rather than a rewrite of the last:

1. **Interpreter proof of concept.** Decode and interpret (not JIT) a
   minimal AArch32 instruction subset against a synthetic test binary, no
   real app involved yet. Goal: prove the decode/register-mapping model
   works at all.
2. **Real library, still interpreted.** Get one small, real, permissively
   licensed armeabi-v7a `.so` running via native bridge on a test device,
   however slowly. Goal: prove the native bridge integration end to end.
3. **Baseline JIT.** Replace the interpreter with straight-line code
   generation and a translation cache. No optimization yet, just "compiled
   once, not re-interpreted every call."
4. **Thunking.** Route common libc/libm/liblog/GLES calls to the host's
   real implementations instead of translating their internals.
5. **Breadth and correctness.** NEON/VFP coverage, condition-flag edge
   cases, exception/signal handling, expanding the set of apps that work.

Phase 1 is where a new contributor should look first; see
`docs/CONTRIBUTING.md`.

`linux/`'s standalone support is a parallel track, not a blocker on any of
the phases above or vice versa: it shares Phase 1's decoder/interpreter
core directly, but needs its own ELF loading and syscall thunking before
it reaches anything like Phase 2. See `linux/README.md` for where that
stands.
