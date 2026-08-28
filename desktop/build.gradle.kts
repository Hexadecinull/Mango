import org.jetbrains.compose.desktop.application.dsl.TargetFormat

plugins {
    alias(libs.plugins.kotlin.jvm)
    alias(libs.plugins.compose.multiplatform)
    alias(libs.plugins.compose.compiler)
}

kotlin {
    jvmToolchain(21)
}

dependencies {
    implementation(project(":core"))
    implementation(compose.desktop.currentOs)
    implementation(compose.material3)
    implementation(compose.foundation)
    implementation(libs.kotlinx.coroutines.core)
}

compose.desktop {
    application {
        mainClass = "dev.mango.desktop.MainKt"

        nativeDistributions {
            // Windows/macOS APK-inspection helper, not Linux support (that's
            // linux/ now, a real standalone translator, see its README).
            targetFormats(TargetFormat.Msi, TargetFormat.Dmg)
            packageName = "Mango"
            packageVersion = "0.1.0"
        }
    }
}
