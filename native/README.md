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
  `SUB`, `CMP` (register operand2 can carry a shift now: LSL/LSR/ASR/ROR
  by an immediate amount, not by a register-specified amount yet, and the
  `LSR #0`/`ASR #0`/`ROR #0` encodings are rejected since those actually
  mean `LSR #32`/`ASR #32`/RRX, not "shift by zero"), `B`, `BL`,
  `BX`, and `LDR`/`STR`/`LDRB`/`STRB` (immediate offset only: no
  register-offset addressing, no post-indexing or writeback; `LDRB`
  zero-extends, there's no signed byte load). Every one of ARM's 14 real
  condition codes works, not just `AL`: `BEQ`, `BNE`, `MOVLT`, and so on
  all execute (or don't) based on the current NZCV flags, the same as
  real hardware. `ADDS`/`SUBS`/`CMP` all compute full NZCV now (not just
  `CMP`, and not just N/Z); `MOVS` updates N/Z correctly but leaves C
  exactly where it was rather than computing it from the shifter's
  carry-out when operand2 involves a shift, that's a known, documented
  gap, not silently wrong. Register reads correctly treat r15 (PC) as
  "current instruction address + 8" per real hardware semantics, which
  matters for the very common `LDR Rd, [PC, #imm]` literal-pool pattern.
  This is genuinely a small subset, real apps will use far more of the
  ISA (Thumb-2, register-offset addressing, `MOVS`'s shift-carry, NEON,
  and so on all still need doing).
- The interpreter has an actual memory model (`MangoMemory`): a flat,
  byte-addressable buffer that code and data share, same as real memory.
  Every fetch and every `LDR`/`STR`/`LDRB`/`STRB` is bounds-checked (word
  accesses are also alignment-checked, byte ones aren't since any address
  is a valid byte offset); out of range fails the run rather than reading
  or writing past the buffer, and there are tests specifically proving
  that (not just asserting it in a comment), see `docs/SECURITY.md` for
  why that's the priority here.
- `mango_interp_run` takes a `stop_addr` now, not just `max_steps`: it
  returns 0 when PC reaches that address, checked before every fetch.
  `BX` used to unconditionally return 0 on its own, which happened to
  work for standalone tests but broke the first real nested call: a
  callee's `BX LR` was ending the whole run instead of returning to the
  caller. `BL`/`BX` are both just normal jumps now; the caller supplies
  a `stop_addr` outside `mem` (the same sentinel-in-LR trick the tests
  already used) to detect the top-level function actually returning.
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
- `tests/test_interp.c`: thirteen test programs, hand-encoded by working out the
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

- More A32 instructions: `PUSH`/`POP`/`STM`/`LDM` (stack save/restore,
  needed for any real function's prologue/epilogue), `MUL`, register-
  specified shift amount (`LSL Rs`, not just `LSL #imm`), register-offset
  and post-indexed/writeback addressing for `LDR`/`STR`/`LDRB`/`STRB`,
  `MOVS` computing C from the shifter's carry-out instead of just leaving
  it alone, then Thumb-2. Each addition should come with a hand-derived
  test case the way the existing ones work, see
  `docs/CONTRIBUTING.md`'s testing section.
- `loadLibrary`/`getTrampoline` in the shim: this is where "interpret a
  synthetic test program" turns into "actually run a real `.so`'s code",
  and is a substantially bigger jump than anything implemented so far.
