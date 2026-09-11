package com.cii.videolib

import android.os.Handler
import android.os.Looper
import androidx.annotation.Keep

/**
 * FFmpeg + MediaCodec video exporter for [com.cii.videolib].
 *
 * Renders the same speed, appearance adjustments, and GLSL filter that
 * [VideoPreview] applies live, but writes the result to an MP4 file instead of a
 * surface. Decoding, effect rendering, and MP4 muxing use the bundled FFmpeg;
 * H.264 video is encoded with Android MediaCodec (the bundled LGPL FFmpeg has no
 * usable software H.264 encoder). Source audio is transcoded to AAC and muxed.
 *
 * Export is headless — no [android.view.Surface] is needed. Typical use:
 *  1. [export] a single clip, or [exportTimeline] an ordered list of segments,
 *  2. observe the terminal result on [ExportListener] (delivered on the main
 *     thread),
 *  3. [release] when the exporter is no longer needed.
 *
 * Only one export may be active per instance. Not thread-safe: call from a
 * single owner thread.
 */
class VideoExporter {

    // Opaque pointer to the native VideoExport owner. 0L once released.
    private var nativeHandle: Long = nativeCreate()
    private val callbackHandler = Handler(Looper.getMainLooper())
    private val callbackLock = Any()

    private var startPending = false
    private var activeAttemptId = NO_ATTEMPT
    private var exportListener: ExportListener? = null
    private var pendingNativeEvent: NativeExportEvent? = null

    /**
     * Exports the whole video at [path] (optionally trimmed via [VideoSegment])
     * to [outputPath], applying [appearance] and [speed].
     *
     * Returns `true` when the export was accepted. The output path must be
     * writable, [speed] must be finite and `>= 0.1`, and [appearance] must pass
     * the same validation as [VideoPreview.setAppearance]; otherwise the request
     * is rejected synchronously without notifying [listener]. Accepted exports
     * report exactly one terminal outcome to [listener] on the main thread,
     * unless cancelled with [cancel] or [release].
     */
    fun export(
        path: String,
        outputPath: String,
        listener: ExportListener,
        appearance: VideoAppearance = VideoAppearance(),
        speed: Double = 1.0,
        includeAudio: Boolean = true,
    ): Boolean {
        if (path.isBlank()) return false
        return exportTimeline(
            segments = listOf(
                VideoSegment(
                    path = path,
                    startMs = 0L,
                    endMs = Long.MAX_VALUE,
                    speed = speed,
                    appearance = appearance,
                ),
            ),
            outputPath = outputPath,
            listener = listener,
            includeAudio = includeAudio,
        )
    }

    /**
     * Exports an ordered list of trimmed [segments] concatenated into one MP4 at
     * [outputPath]. Each segment contributes its own `[startMs, endMs)` interval
     * with its own appearance and speed, in list order.
     *
     * Returns `true` when the export was accepted. The list must be non-empty,
     * [outputPath] must be non-blank, and every segment must have a well-formed
     * interval (`0 <= startMs < endMs`), a finite speed `>= 0.1`, and an
     * appearance that passes the same validation as [VideoPreview.setAppearance];
     * otherwise the whole request is rejected before any frame is written. Only
     * one export (single clip or timeline) may be active. Accepted exports report
     * exactly one terminal outcome to [listener] on the main thread, unless
     * cancelled with [cancel] or [release].
     */
    fun exportTimeline(
        segments: List<VideoSegment>,
        outputPath: String,
        listener: ExportListener,
        includeAudio: Boolean = true,
    ): Boolean {
        val handle = nativeHandle
        if (handle == 0L || segments.isEmpty() || outputPath.isBlank()) return false
        for (segment in segments) {
            if (segment.path.isBlank() || !segment.hasValidInterval) return false
            if (!segment.speed.isFinite() || segment.speed < MIN_PLAYBACK_SPEED) return false
            if (!isAppearanceValid(segment.appearance)) return false
        }

        synchronized(callbackLock) {
            if (startPending || activeAttemptId != NO_ATTEMPT) return false
            startPending = true
            exportListener = listener
            pendingNativeEvent = null
        }

        val attemptId = nativeStartExport(
            handle = handle,
            outputPath = outputPath,
            includeAudio = includeAudio,
            paths = Array(segments.size) { segments[it].path },
            startsMs = LongArray(segments.size) { segments[it].startMs },
            endsMs = LongArray(segments.size) { segments[it].endMs },
            speeds = DoubleArray(segments.size) { segments[it].speed },
            adjustments = Array(segments.size) { segments[it].appearance.adjustments.toNativeArray() },
            filterVersions = IntArray(segments.size) {
                segments[it].appearance.filter?.version ?: NO_FILTER_VERSION
            },
            filterSources = Array(segments.size) { segments[it].appearance.filter?.source },
            filterOpacities = FloatArray(segments.size) {
                segments[it].appearance.filter?.opacity ?: 1f
            },
            textureWidths = Array(segments.size) {
                segments[it].appearance.filter?.textures?.map { texture -> texture.width }
                    ?.toIntArray() ?: IntArray(0)
            },
            textureHeights = Array(segments.size) {
                segments[it].appearance.filter?.textures?.map { texture -> texture.height }
                    ?.toIntArray() ?: IntArray(0)
            },
            textureBytes = Array(segments.size) {
                segments[it].appearance.filter?.textures?.map { texture -> texture.copyRgba8888() }
                    ?.toTypedArray() ?: emptyArray()
            },
        )
        val pendingEvent: NativeExportEvent?
        synchronized(callbackLock) {
            startPending = false
            if (attemptId == NO_ATTEMPT) {
                exportListener = null
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
     * Cancels an active export. When this call returns, the cancelled attempt can
     * no longer complete or notify its listener, and its partial output file is
     * removed. Idempotent.
     */
    fun cancel() {
        invalidateListener()
        val handle = nativeHandle
        if (handle != 0L) {
            nativeCancelExport(handle)
        }
    }

    /**
     * Releases the native exporter. After this the instance is inert; create a
     * new [VideoExporter] to export again. Idempotent.
     */
    fun release() {
        val handle = nativeHandle
        if (handle == 0L) return
        invalidateListener()
        nativeDestroy(handle)
        nativeHandle = 0L
    }

    @Keep
    @Suppress("unused") // Called from JNI on the native export worker.
    private fun onNativeExportCompleted(attemptId: Long) {
        receiveNativeEvent(NativeExportEvent.Completed(attemptId))
    }

    @Keep
    @Suppress("unused") // Called from JNI on the native export worker.
    private fun onNativeExportError(attemptId: Long, errorCode: Int, segmentIndex: Int) {
        receiveNativeEvent(
            NativeExportEvent.Error(
                attemptId = attemptId,
                error = when (errorCode) {
                    NATIVE_ERROR_INPUT_OPEN -> ExportError.INPUT_OPEN
                    NATIVE_ERROR_UNSUPPORTED_VIDEO -> ExportError.UNSUPPORTED_VIDEO
                    NATIVE_ERROR_RENDER -> ExportError.RENDER
                    NATIVE_ERROR_ENCODE -> ExportError.ENCODE
                    NATIVE_ERROR_MUX -> ExportError.MUX
                    NATIVE_ERROR_OUTPUT -> ExportError.OUTPUT
                    else -> ExportError.DECODE
                },
                segmentIndex = segmentIndex,
            ),
        )
    }

    private fun receiveNativeEvent(event: NativeExportEvent) {
        synchronized(callbackLock) {
            if (startPending) {
                pendingNativeEvent = event
                return
            }
        }
        enqueueNativeEvent(event)
    }

    private fun enqueueNativeEvent(event: NativeExportEvent) {
        callbackHandler.post {
            val listener = synchronized(callbackLock) {
                if (activeAttemptId != event.attemptId) return@post
                activeAttemptId = NO_ATTEMPT
                pendingNativeEvent = null
                val captured = exportListener
                exportListener = null
                captured
            }
            when (event) {
                is NativeExportEvent.Completed -> listener?.onExportCompleted()
                is NativeExportEvent.Error -> listener?.onExportError(event.error, event.segmentIndex)
            }
        }
    }

    private fun invalidateListener() {
        synchronized(callbackLock) {
            startPending = false
            activeAttemptId = NO_ATTEMPT
            exportListener = null
            pendingNativeEvent = null
        }
    }

    /**
     * Structural validation mirroring [VideoPreview]'s appearance rules. The
     * authoritative check also runs natively; this rejects obviously-invalid
     * input synchronously before an attempt is started.
     */
    private fun isAppearanceValid(candidate: VideoAppearance): Boolean {
        val a = candidate.adjustments
        val valueError = listOf(
            a.brightness to (-0.5f..0.5f),
            a.contrast to (0f..2f),
            a.saturation to (0f..2f),
            a.exposure to (-1f..1f),
            a.darks to (0.5f..1.5f),
            a.vignette to (0f..1f),
            a.vibrance to (-1f..1f),
            a.temperature to (-0.5f..0.5f),
            a.hue to (-1f..1f),
            a.highlights to (-2f..2f),
            a.shadows to (-1f..1f),
            a.lights to (0f..2f),
            a.clarity to (-1f..1f),
            a.levels.minimumInput to (-1f..1f),
            a.levels.gamma to (0.5f..1.5f),
            a.levels.maximumInput to (0.5f..1.5f),
        ).any { (value, range) -> !value.isFinite() || value !in range }
        if (valueError) return false
        if (a.levels.minimumInput >= a.levels.maximumInput) return false

        val filter = candidate.filter ?: return true
        if (filter.version != VideoFilter.VERSION_1) return false
        if (!filter.opacity.isFinite() || filter.opacity !in 0f..1f) return false
        if (!FILTER_ENTRY_POINT.containsMatchIn(filter.source) ||
            RESERVED_FILTER_SOURCE.any { filter.source.contains(it) }
        ) {
            return false
        }
        return filter.textures.none { texture ->
            val expected = texture.width.toLong() * texture.height.toLong() * RGBA_CHANNELS
            texture.width <= 0 || texture.height <= 0 ||
                expected > Int.MAX_VALUE || texture.rgba8888.size.toLong() != expected
        }
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

    private external fun nativeCreate(): Long
    private external fun nativeStartExport(
        handle: Long,
        outputPath: String,
        includeAudio: Boolean,
        paths: Array<String>,
        startsMs: LongArray,
        endsMs: LongArray,
        speeds: DoubleArray,
        adjustments: Array<FloatArray>,
        filterVersions: IntArray,
        filterSources: Array<String?>,
        filterOpacities: FloatArray,
        textureWidths: Array<IntArray>,
        textureHeights: Array<IntArray>,
        textureBytes: Array<Array<ByteArray>>,
    ): Long
    private external fun nativeCancelExport(handle: Long)
    private external fun nativeDestroy(handle: Long)

    companion object {
        private const val MIN_PLAYBACK_SPEED = 0.1
        private const val NO_ATTEMPT = 0L
        private const val NO_FILTER_VERSION = 0
        private const val RGBA_CHANNELS = 4L
        private const val NATIVE_ERROR_INPUT_OPEN = 1
        private const val NATIVE_ERROR_UNSUPPORTED_VIDEO = 2
        private const val NATIVE_ERROR_RENDER = 4
        private const val NATIVE_ERROR_ENCODE = 5
        private const val NATIVE_ERROR_MUX = 6
        private const val NATIVE_ERROR_OUTPUT = 7
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

    private sealed interface NativeExportEvent {
        val attemptId: Long

        data class Completed(override val attemptId: Long) : NativeExportEvent

        data class Error(
            override val attemptId: Long,
            val error: ExportError,
            val segmentIndex: Int,
        ) : NativeExportEvent
    }
}
