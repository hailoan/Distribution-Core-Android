package com.cii.videolib

/**
 * Stable error categories reported by [VideoExporter] export.
 *
 * Kept separate from [PlaybackError] so that adding export-only failure modes
 * never changes the playback taxonomy — extending the shared playback enum would
 * break existing consumers' exhaustive `when` expressions. The first four
 * categories mirror the playback ones for a familiar vocabulary; the last three
 * are specific to the encode/mux/output stages of export.
 */
enum class ExportError {
    /** The supplied local source file could not be opened or read. */
    INPUT_OPEN,

    /** The source has no video stream supported by the bundled FFmpeg build. */
    UNSUPPORTED_VIDEO,

    /** Video decoding, timing, or pixel conversion failed. */
    DECODE,

    /** The offscreen EGL/OpenGL ES effect pass failed. */
    RENDER,

    /** The H.264 (MediaCodec) encode step failed. */
    ENCODE,

    /** Writing or finalizing the MP4 container failed. */
    MUX,

    /** The output file could not be created or written. */
    OUTPUT,
}
