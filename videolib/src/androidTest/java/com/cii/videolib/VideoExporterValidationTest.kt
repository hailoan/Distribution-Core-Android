package com.cii.videolib

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

/**
 * T-VAL — synchronous input-validation coverage for [VideoExporter] (AC-7).
 *
 * NOTE ON LEVEL: this was planned as a host-JVM unit test, but [VideoExporter]
 * calls `System.loadLibrary("videolib")` in its companion initializer and
 * `nativeCreate()` + `Handler(Looper.getMainLooper())` in its constructor, so it
 * cannot be instantiated on the host JVM. It is therefore an instrumented test,
 * exactly like the equivalent [VideoPreview] validation coverage. Reclassified by
 * testing with that evidence (UNIT-TEST-REPORT §4).
 *
 * These cases assert the pure, pre-native rejection contract only: an invalid
 * request must return `false` from `export`/`exportTimeline` WITHOUT starting an
 * attempt and WITHOUT ever notifying the [ExportListener]. They do not run a real
 * export, so they need no encoder output and no output-file assertions — the
 * end-to-end pipeline is covered by [VideoExportInstrumentedTest].
 */
@RunWith(AndroidJUnit4::class)
class VideoExporterValidationTest {

    private lateinit var exporter: VideoExporter
    private lateinit var outputPath: String

    @Before
    fun setUp() {
        exporter = VideoExporter()
        outputPath = File(cacheRoot(), "validation_out_${System.nanoTime()}.mp4").absolutePath
    }

    @After
    fun tearDown() {
        exporter.release()
    }

    /** A listener that fails the test if it is ever called for a rejected request. */
    private class FailIfCalledListener : ExportListener {
        @Volatile
        var called = false
            private set

        override fun onExportCompleted() {
            called = true
        }

        override fun onExportError(error: ExportError, segmentIndex: Int) {
            called = true
        }
    }

    @Test
    fun export_rejectsBlankSourcePath_withoutNotifying() {
        val listener = FailIfCalledListener()
        assertFalse(
            "a blank source path must be rejected synchronously",
            exporter.export(path = "", outputPath = outputPath, listener = listener),
        )
        assertFalse("a rejected request must not notify the listener", listener.called)
    }

    @Test
    fun export_rejectsBlankOutputPath_withoutNotifying() {
        val listener = FailIfCalledListener()
        assertFalse(
            "a blank output path must be rejected synchronously",
            exporter.export(path = "/cache/in.mp4", outputPath = "", listener = listener),
        )
        assertFalse(listener.called)
    }

    @Test
    fun export_rejectsSubMinimumSpeed_withoutNotifying() {
        val listener = FailIfCalledListener()
        assertFalse(
            "a speed below the 0.1 floor must be rejected",
            exporter.export(
                path = "/cache/in.mp4",
                outputPath = outputPath,
                listener = listener,
                speed = 0.05,
            ),
        )
        assertFalse(listener.called)
    }

    @Test
    fun export_rejectsNonFiniteSpeed_withoutNotifying() {
        val listener = FailIfCalledListener()
        assertFalse(
            exporter.export(
                path = "/cache/in.mp4",
                outputPath = outputPath,
                listener = listener,
                speed = Double.NaN,
            ),
        )
        assertFalse(listener.called)
    }

    @Test
    fun export_rejectsAppearanceOutOfRange_withoutNotifying() {
        val listener = FailIfCalledListener()
        // brightness range is -0.5..0.5; 0.9 is out of range and must be rejected
        // by the same rule VideoPreview.setAppearance enforces.
        val badAppearance = VideoAppearance(adjustments = VideoAdjustments(brightness = 0.9f))
        assertFalse(
            exporter.export(
                path = "/cache/in.mp4",
                outputPath = outputPath,
                listener = listener,
                appearance = badAppearance,
            ),
        )
        assertFalse(listener.called)
    }

    @Test
    fun export_rejectsInvertedLevels_withoutNotifying() {
        val listener = FailIfCalledListener()
        val badLevels = VideoAppearance(
            adjustments = VideoAdjustments(
                levels = VideoLevels(minimumInput = 0.9f, gamma = 1f, maximumInput = 0.6f),
            ),
        )
        assertFalse(
            exporter.export(
                path = "/cache/in.mp4",
                outputPath = outputPath,
                listener = listener,
                appearance = badLevels,
            ),
        )
        assertFalse(listener.called)
    }

    @Test
    fun export_rejectsMalformedFilterSource_withoutNotifying() {
        val listener = FailIfCalledListener()
        // Missing the required `addFilter` entry point → rejected pre-native.
        val badFilter = VideoAppearance(
            filter = VideoFilter(source = "vec4 notTheEntryPoint(vec4 c){ return c; }"),
        )
        assertFalse(
            exporter.export(
                path = "/cache/in.mp4",
                outputPath = outputPath,
                listener = listener,
                appearance = badFilter,
            ),
        )
        assertFalse(listener.called)
    }

    @Test
    fun exportTimeline_rejectsEmptyList_withoutNotifying() {
        val listener = FailIfCalledListener()
        assertFalse(
            "an empty timeline must be rejected",
            exporter.exportTimeline(emptyList(), outputPath, listener),
        )
        assertFalse(listener.called)
    }

    @Test
    fun exportTimeline_rejectsMalformedInterval_withoutNotifying() {
        val listener = FailIfCalledListener()
        // endMs <= startMs is a structurally invalid trim window.
        val bad = VideoSegment(path = "/cache/in.mp4", startMs = 500L, endMs = 500L)
        assertFalse(
            exporter.exportTimeline(listOf(bad), outputPath, listener),
        )
        assertFalse(listener.called)
    }

    @Test
    fun operationsOnReleasedExporter_areRejected() {
        val listener = FailIfCalledListener()
        exporter.release()
        assertFalse(
            "a released exporter must reject new work",
            exporter.export(path = "/cache/in.mp4", outputPath = outputPath, listener = listener),
        )
        assertFalse(listener.called)
        // release() is idempotent and cancel() is a no-op after release.
        exporter.release()
        exporter.cancel()
        assertEquals(false, listener.called)
    }

    private fun cacheRoot(): File =
        File(context.cacheDir, "videolib_test_fixtures").apply { mkdirs() }

    private val context: Context
        get() = InstrumentationRegistry.getInstrumentation().targetContext
}
