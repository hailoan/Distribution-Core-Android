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
    var appearance: VideoAppearance = VideoAppearance()
        private set
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

    /** Pauses active playback after any frame already being presented. */
    fun pause(): Boolean {
        val handle = nativeHandle
        if (handle == 0L || !surfaceAttached) return false
        return nativePause(handle)
    }

    /** Resumes a paused playback attempt. */
    fun resume(): Boolean {
        val handle = nativeHandle
        if (handle == 0L || !surfaceAttached) return false
        return nativeResume(handle)
    }

    /** Enables or disables continuous replay for this preview instance. */
    fun setLooping(enabled: Boolean): Boolean {
        val handle = nativeHandle
        if (handle == 0L) return false
        return nativeSetLooping(handle, enabled)
    }

    /** Sets playback speed. Any finite value greater than or equal to 0.1 is accepted. */
    fun setPlaybackSpeed(speed: Double): Boolean {
        val handle = nativeHandle
        if (handle == 0L || !speed.isFinite() || speed < MIN_PLAYBACK_SPEED) return false
        return nativeSetPlaybackSpeed(handle, speed)
    }

    /** Applies one complete immutable appearance snapshot synchronously. */
    fun setAppearance(appearance: VideoAppearance): AppearanceUpdateResult {
        val handle = nativeHandle
        if (handle == 0L) return rejected(AppearanceRejectionReason.RELEASED)
        validate(appearance)?.let { return it }
        if (appearance.filter != null &&
            appearance.filter != this.appearance.filter &&
            !surfaceAttached
        ) {
            return rejected(AppearanceRejectionReason.SURFACE_UNAVAILABLE)
        }

        val filter = appearance.filter
        val nativeResult = nativeApplyAppearance(
            handle = handle,
            adjustments = appearance.adjustments.toNativeArray(),
            filterVersion = filter?.version ?: NO_FILTER_VERSION,
            filterSource = filter?.source,
            filterOpacity = filter?.opacity ?: 1f,
            textureWidths = filter?.textures?.map { it.width }?.toIntArray() ?: IntArray(0),
            textureHeights = filter?.textures?.map { it.height }?.toIntArray() ?: IntArray(0),
            textureBytes = filter?.textures?.map { it.copyRgba8888() }?.toTypedArray()
                ?: emptyArray(),
        )
        return nativeResult.toPublicResult().also { result ->
            if (result === AppearanceUpdateResult.Accepted) {
                this.appearance = appearance
            }
        }
    }

    fun setAdjustments(adjustments: VideoAdjustments): AppearanceUpdateResult =
        setAppearance(appearance.copy(adjustments = adjustments))

    fun resetAdjustments(): AppearanceUpdateResult = setAdjustments(VideoAdjustments())

    fun setBrightness(value: Float) = setAdjustments(appearance.adjustments.copy(brightness = value))
    fun setContrast(value: Float) = setAdjustments(appearance.adjustments.copy(contrast = value))
    fun setSaturation(value: Float) = setAdjustments(appearance.adjustments.copy(saturation = value))
    fun setExposure(value: Float) = setAdjustments(appearance.adjustments.copy(exposure = value))
    fun setDarks(value: Float) = setAdjustments(appearance.adjustments.copy(darks = value))
    fun setLevels(value: VideoLevels) = setAdjustments(appearance.adjustments.copy(levels = value))
    fun setVignette(value: Float) = setAdjustments(appearance.adjustments.copy(vignette = value))
    fun setVibrance(value: Float) = setAdjustments(appearance.adjustments.copy(vibrance = value))
    fun setTemperature(value: Float) = setAdjustments(appearance.adjustments.copy(temperature = value))
    fun setHue(value: Float) = setAdjustments(appearance.adjustments.copy(hue = value))
    fun setHighlights(value: Float) = setAdjustments(appearance.adjustments.copy(highlights = value))
    fun setShadows(value: Float) = setAdjustments(appearance.adjustments.copy(shadows = value))
    fun setLights(value: Float) = setAdjustments(appearance.adjustments.copy(lights = value))
    fun setClarity(value: Float) = setAdjustments(appearance.adjustments.copy(clarity = value))

    /** Installs [filter], or removes the active filter when it is null. */
    fun setFilter(filter: VideoFilter?): AppearanceUpdateResult =
        setAppearance(appearance.copy(filter = filter))

    /** Seeks to [positionMs], clamped by native playback to the playable interval. */
    fun seekTo(positionMs: Long): Boolean {
        val handle = nativeHandle
        if (handle == 0L || !surfaceAttached) return false
        return nativeSeekTo(handle, positionMs)
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

    private fun validate(candidate: VideoAppearance): AppearanceUpdateResult.Rejected? {
        val valueError = candidate.adjustments.run {
            listOf(
                brightness to (-0.5f..0.5f),
                contrast to (0f..2f),
                saturation to (0f..2f),
                exposure to (-1f..1f),
                darks to (0.5f..1.5f),
                vignette to (0f..1f),
                vibrance to (-1f..1f),
                temperature to (-0.5f..0.5f),
                hue to (-1f..1f),
                highlights to (-2f..2f),
                shadows to (-1f..1f),
                lights to (0f..2f),
                clarity to (-1f..1f),
                levels.minimumInput to (-1f..1f),
                levels.gamma to (0.5f..1.5f),
                levels.maximumInput to (0.5f..1.5f),
            ).any { (value, range) -> !value.isFinite() || value !in range }
        }
        if (valueError) return rejected(AppearanceRejectionReason.INVALID_VALUE)
        if (candidate.adjustments.levels.minimumInput >= candidate.adjustments.levels.maximumInput) {
            return rejected(AppearanceRejectionReason.INVALID_LEVELS)
        }

        val filter = candidate.filter ?: return null
        if (filter.version != VideoFilter.VERSION_1) {
            return rejected(AppearanceRejectionReason.UNSUPPORTED_FILTER_VERSION)
        }
        if (!filter.opacity.isFinite() || filter.opacity !in 0f..1f) {
            return rejected(AppearanceRejectionReason.INVALID_FILTER_OPACITY)
        }
        if (!FILTER_ENTRY_POINT.containsMatchIn(filter.source) ||
            RESERVED_FILTER_SOURCE.any { filter.source.contains(it) }
        ) {
            return rejected(AppearanceRejectionReason.INVALID_FILTER_SOURCE)
        }
        if (filter.textures.any { texture ->
                val expected = texture.width.toLong() * texture.height.toLong() * RGBA_CHANNELS
                texture.width <= 0 || texture.height <= 0 ||
                    expected > Int.MAX_VALUE || texture.rgba8888.size.toLong() != expected
            }
        ) {
            return rejected(AppearanceRejectionReason.INVALID_FILTER_TEXTURE)
        }
        return null
    }

    private fun VideoAdjustments.toNativeArray() = floatArrayOf(
        brightness,
        contrast,
        saturation,
        exposure,
        darks,
        levels.minimumInput,
        levels.gamma,
        levels.maximumInput,
        vignette,
        vibrance,
        temperature,
        hue,
        highlights,
        shadows,
        lights,
        clarity,
    )

    private fun NativeAppearanceResult.toPublicResult(): AppearanceUpdateResult {
        if (code == NATIVE_APPEARANCE_ACCEPTED) return AppearanceUpdateResult.Accepted
        val reason = AppearanceRejectionReason.entries.getOrNull(code - 1)
            ?: AppearanceRejectionReason.RENDER_FAILURE
        return AppearanceUpdateResult.Rejected(reason, diagnostic)
    }

    private fun rejected(reason: AppearanceRejectionReason) =
        AppearanceUpdateResult.Rejected(reason)

    private external fun nativeCreate(): Long
    private external fun nativeSurfaceAvailable(handle: Long, surface: Surface): Boolean
    private external fun nativePlay(handle: Long, path: String): Long
    private external fun nativeStop(handle: Long)
    private external fun nativePause(handle: Long): Boolean
    private external fun nativeResume(handle: Long): Boolean
    private external fun nativeSetLooping(handle: Long, enabled: Boolean): Boolean
    private external fun nativeSetPlaybackSpeed(handle: Long, speed: Double): Boolean
    private external fun nativeSeekTo(handle: Long, positionMs: Long): Boolean
    private external fun nativeApplyAppearance(
        handle: Long,
        adjustments: FloatArray,
        filterVersion: Int,
        filterSource: String?,
        filterOpacity: Float,
        textureWidths: IntArray,
        textureHeights: IntArray,
        textureBytes: Array<ByteArray>,
    ): NativeAppearanceResult
    private external fun nativePushFrame(handle: Long, frame: ByteBuffer, width: Int, height: Int)
    private external fun nativeRequestPattern(handle: Long)
    private external fun nativeReleaseSurface(handle: Long)
    private external fun nativeDestroy(handle: Long)

    companion object {
        private const val MIN_PLAYBACK_SPEED = 0.1
        private const val NO_ATTEMPT = 0L
        private const val NATIVE_ERROR_INPUT_OPEN = 1
        private const val NATIVE_ERROR_UNSUPPORTED_VIDEO = 2
        private const val NATIVE_ERROR_RENDER = 4
        private const val NATIVE_APPEARANCE_ACCEPTED = 0
        private const val NO_FILTER_VERSION = 0
        private const val RGBA_CHANNELS = 4L
        private val FILTER_ENTRY_POINT = Regex(
            """\bvec4\s+addFilter\s*\(\s*vec4\s+\w+\s*,\s*vec2\s+\w+\s*\)""",
        )
        private val RESERVED_FILTER_SOURCE = listOf(
            "#version",
            "void main",
            "u_texture",
            "v_texCoord",
            "fragColor",
        )

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
