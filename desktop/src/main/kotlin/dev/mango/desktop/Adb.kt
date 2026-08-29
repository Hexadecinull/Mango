package dev.mango.desktop

import dev.mango.core.Abi
import dev.mango.core.DeviceProfile
import java.io.File
import java.util.concurrent.TimeUnit

/** Real adb-backed device queries and the module push, see docs/USAGE.md. */
object Adb {
    private const val TIMEOUT_SECONDS = 10L

    private fun run(vararg command: String): String? =
        try {
            val process = ProcessBuilder(*command).redirectErrorStream(true).start()
            val output = process.inputStream.bufferedReader().readText()
            val finished = process.waitFor(TIMEOUT_SECONDS, TimeUnit.SECONDS)
            if (finished && process.exitValue() == 0) output else null
        } catch (e: Exception) {
            null // adb not on PATH, no device attached, or it timed out
        }

    /** Null if adb isn't reachable, no device answered, or the reply didn't parse as any
     * ABI Mango knows; callers should treat that as "can't tell", not "incompatible". */
    fun queryDeviceProfile(): DeviceProfile? {
        val abiList = run("adb", "shell", "getprop", "ro.product.cpu.abilist") ?: return null
        val supportedAbis = abiList.trim().split(",").mapNotNull { Abi.fromLibDir(it.trim()) }.toSet()
        if (supportedAbis.isEmpty()) {
            return null
        }
        val nativeBridge = run("adb", "shell", "getprop", "ro.dalvik.vm.native.bridge")
        return DeviceProfile(
            supportedAbis = supportedAbis,
            nativeBridgeActive = nativeBridge?.trim() == "libmango_translator.so",
        )
    }

    /** Pushes a packaged module zip (scripts/package_module.sh's output) to the device's
     * Downloads folder; flashing it in the root manager app is still a manual step. */
    fun pushModule(zip: File): Result<String> {
        if (!zip.isFile) {
            return Result.failure(
                IllegalArgumentException("${zip.path} doesn't exist, run scripts/package_module.sh first"),
            )
        }
        val output = run("adb", "push", zip.path, "/sdcard/Download/")
        return if (output != null) {
            Result.success(output)
        } else {
            Result.failure(IllegalStateException("adb push failed, check a device is connected and authorized"))
        }
    }
}
