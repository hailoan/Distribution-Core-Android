package com.cii.videolib

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * UT-SEG — JVM unit coverage for [VideoSegment] (AC-1; interval invariant).
 *
 * Pure value-type behavior only: construction/defaults, `data class` equality and
 * immutability, and the surface-independent trim rule [VideoSegment.hasValidInterval]
 * (`0 <= startMs < endMs`). No native library is loaded — [VideoSegment] and the
 * [VideoAppearance] it composes are ordinary Kotlin values, so this runs on the host
 * JVM (`:videolib:testDebugUnitTest`).
 *
 * The B-vs-real-duration bound is intentionally NOT covered here: it depends on the
 * decoded media and is enforced natively during playback (see ANDT-TL-2), not by this
 * value type.
 */
class VideoSegmentTest {

    @Test
    fun construction_retainsFields_andAppliesDefaults() {
        val segment = VideoSegment(path = PATH, startMs = 250L, endMs = 1_000L)

        assertEquals(PATH, segment.path)
        assertEquals(250L, segment.startMs)
        assertEquals(1_000L, segment.endMs)
        // Documented defaults: full-rate playback, neutral appearance.
        assertEquals(1.0, segment.speed, 0.0)
        assertEquals(VideoAppearance(), segment.appearance)
        assertNull("default appearance carries no filter", segment.appearance.filter)
    }

    @Test
    fun explicitSpeedAndAppearance_areRetained() {
        val appearance = VideoAppearance(adjustments = VideoAdjustments(brightness = 0.2f))
        val segment = VideoSegment(
            path = PATH,
            startMs = 0L,
            endMs = 500L,
            speed = 1.75,
            appearance = appearance,
        )

        assertEquals(1.75, segment.speed, 0.0)
        assertEquals(appearance, segment.appearance)
    }

    @Test
    fun valueEquality_holdsForIdenticalFields() {
        val a = VideoSegment(PATH, 100L, 900L, 1.25, APPEARANCE)
        val b = VideoSegment(PATH, 100L, 900L, 1.25, APPEARANCE)

        assertEquals(a, b)
        assertEquals("equal values share a hash code", a.hashCode(), b.hashCode())
    }

    @Test
    fun valueEquality_distinguishesEachField() {
        val base = VideoSegment(PATH, 100L, 900L, 1.0, APPEARANCE)

        assertNotEquals(base, base.copy(path = "other.mp4"))
        assertNotEquals(base, base.copy(startMs = 101L))
        assertNotEquals(base, base.copy(endMs = 901L))
        assertNotEquals(base, base.copy(speed = 1.5))
        assertNotEquals(base, base.copy(appearance = VideoAppearance()))
    }

    @Test
    fun copy_producesIndependentValue_leavingOriginalUnchanged() {
        val original = VideoSegment(PATH, 0L, 1_000L)
        val trimmed = original.copy(startMs = 200L, endMs = 800L)

        // The original value is unaffected by deriving a copy (immutability).
        assertEquals(0L, original.startMs)
        assertEquals(1_000L, original.endMs)
        assertEquals(200L, trimmed.startMs)
        assertEquals(800L, trimmed.endMs)
        assertEquals("unchanged fields are carried over", original.path, trimmed.path)
    }

    @Test
    fun hasValidInterval_trueOnlyWhenStartNonNegativeAndBelowEnd() {
        assertTrue("0 <= A < B is well-formed", VideoSegment(PATH, 0L, 1L).hasValidInterval)
        assertTrue(VideoSegment(PATH, 250L, 1_000L).hasValidInterval)
    }

    @Test
    fun hasValidInterval_falseWhenEndNotAfterStart() {
        assertFalse("B == A is empty", VideoSegment(PATH, 500L, 500L).hasValidInterval)
        assertFalse("B < A is inverted", VideoSegment(PATH, 500L, 499L).hasValidInterval)
    }

    @Test
    fun hasValidInterval_falseWhenStartNegative() {
        assertFalse("negative A is invalid", VideoSegment(PATH, -1L, 1_000L).hasValidInterval)
        assertFalse(
            "negative A stays invalid even below a positive B",
            VideoSegment(PATH, -100L, -1L).hasValidInterval,
        )
    }

    private companion object {
        const val PATH = "/cache/clip.mp4"
        val APPEARANCE = VideoAppearance(adjustments = VideoAdjustments(contrast = 1.1f))
    }
}
