package com.cii.videolib

import android.graphics.PixelFormat
import android.hardware.HardwareBuffer
import android.media.ImageReader
import android.os.Build
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import android.view.Surface
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicLong

/**
 * Test-only fixtures (C-08) for the [VideoPreview] progressive-playback device
 * suite. Three cooperating pieces, kept in one file because they share a single
 * logical event clock:
 *
 *  - [PlaybackEventLog]      — one monotonic sequence shared by frame and terminal
 *                              observations so a test can assert their ORDER
 *                              (e.g. "first visible frame precedes completion").
 *  - [PlaybackSurfaceProbe]  — a real, consumable Android [Surface] backed by an
 *                              [ImageReader]. Every `eglSwapBuffers` the native
 *                              renderer performs surfaces here as one
 *                              `onImageAvailable`; the probe consumes (acquires +
 *                              closes) each buffer so the producer can never
 *                              stall, and records the presented centre pixel so
 *                              successive frames are distinguishable.
 *  - [RecordingPlaybackListener] — records the single terminal outcome and the
 *                              thread it arrived on (must be main).
 *
 * These use only the platform + the public [VideoPreview] API. No production
 * observability hook is added.
 */

/** Kind of terminal outcome observed through [PlaybackListener]. */
enum class TerminalKind { COMPLETED, ERROR }

/** One presented-frame observation: its order stamp and centre-pixel luma. */
data class FrameObservation(val sequence: Long, val luma: Int)

/** The terminal observation: order stamp, kind, error (if any), delivery thread. */
data class TerminalObservation(
    val sequence: Long,
    val kind: TerminalKind,
    val error: PlaybackError?,
    val onMainThread: Boolean,
)

/**
 * A single monotonic clock shared by the surface probe and the listener so that
 * frame presentation and terminal delivery are totally ordered relative to each
 * other, regardless of which thread each arrives on.
 */
class PlaybackEventLog {
    private val clock = AtomicLong(0)
    private val firstFrameLatch = CountDownLatch(1)
    private val terminalLatch = CountDownLatch(1)
    private val terminalCount = AtomicInteger(0)

    val frames = CopyOnWriteArrayList<FrameObservation>()

    @Volatile
    var terminal: TerminalObservation? = null
        private set

    fun recordFrame(luma: Int) {
        val sequence = clock.incrementAndGet()
        frames.add(FrameObservation(sequence, luma))
        firstFrameLatch.countDown()
    }

    fun recordTerminal(kind: TerminalKind, error: PlaybackError?, onMainThread: Boolean) {
        val sequence = clock.incrementAndGet()
        // Keep the FIRST terminal; count every call so a test can assert exactly one.
        if (terminalCount.getAndIncrement() == 0) {
            terminal = TerminalObservation(sequence, kind, error, onMainThread)
        }
        terminalLatch.countDown()
    }

    /** Total terminal callbacks received (must be exactly 1 for a non-cancelled attempt). */
    fun terminalDeliveryCount(): Int = terminalCount.get()

    fun frameCount(): Int = frames.size

    fun firstFrameSequence(): Long? = frames.firstOrNull()?.sequence

    /** Distinct centre-pixel values seen — successive fixture frames differ. */
    fun distinctLumaCount(): Int = frames.map { it.luma }.toSet().size

    fun awaitFirstFrame(timeoutMs: Long): Boolean =
        firstFrameLatch.await(timeoutMs, TimeUnit.MILLISECONDS)

    fun awaitTerminal(timeoutMs: Long): Boolean =
        terminalLatch.await(timeoutMs, TimeUnit.MILLISECONDS)

    /** True only if a terminal was recorded AND at least one frame preceded it. */
    fun firstFramePrecededTerminal(): Boolean {
        val firstFrame = firstFrameSequence() ?: return false
        val terminalSequence = terminal?.sequence ?: return false
        return firstFrame < terminalSequence
    }
}

/**
 * A real consumable preview surface. Each presented frame (one producer
 * `eglSwapBuffers`) is delivered as one `onImageAvailable`; the probe acquires and
 * closes it — consuming the buffer so the render thread is never blocked — and
 * records the centre pixel into [log].
 */
class PlaybackSurfaceProbe(
    initialLog: PlaybackEventLog,
    width: Int = DEFAULT_WIDTH,
    height: Int = DEFAULT_HEIGHT,
) {
    /**
     * The event log presented frames are recorded into. Reassignable so one
     * physical surface can separate observations across successive attempts
     * (e.g. an initial attempt vs. a retry). Assign the retry's log before
     * starting the retry.
     *
     * The `onImageAvailable` lambda reads this property (not the constructor
     * parameter) on every frame, so a reassignment takes effect for subsequent
     * presented frames.
     */
    @Volatile
    var log: PlaybackEventLog = initialLog

    private val thread = HandlerThread("videolib-probe").apply { start() }
    private val handler = Handler(thread.looper)
    private val imageReader = newReader(width, height)

    /** The surface to hand to [VideoPreview.attachSurface]. Valid until [close]. */
    val surface: Surface get() = imageReader.surface

    init {
        imageReader.setOnImageAvailableListener({ reader ->
            while (true) {
                val image = try {
                    reader.acquireNextImage()
                } catch (_: IllegalStateException) {
                    null
                } ?: break
                try {
                    val plane = image.planes[0]
                    val buffer = plane.buffer
                    val centreX = image.width / 2
                    val centreY = image.height / 2
                    val offset = centreY * plane.rowStride + centreX * plane.pixelStride
                    val luma = if (offset in 0 until buffer.limit()) {
                        buffer.get(offset).toInt() and 0xFF
                    } else {
                        0
                    }
                    log.recordFrame(luma)
                } finally {
                    image.close()
                }
            }
        }, handler)
    }

    /** Releases the reader and quits the probe thread. Idempotent. */
    fun close() {
        imageReader.setOnImageAvailableListener(null, null)
        imageReader.close()
        thread.quitSafely()
    }

    private fun newReader(width: Int, height: Int): ImageReader =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            // Correct GPU-output path: the renderer produces this surface via EGL.
            ImageReader.newInstance(
                width,
                height,
                PixelFormat.RGBA_8888,
                MAX_IMAGES,
                HardwareBuffer.USAGE_GPU_COLOR_OUTPUT or HardwareBuffer.USAGE_CPU_READ_OFTEN,
            )
        } else {
            @Suppress("DEPRECATION")
            ImageReader.newInstance(width, height, PixelFormat.RGBA_8888, MAX_IMAGES)
        }

    companion object {
        const val DEFAULT_WIDTH = 320
        const val DEFAULT_HEIGHT = 240

        // >=2 for a double-buffered GLES producer; a few extra absorbs jitter.
        private const val MAX_IMAGES = 4
    }
}

/** Records the single terminal outcome of an accepted [VideoPreview.play]. */
class RecordingPlaybackListener(private val log: PlaybackEventLog) : PlaybackListener {
    override fun onPlaybackCompleted() {
        log.recordTerminal(TerminalKind.COMPLETED, null, isMainThread())
    }

    override fun onPlaybackError(error: PlaybackError) {
        log.recordTerminal(TerminalKind.ERROR, error, isMainThread())
    }

    private fun isMainThread(): Boolean = Looper.myLooper() == Looper.getMainLooper()
}
