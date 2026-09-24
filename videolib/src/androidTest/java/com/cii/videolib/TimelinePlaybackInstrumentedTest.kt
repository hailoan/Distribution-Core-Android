package com.cii.videolib

import android.content.Context
import android.os.Looper
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
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger
import kotlin.math.abs

/**
 * ANDT-TL-1..7 — sequential-timeline device coverage for [VideoPreview.playTimeline]
 * (AC-1..8, D-6 fail-fast).
 *
 * Exercises the real path `playTimeline` → JNI `nativePlayTimeline` →
 * `VideoPlayback::runTimeline` → per-segment `decodeAttempt` → `PreviewRenderer` against
 * a consumable [PlaybackSurfaceProbe] and the deterministic [TestVideoFixture] clip.
 * No mock decoder/renderer.
 *
 * Observability without a per-segment boundary callback: the fixture's centre pixel
 * ramps its RED channel 0→255 across the clip and the probe records that value per
 * presented frame ([PlaybackEventLog.frames]). Behaviour is therefore asserted from the
 * aggregate luma signature — a trim window yields a bounded luma band; a constant-output
 * per-segment filter pins that segment's frames to a known value; back-to-back playback
 * produces a luma "sawtooth" reset; segment order is read from the sequence of presented
 * frames. Luma tolerances absorb H.264 + RGBA-readback colour error; they are device
 * checks tunable at IT-DEVICE, never latency assertions (the one timing check, ANDT-TL-4,
 * is a coarse completion-time ratio with a wide margin).
 *
 * Requires a supported EGL/GLES 3.0 ARM device: attaching the probe performs real EGL
 * init, so on an unsupported environment `attachSurface` returns false and the suite
 * fails with that explicit cause rather than passing silently.
 */
@RunWith(AndroidJUnit4::class)
class TimelinePlaybackInstrumentedTest {

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
     * ANDT-TL-1 (AC-1, AC-7): a ≥2-segment timeline is accepted, every segment is
     * retained and played back-to-back on the one surface, and exactly one completion is
     * delivered on the main thread. Back-to-back is proven by a luma "sawtooth": the ramp
     * climbs, then resets when the second segment restarts from its start.
     */
    @Test
    fun multiSegmentTimeline_playsBackToBack_completesExactlyOnceOnMain() {
        val log = PlaybackEventLog()
        probe.log = log
        val listener = RecordingTimelineListener()

        val accepted = preview.playTimeline(listOf(fullSegment(), fullSegment()), listener)
        assertTrue("playTimeline must accept a non-empty list on a ready surface", accepted)

        // AC-1: a second concurrent timeline is rejected while one is active.
        assertFalse(
            "a second concurrent timeline must be rejected",
            preview.playTimeline(listOf(fullSegment()), RecordingTimelineListener()),
        )

        assertTrue(
            "expected timeline completion within ${MULTI_TERMINAL_TIMEOUT_MS} ms",
            listener.await(MULTI_TERMINAL_TIMEOUT_MS),
        )
        assertTrue("timeline must complete, not error: ${listener.error}", listener.completed)
        assertEquals(
            "exactly one terminal for the whole timeline (AC-7)",
            1,
            listener.deliveryCount(),
        )
        assertTrue("terminal must be delivered on the main thread", listener.onMainThread)

        val luma = log.frames.map { it.luma }
        assertTrue("expected many presented frames across two segments, got ${luma.size}", luma.size >= 4)
        assertTrue(
            "two segments must present a luma reset (sawtooth) proving a second segment " +
                "played back-to-back: $luma",
            hasLumaReset(luma, drop = SEGMENT_RESET_DROP),
        )
    }

    /**
     * ANDT-TL-2 (AC-2, AC-3): a single segment trimmed to `[A, B)` presents only frames
     * whose media time lies in that window — nothing before A (the fixture's low-luma
     * head is skipped) and nothing at or after B (the high-luma tail is not shown).
     */
    @Test
    fun trimmedSegment_presentsOnlyFramesInsideTheHalfOpenWindow() {
        val log = PlaybackEventLog()
        probe.log = log
        val listener = RecordingTimelineListener()

        // Window [1000, 2000) ms → fixture frames ~15..29 → centre luma ~87..168.
        val accepted = preview.playTimeline(
            listOf(VideoSegment(video.absolutePath, TRIM_A_MS, TRIM_B_MS)),
            listener,
        )
        assertTrue("trimmed segment must be accepted", accepted)
        assertTrue(listener.await(TERMINAL_TIMEOUT_MS))
        assertTrue("trimmed segment must complete: ${listener.error}", listener.completed)

        val luma = log.frames.map { it.luma }
        assertTrue("expected presented frames inside the window, got ${luma.size}", luma.size >= 2)
        assertTrue(
            "no frame before A may be presented (min luma ${luma.min()} >= " +
                "${TRIM_LUMA_LOW - LUMA_TOL}): $luma",
            luma.min() >= TRIM_LUMA_LOW - LUMA_TOL,
        )
        assertTrue(
            "no frame at/after B may be presented (max luma ${luma.max()} <= " +
                "${TRIM_LUMA_HIGH + LUMA_TOL}): $luma",
            luma.max() <= TRIM_LUMA_HIGH + LUMA_TOL,
        )
    }

    /**
     * ANDT-TL-3 (AC-4): a per-segment filter is applied only to its own segment. Segment 0
     * carries a constant-output filter (centre RED pinned to ~[FILTER_LUMA]); segment 1 is
     * unfiltered (the raw ramp). Ordering pins the filter to segment 0: the first presented
     * frame is the filter constant, and a raw low-luma frame appears only later.
     */
    @Test
    fun perSegmentFilter_appliesOnlyToItsOwnSegment() {
        val log = PlaybackEventLog()
        probe.log = log
        val listener = RecordingTimelineListener()

        val filtered = VideoSegment(
            path = video.absolutePath,
            startMs = 0L,
            endMs = FULL_END_MS,
            appearance = VideoAppearance(filter = CONSTANT_FILTER),
        )
        val plain = fullSegment()
        assertTrue(preview.playTimeline(listOf(filtered, plain), listener))
        assertTrue(listener.await(MULTI_TERMINAL_TIMEOUT_MS))
        assertTrue("filtered timeline must complete: ${listener.error}", listener.completed)

        val luma = log.frames.map { it.luma }
        assertTrue("expected frames from both segments, got ${luma.size}", luma.size >= 4)

        // Segment 0 filtered → a substantial run of frames pinned to the filter constant,
        // starting from the very first presented frame.
        assertTrue(
            "first presented frame must be the filter constant (segment 0 filtered): " +
                "${luma.first()}",
            abs(luma.first() - FILTER_LUMA) <= LUMA_TOL,
        )
        assertTrue(
            "the filtered segment must pin many frames near ${FILTER_LUMA}: $luma",
            luma.count { abs(it - FILTER_LUMA) <= LUMA_TOL } >= FILTER_CLUSTER_MIN,
        )
        // Segment 1 unfiltered → the raw ramp reaches values far from the filter constant,
        // and those appear only after the filtered block (per-segment, not whole-timeline).
        val firstRawLowIndex = luma.indexOfFirst { it <= RAW_LOW_LUMA }
        assertTrue("segment 1 must present raw low-luma frames: $luma", firstRawLowIndex >= 0)
        assertTrue(
            "raw (unfiltered) frames must appear only after the filtered segment: $luma",
            firstRawLowIndex > 0,
        )
    }

    /**
     * ANDT-TL-4 (AC-5): a per-segment speed governs that segment's rate. A single-segment
     * timeline at 2x completes meaningfully sooner than the same content at 1x. Coarse
     * completion-time comparison with a wide margin (the fixture is ~3 s at 1x, ~1.5 s at
     * 2x); both are still bounded by the terminal timeout.
     */
    @Test
    fun perSegmentSpeed_governsSegmentRate() {
        val slowMs = measureTimelineDurationMs(speed = 1.0)
        val fastMs = measureTimelineDurationMs(speed = 2.0)

        assertTrue(
            "2x playback ($fastMs ms) must be clearly faster than 1x ($slowMs ms)",
            fastMs < slowMs - MIN_SPEEDUP_DELTA_MS,
        )
    }

    /**
     * ANDT-TL-5 (AC-6): on reaching a segment's end the timeline advances to the next
     * segment, which begins at its own start A with its own appearance. Segment 0 is
     * filtered (constant), segment 1 is unfiltered and trimmed to start at A>0; the
     * presented sequence is therefore a filtered-constant block followed by a raw ramp that
     * starts at A's luma (not at 0), proving advance-at-B and start-at-A with segment 1's
     * appearance.
     */
    @Test
    fun segmentBoundary_advancesToNextSegmentAtItsStartWithItsAppearance() {
        val log = PlaybackEventLog()
        probe.log = log
        val listener = RecordingTimelineListener()

        val filtered = VideoSegment(
            path = video.absolutePath,
            startMs = 0L,
            endMs = FULL_END_MS,
            appearance = VideoAppearance(filter = CONSTANT_FILTER),
        )
        // Segment 1 unfiltered, trimmed to start at A=1500 ms → raw ramp begins ~luma 133.
        val trimmedTail = VideoSegment(video.absolutePath, ADVANCE_A_MS, FULL_END_MS)
        assertTrue(preview.playTimeline(listOf(filtered, trimmedTail), listener))
        assertTrue(listener.await(MULTI_TERMINAL_TIMEOUT_MS))
        assertTrue("advance timeline must complete: ${listener.error}", listener.completed)

        val luma = log.frames.map { it.luma }
        assertTrue("expected frames from both segments, got ${luma.size}", luma.size >= 4)

        // Segment 0 (filtered) plays first.
        assertTrue(
            "first frame must be segment 0's filter constant: ${luma.first()}",
            abs(luma.first() - FILTER_LUMA) <= LUMA_TOL,
        )
        // Segment 1 begins at its A: its raw frames start around the A-luma band and never
        // present segment 1's skipped head (nothing raw below the A band).
        val tail = luma.drop(luma.indexOfLast { abs(it - FILTER_LUMA) <= LUMA_TOL } + 1)
        assertTrue("segment 1 must present frames after the boundary: $luma", tail.size >= 2)
        assertTrue(
            "segment 1 must begin at its start A (no raw frame below the A band): $tail",
            tail.none { it < ADVANCE_LUMA_START - LUMA_TOL },
        )
        assertTrue(
            "segment 1 must reach its high (unfiltered) ramp tail: $tail",
            tail.max() >= ADVANCE_LUMA_END - LUMA_TOL,
        )
    }

    /**
     * ANDT-TL-6 (D-6 fail-fast): a segment that cannot be opened ends the whole timeline
     * with a single error that carries the failing segment's index. Segment 0 is valid,
     * segment 1 is a missing local file → one `onTimelineError(INPUT_OPEN, 1)` on main.
     */
    @Test
    fun failingSegment_endsTimelineOnceWithFailingSegmentIndex() {
        val log = PlaybackEventLog()
        probe.log = log
        val listener = RecordingTimelineListener()

        val segments = listOf(
            VideoSegment(video.absolutePath, 0L, SHORT_END_MS), // 0: valid, short
            VideoSegment(TestVideoFixture.missingLocalPath(context), 0L, FULL_END_MS), // 1: missing
            fullSegment(), // 2: never reached
        )
        assertTrue(preview.playTimeline(segments, listener))

        assertTrue(
            "expected a terminal outcome within ${TERMINAL_TIMEOUT_MS} ms",
            listener.await(TERMINAL_TIMEOUT_MS),
        )
        assertFalse("a failing timeline must not report completion", listener.completed)
        assertEquals("fail-fast on the missing input", PlaybackError.INPUT_OPEN, listener.error)
        assertEquals("the error must carry the failing segment index (1)", 1, listener.segmentIndex)
        assertTrue("the error must be delivered on the main thread", listener.onMainThread)
        settle(QUIESCENCE_SETTLE_MS)
        assertEquals("exactly one terminal for the whole timeline", 1, listener.deliveryCount())
    }

    /**
     * ANDT-TL-7 (AC-8 regression): the additive timeline API leaves the single-clip
     * [VideoPreview.play] path unchanged, and the single-active-attempt guard is shared —
     * a timeline is rejected while a clip plays, and a clip is rejected while a timeline
     * plays.
     */
    @Test
    fun singleClipPlay_isUnchanged_andSharesTheOneAttemptGuardWithTimeline() {
        // Single-clip play still completes exactly once on the main thread.
        val clipLog = PlaybackEventLog()
        probe.log = clipLog
        val clipListener = RecordingPlaybackListener(clipLog)
        assertTrue("single-clip play must still be accepted", preview.play(video.absolutePath, clipListener))
        // A timeline is rejected while a single-clip attempt is active (shared guard).
        assertFalse(
            "playTimeline must be rejected while a single clip is active",
            preview.playTimeline(listOf(fullSegment()), RecordingTimelineListener()),
        )
        assertTrue(clipLog.awaitTerminal(TERMINAL_TIMEOUT_MS))
        assertEquals("single-clip natural completion (AC-8)", TerminalKind.COMPLETED, clipLog.terminal?.kind)
        assertEquals("exactly one single-clip terminal", 1, clipLog.terminalDeliveryCount())
        assertTrue("single-clip terminal on main thread", clipLog.terminal?.onMainThread == true)

        // Conversely, a single-clip play is rejected while a timeline is active.
        val tlLog = PlaybackEventLog()
        probe.log = tlLog
        val tlListener = RecordingTimelineListener()
        assertTrue(preview.playTimeline(listOf(fullSegment()), tlListener))
        assertFalse(
            "play must be rejected while a timeline is active",
            preview.play(video.absolutePath, RecordingPlaybackListener(PlaybackEventLog())),
        )
        assertTrue(tlListener.await(TERMINAL_TIMEOUT_MS))
        assertTrue("timeline must complete: ${tlListener.error}", tlListener.completed)
    }

    // --- helpers --------------------------------------------------------------

    /** A whole-clip segment (native clamps the oversized end to the real duration). */
    private fun fullSegment() = VideoSegment(video.absolutePath, 0L, FULL_END_MS)

    private fun measureTimelineDurationMs(speed: Double): Long {
        val log = PlaybackEventLog()
        probe.log = log
        val listener = RecordingTimelineListener()
        val segment = VideoSegment(video.absolutePath, 0L, FULL_END_MS, speed = speed)
        val start = SystemClock.elapsedRealtime()
        assertTrue("timeline at speed $speed must be accepted", preview.playTimeline(listOf(segment), listener))
        assertTrue(
            "timeline at speed $speed must complete within ${TERMINAL_TIMEOUT_MS} ms",
            listener.await(TERMINAL_TIMEOUT_MS),
        )
        assertTrue("timeline at speed $speed must complete: ${listener.error}", listener.completed)
        return SystemClock.elapsedRealtime() - start
    }

    // Bounded drain used ONLY to assert the absence of a late callback after a terminal
    // has already been delivered; never the sole synchronization for a positive outcome.
    private fun settle(ms: Long) = SystemClock.sleep(ms)

    /** True if consecutive luma values ever drop by at least [drop] (a segment restart). */
    private fun hasLumaReset(luma: List<Int>, drop: Int): Boolean {
        for (i in 1 until luma.size) {
            if (luma[i - 1] - luma[i] >= drop) return true
        }
        return false
    }

    /** Records the single terminal outcome of an accepted [VideoPreview.playTimeline]. */
    private class RecordingTimelineListener : TimelineListener {
        private val latch = CountDownLatch(1)
        private val count = AtomicInteger(0)

        @Volatile
        var completed = false
            private set

        @Volatile
        var error: PlaybackError? = null
            private set

        @Volatile
        var segmentIndex: Int = UNSET_INDEX
            private set

        @Volatile
        var onMainThread = false
            private set

        override fun onTimelineCompleted() {
            if (count.getAndIncrement() == 0) {
                completed = true
                onMainThread = isMainThread()
            }
            latch.countDown()
        }

        override fun onTimelineError(error: PlaybackError, segmentIndex: Int) {
            if (count.getAndIncrement() == 0) {
                this.error = error
                this.segmentIndex = segmentIndex
                onMainThread = isMainThread()
            }
            latch.countDown()
        }

        fun await(timeoutMs: Long): Boolean = latch.await(timeoutMs, TimeUnit.MILLISECONDS)

        fun deliveryCount(): Int = count.get()

        private fun isMainThread(): Boolean = Looper.myLooper() == Looper.getMainLooper()

        private companion object {
            const val UNSET_INDEX = Int.MIN_VALUE
        }
    }

    private companion object {
        const val TERMINAL_TIMEOUT_MS = 30_000L
        const val MULTI_TERMINAL_TIMEOUT_MS = 60_000L
        const val QUIESCENCE_SETTLE_MS = 500L

        // A whole-clip end (ms) well beyond the ~3 s fixture; native clamps it to the real
        // playable duration, so this yields the untrimmed clip.
        const val FULL_END_MS = 100_000L
        const val SHORT_END_MS = 500L

        // Trim window [A, B) for ANDT-TL-2 and its expected centre-luma band. The fixture
        // ramps luma = round(frameIndex * 255 / 44) at 15 fps; frame(A=1000ms)~15 → ~87,
        // frame(just below B=2000ms)~29 → ~168.
        const val TRIM_A_MS = 1_000L
        const val TRIM_B_MS = 2_000L
        const val TRIM_LUMA_LOW = 87
        const val TRIM_LUMA_HIGH = 168

        // Segment-1 trim start for ANDT-TL-5 and the raw luma band it should present.
        const val ADVANCE_A_MS = 1_500L
        const val ADVANCE_LUMA_START = 133 // frame(~1500ms) ≈ 133
        const val ADVANCE_LUMA_END = 240 // raw ramp tail approaching 255

        // Constant-output filter centre luma (0.784 * 255 ≈ 200) and cluster thresholds.
        const val FILTER_LUMA = 200
        const val FILTER_CLUSTER_MIN = 6
        const val RAW_LOW_LUMA = 40

        const val LUMA_TOL = 25
        const val SEGMENT_RESET_DROP = 90

        const val MIN_SPEEDUP_DELTA_MS = 500L

        /**
         * A trusted GLES 3.0 filter whose centre output is a constant colour, independent of
         * the source ramp, so filtered frames are identifiable by their pinned luma.
         */
        val CONSTANT_FILTER = VideoFilter(
            source = """
                vec4 addFilter(vec4 color, vec2 uv) {
                    return vec4(0.784, 0.0, 0.0, 1.0);
                }
            """.trimIndent(),
        )

        private val context: Context
            get() = InstrumentationRegistry.getInstrumentation().targetContext

        private lateinit var video: java.io.File

        @JvmStatic
        @BeforeClass
        fun generateFixture() {
            video = TestVideoFixture.generateProgressiveVideo(context)
        }
    }
}
