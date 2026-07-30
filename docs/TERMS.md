# Terms of Service

Last updated: 2026-07-29

## 1. What this document is, and isn't

Mango's source code is licensed under GPL-3.0 (see `LICENSE`). That license
already grants you the right to run, study, modify, and share the software
for any purpose, and by design it cannot be narrowed by a side document like
this one. Nothing here removes or limits any right the GPL gives you over
the code itself.

What this document *does* cover is narrower: the project's own stated
purpose, and the rules for using spaces the maintainers run, such as the
issue tracker, discussions, and any official builds or website. Think of it
as "what we're building this for and what we'll help with," not "what you're
legally permitted to compile and run."

## 2. Intended purpose

Mango exists to let people run 32-bit-only (armeabi-v7a) Android apps they
already legitimately own on newer 64-bit-only (arm64-v8a) hardware, in cases
such as:

- The app was abandoned by its developer and never received a 64-bit build.
- The developer no longer exists, or the app is no longer maintained in any
  form, but the app itself still works fine otherwise.
- Niche or special-case hardware/firmware combinations where no 32-bit
  compatibility layer is available at all.

This mirrors what Tango (Amanieu Systems) already does at the OEM/ROM level
for a handful of devices. Mango is an independent, from-scratch project and
has no affiliation with Amanieu Systems or Tango, and does not use or
contain any of their code.

## 3. What Mango is not for

Mango is not intended, designed, or maintained to help anyone bypass
purchase requirements, license checks, DRM, subscription paywalls, or any
other access control that an app or its developer puts in place. Using
Mango, or any patch, plugin, or fork of it, for that purpose falls outside
the project's intended use.

In practice this means:

- The maintainers will not knowingly accept, review, or merge contributions
  whose primary purpose is defeating license verification, purchase checks,
  or similar restrictions.
- Issues or discussions asking for help with that kind of use may be closed
  without a detailed response.
- This is a statement of project intent and community norms, not a
  technical restriction. Mango cannot inspect what any given user does with
  it, and the GPL does not allow the software itself to enforce this.

## 4. Your responsibilities

If you use Mango:

- You're responsible for complying with the law in your jurisdiction,
  including copyright law, and with the terms of service of any app you
  patch.
- You're responsible for only patching APKs you're legitimately entitled to
  run.
- Rooting a device and patching system-adjacent files carries real risk,
  up to and including a bricked device or a broken app. You take that risk
  on yourself; see `LICENSE` sections 15-16 for the underlying "no warranty"
  terms.
- Mango is not a replacement for judgment. If an app's own terms forbid
  reverse engineering or modification, patching it may put you in breach of
  that agreement even where copyright law itself would allow it.

## 5. No warranty, no guarantee of compatibility

Mango is a community passion project, not a commercial product, and it
makes no promise that any given app will work after patching, or that
patching won't damage the app, your data, or your device. See `LICENSE`
for the full disclaimer that governs the software.

## 6. Changes to these terms

As the project and community grow, these terms may change. Material changes
will be noted in the project's changelog.

## 7. Contact

Questions about these terms belong in a GitHub issue or discussion on the
project repository.
