package dev.mango.core

import java.io.File
import java.util.zip.ZipFile

/** What native ABIs an APK ships lib/ folders for; reads ZIP entries only, no AndroidManifest.xml parsing (see docs/ARCHITECTURE.md for why). */
data class ApkAbiInfo(
    val abisPresent: Set<Abi>,
    val hasAnyNativeLibs: Boolean,
)

object ApkAbiInspector {
    private val libEntryRegex = Regex("^lib/([^/]+)/[^/]+$")

    fun inspect(apk: File): ApkAbiInfo {
        val found = mutableSetOf<Abi>()
        var sawAnyLib = false

        ZipFile(apk).use { zip ->
            val entries = zip.entries()
            while (entries.hasMoreElements()) {
                val entry = entries.nextElement()
                val match = libEntryRegex.matchEntire(entry.name) ?: continue
                sawAnyLib = true
                Abi.fromLibDir(match.groupValues[1])?.let { found.add(it) }
            }
        }

        return ApkAbiInfo(abisPresent = found, hasAnyNativeLibs = sawAnyLib)
    }
}
