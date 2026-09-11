package com.cii.videolib

/** Receives the single terminal outcome of an accepted [VideoExporter] request. */
interface ExportListener {
    /** Called once after the export has finished and the output file is complete. */
    fun onExportCompleted()

    /**
     * Called once when the export cannot complete. [segmentIndex] is the 0-based
     * index of the failing segment for a timeline export, or `-1` when no single
     * segment is implicated (single-clip export, or a whole-request failure such
     * as output-file creation).
     */
    fun onExportError(error: ExportError, segmentIndex: Int)
}
