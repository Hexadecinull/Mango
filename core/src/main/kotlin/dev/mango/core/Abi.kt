package dev.mango.core

/** Native library ABIs Android recognizes, and their lib/ folder name in an APK. */
enum class Abi(
    val libDir: String,
) {
    ARMEABI_V7A("armeabi-v7a"),
    ARM64_V8A("arm64-v8a"),
    X86("x86"),
    X86_64("x86_64"),
    ;

    companion object {
        fun fromLibDir(name: String): Abi? = entries.find { it.libDir == name }
    }
}
