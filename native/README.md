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
  `B`, and `BX`. Only the `AL` (always) condition is handled; conditional
  execution is unimplemented. This is genuinely a small subset, real apps
  will use far more of the ISA (Thumb-2, conditional execution, load/store,
  NEON, and so on all still need doing).
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
- `tests/test_interp.c`: two test programs, hand-encoded by working out the
  A32 bit patterns by hand and checking them against the ARMv7-A encoding.
  Both pass under `-Wall -Wextra -Werror -fsanitize=address,undefined`. One
  of them (the BX check) caught a real bit-mask width bug during
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

- More A32 instructions (load/store is a big, obviously-needed gap), then
  Thumb-2, then conditional execution. Each addition should come with a
  hand-derived test case the way the existing two work, see
  `docs/CONTRIBUTING.md`'s testing section.
- `loadLibrary`/`getTrampoline` in the shim: this is where "interpret a
  synthetic test program" turns into "actually run a real `.so`'s code",
  and is a substantially bigger jump than anything implemented so far.
