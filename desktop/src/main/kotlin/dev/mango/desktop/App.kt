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
import dev.mango.core.ApkAbiInfo
import dev.mango.core.ApkAbiInspector
import dev.mango.core.CompatibilityChecker
import dev.mango.core.CompatibilityReport
import dev.mango.core.DeviceProfile
import java.awt.FileDialog
import java.awt.Frame
import java.io.File

/** No root itself; adb (Adb.kt) is how it reaches a device, see docs/USAGE.md. */
@Composable
fun App() {
    var apkInfo by remember { mutableStateOf<ApkAbiInfo?>(null) }
    var pickedName by remember { mutableStateOf<String?>(null) }
    var deviceProfile by remember { mutableStateOf<DeviceProfile?>(null) }
    var deviceCheckAttempted by remember { mutableStateOf(false) }
    var pushStatus by remember { mutableStateOf<String?>(null) }

    val report: CompatibilityReport? =
        apkInfo?.let { info -> deviceProfile?.let { device -> CompatibilityChecker.check(info, device) } }

    MaterialTheme {
        Surface(modifier = Modifier.fillMaxSize()) {
            Column(
                modifier = Modifier.fillMaxSize().padding(24.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center,
            ) {
                Text("Mango", style = MaterialTheme.typography.headlineMedium)
                Text(
                    "Inspect an APK, check it against a connected device, no root here.",
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
                            apkInfo = ApkAbiInspector.inspect(File(dir, name))
                        }
                    },
                    modifier = Modifier.padding(top = 16.dp),
                ) {
                    Text("Select APK")
                }

                pickedName?.let { Text("File: $it", modifier = Modifier.padding(top = 16.dp)) }
                apkInfo?.let { info ->
                    Text("ABIs present: ${info.abisPresent.joinToString { it.libDir }}")
                }

                Button(
                    onClick = {
                        deviceCheckAttempted = true
                        deviceProfile = Adb.queryDeviceProfile()
                    },
                    modifier = Modifier.padding(top = 16.dp),
                ) {
                    Text("Check connected device")
                }

                if (deviceCheckAttempted && deviceProfile == null) {
                    Text("No device found; connect one over adb and authorize it, then try again.")
                }
                deviceProfile?.let { device ->
                    Text("Device ABIs: ${device.supportedAbis.joinToString { it.libDir }}")
                    Text(
                        if (device.nativeBridgeActive) {
                            "Mango's native bridge is active on this device."
                        } else {
                            "Mango's native bridge is not active on this device."
                        },
                    )
                }

                report?.let { r ->
                    Text("Verdict: ${r.verdict}", modifier = Modifier.padding(top = 16.dp))
                    r.notes.forEach { Text(it) }
                }

                Button(
                    onClick = {
                        val zip = File("build/mango-module.zip")
                        pushStatus =
                            Adb.pushModule(zip).fold(
                                onSuccess = { "Pushed to /sdcard/Download/. Flash it from your root manager app." },
                                onFailure = { it.message ?: "adb push failed" },
                            )
                    },
                    modifier = Modifier.padding(top = 16.dp),
                ) {
                    Text("Push module to device")
                }
                pushStatus?.let { Text(it) }
            }
        }
    }
}
