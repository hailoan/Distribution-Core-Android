package com.chiistudio.camerandk.jni

import android.view.Surface
import java.nio.ByteBuffer

object NativeRenderer {

    external fun startCamera()

    external fun nativeInit(surface: Surface, vertex: String, fragment: String)
    external fun nativeRender()

    /**
     * Push the per-(mode × lens) resolution map to the native controller.
     * All four parallel arrays must be the same length.
     *
     * @param modes   per-entry mode (see [com.chiistudio.camerandk.model.CameraMode.nativeValue])
     * @param lenses  per-entry lens facing (see [com.chiistudio.camerandk.model.CameraLens.nativeValue])
     * @param widths  requested width per entry
     * @param heights requested height per entry
     */
    external fun nativeSetResolutionMap(
        modes: IntArray,
        lenses: IntArray,
        widths: IntArray,
        heights: IntArray,
    )

    /** Switch active capture mode. Reopens session if resolution differs. */
    external fun nativeSetMode(mode: Int)

    /** Switch active lens facing. Closes and reopens the camera device. */
    external fun nativeSetLens(lens: Int)

    /**
     * Tap-to-focus + tap-to-meter at a view-space point. The native side maps
     * (viewX, viewY) into sensor active-array coordinates accounting for the
     * sensor orientation and front-lens horizontal mirroring, then issues a
     * one-shot AF_TRIGGER_START with AF/AE regions set, and resumes the
     * repeating preview request.
     *
     * @param viewX touch X in view pixels
     * @param viewY touch Y in view pixels
     * @param viewW view width  in pixels (must be > 0)
     * @param viewH view height in pixels (must be > 0)
     */
    external fun nativeFocusAt(viewX: Float, viewY: Float, viewW: Float, viewH: Float)

    /**
     * Lock or unlock auto-focus.
     *  - true  → switches the HAL to AF_MODE_AUTO with TRIGGER_IDLE so the lens
     *            stays parked at the last converged position.
     *  - false → restores continuous AF (CONTINUOUS_PICTURE for PHOTO mode,
     *            CONTINUOUS_VIDEO otherwise).
     */
    external fun nativeLockFocus(locked: Boolean)

    /**
     * Set sensor-level AE exposure compensation in HAL EV steps. Each step is
     * `AE_COMPENSATION_STEP` of an EV (commonly 1/6 EV). The value is clamped
     * to the device's reported range — query [nativeGetExposureCompensationRange].
     *
     * Distinct from the shader-based brightness in `VideoConfigure` (which is
     * a post-process gain in [-0.5, 0.5]).
     */
    external fun nativeSetExposureCompensation(ev: Int)

    /**
     * Returns `[min, max]` of AE compensation steps reported by the active
     * camera. Both are 0 until the camera has been opened.
     */
    external fun nativeGetExposureCompensationRange(): IntArray

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

    /**
     * Start H.264 / MP4 video recording to [outputPath]. Only valid after
     * [nativeSetMode] has switched the camera into VIDEO mode. Recording uses
     * the currently-configured camera resolution and the FPS from the active
     * preview-quality preset.
     *
     * @param bitrate     target bitrate in bits/s, or ≤ 0 to use a resolution-
     *                    derived default (≈ 6 Mbps at 1080p30).
     * @param orientation MP4 rotation hint in degrees (0/90/180/270). Players
     *                    apply this on playback so the stream displays right-
     *                    side up. Ignored on devices below API 26.
     * @return true if the encoder opened successfully.
     */
    external fun nativeStartRecording(
        outputPath: String,
        bitrate: Int,
        orientation: Int,
    ): Boolean

    /**
     * Finalize the current recording. [callback] is invoked on the encoder
     * thread with the output path on success, or an empty string on failure
     * or when no recording is active. Consumers must marshal to the main
     * thread themselves.
     */
    external fun nativeStopRecording(callback: RecordingCallback)

    fun interface RecordingCallback {
        /** @param outputPath path to the finalized MP4, or "" on failure */
        fun onStopped(outputPath: String)
    }

}