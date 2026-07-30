// Modules: :core (shared Kotlin, used by :desktop), :desktop (Compose Desktop).
// The on-device UI is a WebUI in module/webroot/, not a Gradle module, see
// docs/ARCHITECTURE.md. Native translator lives in /native, built
// separately with the NDK. See docs/BUILDING.md.

pluginManagement {
    repositories {
        google()
        gradlePluginPortal()
        mavenCentral()
    }
}

dependencyResolutionManagement {
    repositories {
        google()
        mavenCentral()
    }
}

plugins {
    id("org.gradle.toolchains.foojay-resolver-convention") version "0.8.0"
}

rootProject.name = "Mango"

include(":core")
include(":desktop")
