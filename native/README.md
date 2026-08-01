# native/

The translator. See `docs/ARCHITECTURE.md` for the overall design and
`docs/BUILDING.md` for build commands. This file is about what's actually
here right now.

## Current status

Phase 1 from the roadmap in `docs/ARCHITECTURE.md`: a decoder and
interpreter for a small subset of A32 (not Thumb-2), tested against hand-
encoded synthetic programs, not real app code yet.

- `include/mango/cpu.h`, `include/mango/decoder.h`, `include/mango/interp.h`,
  `src/decoder.c`, `src/interp.c`: the portable core. Handles `MOV`, `ADD`,
  `SUB`, `CMP` (register or immediate operand2, no shifts yet), unconditional
  `B`, `BX`, and `LDR`/`STR`/`LDRB`/`STRB` (immediate offset only: no
  register-offset addressing, no post-indexing or writeback; `LDRB`
  zero-extends, there's no signed byte load). Only the `AL` (always)
  condition is handled; conditional execution is unimplemented. Register
  reads correctly treat r15 (PC) as "current instruction address + 8" per
  real hardware semantics, which matters for the very common
  `LDR Rd, [PC, #imm]` literal-pool pattern. This is genuinely a small
  subset, real apps will use far more of the ISA (Thumb-2, conditional
  execution, shifted operands, register-offset addressing, NEON, and so on
  all still need doing).
- The interpreter has an actual memory model (`MangoMemory`): a flat,
  byte-addressable buffer that code and data share, same as real memory.
  Every fetch and every `LDR`/`STR`/`LDRB`/`STRB` is bounds-checked (word
  accesses are also alignment-checked, byte ones aren't since any address
  is a valid byte offset); out of range fails the run rather than reading
  or writing past the buffer, and there are tests specifically proving
  that (not just asserting it in a comment), see `docs/SECURITY.md` for
  why that's the priority here.
- `include/mango/native_bridge.h`: the AOSP native bridge interface Mango
  implements, adapted from the real header (see file for the source and
  license). `loadLibrary` and `getTrampoline`, the two functions that would
  actually run guest code, are not implemented yet: `isSupported`/
  `isCompatibleWith` work for real, everything that would execute a real
  32-bit library is still a stub. This is genuinely the current state, not
  modesty.
- `src/native_bridge_shim.c`: the Android-specific glue exposing
  `NativeBridgeItf`. `isSupported()` really does check the ELF header
  (class + machine), the rest of the interface returns "not implemented"
  values.
- `tests/test_interp.c`: seven test programs, hand-encoded by working out the
  A32 bit patterns by hand and cross-checked against an independently
  written encoder before trusting them. All pass under
  `-Wall -Wextra -Werror -fsanitize=address,undefined`, including two
  negative tests that specifically try an out-of-bounds and a misaligned
  `LDR` and check they're rejected, not just that the happy path works.
  Earlier rounds of this (the `BX` mask-width bug) caught real bugs during
  development, which is exactly the kind of mistake this subset of the
  project is prone to; more test cases from more people is how this gets
  more trustworthy, see `docs/CONTRIBUTING.md`.

## Building and testing without an Android device

The core and its tests don't need the NDK:

```
cmake -B build
cmake --build build
./build/mango_core_tests
```

or without CMake at all, plain gcc/clang works fine for iterating on the
decoder and interpreter:

```
cc -std=c11 -Wall -Wextra -Werror -Iinclude src/decoder.c src/interp.c tests/test_interp.c -o /tmp/mango_core_tests
/tmp/mango_core_tests
```

Building the actual `.so` (`mango_translator`, which needs `native_bridge_shim.c`
and NDK/bionic headers) does need the NDK toolchain; see `docs/BUILDING.md`.

## Where to look if you want to help

- More A32 instructions: shifted operand2 (`ADD r0, r1, r2, LSL #2` and
  friends), register-offset and post-indexed/writeback addressing for
  `LDR`/`STR`/`LDRB`/`STRB`, then conditional execution, then Thumb-2.
  Each addition should come with a hand-derived test case the way the
  existing ones work, see `docs/CONTRIBUTING.md`'s testing section.
- `loadLibrary`/`getTrampoline` in the shim: this is where "interpret a
  synthetic test program" turns into "actually run a real `.so`'s code",
  and is a substantially bigger jump than anything implemented so far.
