# linux/

Standalone Linux support: running an `armeabi-v7a` (32-bit ARM) executable
on a 64-bit-only Linux system (arm64, no AArch32 hardware), with no Android,
no root, and no native bridge involved. Same underlying problem Mango solves
on Android, same class of solution Tango itself calls its "standalone mode"
(see `docs/ARCHITECTURE.md`'s "System and library calls" section) and what
QEMU's `linux-user` does for other ISA pairs: a full process-level dynamic
binary translator, not an in-process hook.

This is a deliberate, early addition, not a promise it's finished. See
"Current status" below before assuming anything here actually runs a real
binary yet.

## Why this is a different shape of problem than `module/`

Android's native bridge (what `native/` implements, see
`docs/ARCHITECTURE.md`'s "Option A") works because ART already knows how to
defer to a registered bridge when it needs to load a foreign-ISA library
inside an otherwise-native process. Plain Linux has no ART and no native
bridge concept: there's no host process for a 32-bit executable to be
loaded into, because on Linux, the 32-bit binary *is* the process.

That means standalone Linux support needs its own process from address
zero: an ELF loader (parse the guest binary, map its segments into a guest
memory image) and a syscall layer (the guest's `SVC`/`SWI` traps have to
land somewhere, since there's no ART to route library calls through
instead). This is much closer to QEMU-user or Box64 than to Mango's own
Android module, and reuses `native/`'s decoder and interpreter (`mango_core`)
completely unchanged: the CPU-instruction-level problem is identical, only
what surrounds it (ELF loading, syscalls) differs from the Android side.

## Design

- **ELF loading.** Parse the guest's ELF32 header and `PT_LOAD` program
  headers, and copy each segment into a flat `MangoMemory` buffer at its
  intended virtual address, the same memory model `native/`'s interpreter
  already uses. Scoped narrow at first: static, non-PIE executables only.
  PIE (position-independent, the default for modern toolchains) and dynamic
  linking (resolving a guest `.so`'s own imports) are real follow-up work,
  not this phase.
- **Syscall thunking.** When the guest executes `SVC #0`, AAPCS32 says the
  syscall number is in `r7` and up to six arguments are in `r0`-`r6`. The
  host is a 64-bit Linux process, so instead of translating that into the
  literal AArch32 Linux syscall ABI (what a full emulator like `qemu-arm`
  does), the plan is to translate the small set of syscalls a typical app
  actually needs (`exit`, `write`, `mmap`, `read`, ...) directly into the
  equivalent host libc/syscall call, register-mapped, the same "thunk to
  the real implementation instead of emulating it" idea `docs/
  ARCHITECTURE.md`'s prior-art table credits Box64 for. Everything else
  fails loudly rather than silently doing the wrong thing.
- **The interpreter itself doesn't change.** `mango_core` (`native/src/
  decoder.c`, `native/src/interp.c`) has no Android dependency already;
  this directory links against it as-is, see `CMakeLists.txt`.

## Current status

Proof of concept, not a translator:

- `src/selfcheck.c` / `mango_linux_selfcheck`: really works. Runs a small
  hand-encoded A32 program through `mango_core` as an ordinary host binary
  and checks the result, proving the interpreter builds and runs with zero
  Android/NDK/JNI in the loop. This is a demo, not a replacement for
  `native/tests/test_interp.c`'s real coverage.
- `src/loader.c` / `mango_linux_loader`: a stub. Takes a path, prints what
  isn't implemented yet, exits nonzero. No ELF parsing, no syscalls, no
  guest memory setup. This exists so the CMake target, the CI job, and the
  docs referencing it are all real today, and a contributor has one
  obvious file to start filling in rather than a bare directory.
- The decoder's real limitations are `native/`'s, not new: `SVC`/`SWI`
  (the syscall trap itself) isn't decoded yet, so even a trivial guest
  program that calls `exit()` can't run end to end until that's added,
  with its own test coverage, per `docs/CONTRIBUTING.md`.

## Building

No NDK needed here, unlike `native/`'s Android build: this compiles with
the host's own toolchain.

```
cmake -B build
cmake --build build
./build/mango_linux_selfcheck
```

or without CMake, same as `native/README.md`'s alternative:

```
cc -std=c11 -Wall -Wextra -Werror -I../native/include \
  ../native/src/decoder.c ../native/src/interp.c src/selfcheck.c \
  -o /tmp/mango_linux_selfcheck
```

## Where to look if you want to help

- `SVC`/`SWI` decoding in `native/src/decoder.c` (with its own test case in
  `native/tests/test_interp.c`, per `docs/CONTRIBUTING.md`) is the actual
  unblocking step; nothing here can run a real syscall-making binary
  without it.
- ELF32 header/program-header parsing for `src/loader.c`, scoped to static
  non-PIE binaries first.
- A syscall thunk table, starting with just `exit`/`write`, once `SVC` is
  decodable.
