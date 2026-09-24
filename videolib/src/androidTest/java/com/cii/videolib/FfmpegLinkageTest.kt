package com.cii.videolib

import androidx.test.ext.junit.runners.AndroidJUnit4

import org.junit.Test
import org.junit.runner.RunWith

import org.junit.Assert.*

/**
 * Instrumented test proving that the FFmpeg 7.1 static archives are linked into
 * libvideolib.so and callable at runtime (AC-2 / Test-ID T-LINK).
 *
 * Instantiating [NativeLib] triggers System.loadLibrary("videolib"); a broken
 * link, missing ABI, or unresolved FFmpeg symbol surfaces here as an
 * UnsatisfiedLinkError rather than a silent pass.
 */
@RunWith(AndroidJUnit4::class)
class FfmpegLinkageTest {

    @Test
    fun nativeFFmpegVersion_returnsLinkedFfmpegVersion() {
        val version = NativeLib().nativeFFmpegVersion()

        // Exact version text is intentionally not asserted (build-dependent);
        // assert only that a real FFmpeg-provided value came back.
        assertNotNull(version)
        assertTrue(
            "expected a non-blank FFmpeg version, got '$version'",
            version.isNotBlank()
        )
        // av_version_info() + avformat_version() are formatted as
        // "<ver> (avformat <maj>.<min>.<micro>)" by the native accessor.
        assertTrue(
            "expected the linked avformat marker, got '$version'",
            version.contains("avformat")
        )
    }

    /**
     * added-by-testing (regression): the pre-existing stringFromJNI contract must
     * still hold now that FFmpeg is statically linked into the same .so and the
     * library-load path is exercised. Guards the unchanged
     * Java_com_cii_videolib_NativeLib_stringFromJNI symbol.
     */
    @Test
    fun stringFromJNI_stillReturnsHello_afterFfmpegLinked() {
        assertEquals("Hello from C++", NativeLib().stringFromJNI())
    }
}
