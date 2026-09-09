package com.cii.videolib

/** Stable error categories reported by [VideoPreview] playback. */
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
