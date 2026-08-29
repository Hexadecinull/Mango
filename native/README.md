# native/

The translator. See `docs/ARCHITECTURE.md` for the overall design and
`docs/BUILDING.md` for build commands. This file is about what's actually
here right now.

## Current status

Phase 1 from the roadmap in `docs/ARCHITECTURE.md`: a decoder and
interpreter for a subset of A32 (not Thumb-2), tested against hand-encoded
synthetic programs, not real app code yet.

- `include/mango/cpu.h`, `include/mango/decoder.h`, `include/mango/interp.h`,
  `src/decoder.c`, `src/interp.c`: the portable core. Handles the full A32
  data-processing set (`AND`, `EOR`, `SUB`, `RSB`, `ADD`, `ADC`, `SBC`,
  `RSC`, `TST`, `TEQ`, `CMP`, `CMN`, `ORR`, `MOV`, `BIC`, `MVN`, all 16
  ARM ALU opcodes), `MUL` (not `MLA`, its accumulate-form sibling, that's
  explicitly rejected rather than misdecoded, see below), `B`, `BL`, `BX`,
  and `LDR`/`STR`/`LDRB`/`STRB` (immediate offset only: no register-offset
  addressing, no post-indexing or writeback; `LDRB` zero-extends, there's
  no signed byte load). Register operand2 can carry a shift now:
  LSL/LSR/ASR/ROR by an immediate amount, not by a register-specified
  amount yet, and the `LSR #0`/`ASR #0`/`ROR #0` encodings are rejected
  since those actually mean `LSR #32`/`ASR #32`/RRX, not "shift by zero".
  Every one of ARM's 14 real condition codes works, not just `AL`: `BEQ`,
  `BNE`, `MOVLT`, and so on all execute (or don't) based on the current
  NZCV flags, the same as real hardware. Every S-suffixed arithmetic op
  (`ADDS`/`SUBS`/`ADCS`/`SBCS`/`RSBS`/`RSCS`/`CMP`/`CMN`) computes full,
  correct NZCV, including carry propagation through `ADC`/`SBC`/`RSC` for
  multi-word arithmetic (see `test_adc_carry_chain` for the actual 64-bit-
  add-via-two-32-bit-registers case this exists for). The logical ops
  (`AND`/`EOR`/`ORR`/`BIC`/`MVN`/`MOV`/`TST`/`TEQ`) update N/Z correctly,
  preserve V exactly (this project's own earlier `MOVS` didn't, a real bug
  caught while adding the rest of this set, fixed for all of them at
  once), and leave C where it was rather than computing it from the
  shifter's carry-out when operand2 involves a shift; that's a known,
  documented gap, not silently wrong. `TST`/`TEQ`/`CMP`/`CMN` with `S=0`
  aren't decoded as those ops at all: that bit pattern is actually
  `MRS`/`MSR` territory, a different instruction family this project
  doesn't support, so it's correctly rejected instead of silently
  misinterpreted as a flag-less compare that can't exist on real
  hardware. Register reads correctly treat r15 (PC) as "current
  instruction address + 8" per real hardware semantics, which matters for
  the very common `LDR Rd, [PC, #imm]` literal-pool pattern. This is
  genuinely a subset, real apps will use far more of the ISA (Thumb-2,
  register-offset addressing, `PUSH`/`POP`/`STM`/`LDM`, NEON, and so on
  all still need doing).
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
  license). `loadLibrary` really parses and loads a guest `.so`'s
  `PT_LOAD` segments into guest memory now (via `elf32.c` below);
  `getTrampoline` really looks its requested symbol up in that guest
  image, but still returns `NULL`: turning a found symbol into something
  ART can actually call as a normal AArch64 function is a real, separate
  piece of work (a generated trampoline or a libffi closure) that isn't
  written yet. `isSupported`/`isCompatibleWith` work too, and
  `isCompatibleWith` is more load-bearing than its name suggests: AOSP's
  own `libnativebridge` calls it with `NAMESPACE_VERSION` (3)
  *unconditionally at load time* and discards the entire bridge if that's
  false, confirmed against AOSP's `libnativebridge/native_bridge.cc`, not
  guessed. This isn't gated by the `.version` field the way it looks like
  it should be; it used to return `false` for that call by accident here,
  which would have meant Mango's bridge got silently rejected by ART
  before anything else in this list ever ran, on every device, every
  time. Fixed by actually implementing the rest of the v3 interface
  (`unloadLibrary` now really frees what `loadLibrary` allocated; the
  namespace-related functions are safe no-ops, since Mango doesn't do
  real linker-namespace isolation) rather than papering over the
  symptom, see `tests/test_native_bridge_shim.c` for what's actually
  checked and `src/native_bridge_shim.c`'s comment on `isCompatibleWith`
  for the full explanation.
- `src/elf32.c` / `include/mango/elf32.h`: portable ELF32 parsing
  (`mango_elf32_parse`, `mango_elf32_find_symbol`), part of `mango_core`
  alongside the decoder/interpreter, so both this shim and `linux/`'s
  standalone loader share one implementation. Reads `PT_LOAD` segments
  from the program header table and `.dynsym`/`.dynstr` from the section
  header table, so (unlike a real OS loader) it needs section headers to
  be present for symbol lookup. See `tests/test_elf32.c`, checked against
  both a real `gcc -m32`-produced shared object and a small hand-built
  one, both independently verified with `readelf` before being trusted as
  fixtures, same reasoning as the decoder's hand-encoded test words.
- `src/native_bridge_shim.c`: the Android-specific glue exposing
  `NativeBridgeItf`. `isSupported()` really does check the ELF header
  (class + machine); `loadLibrary`/`getTrampoline`/`unloadLibrary`/the
  namespace functions are real as far as `elf32.c` above goes, not
  further (`getTrampoline` still can't hand back a real callable
  function). `tests/test_native_bridge_shim.c` (built against
  `tests/fake_jni/jni.h`, a deliberately minimal stand-in, not the real
  NDK header, see that file) exercises this on the host without needing
  a device.
- `native/src/decoder.c`'s `SVC`/`SWI` decoding and `mango_interp_run`'s
  resulting stop-and-resume contract (see `interp.h`): the interpreter
  itself stays syscall-agnostic on purpose, it just stops with `r7`/
  `r0`-`r6` intact and `PC` still pointing at the `SVC`, see
  `test_svc_stops_and_can_resume` in `tests/test_interp.c`. Actually
  thunking syscalls to something real is a per-context decision (a
  standalone process versus a JNI-loaded library want different things),
  see `linux/loader_core.c` for where that's actually implemented.
- `tests/test_interp.c`: twenty test programs, hand-encoded by working
  out the A32 bit patterns by hand and cross-checked against an
  independently written encoder before trusting them (this caught a real
  mistake in a hand-derived test word during development, exactly why
  that second encoder exists). All pass under
  `-Wall -Wextra -Werror -fsanitize=address,undefined`, including two
  negative tests that specifically try an out-of-bounds and a misaligned
  `LDR` and check they're rejected, not just that the happy path works,
  and two more checking `MLA` and `S=0` `TST`/`TEQ`/`CMP`/`CMN` shapes are
  rejected rather than misdecoded. Earlier rounds of this (the `BX`
  mask-width bug) caught real bugs during development, which is exactly
  the kind of mistake this subset of the project is prone to; more test
  cases from more people is how this gets more trustworthy, see
  `docs/CONTRIBUTING.md`.

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
  needed for any real function's prologue/epilogue), `MLA` (`MUL`'s
  accumulate-form sibling, decode already rejects it explicitly so it's
  not silently wrong, just not implemented), register-specified shift
  amount (`LSL Rs`, not just `LSL #imm`), register-offset and
  post-indexed/writeback addressing for `LDR`/`STR`/`LDRB`/`STRB`, the
  logical ops computing C from the shifter's carry-out instead of just
  leaving it alone, then Thumb-2. Each addition should come with a
  hand-derived test case the way the existing ones work, see
  `docs/CONTRIBUTING.md`'s testing section.
- A real trampoline mechanism for `getTrampoline`: it can already find a
  requested symbol in a loaded guest library (`elf32.c`), but the real
  blocker is deeper than "generate a trampoline". A JNI native method's
  guest code expects `JNIEnv*`/`jobject` in its own registers, both real
  64-bit host pointers the 32-bit guest can't hold as-is; making guest
  code able to call back into real JNI functions (`NewStringUTF` and
  friends) at all needs some kind of 32-bit-handle-to-64-bit-pointer
  proxy layer first. That's a substantially bigger, riskier piece of
  work than anything in this file so far (closer to the roadmap's Phase
  4 "thunking" in `docs/ARCHITECTURE.md` than Phase 2), and not
  something to attempt without a real device to test against.
