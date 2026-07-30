package dev.mango.core

import java.io.File
import java.util.zip.ZipFile

/**
 * What native ABIs an APK ships lib/ folders for. This only reads ZIP
 * entries, no AndroidManifest.xml parsing, that's on purpose: ABI folders
 * under lib/ are enough to answer "will this run here", and avoiding a
 * binary AXML parser keeps this small and easy to trust. See
 * docs/ARCHITECTURE.md if you're wondering why manifest parsing (package
 * name, minSdk) isn't here yet.
 */
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
