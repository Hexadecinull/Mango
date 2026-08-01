/*
 * Adapted from AOSP's libnativebridge, native_bridge.h (Apache-2.0),
 * android.googlesource.com/platform/system/core, tag android-8.1.0_r1.
 * Original copyright:
 *
 *   Copyright (C) 2014 The Android Open Source Project
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 * Changes from the original: trimmed to the struct shapes an implementation
 * (NBImpl) needs to fill in, with the free functions AOSP's own runtime
 * calls removed since we're not implementing those. Namespace-related v3
 * callbacks are declared but not yet wired up in mango's shim, see the
 * TODO in native/src/native_bridge_shim.c. Check this against the current
 * AOSP tree for the API level you're targeting, this interface is old and
 * stable but not guaranteed unchanged.
 */
#ifndef MANGO_NATIVE_BRIDGE_H_
#define MANGO_NATIVE_BRIDGE_H_

/* Needed for siginfo_t on glibc hosts (bionic doesn't gate it the same
 * way). Defined before any system headers pull in <signal.h> first. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include <jni.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct NativeBridgeRuntimeCallbacks;
struct NativeBridgeRuntimeValues;
struct native_bridge_namespace_t;

/* Signal handler signature the runtime chains into ours. */
typedef bool (*NativeBridgeSignalHandlerFn)(int, siginfo_t*, void*);

/* The symbol name ART's libnativebridge dlsym()s for after loading us. */
#define MANGO_NATIVE_BRIDGE_SYMBOL "NativeBridgeItf"

struct NativeBridgeCallbacks {
  uint32_t version;

  /* Called once by the runtime before first use. */
  bool (*initialize)(const struct NativeBridgeRuntimeCallbacks* runtime_cbs,
                     const char* private_dir, const char* instruction_set);

  /* Load a guest (armeabi-v7a) .so and hand back an opaque handle. */
  void* (*loadLibrary)(const char* libpath, int flag);

  /* Return a trampoline the runtime can call as if it were the real
   * native method, matching a JNI shorty signature. This is where a
   * call into guest code enters the translator. */
  void* (*getTrampoline)(void* handle, const char* name, const char* shorty, uint32_t len);

  /* Whether libpath is a library this bridge can handle. */
  bool (*isSupported)(const char* libpath);

  /* Optional environment values to apply after fork, per ISA. */
  const struct NativeBridgeRuntimeValues* (*getAppEnv)(const char* instruction_set);

  /* v2 */
  bool (*isCompatibleWith)(uint32_t bridge_version);
  NativeBridgeSignalHandlerFn (*getSignalHandler)(int signal);

  /* v3, namespace-aware variants. Not wired up yet, see shim TODO. */
  int (*unloadLibrary)(void* handle);
  const char* (*getError)(void);
  bool (*isPathSupported)(const char* library_path);
  bool (*initAnonymousNamespace)(const char* public_ns_sonames, const char* anon_ns_library_path);
  struct native_bridge_namespace_t* (*createNamespace)(const char* name,
                                                        const char* ld_library_path,
                                                        const char* default_library_path,
                                                        uint64_t type,
                                                        const char* permitted_when_isolated_path,
                                                        struct native_bridge_namespace_t* parent_ns);
  bool (*linkNamespaces)(struct native_bridge_namespace_t* from,
                         struct native_bridge_namespace_t* to, const char* shared_libs_sonames);
  void* (*loadLibraryExt)(const char* libpath, int flag, struct native_bridge_namespace_t* ns);
  struct native_bridge_namespace_t* (*getVendorNamespace)(void);
};

struct NativeBridgeRuntimeCallbacks {
  const char* (*getMethodShorty)(JNIEnv* env, jmethodID mid);
  uint32_t (*getNativeMethodCount)(JNIEnv* env, jclass clazz);
  uint32_t (*getNativeMethods)(JNIEnv* env, jclass clazz, JNINativeMethod* methods,
                               uint32_t method_count);
};

#ifdef __cplusplus
}
#endif

#endif  /* MANGO_NATIVE_BRIDGE_H_ */
