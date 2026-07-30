package dev.mango.core

/** Simple vs advanced mode, switchable in Settings per the original idea. */
sealed interface PatchOptions {
    data object Simple : PatchOptions

    data class Advanced(
        val forceAbi: Abi? = null,
        val skipModuleCheck: Boolean = false,
        val verboseLogging: Boolean = false,
    ) : PatchOptions
}
