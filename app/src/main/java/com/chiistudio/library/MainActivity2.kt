package com.chiistudio.library

import android.content.ActivityNotFoundException
import android.content.Intent
import android.media.MediaMetadataRetriever
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.LayoutInflater
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.FileProvider
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.cii.videolib.ExportError
import com.cii.videolib.ExportListener
import com.cii.videolib.PlaybackError
import com.cii.videolib.TimelineListener
import com.cii.videolib.VideoAppearance
import com.cii.videolib.VideoExporter
import com.cii.videolib.VideoFilter
import com.cii.videolib.VideoPreview
import com.cii.videolib.VideoSegment
import com.google.android.material.switchmaterial.SwitchMaterial
import java.io.File
import java.io.IOException
import java.util.Locale
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

/**
 * Demo host for the sequential video timeline. The user adds several local
 * videos, trims each to an `[A, B)` window with its own speed and filter, and
 * previews them back-to-back on one surface via [VideoPreview.playTimeline].
 *
 * This is demonstration wiring: the host owns picking UX, cache files, and
 * lifecycle; playback timing/sequencing is owned natively. Cross-segment
 * progress shown here is a wall-clock estimate, as the library reports only one
 * terminal outcome for the whole timeline.
 */
class MainActivity2 : AppCompatActivity(), SurfaceHolder.Callback {

    /** Mutable per-clip UI model. A/B are in ms; [durationMs] bounds both. */
    private class Clip(
        val cachedFile: File,
        val displayName: String,
        val durationMs: Long,
    ) {
        var startMs: Long = 0L
        var endMs: Long = durationMs
        var speed: Double = 1.0
        var filterEnabled: Boolean = true

        /** Effective playable span at the current trim and speed, in ms. */
        val effectiveWallMs: Long
            get() = (((endMs - startMs).coerceAtLeast(0L)) / speed).toLong()
    }

    private val mainHandler = Handler(Looper.getMainLooper())
    private val fileExecutor: ExecutorService = Executors.newSingleThreadExecutor()
    private val videoPreview = VideoPreview()
    private val videoExporter = VideoExporter()

    private lateinit var surfaceView: SurfaceView
    private lateinit var statusView: TextView
    private lateinit var progressView: SeekBar
    private lateinit var timeView: TextView
    private lateinit var playPauseButton: Button
    private lateinit var loopSwitch: SwitchMaterial
    private lateinit var exportButton: Button
    private lateinit var exportAudioSwitch: SwitchMaterial
    private lateinit var clipListHeader: TextView
    private lateinit var clipListContainer: LinearLayout

    private val clips = mutableListOf<Clip>()

    private var pendingCopies = 0
    private var copyGeneration = 0
    private var surfaceAttached = false
    private var timelinePending = false
    private var timelineActive = false
    private var timelinePaused = false
    private var activityStarted = false
    private var pickerOpen = false
    private var userSeeking = false
    private var positionBeforeSeekMs = 0L
    private var timelineDurationMs = 0L
    private var timelinePositionMs = 0L
    private var progressAnchorElapsedMs = 0L

    // Export state. The exporter reports one terminal outcome for the whole
    // timeline; the shown percentage is a wall-clock estimate over the same
    // effective duration used for the preview progress bar.
    private var exportActive = false
    private var exportOutput: File? = null
    private var exportAnchorElapsedMs = 0L

    private val progressUpdate = object : Runnable {
        override fun run() {
            if (timelineActive && !timelinePaused && !userSeeking) {
                val now = SystemClock.elapsedRealtime()
                val elapsed = (now - progressAnchorElapsedMs).coerceAtLeast(0L)
                progressAnchorElapsedMs = now
                timelinePositionMs = (timelinePositionMs + elapsed).coerceAtMost(timelineDurationMs)
                showPlaybackPosition(timelinePositionMs)
                showActiveClipStatus(timelinePositionMs)
            }
            if (timelineActive) {
                mainHandler.postDelayed(this, PROGRESS_UPDATE_INTERVAL_MS)
            }
        }
    }

    // Wall-clock export progress estimate. The library reports one terminal
    // outcome, not per-frame progress, so this is only an indicative percentage
    // capped below 100% until the real completion callback arrives.
    private val exportProgressUpdate = object : Runnable {
        override fun run() {
            if (!exportActive) return
            if (timelineDurationMs > 0L) {
                val elapsed = (SystemClock.elapsedRealtime() - exportAnchorElapsedMs)
                    .coerceAtLeast(0L)
                val percent = (elapsed * 100L / timelineDurationMs).coerceIn(0L, 99L)
                statusView.text = getString(R.string.video_status_exporting, percent.toInt())
            } else {
                showStatus(R.string.video_status_exporting_indeterminate)
            }
            mainHandler.postDelayed(this, PROGRESS_UPDATE_INTERVAL_MS)
        }
    }

    private val pickVideos =
        registerForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { uris ->
            pickerOpen = false
            if (uris.isNullOrEmpty()) {
                tryStartPlayback()
            } else {
                prepareSelectedVideos(uris)
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_main)
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { view, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            view.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        surfaceView = findViewById(R.id.video_surface)
        statusView = findViewById(R.id.video_status)
        progressView = findViewById(R.id.video_progress)
        timeView = findViewById(R.id.video_time)
        playPauseButton = findViewById(R.id.play_pause_button)
        loopSwitch = findViewById(R.id.loop_switch)
        exportButton = findViewById(R.id.export_button)
        exportAudioSwitch = findViewById(R.id.export_audio_switch)
        clipListHeader = findViewById(R.id.clip_list_header)
        clipListContainer = findViewById(R.id.clip_list)
        surfaceView.holder.addCallback(this)
        configurePlaybackControls()
        exportButton.setOnClickListener { onExportClicked() }
        findViewById<Button>(R.id.pick_video_button).setOnClickListener {
            pickerOpen = true
            pickVideos.launch(arrayOf("video/*"))
        }
    }

    override fun onStart() {
        super.onStart()
        activityStarted = true
        if (!pickerOpen) {
            tryStartPlayback()
        }
    }

    override fun onStop() {
        activityStarted = false
        if (timelineActive) {
            stopTimelinePlayback(resetToStart = true)
            timelinePending = true
            showStatus(R.string.video_status_ready)
        }
        super.onStop()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        surfaceAttached = videoPreview.attachSurface(holder.surface)
        if (surfaceAttached) {
            if (clips.isEmpty()) {
                showStatus(R.string.video_status_no_selection)
            }
            tryStartPlayback()
        } else {
            showStatus(R.string.video_status_surface_error)
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) = Unit

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        if (timelineActive) {
            stopTimelinePlayback(resetToStart = true)
            timelinePending = true
        }
        if (surfaceAttached) {
            videoPreview.detachSurface()
            surfaceAttached = false
        }
        if (clips.isNotEmpty() && timelinePending) {
            showStatus(R.string.video_status_waiting_for_surface)
        }
    }

    override fun onDestroy() {
        surfaceView.holder.removeCallback(this)
        copyGeneration += 1
        fileExecutor.shutdownNow()
        stopProgressUpdates()
        stopExportProgressUpdates()
        videoExporter.release()
        exportActive = false
        videoPreview.stop()
        if (surfaceAttached) {
            videoPreview.detachSurface()
            surfaceAttached = false
        }
        videoPreview.release()
        clips.forEach { it.cachedFile.delete() }
        clips.clear()
        // Best-effort cleanup of exported demo files.
        File(cacheDir, EXPORT_CACHE_DIRECTORY).listFiles()?.forEach { it.delete() }
        super.onDestroy()
    }

    private fun prepareSelectedVideos(uris: List<Uri>) {
        // Invalidate any in-flight copies and drop the previous timeline.
        copyGeneration += 1
        val generation = copyGeneration
        videoPreview.stop()
        timelineActive = false
        timelinePaused = false
        timelinePending = false
        timelinePositionMs = 0L
        clips.forEach { it.cachedFile.delete() }
        clips.clear()
        renderClipList()
        updatePlaybackControlsEnabled(false)
        showPlaybackPosition(0L)
        showStatus(R.string.video_status_copying)

        pendingCopies = uris.size
        uris.forEachIndexed { index, uri ->
            fileExecutor.execute { copyOneVideo(uri, index, generation) }
        }
    }

    private fun copyOneVideo(uri: Uri, index: Int, generation: Int) {
        var destination: File? = null
        try {
            val previewCache = File(cacheDir, VIDEO_CACHE_DIRECTORY)
            if (!previewCache.exists() && !previewCache.mkdirs()) {
                throw IOException("Unable to create preview cache directory")
            }
            val copiedVideo = File.createTempFile(
                VIDEO_CACHE_PREFIX,
                VIDEO_CACHE_SUFFIX,
                previewCache,
            )
            destination = copiedVideo
            contentResolver.openInputStream(uri).use { input ->
                if (input == null) throw IOException("Unable to open selected video")
                copiedVideo.outputStream().use { output ->
                    val buffer = ByteArray(COPY_BUFFER_SIZE)
                    while (true) {
                        if (Thread.currentThread().isInterrupted) throw InterruptedException()
                        val count = input.read(buffer)
                        if (count < 0) break
                        output.write(buffer, 0, count)
                    }
                }
            }
            val durationMs = runCatching { readVideoDuration(copiedVideo) }.getOrDefault(0L)
            val displayName = displayName(uri, index)

            mainHandler.post {
                if (isDestroyed || generation != copyGeneration) {
                    copiedVideo.delete()
                    return@post
                }
                if (durationMs > 0L) {
                    clips += Clip(copiedVideo, displayName, durationMs)
                } else {
                    copiedVideo.delete()
                }
                onCopyFinished(generation, failed = durationMs <= 0L)
            }
        } catch (_: InterruptedException) {
            destination?.delete()
        } catch (_: Exception) {
            destination?.delete()
            mainHandler.post {
                if (!isDestroyed && generation == copyGeneration) {
                    onCopyFinished(generation, failed = true)
                }
            }
        }
    }

    private fun onCopyFinished(generation: Int, failed: Boolean) {
        if (generation != copyGeneration) return
        if (failed) {
            showStatus(R.string.video_status_copy_error)
        }
        pendingCopies = (pendingCopies - 1).coerceAtLeast(0)
        if (pendingCopies > 0) return

        renderClipList()
        if (clips.isEmpty()) {
            updatePlaybackControlsEnabled(false)
            if (!failed) showStatus(R.string.video_status_no_selection)
            return
        }
        recomputeTimelineDuration()
        updatePlaybackControlsEnabled(true)
        timelinePositionMs = 0L
        showPlaybackPosition(0L)
        timelinePending = true
        if (surfaceAttached) {
            showStatus(R.string.video_status_ready)
        } else {
            showStatus(R.string.video_status_waiting_for_surface)
        }
        tryStartPlayback()
    }

    private fun tryStartPlayback() {
        if (clips.isEmpty()) return
        if (!timelinePending || timelineActive || !surfaceAttached ||
            !activityStarted || pickerOpen
        ) {
            return
        }

        val generation = copyGeneration
        videoPreview.setLooping(loopSwitch.isChecked)
        val segments = clips.map { clip ->
            VideoSegment(
                path = clip.cachedFile.absolutePath,
                startMs = clip.startMs,
                endMs = clip.endMs,
                speed = clip.speed,
                appearance = if (clip.filterEnabled) FILTERED_APPEARANCE else VideoAppearance(),
            )
        }
        val accepted = videoPreview.playTimeline(
            segments = segments,
            listener = object : TimelineListener {
                override fun onTimelineCompleted() {
                    if (generation != copyGeneration || isDestroyed) return
                    stopProgressUpdates()
                    timelineActive = false
                    timelinePaused = false
                    timelinePositionMs = timelineDurationMs
                    showPlaybackPosition(timelinePositionMs)
                    updatePlayPauseButton()
                    showStatus(R.string.video_status_completed)
                }

                override fun onTimelineError(error: PlaybackError, segmentIndex: Int) {
                    if (generation != copyGeneration || isDestroyed) return
                    stopProgressUpdates()
                    timelineActive = false
                    timelinePaused = false
                    updatePlayPauseButton()
                    if (error == PlaybackError.RENDER) {
                        videoPreview.detachSurface()
                        surfaceAttached = false
                        if (surfaceView.holder.surface.isValid) {
                            surfaceAttached =
                                videoPreview.attachSurface(surfaceView.holder.surface)
                        }
                    }
                    showSegmentError(error, segmentIndex)
                }
            },
        )
        if (accepted) {
            timelinePending = false
            timelineActive = true
            timelinePaused = false
            timelinePositionMs = 0L
            progressAnchorElapsedMs = SystemClock.elapsedRealtime()
            updatePlayPauseButton()
            updateExportControls()
            showPlaybackPosition(timelinePositionMs)
            startProgressUpdates()
            showActiveClipStatus(0L)
        } else {
            showStatus(R.string.video_status_play_error)
        }
    }

    private fun stopTimelinePlayback(resetToStart: Boolean) {
        stopProgressUpdates()
        videoPreview.stop()
        timelineActive = false
        timelinePaused = false
        if (resetToStart) {
            timelinePositionMs = 0L
            showPlaybackPosition(timelinePositionMs)
        }
        updatePlayPauseButton()
        updateExportControls()
    }

    private fun showSegmentError(error: PlaybackError, segmentIndex: Int) {
        if (segmentIndex < 0) {
            showStatus(error.statusMessage)
        } else {
            statusView.text = getString(
                R.string.video_status_segment_error,
                segmentIndex + 1,
                getString(error.statusMessage),
            )
        }
    }

    private val PlaybackError.statusMessage: Int
        get() = when (this) {
            PlaybackError.INPUT_OPEN -> R.string.video_status_input_error
            PlaybackError.UNSUPPORTED_VIDEO -> R.string.video_status_unsupported
            PlaybackError.DECODE -> R.string.video_status_decode_error
            PlaybackError.RENDER -> R.string.video_status_render_error
        }

    // --- Export (VideoExporter demo) -----------------------------------------

    /**
     * Exports the current timeline — the same clips, trims, per-clip speed, and
     * filter the user built for preview — to an MP4 via [VideoExporter]. Export
     * is headless, so it runs without the preview surface; the preview is stopped
     * first only to keep the demo's single status line unambiguous.
     */
    private fun onExportClicked() {
        if (exportActive) {
            // cancel() suppresses the terminal callback, so reset the UI here and
            // remove the partial output file.
            videoExporter.cancel()
            exportOutput?.delete()
            finishExport()
            showStatus(R.string.video_status_export_cancelled)
            return
        }
        if (clips.isEmpty()) return

        val exportsDir = File(cacheDir, EXPORT_CACHE_DIRECTORY)
        if (!exportsDir.exists() && !exportsDir.mkdirs()) {
            showStatus(R.string.video_status_export_start_error)
            return
        }
        val output = File(exportsDir, "export_${System.currentTimeMillis()}.mp4")

        // The same segment mapping used for preview (tryStartPlayback), so the
        // exported file reflects exactly what was previewed.
        val segments = clips.map { clip ->
            VideoSegment(
                path = clip.cachedFile.absolutePath,
                startMs = clip.startMs,
                endMs = clip.endMs,
                speed = clip.speed,
                appearance = if (clip.filterEnabled) FILTERED_APPEARANCE else VideoAppearance(),
            )
        }

        // Stop any active preview before exporting so the status line is clear.
        if (timelineActive) {
            stopTimelinePlayback(resetToStart = true)
            timelinePending = true
        }

        val generation = copyGeneration
        val accepted = videoExporter.exportTimeline(
            segments = segments,
            outputPath = output.absolutePath,
            includeAudio = exportAudioSwitch.isChecked,
            listener = object : ExportListener {
                override fun onExportCompleted() {
                    if (generation != copyGeneration || isDestroyed) return
                    finishExport()
                    statusView.text =
                        getString(R.string.video_status_export_completed, output.name)
                    offerToOpen(output)
                }

                override fun onExportError(error: ExportError, segmentIndex: Int) {
                    if (generation != copyGeneration || isDestroyed) return
                    finishExport()
                    output.delete()
                    val reason = getString(error.statusMessage)
                    statusView.text = if (segmentIndex < 0) {
                        getString(R.string.video_status_export_error, reason)
                    } else {
                        getString(
                            R.string.video_status_export_segment_error,
                            segmentIndex + 1,
                            reason,
                        )
                    }
                }
            },
        )
        if (accepted) {
            exportActive = true
            exportOutput = output
            exportAnchorElapsedMs = SystemClock.elapsedRealtime()
            updateExportControls()
            startExportProgressUpdates()
        } else {
            showStatus(R.string.video_status_export_start_error)
        }
    }

    private fun finishExport() {
        stopExportProgressUpdates()
        exportActive = false
        exportOutput = null
        updateExportControls()
    }

    private fun offerToOpen(file: File) {
        val uri: Uri = FileProvider.getUriForFile(this, "$packageName.fileprovider", file)
        val view = Intent(Intent.ACTION_VIEW).apply {
            setDataAndType(uri, "video/mp4")
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        val chooser = Intent.createChooser(view, getString(R.string.video_export_open_chooser))
        try {
            startActivity(chooser)
        } catch (_: ActivityNotFoundException) {
            Toast.makeText(this, R.string.video_export_open_error, Toast.LENGTH_SHORT).show()
        }
    }

    private fun updateExportControls() {
        // Export is headless and independent of preview: allow it whenever there
        // are clips (onExportClicked stops any active preview first). While an
        // export runs, the button becomes a Cancel action.
        exportButton.isEnabled = exportActive || clips.isNotEmpty()
        exportButton.setText(
            if (exportActive) R.string.video_export_cancel else R.string.video_export,
        )
        exportAudioSwitch.isEnabled = !exportActive
        // Play/pause is disabled while exporting to keep the single status line
        // unambiguous.
        playPauseButton.isEnabled = !exportActive && clips.isNotEmpty()
    }

    private fun startExportProgressUpdates() {
        mainHandler.removeCallbacks(exportProgressUpdate)
        mainHandler.post(exportProgressUpdate)
    }

    private fun stopExportProgressUpdates() {
        mainHandler.removeCallbacks(exportProgressUpdate)
    }

    private val ExportError.statusMessage: Int
        get() = when (this) {
            ExportError.INPUT_OPEN -> R.string.video_export_error_input
            ExportError.UNSUPPORTED_VIDEO -> R.string.video_export_error_unsupported
            ExportError.DECODE -> R.string.video_export_error_decode
            ExportError.RENDER -> R.string.video_export_error_render
            ExportError.ENCODE -> R.string.video_export_error_encode
            ExportError.MUX -> R.string.video_export_error_mux
            ExportError.OUTPUT -> R.string.video_export_error_output
        }

    private fun showStatus(message: Int) {
        statusView.setText(message)
    }

    private fun showActiveClipStatus(positionMs: Long) {
        if (clips.isEmpty()) return
        var boundary = 0L
        var activeIndex = clips.lastIndex
        for ((index, clip) in clips.withIndex()) {
            boundary += clip.effectiveWallMs
            if (positionMs < boundary) {
                activeIndex = index
                break
            }
        }
        statusView.text =
            getString(R.string.video_status_playing_clip, activeIndex + 1, clips.size)
    }

    private fun configurePlaybackControls() {
        playPauseButton.setOnClickListener {
            when {
                timelineActive && timelinePaused && videoPreview.resume() -> {
                    timelinePaused = false
                    progressAnchorElapsedMs = SystemClock.elapsedRealtime()
                    updatePlayPauseButton()
                    startProgressUpdates()
                    showActiveClipStatus(timelinePositionMs)
                }

                timelineActive && !timelinePaused && videoPreview.pause() -> {
                    updatePositionFromClock()
                    timelinePaused = true
                    stopProgressUpdates()
                    updatePlayPauseButton()
                    showStatus(R.string.video_status_paused)
                }

                !timelineActive && clips.isNotEmpty() -> {
                    timelinePositionMs = 0L
                    timelinePending = true
                    tryStartPlayback()
                }
            }
        }

        loopSwitch.setOnCheckedChangeListener { _, enabled ->
            videoPreview.setLooping(enabled)
        }

        progressView.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onStartTrackingTouch(seekBar: SeekBar) {
                userSeeking = true
                positionBeforeSeekMs = timelinePositionMs
            }

            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                if (fromUser) {
                    timelinePositionMs = progress.toLong()
                    showPlaybackPosition(timelinePositionMs, updateSeekBar = false)
                }
            }

            override fun onStopTrackingTouch(seekBar: SeekBar) {
                // Cross-segment scrubbing is out of scope for the timeline API;
                // the progress bar is a read-only estimate. Restore the tracked
                // position rather than issuing an unsupported timeline seek.
                timelinePositionMs = positionBeforeSeekMs
                userSeeking = false
                showPlaybackPosition(timelinePositionMs)
            }
        })
    }

    private fun renderClipList() {
        clipListContainer.removeAllViews()
        if (clips.isEmpty()) {
            clipListHeader.setText(R.string.video_clips_empty)
            return
        }
        clipListHeader.text = getString(R.string.video_clips_header, clips.size)
        val inflater = LayoutInflater.from(this)
        clips.forEachIndexed { index, clip ->
            val row = inflater.inflate(R.layout.clip_control_row, clipListContainer, false)
            bindClipRow(row, index, clip)
            clipListContainer.addView(row)
        }
    }

    private fun bindClipRow(row: View, index: Int, clip: Clip) {
        val header = row.findViewById<TextView>(R.id.clip_header)
        val startValue = row.findViewById<TextView>(R.id.clip_start_value)
        val endValue = row.findViewById<TextView>(R.id.clip_end_value)
        val speedValue = row.findViewById<TextView>(R.id.clip_speed_value)
        val startBar = row.findViewById<SeekBar>(R.id.clip_start)
        val endBar = row.findViewById<SeekBar>(R.id.clip_end)
        val speedBar = row.findViewById<SeekBar>(R.id.clip_speed)
        val filterSwitch = row.findViewById<SwitchMaterial>(R.id.clip_filter)

        val durationInt = clip.durationMs.coerceIn(1L, Int.MAX_VALUE.toLong()).toInt()
        header.text = getString(R.string.video_clip_header, index + 1, clip.displayName)

        startBar.max = durationInt
        startBar.progress = clip.startMs.toInt()
        endBar.max = durationInt
        endBar.progress = clip.endMs.coerceIn(1L, durationInt.toLong()).toInt()
        speedBar.max = SPEED_STEPS
        speedBar.progress = speedProgressFor(clip.speed)
        filterSwitch.isChecked = clip.filterEnabled

        startValue.text = getString(R.string.video_clip_start_value, formatDuration(clip.startMs))
        endValue.text = getString(R.string.video_clip_end_value, formatDuration(clip.endMs))
        speedValue.text = getString(R.string.video_clip_speed_value, clip.speed)

        // Labels and the timeline-duration estimate update live while dragging;
        // the native timeline is only restarted once the drag ends (onClipEdited
        // from onStopTrackingTouch) to avoid thrashing the decoder per tick.
        startBar.setOnSeekBarChangeListener(object : SimpleSeekBarListener() {
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                if (!fromUser) return
                // Keep A strictly below B so the segment stays well-formed.
                val newStart = progress.toLong().coerceAtMost((clip.endMs - 1L).coerceAtLeast(0L))
                if (newStart != progress.toLong()) seekBar.progress = newStart.toInt()
                clip.startMs = newStart
                startValue.text =
                    getString(R.string.video_clip_start_value, formatDuration(newStart))
                recomputeTimelineDuration()
            }

            override fun onStopTrackingTouch(seekBar: SeekBar) = onClipEdited()
        })
        endBar.setOnSeekBarChangeListener(object : SimpleSeekBarListener() {
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                if (!fromUser) return
                val newEnd = progress.toLong().coerceAtLeast(clip.startMs + 1L)
                    .coerceAtMost(clip.durationMs)
                if (newEnd != progress.toLong()) seekBar.progress = newEnd.toInt()
                clip.endMs = newEnd
                endValue.text = getString(R.string.video_clip_end_value, formatDuration(newEnd))
                recomputeTimelineDuration()
            }

            override fun onStopTrackingTouch(seekBar: SeekBar) = onClipEdited()
        })
        speedBar.setOnSeekBarChangeListener(object : SimpleSeekBarListener() {
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                if (!fromUser) return
                clip.speed = speedForProgress(progress)
                speedValue.text = getString(R.string.video_clip_speed_value, clip.speed)
                recomputeTimelineDuration()
            }

            override fun onStopTrackingTouch(seekBar: SeekBar) = onClipEdited()
        })
        filterSwitch.setOnCheckedChangeListener { _, checked ->
            clip.filterEnabled = checked
            onClipEdited()
        }
    }

    /** A committed clip edit restarts the timeline from the top with the new segments. */
    private fun onClipEdited() {
        recomputeTimelineDuration()
        if (timelineActive) {
            stopTimelinePlayback(resetToStart = true)
        }
        timelinePending = true
        showPlaybackPosition(0L)
        tryStartPlayback()
    }

    private fun recomputeTimelineDuration() {
        timelineDurationMs = clips.sumOf { it.effectiveWallMs }.coerceAtLeast(0L)
        progressView.max = timelineDurationMs.coerceIn(1L, Int.MAX_VALUE.toLong()).toInt()
    }

    private fun updatePlaybackControlsEnabled(enabled: Boolean) {
        progressView.isEnabled = false
        playPauseButton.isEnabled = enabled && !exportActive
        updatePlayPauseButton()
        updateExportControls()
    }

    private fun updatePlayPauseButton() {
        playPauseButton.setText(
            if (timelineActive && !timelinePaused) R.string.video_pause else R.string.video_play,
        )
    }

    private fun startProgressUpdates() {
        mainHandler.removeCallbacks(progressUpdate)
        mainHandler.postDelayed(progressUpdate, PROGRESS_UPDATE_INTERVAL_MS)
    }

    private fun stopProgressUpdates() {
        mainHandler.removeCallbacks(progressUpdate)
    }

    private fun updatePositionFromClock() {
        if (!timelineActive || timelinePaused || userSeeking) return
        val now = SystemClock.elapsedRealtime()
        timelinePositionMs = (timelinePositionMs + (now - progressAnchorElapsedMs)
            .coerceAtLeast(0L)).coerceAtMost(timelineDurationMs)
        progressAnchorElapsedMs = now
        showPlaybackPosition(timelinePositionMs)
    }

    private fun showPlaybackPosition(positionMs: Long, updateSeekBar: Boolean = true) {
        val boundedPosition = positionMs.coerceIn(0L, timelineDurationMs)
        if (updateSeekBar) {
            progressView.progress = boundedPosition.coerceAtMost(Int.MAX_VALUE.toLong()).toInt()
        }
        timeView.text = getString(
            R.string.video_time_format,
            formatDuration(boundedPosition),
            formatDuration(timelineDurationMs),
        )
    }

    private fun displayName(uri: Uri, index: Int): String {
        val last = uri.lastPathSegment?.substringAfterLast('/')?.substringAfterLast(':')
        return if (last.isNullOrBlank()) {
            getString(R.string.video_clip_default_name, index + 1)
        } else {
            last
        }
    }

    private fun readVideoDuration(video: File): Long {
        val retriever = MediaMetadataRetriever()
        return try {
            retriever.setDataSource(video.absolutePath)
            retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION)
                ?.toLongOrNull()
                ?.coerceAtLeast(0L)
                ?: 0L
        } finally {
            retriever.release()
        }
    }

    private fun speedProgressFor(speed: Double): Int {
        val clamped = speed.coerceIn(MIN_SPEED, MAX_SPEED)
        return (((clamped - MIN_SPEED) / (MAX_SPEED - MIN_SPEED)) * SPEED_STEPS).toInt()
    }

    private fun speedForProgress(progress: Int): Double {
        val fraction = progress.toDouble() / SPEED_STEPS
        val raw = MIN_SPEED + fraction * (MAX_SPEED - MIN_SPEED)
        return (Math.round(raw * 100.0) / 100.0).coerceIn(MIN_SPEED, MAX_SPEED)
    }

    private fun formatDuration(durationMs: Long): String {
        val totalSeconds = durationMs.coerceAtLeast(0L) / 1_000L
        val hours = totalSeconds / 3_600L
        val minutes = totalSeconds / 60L % 60L
        val seconds = totalSeconds % 60L
        return if (hours > 0L) {
            String.format(Locale.getDefault(), "%d:%02d:%02d", hours, minutes, seconds)
        } else {
            String.format(Locale.getDefault(), "%02d:%02d", minutes, seconds)
        }
    }

    /**
     * SeekBar listener with default no-op tracking callbacks so subclasses only
     * implement what they need; [onStopTrackingTouch] may be overridden to
     * commit a drag.
     */
    private abstract class SimpleSeekBarListener : SeekBar.OnSeekBarChangeListener {
        override fun onStartTrackingTouch(seekBar: SeekBar) = Unit
        override fun onStopTrackingTouch(seekBar: SeekBar) = Unit
    }

    private companion object {
        const val VIDEO_CACHE_DIRECTORY = "video_preview"
        const val VIDEO_CACHE_PREFIX = "selected_"
        const val VIDEO_CACHE_SUFFIX = ".video"
        const val EXPORT_CACHE_DIRECTORY = "exports"
        const val COPY_BUFFER_SIZE = 64 * 1024
        const val PROGRESS_UPDATE_INTERVAL_MS = 250L
        const val SPEED_STEPS = 190
        const val MIN_SPEED = 0.1
        const val MAX_SPEED = 2.0

        val FILTERED_APPEARANCE = VideoAppearance(
            filter = VideoFilter(
                source = """
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
                """.trimIndent(),
            ),
        )
    }
}
