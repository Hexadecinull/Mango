package dev.mango.core

import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

class RootManagerDetectorTest {
    @Test
    fun `ksu env var means kernelsu family`() {
        assertEquals(RootManagerKind.KERNEL_SU_FAMILY, RootManagerDetector.classify("true", null))
    }

    @Test
    fun `magisk version name means magisk`() {
        assertEquals(RootManagerKind.MAGISK, RootManagerDetector.classify(null, "v28.0"))
    }

    @Test
    fun `neither signal means unknown`() {
        assertEquals(RootManagerKind.UNKNOWN, RootManagerDetector.classify(null, null))
    }

    @Test
    fun `ksu takes priority if somehow both are set`() {
        assertEquals(RootManagerKind.KERNEL_SU_FAMILY, RootManagerDetector.classify("true", "v28.0"))
    }
}
