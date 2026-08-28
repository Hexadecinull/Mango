# Contributing to Mango

Mango is a volunteer, passion-project alternative to Tango. There's no
company behind it and no roadmap set in stone, just people who want 32-bit
apps to keep working on 64-bit-only phones. If that's you, you're welcome
here, at any skill level.

Before contributing, please read `docs/CODE_OF_CONDUCT.md` and
`docs/TERMS.md`. The short version of the latter: this project is for
running apps you already own, not for defeating purchase or license
checks. Contributions aimed at the latter won't be accepted.

## Ways to help that don't require writing C++

- Try the WebUI against a real app and report what broke. Real-world
  failure reports are worth more than speculative code right now.
- Improve the docs. If something in here confused you, it'll confuse the
  next person too.
- Triage issues: reproduce bugs, ask for missing details, link duplicates.
- Design/UX for the WebUI (`module/webroot/`, plain HTML/CSS/JS) or the
  desktop app.
- Test the WebUI on a root manager other than the one it was written
  against; see `docs/ARCHITECTURE.md`'s note on unconfirmed cross-manager
  JS bridge compatibility.
- Write tests, even for code you didn't write.

## Project layout

```
core/     shared Kotlin patch engine (APK inspection, compatibility
          checks), used by desktop/
desktop/  Windows/macOS APK-inspection helper app (Compose Desktop)
native/   the ARM32 -> ARM64 translator for Android (C, CMake, standalone)
linux/    early standalone ARM32 -> ARM64 support for Linux itself
          (C, CMake, standalone; reuses native/'s decoder+interpreter)
module/   the Magisk/KernelSU/KernelSU-Next/APatch module;
          module/webroot/ is the on-device WebUI (plain HTML/CSS/JS,
          no build step, no Gradle)
docs/     you are here
```

See `docs/ARCHITECTURE.md` for how these fit together, and
`docs/BUILDING.md` for how to build each one.

## Before you open a PR

1. Check open issues and PRs so you're not duplicating work in progress.
2. For anything nontrivial, open an issue first to talk through the
   approach. This saves everyone rework, especially in `native/`, where
   the "right" approach is genuinely still an open question.
3. Keep PRs focused. A PR that does one thing is easy to review; a PR
   that also reformats three unrelated files is not.

## Code style

- Kotlin: follow the official Kotlin style guide
  (`kotlin.code.style=official` is already set in `gradle.properties`).
  Let ktlint (run by the lint CI) settle formatting arguments. Composable
  functions are exempted from the naming rule via `.editorconfig`, that's
  intentional, not a gap to fix.
- C/C++: formatted per `.clang-format` (Google-based, 2-space indent, left
  pointer alignment). Run `scripts/format_native.sh` before committing, or
  the lint CI's `clang-format --dry-run` will flag it.
- Comments should explain *why*, not restate the code. Skip comments that
  just narrate what the next line obviously does.
- Commit messages: a short summary line, and a body if the "why" isn't
  obvious from the summary alone. No fixed format is enforced.

## Native engine contributions

`native/` is the hardest and least finished part of this project by far.
If you're comfortable with ARM assembly, instruction decoding, JIT
compilers, or projects like QEMU/box64/FEX-Emu, this is where the help
matters most. Read `docs/ARCHITECTURE.md` first: it's honest about what's
a working proof of concept versus what's still a design sketch.

## Testing

- `core/` has unit tests (`./gradlew :core:test`). New patching logic
  should come with tests where practical.
- `native/` has a small test harness in `native/tests/`. Given the memory
  safety stakes described in `docs/SECURITY.md`, PRs touching the decoder
  or code generator should include a test case reproducing the input
  that motivated the change.
- `module/webroot/` has `app.test.js` (`node --test module/webroot/
  app.test.js`) covering its pure logic, including `checkCompatibility`,
  which is still hand-synced against `core/`'s Kotlin version rather than
  shared code, so check both when you change either. Manual, on-device
  testing still matters most for anything touching the `ksu` bridge
  itself, see `docs/BUILDING.md`'s local-preview tip.
- `linux/` has `mango_linux_selfcheck`, a much smaller sibling of
  `native/tests/`' harness proving the shared core builds standalone; the
  same test-case-per-decoder-change bar above applies here too, since it
  shares `native/`'s decoder and interpreter directly.
- There's no device lab. If you can test on real hardware (ideally a
  device that's actually 64-bit-only), say what you tested in the PR
  description.

## License

By contributing, you agree your contribution is licensed under GPL-3.0,
the same as the rest of the project (see `LICENSE`).
