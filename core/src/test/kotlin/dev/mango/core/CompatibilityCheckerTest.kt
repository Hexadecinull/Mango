package dev.mango.core

import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

class CompatibilityCheckerTest {
    @Test
    fun `native abi present runs natively`() {
        val apk = ApkAbiInfo(abisPresent = setOf(Abi.ARM64_V8A), hasAnyNativeLibs = true)
        val device = DeviceProfile(supportedAbis = setOf(Abi.ARM64_V8A), nativeBridgeActive = false)
        assertEquals(Verdict.RUNS_NATIVELY, CompatibilityChecker.check(apk, device).verdict)
    }

    @Test
    fun `32-bit only app needs bridge when module active`() {
        val apk = ApkAbiInfo(abisPresent = setOf(Abi.ARMEABI_V7A), hasAnyNativeLibs = true)
        val device = DeviceProfile(supportedAbis = setOf(Abi.ARM64_V8A), nativeBridgeActive = true)
        assertEquals(Verdict.NEEDS_BRIDGE, CompatibilityChecker.check(apk, device).verdict)
    }

    @Test
    fun `32-bit only app blocked without module`() {
        val apk = ApkAbiInfo(abisPresent = setOf(Abi.ARMEABI_V7A), hasAnyNativeLibs = true)
        val device = DeviceProfile(supportedAbis = setOf(Abi.ARM64_V8A), nativeBridgeActive = false)
        assertEquals(Verdict.BLOCKED, CompatibilityChecker.check(apk, device).verdict)
    }

    @Test
    fun `app with no native libs runs natively trivially`() {
        val apk = ApkAbiInfo(abisPresent = emptySet<Abi>(), hasAnyNativeLibs = false)
        val device = DeviceProfile(supportedAbis = setOf(Abi.ARM64_V8A), nativeBridgeActive = false)
        assertEquals(Verdict.RUNS_NATIVELY, CompatibilityChecker.check(apk, device).verdict)
    }

    @Test
    fun `unsupported non-32-bit abi is blocked, not offered the bridge`() {
        val apk = ApkAbiInfo(abisPresent = setOf(Abi.X86_64), hasAnyNativeLibs = true)
        val device = DeviceProfile(supportedAbis = setOf(Abi.ARM64_V8A), nativeBridgeActive = true)
        assertEquals(Verdict.BLOCKED, CompatibilityChecker.check(apk, device).verdict)
    }
}
