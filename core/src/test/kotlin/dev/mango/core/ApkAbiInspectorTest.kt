package dev.mango.core

import org.junit.jupiter.api.Test
import org.junit.jupiter.api.io.TempDir
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class ApkAbiInspectorTest {
    private fun fakeApk(
        dir: File,
        vararg libPaths: String,
    ): File {
        val apk = File(dir, "fake.apk")
        ZipOutputStream(apk.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("AndroidManifest.xml"))
            zip.write(byteArrayOf(1, 2, 3))
            zip.closeEntry()
            for (path in libPaths) {
                zip.putNextEntry(ZipEntry(path))
                zip.write(byteArrayOf(0x7f, 0x45, 0x4c, 0x46))
                zip.closeEntry()
            }
        }
        return apk
    }

    @Test
    fun `detects a single supported abi`(
        @TempDir dir: File,
    ) {
        val apk = fakeApk(dir, "lib/arm64-v8a/libfoo.so")
        val info = ApkAbiInspector.inspect(apk)
        assertEquals(setOf(Abi.ARM64_V8A), info.abisPresent)
        assertTrue(info.hasAnyNativeLibs)
    }

    @Test
    fun `detects a 32-bit-only app`(
        @TempDir dir: File,
    ) {
        val apk = fakeApk(dir, "lib/armeabi-v7a/libfoo.so", "lib/armeabi-v7a/libbar.so")
        val info = ApkAbiInspector.inspect(apk)
        assertEquals(setOf(Abi.ARMEABI_V7A), info.abisPresent)
    }

    @Test
    fun `detects multiple abis`(
        @TempDir dir: File,
    ) {
        val apk = fakeApk(dir, "lib/armeabi-v7a/libfoo.so", "lib/arm64-v8a/libfoo.so")
        val info = ApkAbiInspector.inspect(apk)
        assertEquals(setOf(Abi.ARMEABI_V7A, Abi.ARM64_V8A), info.abisPresent)
    }

    @Test
    fun `app with no native libs at all`(
        @TempDir dir: File,
    ) {
        val apk = fakeApk(dir)
        val info = ApkAbiInspector.inspect(apk)
        assertEquals(emptySet<Abi>(), info.abisPresent)
        assertFalse(info.hasAnyNativeLibs)
    }
}
