# Privacy Policy

Last updated: 2026-07-29

## Short version

Mango doesn't collect, transmit, or sell any of your data. It has no
backend server, no analytics, no crash reporting, and no ads. Everything it
does happens locally on your device or computer.

## What Mango touches, and where it stays

- **APK files you feed it.** Read, patched, and written to a location you
  choose. Never uploaded anywhere.
- **Root shell access.** Used only to install patched APKs and, where
  needed, to read/write app-adjacent files on your own device. Mango does
  not use root access to read data belonging to other apps, and does not
  transmit anything it reads.
- **Logs.** Any command log the WebUI shows you (the "advanced" toggle)
  lives in the page, not written to disk anywhere. If you copy one into a
  GitHub issue, that's a choice you make, not something Mango does on its
  own.
- **Settings.** The WebUI doesn't currently store any settings at all. If
  that changes, it'll be local to the WebView (browser `localStorage`,
  cleared if your root manager's app data is cleared), never synced
  anywhere, and this file will be updated to say so.

## Network access

The WebUI and desktop app don't make network requests. If a future
version adds an optional feature that does (for example, an opt-in update
checker), it will be off by default and documented here before it ships.

## Third-party code

The WebUI (`module/webroot/`) is plain HTML/CSS/JS with no dependencies
at all, nothing to vet there. The desktop app and `core/` depend on a
small number of open-source libraries (listed in `docs/BUILDING.md` and
the Gradle version catalog); none of them are analytics or ad SDKs, and
none were added for that purpose.

## Changes to this policy

If this ever changes, meaning if a future version starts collecting or
transmitting anything, that change will be called out explicitly in the
changelog and in this file, not buried in a diff.

## Contact

Questions go in a GitHub issue or discussion on the project repository.
