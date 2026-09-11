package com.cii.videolib

/** Receives the single terminal outcome of an accepted [VideoPreview.playTimeline] request. */
interface TimelineListener {
    /** Called once after the final segment's last frame has been presented. */
    fun onTimelineCompleted()

    /**
     * Called once when the timeline cannot continue. [segmentIndex] is the
     * 0-based index of the failing segment, or `-1` when no single segment is
     * implicated (for example, the surface was lost).
     */
    fun onTimelineError(error: PlaybackError, segmentIndex: Int)
}
