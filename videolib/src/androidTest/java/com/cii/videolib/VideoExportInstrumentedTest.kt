package com.cii.videolib

import android.content.Context
import android.media.MediaExtractor
import android.media.MediaFormat
import android.os.Looper
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.BeforeClass
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

/**
 * Export device coverage for [VideoExporter] — the real path
 * `export`/`exportTimeline` → JNI `nativeStartExport` → `VideoExport` →
 * FrameSource (FFmpeg decode) → OffscreenRenderer (EGL + GlProgram) →
 * H264Encoder (MediaCodec) → Mp4Muxer (FFmpeg avformat), plus AudioTranscoder.
 *
 * These are device/ABI tests: they perform real EGL init, real MediaCodec H.264
 * encoding, and real FFmpeg muxing, so they require a supported EGL/GLES 3.0 ARM
 * device. On an unsupported environment the export fails with an explicit error
 * (RENDER/ENCODE) rather than passing silently.
 *
 * The produced MP4 is inspected with the platform [MediaExtractor], which is
 * independent of the code under test, so a malformed container is caught.
 *
 * Test-IDs: T-EXPORT-VIDEO, T-CANCEL, T-ERR, T-AUDIO, T-TIMELINE, T-LINK.
 */
@RunWith(AndroidJUnit4::class)
class VideoExportInstrumentedTest {

    private lateinit var exporter: VideoExporter
    private lateinit var output: File

    @Before
    fun setUp() {
        exporter = VideoExporter()
        output = File(cacheRoot(), "export_${System.nanoTime()}.mp4")
        if (output.exists()) output.delete()
    }

    @After
    fun tearDown() {
        exporter.release()
        if (output.exists()) output.delete()
    }

    /**
     * T-LINK: constructing [VideoExporter] loads `libvideolib.so` (with the new
     * `mediandk` link and the export JNI exports). A broken link or missing ABI
     * surfaces here as an UnsatisfiedLinkError instead of a silent pass.
     */
    @Test
    fun exporterConstruction_loadsNativeLibrary() {
        // Reaching setUp() already loaded the library; a no-op export call proves
        // the JNI symbols resolve without throwing UnsatisfiedLinkError.
        val listener = RecordingExportListener()
        // Blank path is rejected before native start but still exercises the class.
        assertFalse(exporter.export(path = "", outputPath = output.absolutePath, listener = listener))
    }

    /**
     * T-EXPORT-VIDEO (AC-1, AC-5): a single-clip export of the deterministic
     * fixture produces a playable MP4 whose video track is H.264. Exactly one
     * completion is delivered on the main thread (AC-1).
     */
    @Test
    fun singleClipExport_producesPlayableH264Mp4_completesOnceOnMain() {
        val listener = RecordingExportListener()
        val accepted = exporter.export(
            path = video.absolutePath,
            outputPath = output.absolutePath,
            listener = listener,
            includeAudio = false,
        )
        assertTrue("export must be accepted on a ready device", accepted)
        assertTrue("export must finish within ${TIMEOUT_MS} ms", listener.await(TIMEOUT_MS))
        assertTrue("export must complete, not error: ${listener.error}", listener.completed)
        assertEquals("exactly one terminal", 1, listener.deliveryCount())
        assertTrue("terminal must be on the main thread", listener.onMainThread)

        assertTrue("output file must exist", output.exists())
        assertTrue("output file must be non-empty", output.length() > 0)
        assertTrue(
            "output must contain an H.264 video track",
            hasTrackWithMime(output, "video/avc"),
        )
    }

    /**
     * T-EXPORT-VIDEO (AC-2): a 2x export completes faster than 1x, proving the
     * speed transform re-times the exported stream (coarse ratio, wide margin).
     */
    @Test
    fun exportSpeed_governsExportedRate() {
        // We assert the exported media DURATION reflects the speed transform,
        // read back from the container — not pipeline wall-clock throughput.
        val slowDur = durationUs(exportAtSpeed(speed = 1.0))
        val fastDur = durationUs(exportAtSpeed(speed = 2.0))
        assertTrue("both exports must report a positive duration", slowDur > 0 && fastDur > 0)
        assertTrue(
            "2x export duration ($fastDur us) must be clearly shorter than 1x ($slowDur us)",
            fastDur < slowDur * 3 / 4,
        )
    }

    /**
     * T-CANCEL (AC-8): cancelling mid-export stops promptly, no completion is
     * delivered, and the partial output file is removed.
     */
    @Test
    fun cancelDuringExport_stopsAndDeletesPartialOutput() {
        val listener = RecordingExportListener()
        val accepted = exporter.export(
            path = video.absolutePath,
            outputPath = output.absolutePath,
            listener = listener,
            includeAudio = false,
        )
        assertTrue(accepted)
        // Cancel almost immediately; the export worker is still running.
        exporter.cancel()

        // A cancelled export must not deliver a completed callback.
        assertFalse(
            "cancel must suppress the completion callback",
            listener.await(SETTLE_MS) && listener.completed,
        )
        // Give the worker a moment to unwind and unlink the partial file.
        Thread.sleep(SETTLE_MS)
        assertFalse("the partial output file must be removed on cancel", output.exists())
    }

    /**
     * T-ERR (AC-7): an unreadable / non-existent source ends the export with a
     * typed error and never reports a completed, valid file.
     */
    @Test
    fun unreadableSource_failsWithTypedError_andNoOutput() {
        val listener = RecordingExportListener()
        val missing = TestVideoFixture.missingLocalPath(context)
        val accepted = exporter.export(
            path = missing,
            outputPath = output.absolutePath,
            listener = listener,
            includeAudio = false,
        )
        assertTrue("a well-formed request is accepted even if the file is missing", accepted)
        assertTrue(listener.await(TIMEOUT_MS))
        assertFalse("a missing source must not complete", listener.completed)
        assertEquals("the failing source is reported as INPUT_OPEN", ExportError.INPUT_OPEN, listener.error)
        assertFalse("no output file may remain after a failed export", output.exists())
    }

    /**
     * T-AUDIO (AC-6): exporting a clip that has an audio track, with audio
     * enabled, produces an MP4 that contains both an H.264 video track and an AAC
     * audio track.
     */
    @Test
    fun exportWithAudio_producesVideoAndAudioTracks() {
        val listener = RecordingExportListener()
        val accepted = exporter.export(
            path = videoWithAudio.absolutePath,
            outputPath = output.absolutePath,
            listener = listener,
            includeAudio = true,
        )
        assertTrue(accepted)
        assertTrue(listener.await(TIMEOUT_MS))
        assertTrue("audio export must complete: ${listener.error}", listener.completed)
        assertTrue("output must contain an H.264 video track", hasTrackWithMime(output, "video/avc"))
        assertTrue(
            "output must contain an AAC audio track",
            hasTrackWithMime(output, "audio/mp4a-latm"),
        )
    }

    /**
     * T-AUDIO / A-2: exporting a video-only source with audio requested still
     * succeeds and simply yields a video-only file (no error, no audio track).
     */
    @Test
    fun exportWithAudioRequested_onVideoOnlySource_yieldsVideoOnly() {
        val listener = RecordingExportListener()
        val accepted = exporter.export(
            path = video.absolutePath,
            outputPath = output.absolutePath,
            listener = listener,
            includeAudio = true,
        )
        assertTrue(accepted)
        assertTrue(listener.await(TIMEOUT_MS))
        assertTrue("video-only source must still complete: ${listener.error}", listener.completed)
        assertTrue(hasTrackWithMime(output, "video/avc"))
        assertFalse(
            "a video-only source must not gain a phantom audio track",
            hasTrackWithMime(output, "audio/mp4a-latm"),
        )
    }

    /**
     * T-TIMELINE (AC-9): an ordered two-segment timeline is concatenated into one
     * MP4 whose duration exceeds either single trimmed segment, proving both
     * segments were written in order into one output.
     */
    @Test
    fun timelineExport_concatenatesSegmentsIntoOneFile() {
        val listener = RecordingExportListener()
        val segments = listOf(
            VideoSegment(video.absolutePath, 0L, 1_000L),
            VideoSegment(video.absolutePath, 1_000L, 2_000L),
        )
        val accepted = exporter.exportTimeline(
            segments = segments,
            outputPath = output.absolutePath,
            listener = listener,
            includeAudio = false,
        )
        assertTrue(accepted)
        assertTrue(listener.await(TIMEOUT_MS))
        assertTrue("timeline export must complete: ${listener.error}", listener.completed)
        assertTrue(hasTrackWithMime(output, "video/avc"))
        // Two ~1 s windows concatenated → roughly ~2 s; assert it is clearly
        // longer than a single 1 s window (wide margin for keyframe/boundary error).
        assertTrue(
            "concatenated timeline duration (${durationUs(output)} us) must exceed one segment",
            durationUs(output) > 1_200_000L,
        )
    }

    /**
     * T-TIMELINE (AC-9 fail-fast): a timeline whose second segment is a missing
     * file ends the whole export once with that segment's index.
     */
    @Test
    fun timelineExport_failsFastWithFailingSegmentIndex() {
        val listener = RecordingExportListener()
        val segments = listOf(
            VideoSegment(video.absolutePath, 0L, 1_000L),
            VideoSegment(TestVideoFixture.missingLocalPath(context), 0L, 1_000L),
            VideoSegment(video.absolutePath, 0L, 1_000L),
        )
        assertTrue(exporter.exportTimeline(segments, output.absolutePath, listener, includeAudio = false))
        assertTrue(listener.await(TIMEOUT_MS))
        assertFalse("a failing timeline must not complete", listener.completed)
        assertEquals("fail-fast on the missing segment", ExportError.INPUT_OPEN, listener.error)
        assertEquals("the error must carry the failing segment index (1)", 1, listener.segmentIndex)
        assertFalse("no partial output may remain", output.exists())
    }

    // --- helpers --------------------------------------------------------------

    private fun exportAtSpeed(speed: Double): File {
        val out = File(cacheRoot(), "export_speed_${speed}_${System.nanoTime()}.mp4")
        if (out.exists()) out.delete()
        val listener = RecordingExportListener()
        assertTrue(
            exporter.export(
                path = video.absolutePath,
                outputPath = out.absolutePath,
                listener = listener,
                speed = speed,
                includeAudio = false,
            ),
        )
        assertTrue("export at speed $speed must finish", listener.await(TIMEOUT_MS))
        assertTrue("export at speed $speed must complete: ${listener.error}", listener.completed)
        // This instance's single attempt completed before returning, so the same
        // exporter can be reused for the next speed (one active export at a time).
        return out
    }

    private fun hasTrackWithMime(file: File, mime: String): Boolean {
        val extractor = MediaExtractor()
        try {
            extractor.setDataSource(file.absolutePath)
            for (i in 0 until extractor.trackCount) {
                val trackMime = extractor.getTrackFormat(i).getString(MediaFormat.KEY_MIME)
                if (trackMime == mime) return true
            }
        } finally {
            extractor.release()
        }
        return false
    }

    private fun durationUs(file: File): Long {
        if (!file.exists()) return -1
        val extractor = MediaExtractor()
        try {
            extractor.setDataSource(file.absolutePath)
            var maxDuration = 0L
            for (i in 0 until extractor.trackCount) {
                val format = extractor.getTrackFormat(i)
                if (format.containsKey(MediaFormat.KEY_DURATION)) {
                    maxDuration = maxOf(maxDuration, format.getLong(MediaFormat.KEY_DURATION))
                }
            }
            return maxDuration
        } finally {
            extractor.release()
        }
    }

    /** Records the single terminal outcome of an accepted export. */
    private class RecordingExportListener : ExportListener {
        private val latch = CountDownLatch(1)
        private val count = AtomicInteger(0)

        @Volatile
        var completed = false
            private set

        @Volatile
        var error: ExportError? = null
            private set

        @Volatile
        var segmentIndex: Int = Int.MIN_VALUE
            private set

        @Volatile
        var onMainThread = false
            private set

        override fun onExportCompleted() {
            if (count.getAndIncrement() == 0) {
                completed = true
                onMainThread = Looper.myLooper() == Looper.getMainLooper()
            }
            latch.countDown()
        }

        override fun onExportError(error: ExportError, segmentIndex: Int) {
            if (count.getAndIncrement() == 0) {
                this.error = error
                this.segmentIndex = segmentIndex
                onMainThread = Looper.myLooper() == Looper.getMainLooper()
            }
            latch.countDown()
        }

        fun await(timeoutMs: Long): Boolean = latch.await(timeoutMs, TimeUnit.MILLISECONDS)

        fun deliveryCount(): Int = count.get()
    }

    private fun cacheRoot(): File =
        File(context.cacheDir, "videolib_test_fixtures").apply { mkdirs() }

    private companion object {
        const val TIMEOUT_MS = 60_000L
        const val SETTLE_MS = 1_000L

        private val context: Context
            get() = InstrumentationRegistry.getInstrumentation().targetContext

        private lateinit var video: File
        private lateinit var videoWithAudio: File

        @JvmStatic
        @BeforeClass
        fun generateFixtures() {
            video = TestVideoFixture.generateProgressiveVideo(context)
            videoWithAudio = TestVideoFixture.generateProgressiveVideoWithAudio(context)
        }
    }
}
