package com.cii.videolib

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.BeforeClass
import org.junit.Test
import org.junit.runner.RunWith

/**
 * TEST-01 / TEST-02 — progressive-playback device coverage (AC-1, AC-2, AC-3, AC-5).
 *
 * Exercises the real path `VideoPreview.play` → JNI → `VideoPlayback::decodeAttempt`
 * → `PreviewRenderer::pushFrame` against a consumable [PlaybackSurfaceProbe] and the
 * deterministic [TestVideoFixture] clip. No mock decoder/renderer and no production
 * observability hook (C-08/C-09). Synchronization is latch-based
 * ([PlaybackEventLog]); sleeps are never used as the sole synchronization. There is
 * no numeric latency assertion — only ordering (first visible frame before terminal
 * completion, and progressive distinguishable frames).
 *
 * Requires a supported EGL/GLES 3.0 ARM device (attaching the probe surface performs
 * real EGL init); on an environment without it, `attachSurface` returns false and the
 * test fails with that explicit cause rather than passing silently.
 */
@RunWith(AndroidJUnit4::class)
class ProgressivePlaybackInstrumentedTest {

    private lateinit var preview: VideoPreview
    private lateinit var probe: PlaybackSurfaceProbe

    @Before
    fun setUp() {
        preview = VideoPreview()
        assertTrue(
            "attachSurface(probe) must succeed on a supported EGL/GLES device",
            preview.attachSurface(probe(PlaybackEventLog()).surface),
        )
    }

    @After
    fun tearDown() {
        // Always quiesce and release, regardless of assertion outcome.
        preview.stop()
        preview.release()
        probe.close()
    }

    /**
     * TEST-01 (AC-1, AC-2, AC-3): acceptance returns before any terminal outcome;
     * the first visible frame is presented before completion/EOF; successive visible
     * frames are distinguishable and in presentation order; a second concurrent play
     * is rejected.
     */
    @Test
    fun play_isAcceptedAsync_presentsFirstFrameBeforeCompletion_andRejectsSecondPlay() {
        val log = PlaybackEventLog()
        probe.log = log
        val listener = RecordingPlaybackListener(log)

        val accepted = preview.play(video.absolutePath, listener)
        assertTrue("play() must accept a valid request on a ready surface", accepted)
        // AC-1: acceptance did not wait for terminal completion. The clip is ~3 s of
        // paced playback, so no terminal can have been delivered by the time play()
        // returned.
        assertNull("play() must return before any terminal outcome (AC-1)", log.terminal)

        // AC-1 (single active attempt): a second concurrent play is rejected while the
        // first attempt is active.
        val secondListener = RecordingPlaybackListener(PlaybackEventLog())
        assertFalse(
            "a second concurrent play must be rejected",
            preview.play(video.absolutePath, secondListener),
        )

        // AC-2: a first visible frame is actually presented (consumed off the surface)
        // before the clip completes.
        assertTrue(
            "expected a first presented frame within ${FIRST_FRAME_TIMEOUT_MS} ms",
            log.awaitFirstFrame(FIRST_FRAME_TIMEOUT_MS),
        )

        // Let the clip finish so ordering between first-frame and completion is decided.
        assertTrue(
            "expected a terminal outcome within ${TERMINAL_TIMEOUT_MS} ms",
            log.awaitTerminal(TERMINAL_TIMEOUT_MS),
        )
        assertEquals(
            "natural EOF must complete, not error",
            TerminalKind.COMPLETED,
            log.terminal?.kind,
        )
        assertTrue(
            "a visible frame must precede terminal completion (AC-2)",
            log.firstFramePrecededTerminal(),
        )

        // AC-3: successive presented frames are distinguishable and in presentation
        // order (the fixture ramps its red channel per frame).
        assertTrue(
            "expected multiple presented frames, got ${log.frameCount()}",
            log.frameCount() >= 2,
        )
        assertTrue(
            "expected distinguishable frames, distinct luma = ${log.distinctLumaCount()}",
            log.distinctLumaCount() >= 2,
        )
        assertTrue(
            "presented frame luma must be non-decreasing (presentation order): " +
                log.frames.map { it.luma },
            isNonDecreasing(log.frames.map { it.luma }),
        )
    }

    /**
     * TEST-02 (AC-5): natural completion is delivered exactly once, on the main
     * thread, after at least one presented frame; a later valid attempt is accepted.
     */
    @Test
    fun naturalCompletion_deliveredOnceOnMain_thenLaterAttemptAccepted() {
        val firstLog = PlaybackEventLog()
        probe.log = firstLog

        assertTrue(preview.play(video.absolutePath, RecordingPlaybackListener(firstLog)))
        assertTrue(
            "expected completion within ${TERMINAL_TIMEOUT_MS} ms",
            firstLog.awaitTerminal(TERMINAL_TIMEOUT_MS),
        )

        assertTrue("completion requires at least one presented frame", firstLog.frameCount() >= 1)
        assertEquals(TerminalKind.COMPLETED, firstLog.terminal?.kind)
        assertEquals(
            "exactly one terminal callback for a non-cancelled attempt",
            1,
            firstLog.terminalDeliveryCount(),
        )
        assertTrue(
            "terminal callback must be delivered on the main thread",
            firstLog.terminal?.onMainThread == true,
        )

        // A later valid attempt is accepted once the prior attempt has completed.
        val secondLog = PlaybackEventLog()
        probe.log = secondLog
        assertTrue(
            "a later valid attempt must be accepted after completion",
            preview.play(video.absolutePath, RecordingPlaybackListener(secondLog)),
        )
        assertTrue(secondLog.awaitTerminal(TERMINAL_TIMEOUT_MS))
        assertEquals(TerminalKind.COMPLETED, secondLog.terminal?.kind)
    }

    // Creates (once) and remembers the probe backed by the given initial log.
    private fun probe(initial: PlaybackEventLog): PlaybackSurfaceProbe {
        probe = PlaybackSurfaceProbe(initial)
        return probe
    }

    private companion object {
        const val FIRST_FRAME_TIMEOUT_MS = 10_000L
        const val TERMINAL_TIMEOUT_MS = 30_000L

        private val context: Context
            get() = InstrumentationRegistry.getInstrumentation().targetContext

        private lateinit var video: java.io.File

        @JvmStatic
        @BeforeClass
        fun generateFixture() {
            video = TestVideoFixture.generateProgressiveVideo(context)
        }

        private fun isNonDecreasing(values: List<Int>): Boolean {
            for (i in 1 until values.size) {
                if (values[i] < values[i - 1]) return false
            }
            return true
        }
    }
}
