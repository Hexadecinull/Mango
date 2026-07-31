// Declares plugins once so :core and :desktop don't each load their own
// copy of the Kotlin plugin (Gradle warns about that otherwise, even when
// the versions match). Subprojects apply these without a version.
plugins {
    alias(libs.plugins.kotlin.jvm) apply false
    alias(libs.plugins.compose.multiplatform) apply false
    alias(libs.plugins.compose.compiler) apply false
}
