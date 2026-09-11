package com.cii.videolib

/**
 * Stable error categories reported by [VideoPreview] playback.
 *
 * Export failures use the separate [ExportError] taxonomy so this playback enum
 * stays source- and binary-compatible for existing consumers (a shared enum
 * extended with export-only values would break their exhaustive `when`).
 */
enum class PlaybackError {
    /** The supplied local file could not be opened or read. */
    INPUT_OPEN,

    /** The file has no video stream supported by the bundled FFmpeg build. */
    UNSUPPORTED_VIDEO,

    /** Video decoding, timing, or pixel conversion failed. */
    DECODE,

    /** The attached surface or its EGL/OpenGL ES presentation failed. */
    RENDER,
}
