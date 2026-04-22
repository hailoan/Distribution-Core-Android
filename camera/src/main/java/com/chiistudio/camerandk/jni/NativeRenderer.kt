package com.chiistudio.camerandk.jni

import android.view.Surface
import java.nio.ByteBuffer

object NativeRenderer {

    external fun startCamera()

    external fun nativeInit(surface: Surface, vertex: String, fragment: String)
    external fun nativeRender()

    external fun nativeCleanup()

    /**
     * filter video
     */
    external fun changeFilter(
        resVertex: String,
        resShader: String,
        dataFilter: List<ByteBuffer>,
        widths: List<Int>,
        heights: List<Int>,
        opacity: Float
    )

    /**
     * One-shot capture of the current preview framebuffer.
     *
     * Native side posts a task onto the EGL thread; at the end of the next
     * [renderFrame] it calls `glReadPixels(GL_RGBA, GL_UNSIGNED_BYTE)` and
     * invokes [callback] once with the raw RGBA8888 buffer plus dimensions.
     *
     * The callback is invoked on the native EGL thread — consumers must
     * marshal to the main thread themselves (see `GLPreview.captureFrame`).
     * Pixels are in OpenGL orientation (origin at bottom-left); callers
     * must flip vertically if a top-left Bitmap is required.
     */
    external fun captureFrame(callback: CaptureCallback)

    fun interface CaptureCallback {
        /** @param rgba tightly packed RGBA8888 pixels, length == width * height * 4 */
        fun onCaptured(rgba: ByteArray, width: Int, height: Int)
    }

}