package com.cii.videolib

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * UT-ERR — JVM unit coverage for the export error taxonomy (feature
 * use-ffmpeg-export-video).
 *
 * Both [ExportError] and [PlaybackError] are ordinary Kotlin enums, so they load
 * on the host JVM with no native library. This test pins two contracts:
 *
 *  - the [ExportError] ordinal contract that the native `ExportErrorCode`
 *    (1..7) → `ExportError` mapping in [VideoExporter] depends on positionally
 *    (INPUT_OPEN=0 … OUTPUT=6, i.e. native code `n` maps to `entries[n-1]` for
 *    the non-default codes);
 *  - the compatibility guarantee behind choosing a separate [ExportError] type:
 *    [PlaybackError] is UNCHANGED (exactly the four playback categories), so
 *    existing consumers' exhaustive `when` over playback errors keep compiling.
 *
 * The `VideoExporter` code→enum mapping itself is exercised on device (the export
 * instrumented suite) because [VideoExporter] loads the native library on
 * construction and cannot be instantiated here.
 */
class ExportErrorTest {

    @Test
    fun exportError_hasSevenOrderedCategories() {
        // Order matters: it encodes the native ExportErrorCode contract.
        assertEquals(0, ExportError.INPUT_OPEN.ordinal)
        assertEquals(1, ExportError.UNSUPPORTED_VIDEO.ordinal)
        assertEquals(2, ExportError.DECODE.ordinal)
        assertEquals(3, ExportError.RENDER.ordinal)
        assertEquals(4, ExportError.ENCODE.ordinal)
        assertEquals(5, ExportError.MUX.ordinal)
        assertEquals(6, ExportError.OUTPUT.ordinal)
    }

    @Test
    fun exportError_containsExactlyThoseSevenCategories() {
        assertEquals(
            listOf(
                ExportError.INPUT_OPEN,
                ExportError.UNSUPPORTED_VIDEO,
                ExportError.DECODE,
                ExportError.RENDER,
                ExportError.ENCODE,
                ExportError.MUX,
                ExportError.OUTPUT,
            ),
            ExportError.entries,
        )
    }

    @Test
    fun exportError_resolvesEncodeMuxOutputByName() {
        for (name in listOf("ENCODE", "MUX", "OUTPUT")) {
            val resolved = ExportError.valueOf(name)
            assertNotNull(resolved)
            assertTrue("$name must be one of the encode/mux/output categories", resolved.ordinal >= 4)
        }
    }

    @Test
    fun playbackError_isUnchanged_soExistingConsumersStillCompile() {
        // Compatibility guard: choosing a separate ExportError means PlaybackError
        // stays exactly the four original playback categories. If a future change
        // re-adds export values here, an exhaustive consumer `when` would break —
        // this test fails first.
        assertEquals(
            listOf(
                PlaybackError.INPUT_OPEN,
                PlaybackError.UNSUPPORTED_VIDEO,
                PlaybackError.DECODE,
                PlaybackError.RENDER,
            ),
            PlaybackError.entries,
        )
    }
}
