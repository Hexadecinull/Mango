#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L /* mkstemp; still satisfies native_bridge.h's own floor */
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE /* glibc gates mkstemp's declaration behind this too */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mango/native_bridge.h"

extern struct NativeBridgeCallbacks NativeBridgeItf;

/*
 * Same fixture as native/tests/test_elf32.c (see that file for how it was
 * built and independently verified with readelf): a minimal ELF32 shared
 * object exporting mango_add and mango_answer. loadLibrary takes a file
 * path, not a buffer, so this gets written to a temp file first.
 */
static const uint8_t kSynthElf[] = {
    0x7Fu, 0x45u, 0x4Cu, 0x46u, 0x01u, 0x01u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x03u, 0x00u, 0x28u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x34u, 0x00u, 0x00u, 0x00u, 0x9Cu, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x34u, 0x00u, 0x20u, 0x00u, 0x01u, 0x00u, 0x28u, 0x00u,
    0x03u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x14u, 0x01u, 0x00u, 0x00u,
    0x14u, 0x01u, 0x00u, 0x00u, 0x05u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x40u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x12u, 0x00u, 0x01u, 0x00u, 0x0Bu, 0x00u, 0x00u, 0x00u,
    0x50u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x12u, 0x00u, 0x01u, 0x00u,
    0x00u, 0x6Du, 0x61u, 0x6Eu, 0x67u, 0x6Fu, 0x5Fu, 0x61u, 0x64u, 0x64u, 0x00u, 0x6Du,
    0x61u, 0x6Eu, 0x67u, 0x6Fu, 0x5Fu, 0x61u, 0x6Eu, 0x73u, 0x77u, 0x65u, 0x72u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x0Bu, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x54u, 0x00u, 0x00u, 0x00u, 0x54u, 0x00u, 0x00u, 0x00u,
    0x30u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x04u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x84u, 0x00u, 0x00u, 0x00u,
    0x84u, 0x00u, 0x00u, 0x00u, 0x18u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
};

int main(void) {
  /* The catastrophic bug this whole file exists to catch: libnativebridge's
   * own LoadNativeBridge() calls isCompatibleWith(NAMESPACE_VERSION=3)
   * unconditionally at load time and rejects the ENTIRE bridge if it's
   * false, confirmed against AOSP's libnativebridge/native_bridge.cc.
   * This isn't gated by .version; it's an independent check we implement. */
  if (!NativeBridgeItf.isCompatibleWith(2) || !NativeBridgeItf.isCompatibleWith(3)) {
    fprintf(stderr, "FAIL: isCompatibleWith(2) and (3) must both be true, or ART rejects the whole "
                    "bridge at load time\n");
    return 1;
  }
  if (NativeBridgeItf.isCompatibleWith(4)) {
    fprintf(stderr, "FAIL: isCompatibleWith(4) should be false, we don't implement getVendorNamespace\n");
    return 1;
  }
  printf("ok: isCompatibleWith(3) is true, the load-time acceptance gate is fixed\n");

  if (NativeBridgeItf.version != 3 || !NativeBridgeItf.unloadLibrary || !NativeBridgeItf.getError ||
      !NativeBridgeItf.isPathSupported || !NativeBridgeItf.initAnonymousNamespace ||
      !NativeBridgeItf.createNamespace || !NativeBridgeItf.linkNamespaces ||
      !NativeBridgeItf.loadLibraryExt) {
    fprintf(stderr, "FAIL: version=3 but not every v3 field it obligates is filled in\n");
    return 1;
  }
  if (NativeBridgeItf.getVendorNamespace != NULL) {
    fprintf(stderr, "FAIL: getVendorNamespace should be NULL, version 4 isn't claimed\n");
    return 1;
  }
  printf("ok: every v3 field claimed by version=3 is filled in, getVendorNamespace correctly isn't\n");

  char path[] = "/tmp/mango_shim_test_XXXXXX";
  int fd = mkstemp(path);
  if (fd < 0 || write(fd, kSynthElf, sizeof(kSynthElf)) != (ssize_t)sizeof(kSynthElf)) {
    fprintf(stderr, "FAIL: couldn't write the test fixture to a temp file\n");
    return 1;
  }
  close(fd);

  void* handle = NativeBridgeItf.loadLibrary(path, 0);
  if (!handle) {
    fprintf(stderr, "FAIL: loadLibrary rejected a valid ARM32 .so\n");
    unlink(path);
    return 1;
  }
  printf("ok: loadLibrary succeeds on a real ARM32 .so\n");

  /* Known, documented gap: the symbol resolves internally, but there's no
   * trampoline-generation mechanism yet, so this is NULL on purpose. */
  void* trampoline = NativeBridgeItf.getTrampoline(handle, "mango_add", "(II)I", 5);
  if (trampoline != NULL) {
    fprintf(stderr, "FAIL: getTrampoline returned non-NULL; either it's implemented now (update this "
                    "test) or something is wrong\n");
    unlink(path);
    return 1;
  }
  printf("ok: getTrampoline resolves the symbol internally without crashing (still NULL, as documented)\n");

  if (NativeBridgeItf.unloadLibrary(handle) != 0) {
    fprintf(stderr, "FAIL: unloadLibrary returned nonzero for a real handle\n");
    unlink(path);
    return 1;
  }
  printf("ok: unloadLibrary succeeds (leak-checked by the ASan build in CI)\n");

  if (!NativeBridgeItf.initAnonymousNamespace("libfoo.so", "/vendor/lib") ||
      !NativeBridgeItf.createNamespace("ns", NULL, NULL, 0, NULL, NULL) ||
      !NativeBridgeItf.linkNamespaces(NULL, NULL, "libfoo.so")) {
    fprintf(stderr, "FAIL: a namespace stub returned failure; they're all supposed to be safe no-ops\n");
    unlink(path);
    return 1;
  }
  void* handle2 = NativeBridgeItf.loadLibraryExt(path, 0, NULL);
  if (!handle2 || NativeBridgeItf.unloadLibrary(handle2) != 0) {
    fprintf(stderr, "FAIL: loadLibraryExt/unloadLibrary round trip failed\n");
    unlink(path);
    return 1;
  }
  printf("ok: namespace stubs behave safely (no crash, sane return values)\n");

  unlink(path);
  printf("all native_bridge_shim tests passed\n");
  return 0;
}
