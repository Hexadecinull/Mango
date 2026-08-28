package dev.mango.core

/** What the device currently supports, independent of Mango. */
data class DeviceProfile(
    val supportedAbis: Set<Abi>,
    val nativeBridgeActive: Boolean,
)

enum class Verdict { RUNS_NATIVELY, NEEDS_BRIDGE, BLOCKED }

data class CompatibilityReport(
    val verdict: Verdict,
    val notes: List<String>,
)

/** The "will this app work here" logic; ABIs and module-active state only, see docs/ARCHITECTURE.md (Option A). */
object CompatibilityChecker {
    fun check(
        apk: ApkAbiInfo,
        device: DeviceProfile,
    ): CompatibilityReport {
        if (!apk.hasAnyNativeLibs) {
            return CompatibilityReport(
                Verdict.RUNS_NATIVELY,
                listOf("No native libraries at all, architecture doesn't apply here."),
            )
        }

        if (apk.abisPresent.any { it in device.supportedAbis }) {
            return CompatibilityReport(
                Verdict.RUNS_NATIVELY,
                listOf("Already ships a native ABI this device supports directly."),
            )
        }

        val is32BitOnly =
            apk.abisPresent.isNotEmpty() &&
                apk.abisPresent.all { it == Abi.ARMEABI_V7A || it == Abi.X86 }

        if (!is32BitOnly) {
            return CompatibilityReport(
                Verdict.BLOCKED,
                listOf("Doesn't ship an ABI this device supports, and it's not a 32-bit-only case Mango handles."),
            )
        }

        return if (device.nativeBridgeActive) {
            CompatibilityReport(
                Verdict.NEEDS_BRIDGE,
                listOf("32-bit-only app. The Mango module is active, this should install and launch through the native bridge."),
            )
        } else {
            CompatibilityReport(
                Verdict.BLOCKED,
                listOf("32-bit-only app, but the Mango module isn't active on this device yet."),
            )
        }
    }
}
