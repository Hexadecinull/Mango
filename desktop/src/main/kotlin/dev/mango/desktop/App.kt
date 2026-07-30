package dev.mango.desktop

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import dev.mango.core.Abi
import dev.mango.core.ApkAbiInfo
import dev.mango.core.ApkAbiInspector
import java.awt.FileDialog
import java.awt.Frame
import java.io.File

/**
 * Inspect-only for now: no root here, see docs/USAGE.md's desktop section.
 * "push to device" (adb) is a planned next step, not implemented yet.
 */
@Composable
fun App() {
    var report by remember { mutableStateOf<ApkAbiInfo?>(null) }
    var pickedName by remember { mutableStateOf<String?>(null) }

    MaterialTheme {
        Surface(modifier = Modifier.fillMaxSize()) {
            Column(
                modifier = Modifier.fillMaxSize().padding(24.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center,
            ) {
                Text("Mango", style = MaterialTheme.typography.headlineMedium)
                Text(
                    "Inspect an APK, no root needed here.",
                    style = MaterialTheme.typography.bodyMedium,
                )

                Button(
                    onClick = {
                        val dialog = FileDialog(null as Frame?, "Select an APK", FileDialog.LOAD)
                        dialog.setFilenameFilter { _, name -> name.endsWith(".apk") }
                        dialog.isVisible = true
                        val dir = dialog.directory
                        val name = dialog.file
                        if (dir != null && name != null) {
                            pickedName = name
                            report = ApkAbiInspector.inspect(File(dir, name))
                        }
                    },
                    modifier = Modifier.padding(top = 16.dp),
                ) {
                    Text("Select APK")
                }

                pickedName?.let { Text("File: $it", modifier = Modifier.padding(top = 16.dp)) }
                report?.let { info ->
                    Text("ABIs present: ${info.abisPresent.joinToString { it.libDir }}")
                    Text(describeAbiInfo(info))
                }
            }
        }
    }
}

private fun describeAbiInfo(info: ApkAbiInfo): String = when {
    info.abisPresent.contains(Abi.ARM64_V8A) -> "Already has an arm64-v8a slice."
    info.abisPresent.contains(Abi.ARMEABI_V7A) -> "32-bit-only, needs the Mango module on-device."
    !info.hasAnyNativeLibs -> "No native libraries, architecture doesn't apply."
    else -> "Doesn't ship an ABI Mango currently handles."
}
