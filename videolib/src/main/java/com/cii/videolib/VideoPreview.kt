package com.cii.videolib

import android.os.Handler
import android.os.Looper
import android.view.Surface
import androidx.annotation.Keep
import java.nio.ByteBuffer

/**
 * OpenGL ES (NDK) preview facade for [com.cii.videolib].
 *
 * Renders a frame onto a host-supplied [Surface] through a native EGL/GLES 3.0
 * pipeline. Frames may come from bundled-FFmpeg playback of a caller-owned
 * local file, a caller-supplied RGBA8888 pixel buffer, or a test pattern.
 *
 * The host owns the [Surface] lifecycle. Typical use:
 *  1. [attachSurface] when the surface becomes available,
 *  2. [play], [pushFrame], or [requestPattern] to render,
 *  3. [detachSurface] when the surface is destroyed,
 *  4. [release] when the preview is no longer needed.
 *
 * Not thread-safe: call from a single owner thread. All GL/EGL work is
 * marshalled onto a dedicated native render thread internally.
 */
class VideoPreview {

    // Opaque pointer to the native VideoPlayback owner. 0L once released.
    private var nativeHandle: Long = nativeCreate()
    private val callbackHandler = Handler(Looper.getMainLooper())
    private val callbackLock = Any()

    @Volatile
    private var surfaceAttached = false
    private var startPending = false
    private var activeAttemptId = NO_ATTEMPT
    private var playbackListener: PlaybackListener? = null
    private var pendingNativeEvent: NativePlaybackEvent? = null

    /**
     * Binds the preview to [surface] and initializes the native EGL/GLES
     * context. Returns true on success; false if the surface is invalid or EGL
     * initialization failed.
     */
    fun attachSurface(surface: Surface): Boolean {
        val handle = nativeHandle
        if (handle == 0L) return false
        val attached = nativeSurfaceAvailable(handle, surface)
        surfaceAttached = attached
        return attached
    }

    /**
     * Starts video-only playback of a local filesystem [path].
     *
     * Returns `true` when a playback attempt was accepted. A valid surface must
     * already be attached and only one attempt may be active. Accepted attempts
     * report exactly one terminal outcome to [listener] on the main thread,
     * unless cancelled with [stop] or [release].
     */
    fun play(path: String, listener: PlaybackListener): Boolean {
        val handle = nativeHandle
        if (handle == 0L || path.isBlank() || !surfaceAttached) return false

        synchronized(callbackLock) {
            if (startPending || activeAttemptId != NO_ATTEMPT) return false
            startPending = true
            playbackListener = listener
            pendingNativeEvent = null
        }

        val attemptId = nativePlay(handle, path)
        val pendingEvent: NativePlaybackEvent?
        synchronized(callbackLock) {
            startPending = false
            if (attemptId == NO_ATTEMPT) {
                playbackListener = null
                pendingNativeEvent = null
                return false
            }
            activeAttemptId = attemptId
            pendingEvent = pendingNativeEvent?.takeIf { it.attemptId == attemptId }
            pendingNativeEvent = null
        }
        pendingEvent?.let(::enqueueNativeEvent)
        return true
    }

    /**
     * Stops active playback. When this call returns, the cancelled attempt can
     * no longer present a frame or notify its listener. Idempotent.
     */
    fun stop() {
        invalidatePlaybackListener()
        val handle = nativeHandle
        if (handle != 0L) {
            nativeStop(handle)
        }
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
        surfaceAttached = false
        nativeReleaseSurface(handle)
    }

    /**
     * Releases the native renderer. After this the instance is inert; create a
     * new [VideoPreview] to render again. Idempotent.
     */
    fun release() {
        val handle = nativeHandle
        if (handle == 0L) return
        surfaceAttached = false
        invalidatePlaybackListener()
        nativeDestroy(handle)
        nativeHandle = 0L
    }

    @Keep
    @Suppress("unused") // Called from JNI on the native playback worker.
    private fun onNativePlaybackCompleted(attemptId: Long) {
        receiveNativeEvent(NativePlaybackEvent.Completed(attemptId))
    }

    @Keep
    @Suppress("unused") // Called from JNI on the native playback worker.
    private fun onNativePlaybackError(attemptId: Long, errorCode: Int) {
        if (errorCode == NATIVE_ERROR_RENDER) {
            surfaceAttached = false
        }
        receiveNativeEvent(
            NativePlaybackEvent.Error(
                attemptId = attemptId,
                error = when (errorCode) {
                    NATIVE_ERROR_INPUT_OPEN -> PlaybackError.INPUT_OPEN
                    NATIVE_ERROR_UNSUPPORTED_VIDEO -> PlaybackError.UNSUPPORTED_VIDEO
                    NATIVE_ERROR_RENDER -> PlaybackError.RENDER
                    else -> PlaybackError.DECODE
                },
            ),
        )
    }

    private fun receiveNativeEvent(event: NativePlaybackEvent) {
        synchronized(callbackLock) {
            if (startPending) {
                pendingNativeEvent = event
                return
            }
        }
        enqueueNativeEvent(event)
    }

    private fun enqueueNativeEvent(event: NativePlaybackEvent) {
        callbackHandler.post {
            val listener = synchronized(callbackLock) {
                if (activeAttemptId != event.attemptId) return@post
                activeAttemptId = NO_ATTEMPT
                pendingNativeEvent = null
                playbackListener.also { playbackListener = null }
            } ?: return@post

            when (event) {
                is NativePlaybackEvent.Completed -> listener.onPlaybackCompleted()
                is NativePlaybackEvent.Error -> listener.onPlaybackError(event.error)
            }
        }
    }

    private fun invalidatePlaybackListener() {
        synchronized(callbackLock) {
            startPending = false
            activeAttemptId = NO_ATTEMPT
            playbackListener = null
            pendingNativeEvent = null
        }
    }

    private external fun nativeCreate(): Long
    private external fun nativeSurfaceAvailable(handle: Long, surface: Surface): Boolean
    private external fun nativePlay(handle: Long, path: String): Long
    private external fun nativeStop(handle: Long)
    private external fun nativePushFrame(handle: Long, frame: ByteBuffer, width: Int, height: Int)
    private external fun nativeRequestPattern(handle: Long)
    private external fun nativeReleaseSurface(handle: Long)
    private external fun nativeDestroy(handle: Long)

    companion object {
        private const val NO_ATTEMPT = 0L
        private const val NATIVE_ERROR_INPUT_OPEN = 1
        private const val NATIVE_ERROR_UNSUPPORTED_VIDEO = 2
        private const val NATIVE_ERROR_RENDER = 4

        init {
            System.loadLibrary("videolib")
        }
    }

    private sealed interface NativePlaybackEvent {
        val attemptId: Long

        data class Completed(override val attemptId: Long) : NativePlaybackEvent

        data class Error(
            override val attemptId: Long,
            val error: PlaybackError,
        ) : NativePlaybackEvent
    }
}
