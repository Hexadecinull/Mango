package dev.mango.core

enum class RootManagerKind { MAGISK, KERNEL_SU_FAMILY, UNKNOWN }

/** Pure classification of $KSU/$MAGISK_VER; callers own the actual shell exec, see docs/ARCHITECTURE.md. */
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
