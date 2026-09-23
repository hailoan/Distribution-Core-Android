package com.cii.videolib

/**
 * Complete, immutable appearance state retained by a [VideoPreview].
 *
 * Passes run in a fixed order: [effect] (coordinate-sampling, may displace),
 * then [filter] (colour transform), then [adjustments] (scalar corrections).
 */
data class VideoAppearance(
    val adjustments: VideoAdjustments = VideoAdjustments(),
    val filter: VideoFilter? = null,
    val effect: VideoEffect? = null,
)

/** Input range and gamma used by the levels adjustment. */
data class VideoLevels(
    val minimumInput: Float = 0f,
    val gamma: Float = 1f,
    val maximumInput: Float = 1f,
)

/**
 * Immutable preview adjustment values. Inputs are finite and never clamped.
 * Ranges are: brightness/temperature `-0.5..0.5`; contrast/saturation/lights
 * `0..2`; exposure/hue/vibrance/shadows/clarity `-1..1`; darks `0.5..1.5`;
 * highlights `-2..2`; and vignette `0..1`. [levels] requires minimum input
 * `-1..1`, gamma `0.5..1.5`, maximum input `0.5..1.5`, and minimum < maximum.
 */
data class VideoAdjustments(
    val brightness: Float = 0f,
    val contrast: Float = 1f,
    val saturation: Float = 1f,
    val exposure: Float = 0f,
    val darks: Float = 1f,
    val levels: VideoLevels = VideoLevels(),
    val vignette: Float = 0f,
    val vibrance: Float = 0f,
    val temperature: Float = 0f,
    val hue: Float = 0f,
    val highlights: Float = -2f,
    val shadows: Float = 0f,
    val lights: Float = 1f,
    val clarity: Float = 0f,
)
