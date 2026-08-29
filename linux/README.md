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

- **ELF loading.** Parses the guest's ELF32 header and `PT_LOAD` program
  headers, and copies each segment into a flat `MangoMemory` buffer at its
  intended virtual address, the same memory model `native/`'s interpreter
  already uses. Real now (`native/src/elf32.c`, `src/loader_core.c`), but
  scoped narrow: static, non-PIE executables only. PIE (position-
  independent, the default for modern toolchains) and dynamic linking
  (resolving a guest `.so`'s own imports) are real follow-up work, listed
  below.
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

Genuinely loads and runs static, non-PIE ARM32 code now, not just a demo,
but still short of running a real program end to end:

- `src/selfcheck.c` / `mango_linux_selfcheck`: runs a small hand-encoded
  A32 program through `mango_core` as an ordinary host binary, proving the
  interpreter builds and runs with zero Android/NDK/JNI in the loop. A
  demo, not a replacement for `native/tests/test_interp.c`'s real coverage.
- `native/src/elf32.c` (`mango_elf32_parse`/`mango_elf32_find_symbol`):
  real ELF32 parsing, shared with `native_bridge_shim.c`. Reads `PT_LOAD`
  segments from the program header table and `.dynsym`/`.dynstr` from the
  section header table (so it needs section headers to be present for
  symbol lookup, though not for pure execution, see
  `native/tests/test_elf32.c`, verified against a real `readelf`-checked
  fixture, both a real one `gcc -m32` produced and a small hand-built one).
- `src/loader_core.c` (`mango_linux_load_and_run`) / `src/loader.c`
  (`mango_linux_loader`, the CLI): really loads an ELF32 ARM executable's
  segments into guest memory and runs it from its entry point via
  `mango_core`, see `tests/test_loader.c`, which does exactly this against
  a hand-built, `readelf`-verified two-instruction ARM32 executable and
  checks the actual resulting register value, not just "it didn't crash."
  Static, non-PIE only (see "Design" above), and the guest memory size is
  a fixed 1 MiB for now, not sized from the binary's actual segments.
- The decoder's real limitation is `native/`'s, not new: `SVC`/`SWI` (the
  syscall trap itself) isn't decoded yet, so a real program that calls
  `exit()` or does anything else syscall-based stops there, reported as
  `MANGO_LINUX_LOAD_INTERP_FAILED`, not a crash, but not a completed run
  either. This is genuinely the one thing blocking a real (not
  hand-built) ARM32 static binary from running end to end.

## Building

No NDK needed here, unlike `native/`'s Android build: this compiles with
the host's own toolchain.

```
cmake -B build
cmake --build build
ctest --test-dir build
./build/mango_linux_loader some-arm32-binary
```

or without CMake, same as `native/README.md`'s alternative:

```
cc -std=c11 -Wall -Wextra -Werror -I../native/include -Iinclude \
  ../native/src/decoder.c ../native/src/interp.c ../native/src/elf32.c \
  src/loader_core.c src/loader.c -o /tmp/mango_linux_loader
```

## Where to look if you want to help

- `SVC`/`SWI` decoding in `native/src/decoder.c` (with its own test case in
  `native/tests/test_interp.c`, per `docs/CONTRIBUTING.md`) is the actual
  unblocking step; nothing here can run a real syscall-making binary
  without it, no matter how good the loader gets.
- A syscall thunk table, starting with just `exit`/`write`, once `SVC` is
  decodable.
- PIE support: reading `PT_DYNAMIC`'s relocation entries and applying
  them, since most real toolchains default to PIE now.
- Sizing guest memory from the binary's own segments instead of a fixed
  1 MiB, and rejecting overlapping/malicious segment layouts more
  carefully than the current bounds checks do.
