/* Must come before any system header, see mango/native_bridge.h for why. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

/* Implements native/include/mango/native_bridge.h, exported as "NativeBridgeItf". */
#include <elf.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "mango/native_bridge.h"

static bool mango_is_arm32_elf(const char* libpath) {
  int fd = open(libpath, O_RDONLY);
  if (fd < 0) {
    return false;
  }

  Elf32_Ehdr hdr;
  ssize_t n = read(fd, &hdr, sizeof(hdr));
  close(fd);

  if (n != (ssize_t)sizeof(hdr)) {
    return false;
  }
  if (memcmp(hdr.e_ident, ELFMAG, SELFMAG) != 0) {
    return false;
  }
  if (hdr.e_ident[EI_CLASS] != ELFCLASS32) {
    return false;
  }
  return hdr.e_machine == EM_ARM;
}

static bool mango_initialize(const struct NativeBridgeRuntimeCallbacks* runtime_cbs,
                             const char* private_dir, const char* instruction_set) {
  (void)runtime_cbs;
  (void)private_dir;
  (void)instruction_set;
  return true; /* TODO: stash runtime_cbs for getTrampoline's JNI lookups */
}

static void* mango_load_library(const char* libpath, int flag) {
  (void)flag;
  if (!mango_is_arm32_elf(libpath)) {
    return NULL;
  }
  /* TODO: actually translate the guest library; NULL here means "recognized, not runnable yet" */
  return NULL;
}

static void* mango_get_trampoline(void* handle, const char* name, const char* shorty,
                                  uint32_t len) {
  (void)handle;
  (void)name;
  (void)shorty;
  (void)len;
  return NULL; /* nothing loads successfully yet, so this never gets called */
}

static bool mango_is_supported(const char* libpath) { return mango_is_arm32_elf(libpath); }

static const struct NativeBridgeRuntimeValues* mango_get_app_env(const char* instruction_set) {
  (void)instruction_set;
  return NULL;
}

static bool mango_is_compatible_with(uint32_t bridge_version) { return bridge_version <= 2; }

static NativeBridgeSignalHandlerFn mango_get_signal_handler(int signal) {
  (void)signal;
  return NULL;
}

__attribute__((visibility("default"))) struct NativeBridgeCallbacks NativeBridgeItf = {
    .version = 2,
    .initialize = mango_initialize,
    .loadLibrary = mango_load_library,
    .getTrampoline = mango_get_trampoline,
    .isSupported = mango_is_supported,
    .getAppEnv = mango_get_app_env,
    .isCompatibleWith = mango_is_compatible_with,
    .getSignalHandler = mango_get_signal_handler,
    /* v3 (namespace-aware) callbacks intentionally left zeroed for now. */
};
