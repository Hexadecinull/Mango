#!/system/bin/sh
# Runs early, blocking. Keep this fast: no setprop here (deadlocks boot),
# system.prop is already handled for us via resetprop.

MODDIR=${0%/*}

if [ -n "$KSU" ]; then
  ROOT_IMPL="KernelSU-family"
else
  ROOT_IMPL="Magisk"
fi

log -t Mango "post-fs-data: root implementation looks like $ROOT_IMPL"

if [ ! -f "$MODDIR/system/lib64/libmango_translator.so" ]; then
  log -t Mango "translator .so missing, this module was packaged incorrectly. See docs/BUILDING.md."
fi
