/*
 * NOT a real JNI implementation, or even a real subset of one. This
 * exists solely so native_bridge_shim.c's *host* test build has
 * something to satisfy its `#include <jni.h>`, since the real one only
 * comes from the Android NDK (see docs/BUILDING.md), which this host
 * build deliberately isn't using, the same reason native_bridge_shim.c
 * itself isn't part of mango_core. Never included by the real Android
 * build: android.toolchain.cmake's own jni.h wins there since this
 * directory is never on that build's include path. Do not use this for
 * anything other than compiling native_bridge_shim.c's tests.
 */
#ifndef MANGO_TESTS_FAKE_JNI_H_
#define MANGO_TESTS_FAKE_JNI_H_

typedef struct JNIEnv JNIEnv;
typedef void* jclass;
typedef void* jmethodID;
typedef struct {
  const char* name;
  const char* signature;
  void* fnPtr;
} JNINativeMethod;

#endif /* MANGO_TESTS_FAKE_JNI_H_ */
