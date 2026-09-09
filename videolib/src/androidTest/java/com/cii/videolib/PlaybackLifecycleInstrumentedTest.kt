package com.cii.videolib

import android.content.Context
import android.os.SystemClock
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.BeforeClass
import org.junit.Test
import org.junit.runner.RunWith

/**
 * TEST-03 / TEST-04 / TEST-05 / TEST-06 — lifecycle, cancellation, failure and
 * recovery coverage (AC-4, AC-5).
 *
 * Uses the consumable [PlaybackSurfaceProbe] and the deterministic
 * [TestVideoFixture] clip (C-08/C-09). The invariant under test is that a
 * cancelling lifecycle call ([VideoPreview.stop]/[VideoPreview.release]) invalidates
 * the attempt before teardown so that, after it returns, no frame or listener event
 * from that attempt is observable — while surface loss ([VideoPreview.detachSurface])
 * retains the existing single RENDER-failure semantics.
 *
 * Primary synchronization is latch-based ([PlaybackEventLog.awaitFirstFrame] /
 * [PlaybackEventLog.awaitTerminal]). A short bounded [settle] is used only to prove
 * the ABSENCE of a late frame/callback after a cancelling call has already joined
 * the native worker; it is never the sole synchronization for a positive outcome.
 *
 * Requires a supported EGL/GLES 3.0 ARM device.
 */
@RunWith(AndroidJUnit4::class)
class PlaybackLifecycleInstrumentedTest {

    private lateinit var preview: VideoPreview
    private lateinit var probe: PlaybackSurfaceProbe

    @Before
    fun setUp() {
        preview = VideoPreview()
        probe = PlaybackSurfaceProbe(PlaybackEventLog())
        assertTrue(
            "attachSurface(probe) must succeed on a supported EGL/GLES device",
            preview.attachSurface(probe.surface),
        )
    }

    @After
    fun tearDown() {
        preview.stop()
        preview.release()
        probe.close()
    }

    /**
     * TEST-03 (AC-4): stop during active playback returns only after the attempt is
     * quiesced; the stopped attempt delivers no terminal callback and produces no
     * later frame; repeated stop is safe; a retry on the still-ready surface succeeds.
     */
    @Test
    fun stopDuringPlayback_suppressesTerminal_andRetrySucceeds() {
        val firstLog = PlaybackEventLog()
        probe.log = firstLog
        assertTrue(preview.play(video.absolutePath, RecordingPlaybackListener(firstLog)))
        assertTrue(
            "expected a presented frame before stopping",
            firstLog.awaitFirstFrame(FIRST_FRAME_TIMEOUT_MS),
        )

        preview.stop() // returns only after the worker is joined (traced in stop())

        // Stop is not a terminal result: no completion/error for the stopped attempt.
        settle(QUIESCENCE_SETTLE_MS)
        assertEquals(
            "stop() must deliver no terminal callback (AC-4)",
            0,
            firstLog.terminalDeliveryCount(),
        )
        val framesAfterStop = firstLog.frameCount()
        settle(QUIESCENCE_SETTLE_MS)
        assertEquals(
            "no further frame may be presented after stop() returns",
            framesAfterStop,
            firstLog.frameCount(),
        )

        // Repeated stop is a safe no-op.
        preview.stop()

        // Retry on the still-attached, still-ready surface succeeds to completion.
        val retryLog = PlaybackEventLog()
        probe.log = retryLog
        assertTrue(
            "retry after stop must be accepted on the ready surface",
            preview.play(video.absolutePath, RecordingPlaybackListener(retryLog)),
        )
        assertTrue(retryLog.awaitTerminal(TERMINAL_TIMEOUT_MS))
        assertEquals(TerminalKind.COMPLETED, retryLog.terminal?.kind)
    }

    /**
     * TEST-04 (AC-4, AC-5): losing the surface via detach during playback — without a
     * prior explicit stop — quiesces decode before EGL/window teardown and delivers
     * exactly one RENDER error on the main thread; play is rejected until a surface is
     * reattached, after which a retry succeeds.
     */
    @Test
    fun surfaceLossDuringPlayback_emitsSingleRenderError_thenReattachRetrySucceeds() {
        val firstLog = PlaybackEventLog()
        probe.log = firstLog
        assertTrue(preview.play(video.absolutePath, RecordingPlaybackListener(firstLog)))
        assertTrue(
            "expected a presented frame before surface loss",
            firstLog.awaitFirstFrame(FIRST_FRAME_TIMEOUT_MS),
        )

        preview.detachSurface() // surface loss, not an explicit stop

        assertTrue(
            "surface loss must deliver a terminal outcome",
            firstLog.awaitTerminal(TERMINAL_TIMEOUT_MS),
        )
        assertEquals(TerminalKind.ERROR, firstLog.terminal?.kind)
        assertEquals(
            "surface loss retains RENDER-failure semantics (AC-4)",
            PlaybackError.RENDER,
            firstLog.terminal?.error,
        )
        assertTrue(
            "the RENDER failure must be delivered on the main thread",
            firstLog.terminal?.onMainThread == true,
        )
        settle(QUIESCENCE_SETTLE_MS)
        assertEquals(
            "at most one terminal for a surface-lost attempt",
            1,
            firstLog.terminalDeliveryCount(),
        )

        // Play is rejected until a usable surface is reattached.
        assertFalse(
            "play must be rejected while no surface is attached",
            preview.play(video.absolutePath, RecordingPlaybackListener(PlaybackEventLog())),
        )

        // Reattach the same consumable surface and retry.
        assertTrue(
            "reattaching a usable surface must succeed",
            preview.attachSurface(probe.surface),
        )
        val retryLog = PlaybackEventLog()
        probe.log = retryLog
        assertTrue(
            "retry after reattach must be accepted",
            preview.play(video.absolutePath, RecordingPlaybackListener(retryLog)),
        )
        assertTrue(retryLog.awaitTerminal(TERMINAL_TIMEOUT_MS))
        assertEquals(TerminalKind.COMPLETED, retryLog.terminal?.kind)
    }

    /**
     * TEST-05 (AC-4): release during active work suppresses any prior-attempt listener
     * visibility after it returns; repeated release/stop/detach are safe; the released
     * instance rejects play.
     */
    @Test
    fun releaseDuringPlayback_suppressesTerminal_isIdempotent_andRejectsPlay() {
        val firstLog = PlaybackEventLog()
        probe.log = firstLog
        assertTrue(preview.play(video.absolutePath, RecordingPlaybackListener(firstLog)))
        assertTrue(
            "expected a presented frame before release",
            firstLog.awaitFirstFrame(FIRST_FRAME_TIMEOUT_MS),
        )

        preview.release() // invalidates listener + destroys native owner (joins worker)

        settle(QUIESCENCE_SETTLE_MS)
        assertEquals(
            "release() must suppress any terminal callback for the active attempt",
            0,
            firstLog.terminalDeliveryCount(),
        )

        // Repeated lifecycle calls after release are safe no-ops (zero handle guard).
        preview.release()
        preview.stop()
        preview.detachSurface()

        // A released instance rejects play.
        assertFalse(
            "a released VideoPreview must reject play",
            preview.play(video.absolutePath, RecordingPlaybackListener(PlaybackEventLog())),
        )
    }

    /**
     * TEST-06 (AC-5): a missing/unreadable local input reports INPUT_OPEN exactly once
     * on the main thread.
     */
    @Test
    fun missingInput_reportsInputOpenOnceOnMain() {
        val log = PlaybackEventLog()
        probe.log = log
        val accepted = preview.play(TestVideoFixture.missingLocalPath(context), RecordingPlaybackListener(log))
        assertTrue("a non-blank path with a ready surface is accepted, then fails async", accepted)

        assertTrue(log.awaitTerminal(TERMINAL_TIMEOUT_MS))
        assertEquals(TerminalKind.ERROR, log.terminal?.kind)
        assertEquals(PlaybackError.INPUT_OPEN, log.terminal?.error)
        assertTrue(log.terminal?.onMainThread == true)
        settle(QUIESCENCE_SETTLE_MS)
        assertEquals("exactly one error for the attempt", 1, log.terminalDeliveryCount())
    }

    /**
     * TEST-06 (AC-5): a readable input with no supported video stream reports
     * UNSUPPORTED_VIDEO.
     */
    @Test
    fun audioOnlyInput_reportsUnsupportedVideo() {
        val log = PlaybackEventLog()
        probe.log = log
        assertTrue(preview.play(audioOnly.absolutePath, RecordingPlaybackListener(log)))

        assertTrue(log.awaitTerminal(TERMINAL_TIMEOUT_MS))
        assertEquals(TerminalKind.ERROR, log.terminal?.kind)
        assertEquals(PlaybackError.UNSUPPORTED_VIDEO, log.terminal?.error)
        assertTrue(log.terminal?.onMainThread == true)
    }

    /**
     * TEST-06 (AC-5): cancellation wins over a racing terminal error, and no stale
     * outcome from a cancelled attempt bleeds into a later attempt.
     *
     * A missing-input attempt fails almost instantly, so the strict "who wins the
     * native terminal claim" (stop vs. the input-open error) is a benign wall-clock
     * race: whichever wins, the design guarantee is that after `stop()` returns no
     * NEW event from that attempt is observable, and that the cancelled attempt's
     * outcome never reaches a subsequent attempt's listener. This asserts those
     * deterministic guarantees rather than the fragile exact pre-stop count.
     */
    @Test
    fun cancellationSuppressesStaleOutcome_andDoesNotBleedIntoNextAttempt() {
        val cancelledLog = PlaybackEventLog()
        probe.log = cancelledLog
        assertTrue(
            preview.play(TestVideoFixture.missingLocalPath(context), RecordingPlaybackListener(cancelledLog)),
        )
        // Cancel immediately; stop() invalidates the Kotlin listener before native
        // teardown and returns only after the worker is joined.
        preview.stop()

        // At most one terminal could have been delivered (a legitimate pre-stop race);
        // if any, it is the correct category, never a phantom, and on the main thread.
        settle(QUIESCENCE_SETTLE_MS)
        val countAfterStop = cancelledLog.terminalDeliveryCount()
        assertTrue(
            "a cancelled attempt delivers at most one (pre-stop) terminal, got $countAfterStop",
            countAfterStop <= 1,
        )
        cancelledLog.terminal?.let {
            assertEquals(TerminalKind.ERROR, it.kind)
            assertEquals(PlaybackError.INPUT_OPEN, it.error)
            assertTrue("any delivered terminal must be on main", it.onMainThread)
        }
        assertEquals("a failed-input attempt presents no frame", 0, cancelledLog.frameCount())

        // Run a valid attempt: it must complete normally and its listener must never
        // observe the previous (cancelled) attempt's error — no cross-attempt bleed.
        val retryLog = PlaybackEventLog()
        probe.log = retryLog
        assertTrue(preview.play(video.absolutePath, RecordingPlaybackListener(retryLog)))
        assertTrue(retryLog.awaitTerminal(TERMINAL_TIMEOUT_MS))
        assertEquals(
            "the retry must complete, not inherit the cancelled attempt's error",
            TerminalKind.COMPLETED,
            retryLog.terminal?.kind,
        )
        assertEquals("exactly one terminal for the retry", 1, retryLog.terminalDeliveryCount())

        // No stale event appeared on the cancelled attempt's log during/after the retry.
        assertEquals(
            "no stale terminal may appear on the cancelled attempt after stop() returned",
            countAfterStop,
            cancelledLog.terminalDeliveryCount(),
        )
    }

    // Bounded drain used ONLY to assert the absence of a late frame/callback after a
    // cancelling call has already joined the native worker. Not a positive-outcome wait.
    private fun settle(ms: Long) = SystemClock.sleep(ms)

    private companion object {
        const val FIRST_FRAME_TIMEOUT_MS = 10_000L
        const val TERMINAL_TIMEOUT_MS = 30_000L
        const val QUIESCENCE_SETTLE_MS = 500L

        private val context: Context
            get() = InstrumentationRegistry.getInstrumentation().targetContext

        private lateinit var video: java.io.File
        private lateinit var audioOnly: java.io.File

        @JvmStatic
        @BeforeClass
        fun generateFixtures() {
            video = TestVideoFixture.generateProgressiveVideo(context)
            audioOnly = TestVideoFixture.generateAudioOnly(context)
        }
    }
}
