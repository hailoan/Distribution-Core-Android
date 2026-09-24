package com.cii.videolib

import androidx.test.ext.junit.runners.AndroidJUnit4
import java.nio.ByteBuffer
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Instrumented test for the OpenGL ES preview JNI binding (Test-ID T-LOAD).
 *
 * Scope is symbol resolution and lifecycle safety, NOT rendering: it proves that
 * every [VideoPreview] native method is linked into libvideolib.so (a missing or
 * mismatched export surfaces here as an UnsatisfiedLinkError, not a silent pass),
 * that the library loads, and that the no-Surface lifecycle no-ops safely.
 *
 * Visible-output ACs (host frame, test pattern, surface destroy/recreate) require
 * a real Surface + EGL context and are covered by the device integration check
 * (IT3), not by this test. No Surface is created here.
 */
@RunWith(AndroidJUnit4::class)
class PreviewBindingTest {

    /**
     * Constructing [VideoPreview] triggers System.loadLibrary("videolib") and the
     * nativeCreate export. If the library fails to load, an ABI is missing, or
     * nativeCreate is unresolved, this throws instead of returning.
     */
    @Test
    fun videoPreview_constructs_loadsLibraryAndResolvesNativeCreate() {
        val preview = VideoPreview()
        preview.release()
    }

    /**
     * Exercises every preview native method reachable without a Surface. In the
     * pre-attach (Idle) state each native call hits an early-return guard, so this
     * proves the symbols resolve without initializing EGL or requiring a Surface.
     * A direct buffer is used so pushFrame reaches its native export rather than
     * being rejected by the Kotlin direct/capacity pre-checks.
     */
    @Test
    fun previewNativeMethods_resolve_andNoOpSafely_withoutSurface() {
        val preview = VideoPreview()
        try {
            preview.requestPattern()

            val frame = ByteBuffer.allocateDirect(2 * 2 * 4)
            preview.pushFrame(frame, 2, 2)

            preview.detachSurface()
        } finally {
            preview.release()
        }
    }

    /**
     * release() is idempotent: a second call after the handle is cleared must be a
     * safe no-op (guards on a zero handle), not a double-free crash.
     */
    @Test
    fun release_isIdempotent() {
        val preview = VideoPreview()
        preview.release()
        preview.release()
    }

    /**
     * pushFrame ignores an undersized buffer at the Kotlin boundary (no native
     * call, no crash) — verifies the direct/capacity guard for a below-capacity
     * direct buffer.
     */
    @Test
    fun pushFrame_ignoresUndersizedBuffer_withoutCrashing() {
        val preview = VideoPreview()
        try {
            val tooSmall = ByteBuffer.allocateDirect(4) // < 2*2*4
            preview.pushFrame(tooSmall, 2, 2)
        } finally {
            preview.release()
        }
    }

    /**
     * Regression (R3): the pre-existing NativeLib JNI exports must still resolve
     * now that the preview code shares the same libvideolib.so. Guards the
     * unchanged Java_com_cii_videolib_NativeLib_stringFromJNI symbol.
     */
    @Test
    fun nativeLib_stringFromJNI_stillResolves_afterPreviewAdded() {
        assertEquals("Hello from C++", NativeLib().stringFromJNI())
    }

    /**
     * Regression (R3): the FFmpeg-linkage export is unaffected by the added EGL/
     * GLESv3 linkage and preview sources.
     */
    @Test
    fun nativeLib_ffmpegVersion_stillResolves_afterPreviewAdded() {
        val version = NativeLib().nativeFFmpegVersion()
        assertTrue(
            "expected the linked avformat marker, got '$version'",
            version.contains("avformat")
        )
    }
}
