package com.cii.videolib

/**
 * Trusted GLES 3.0 effect snippet, applied to the frame *before* the
 * [VideoFilter] colour pass. Version 1 source must define
 * `vec4 addEffect(vec2 uv)`.
 *
 * An effect differs from a filter in what it is handed. A filter receives an
 * already-sampled colour and transforms it; an effect receives only a
 * coordinate and samples the frame itself through the provided
 * `vec4 getColor(vec2 uv)` accessor. That is what lets an effect displace
 * coordinates — RGB channel splits, scanlines, jitter — which a filter cannot
 * express. Reads outside `0..1` return opaque black rather than the clamped
 * edge texel.
 *
 * The snippet's `u_time` is supplied by the engine: it is the frame's own
 * presentation time in seconds, measured from the start of the segment, so the
 * effect animates with playback. Because it comes from media time rather than
 * a wall clock, the same frame always renders identically — a paused preview
 * redraws stably, seeking is repeatable, and an exported video matches the
 * preview it came from.
 *
 * [speed] scales that clock (`u_time = frameTime * speed`), so it controls how
 * fast the effect animates relative to the video: below 1 slower, above 1
 * faster, and 0 freezes it on its opening pose. It is a plain uniform, so
 * changing it never rebuilds the shader and it is safe to drive from a slider.
 * Must be finite and non-negative.
 *
 * The same reserved substrings as [VideoFilter] are forbidden (`#version`,
 * `void main`, `u_texture`, `v_texCoord`, `fragColor`) — sample via `getColor`
 * instead of touching the frame sampler directly. An effect declares no
 * auxiliary textures.
 */
class VideoEffect(
    val version: Int = VERSION_1,
    val source: String,
    val opacity: Float = 1f,
    val speed: Float = 1f,
) {
    override fun equals(other: Any?): Boolean =
        other is VideoEffect &&
            version == other.version &&
            source == other.source &&
            opacity == other.opacity &&
            speed == other.speed

    override fun hashCode(): Int {
        var result = version
        result = 31 * result + source.hashCode()
        result = 31 * result + opacity.hashCode()
        result = 31 * result + speed.hashCode()
        return result
    }

    companion object {
        const val VERSION_1 = 1
    }
}
