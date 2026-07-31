package dev.mango.core

enum class RootManagerKind { MAGISK, KERNEL_SU_FAMILY, UNKNOWN }

/**
 * Pure classification. Actual shell execution (reading $KSU, running
 * `magisk -v`) happens wherever a caller wires up a real root shell (the
 * WebUI in module/webroot/, for instance), this just interprets the
 * result, so it's testable without a device. $KSU=true is how KernelSU
 * (and KernelSU-Next, and apparently APatch) mark their scripts; Magisk
 * sets $MAGISK_VER instead. See docs/ARCHITECTURE.md for the
 * module-compat research this is based on.
 */
object RootManagerDetector {
    fun classify(
        ksuEnvValue: String?,
        magiskVersionName: String?,
    ): RootManagerKind =
        when {
            ksuEnvValue == "true" -> RootManagerKind.KERNEL_SU_FAMILY
            !magiskVersionName.isNullOrBlank() -> RootManagerKind.MAGISK
            else -> RootManagerKind.UNKNOWN
        }
}
