# Security Policy

## Why this matters more than usual here

Mango runs with root, patches other apps' binaries, and (once the native
translator exists) executes translated machine code inside another app's
process. That's a larger attack surface than a typical hobby app: a bug in
the translator's decoder, code generator, or JIT cache is a plausible path
to memory corruption, and a bug in the patching pipeline could produce a
maliciously-alterable APK. Please treat "this looks like a memory safety
bug in `native/`" as worth reporting even if you can't fully weaponize it.

## Supported versions

This project is early and doesn't have stable release branches yet. Until
it does, only the latest commit on `main` is supported. That will change
and be reflected here once tagged releases exist.

## Reporting a vulnerability

Please don't open a public GitHub issue for security bugs.

Instead, use GitHub's private vulnerability reporting for this repository
(Security tab -> Report a vulnerability). If that's unavailable to you for
some reason, open a regular issue titled "security contact request" with no
technical details, and a maintainer will follow up with a private channel.

Please include:

- What component is affected (`native/`, `core/`, `module/webroot/`,
  `desktop/`, CI).
- Steps to reproduce, or a minimal proof of concept.
- What you think the impact is (crash, memory corruption, arbitrary code
  execution, privilege escalation via the root shell, APK integrity
  bypass, etc).

## What to expect

This is a community project run by volunteers, so response times will vary.
As a target: acknowledgment within a week, and a plan (fix, mitigation, or
"not a vulnerability, here's why") within 30 days for anything credible.
Credit is given in the fix's changelog entry unless you ask not to be
named.

## Scope notes

- Bugs in the native translator (`native/`) that could lead to memory
  corruption or code execution when translating untrusted 32-bit code are
  the highest priority.
- Bugs in APK patching or re-signing (`core/`) that could let a patched APK
  be tampered with undetected are also high priority.
- The WebUI (`module/webroot/app.js`) builds shell commands from user
  input without escaping, on purpose: it only runs inside a WebView that
  already has root on your own device, so the only person who could
  inject anything is you, into your own shell. That's not itself
  something to report. What *is* worth reporting: any way for content
  Mango didn't ship (a remote page, another app, another module) to reach
  that `ksu.exec` bridge in the first place. That would be a flaw in
  either the root manager's WebView isolation or in something Mango did
  to weaken it, either way, please report it.
- Reports about the underlying Android root/permission model itself (not
  something Mango controls) are out of scope here; report those upstream.
- Social engineering, physical access attacks, and reports that require the
  attacker to already have root on the target device are generally out of
  scope, since Mango's whole premise is running with root.

## Safe harbor

Good-faith security research against your own device, in line with this
policy, won't be treated as a violation of `docs/TERMS.md`.
