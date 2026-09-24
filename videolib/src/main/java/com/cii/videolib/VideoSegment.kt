package com.cii.videolib

/**
 * Immutable descriptor for one segment of a sequential timeline.
 *
 * A segment presents the half-open interval `[startMs, endMs)` of the video at
 * [path] on the shared preview surface, applying its own [appearance] and
 * playing at [speed]. Segments are previewed back-to-back in list order by
 * [VideoPreview.playTimeline].
 *
 * Field values are retained verbatim and never clamped here. Structural
 * validity of the trim interval is exposed by [hasValidInterval]; overall
 * acceptance — including [speed] and the media-duration bound on [endMs] — is
 * enforced by [VideoPreview.playTimeline].
 */
data class VideoSegment(
    val path: String,
    val startMs: Long,
    val endMs: Long,
    val speed: Double = 1.0,
    val appearance: VideoAppearance = VideoAppearance(),
) {
    /**
     * True when the trim interval is well-formed: `0 <= startMs < endMs`. This
     * is the pure, surface-independent half of playback acceptance; the upper
     * bound of [endMs] against the real media duration is checked natively
     * during playback.
     */
    val hasValidInterval: Boolean
        get() = startMs >= 0 && endMs > startMs
}
