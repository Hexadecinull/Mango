#!/system/bin/sh
# Runs during install, inside the manager app (Magisk/KernelSU/APatch all
# support this file the same way).

ui_print "- Mango: ARM32 -> ARM64 native bridge translator"

if [ -n "$KSU" ]; then
  ui_print "- Detected a KernelSU-family root solution."
  ui_print "- Note: /system mounting needs a metamodule (e.g. meta-overlayfs)"
  ui_print "  on KernelSU/APatch, unlike Magisk which has this built in."
  ui_print "  See docs/USAGE.md if apps don't launch after installing this."
fi

if [ "$ARCH" != "arm64" ]; then
  abort "! Mango only supports arm64-v8a devices."
fi
