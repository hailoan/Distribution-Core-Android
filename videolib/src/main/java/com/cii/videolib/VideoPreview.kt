package com.cii.videolib

import android.view.Surface
import java.nio.ByteBuffer

/**
 * OpenGL ES (NDK) preview facade for [com.cii.videolib].
 *
 * Renders a frame onto a host-supplied [Surface] through a native EGL/GLES 3.0
 * pipeline. The frame source is a caller-supplied RGBA8888 pixel buffer or a
 * built-in test pattern; there is no video-decode or camera dependency.
 *
 * The host owns the [Surface] lifecycle. Typical use:
 *  1. [attachSurface] when the surface becomes available,
 *  2. [pushFrame] / [requestPattern] to render,
 *  3. [detachSurface] when the surface is destroyed,
 *  4. [release] when the preview is no longer needed.
 *
 * Not thread-safe: call from a single owner thread. All GL/EGL work is
 * marshalled onto a dedicated native render thread internally.
 */
class VideoPreview {

    // Opaque pointer to the native PreviewRenderer. 0L once released.
    private var nativeHandle: Long = nativeCreate()

    /**
     * Binds the preview to [surface] and initializes the native EGL/GLES
     * context. Returns true on success; false if the surface is invalid or EGL
     * initialization failed.
     */
    fun attachSurface(surface: Surface): Boolean {
        val handle = nativeHandle
        if (handle == 0L) return false
        return nativeSurfaceAvailable(handle, surface)
    }

    /**
     * Uploads and draws an RGBA8888 [frame] of [width] x [height].
     *
     * [frame] must be a **direct** [ByteBuffer] with at least width*height*4
     * bytes; a non-direct or undersized buffer is ignored. The buffer only needs
     * to remain valid for the duration of this call.
     */
    fun pushFrame(frame: ByteBuffer, width: Int, height: Int) {
        val handle = nativeHandle
        if (handle == 0L) return
        if (!frame.isDirect) return
        if (width <= 0 || height <= 0) return
        if (frame.capacity() < width * height * 4) return
        nativePushFrame(handle, frame, width, height)
    }

    /** Draws the built-in test pattern (no host frame needed). */
    fun requestPattern() {
        val handle = nativeHandle
        if (handle == 0L) return
        nativeRequestPattern(handle)
    }

    /** Tears down the EGL context and releases the surface. Idempotent. */
    fun detachSurface() {
        val handle = nativeHandle
        if (handle == 0L) return
        nativeReleaseSurface(handle)
    }

    /**
     * Releases the native renderer. After this the instance is inert; create a
     * new [VideoPreview] to render again. Idempotent.
     */
    fun release() {
        val handle = nativeHandle
        if (handle == 0L) return
        nativeDestroy(handle)
        nativeHandle = 0L
    }

    private external fun nativeCreate(): Long
    private external fun nativeSurfaceAvailable(handle: Long, surface: Surface): Boolean
    private external fun nativePushFrame(handle: Long, frame: ByteBuffer, width: Int, height: Int)
    private external fun nativeRequestPattern(handle: Long)
    private external fun nativeReleaseSurface(handle: Long)
    private external fun nativeDestroy(handle: Long)

    companion object {
        init {
            System.loadLibrary("videolib")
        }
    }
}
