package com.cii.videolib

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

/** Runtime coverage for the two version-1 GLSL component-provider modes. */
@RunWith(AndroidJUnit4::class)
class VideoFilterComponentsInstrumentedTest {

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
        preview.release()
        probe.close()
    }

    @Test
    fun addFilterOnlySource_usesLibraryComponents_andPresentsFrame() {
        assertSame(AppearanceUpdateResult.Accepted, preview.setFilter(VideoFilter(source = SHORT_SOURCE)))

        assertFramePresented()
    }

    @Test
    fun consumerOwnedSource_avoidsDuplicateLibraryComponents_andPresentsFrame() {
        assertSame(AppearanceUpdateResult.Accepted, preview.setFilter(VideoFilter(source = LEGACY_SOURCE)))

        assertFramePresented()
    }

    @Test
    fun invalidCandidate_isRejected_andPreviousGenerationKeepsRendering() {
        assertSame(AppearanceUpdateResult.Accepted, preview.setFilter(VideoFilter(source = SHORT_SOURCE)))
        assertFramePresented()

        val rejected = preview.setFilter(VideoFilter(source = INVALID_RUNTIME_SOURCE))
        assertTrue(rejected is AppearanceUpdateResult.Rejected)
        assertEquals(
            AppearanceRejectionReason.SHADER_COMPILATION,
            (rejected as AppearanceUpdateResult.Rejected).reason,
        )

        assertFramePresented()
        assertSame(
            AppearanceUpdateResult.Accepted,
            preview.setFilter(VideoFilter(source = "$SHORT_SOURCE\n")),
        )
        assertFramePresented()
    }

    private fun assertFramePresented() {
        val log = PlaybackEventLog()
        probe.log = log
        preview.requestPattern()
        assertTrue("expected the active filter generation to present a frame", log.awaitFirstFrame(10_000L))
    }

    private companion object {
        val SHORT_SOURCE = """
            vec4 addFilter(vec4 color, vec2 uv) {
                color = exposureAdjust(color, 0.18);
                color = contrastAdjust(color, 0.755);
                color = shadowAdjust(color, 0.29);
                color = saturationAdjust(color, 0.84);
                color.rgb = vibranceAdjust(color.rgb, 0.24);
                color = hueAdjust(color, -0.15);
                color = temperatureAdjust(color, 0.25);

                vec3 hsl = RGBtoHSL(color.rgb);
                current_hue = hsl.x;
                hsl = mixColorRed(hsl, 1.46, 1.38, 1.0);
                hsl = mixColorOrange(hsl, 1.2, 0.79, 1.11);
                hsl = mixColorYellow(hsl, 1.0, 0.1, 1.12);
                hsl = mixColorGreen(hsl, 1.0, 0.09, 0.89);
                hsl = mixColorCyan(hsl, 1.29, 1.21, 1.0);
                hsl = mixColorBlue(hsl, 0.42, 1.26, 1.0);
                color.rgb = HSLtoRGB(hsl);
                return color;
            }
        """.trimIndent()

        val LEGACY_SOURCE = """
            float cubicPulse(float center, float width, float value) {
                return smoothstep(center - width, center, value) -
                    smoothstep(center, center + width, value);
            }

            vec4 addFilter(vec4 color, vec2 uv) {
                float pulse = cubicPulse(0.5, 0.5, uv.x);
                return vec4(mix(color.rgb, color.bgr, pulse), color.a);
            }
        """.trimIndent()

        const val INVALID_RUNTIME_SOURCE =
            "vec4 addFilter(vec4 color, vec2 uv) { return missingFilterFunction(color); }"
    }
}
