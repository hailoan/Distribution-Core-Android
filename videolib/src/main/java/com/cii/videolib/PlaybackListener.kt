package com.cii.videolib

/** Receives the terminal outcome of an accepted [VideoPreview.play] request. */
interface PlaybackListener {
    /** Called once after the final decoded frame has been presented. */
    fun onPlaybackCompleted()

    /** Called once when accepted playback cannot continue. */
    fun onPlaybackError(error: PlaybackError)
}
