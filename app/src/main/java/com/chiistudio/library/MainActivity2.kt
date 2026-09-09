package com.chiistudio.library

import android.media.MediaMetadataRetriever
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.Button
import android.widget.SeekBar
import android.widget.TextView
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.cii.videolib.PlaybackError
import com.cii.videolib.PlaybackListener
import com.cii.videolib.VideoPreview
import com.google.android.material.switchmaterial.SwitchMaterial
import java.io.File
import java.io.IOException
import java.util.Locale
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.Future

class MainActivity2 : AppCompatActivity(), SurfaceHolder.Callback {

    private val mainHandler = Handler(Looper.getMainLooper())
    private val fileExecutor: ExecutorService = Executors.newSingleThreadExecutor()
    private val videoPreview = VideoPreview()

    private lateinit var surfaceView: SurfaceView
    private lateinit var statusView: TextView
    private lateinit var progressView: SeekBar
    private lateinit var timeView: TextView
    private lateinit var playPauseButton: Button
    private lateinit var loopSwitch: SwitchMaterial

    private var copyTask: Future<*>? = null
    private var cachedVideo: File? = null
    private var selectionGeneration = 0
    private var surfaceAttached = false
    private var playbackPending = false
    private var playbackActive = false
    private var activityStarted = false
    private var pickerOpen = false
    private var playbackPaused = false
    private var userSeeking = false
    private var positionBeforeSeekMs = 0L
    private var videoDurationMs = 0L
    private var playbackPositionMs = 0L
    private var progressAnchorElapsedMs = 0L

    private val progressUpdate = object : Runnable {
        override fun run() {
            if (playbackActive && !playbackPaused && !userSeeking) {
                val now = SystemClock.elapsedRealtime()
                val elapsed = (now - progressAnchorElapsedMs).coerceAtLeast(0L)
                progressAnchorElapsedMs = now
                playbackPositionMs = nextPlaybackPosition(playbackPositionMs, elapsed)
                showPlaybackPosition(playbackPositionMs)
            }
            if (playbackActive) {
                mainHandler.postDelayed(this, PROGRESS_UPDATE_INTERVAL_MS)
            }
        }
    }

    private val pickVideo = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        pickerOpen = false
        if (uri == null) {
            tryStartPlayback()
        } else {
            prepareSelectedVideo(uri)
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
        surfaceView.holder.addCallback(this)
        configurePlaybackControls()
        findViewById<Button>(R.id.pick_video_button).setOnClickListener {
            pickerOpen = true
            pickVideo.launch(arrayOf("video/*"))
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
        if (playbackActive) {
            stopProgressUpdates()
            videoPreview.stop()
            playbackActive = false
            playbackPaused = false
            playbackPending = true
            playbackPositionMs = 0L
            showPlaybackPosition(playbackPositionMs)
            updatePlayPauseButton()
            showStatus(R.string.video_status_ready)
        }
        super.onStop()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        surfaceAttached = videoPreview.attachSurface(holder.surface)
        if (surfaceAttached) {
            if (cachedVideo == null) {
                showStatus(R.string.video_status_no_selection)
            }
            tryStartPlayback()
        } else {
            showStatus(R.string.video_status_surface_error)
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) = Unit

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        if (playbackActive) {
            stopProgressUpdates()
            videoPreview.stop()
            playbackActive = false
            playbackPaused = false
            playbackPending = true
            playbackPositionMs = 0L
            showPlaybackPosition(playbackPositionMs)
            updatePlayPauseButton()
        }
        if (surfaceAttached) {
            videoPreview.detachSurface()
            surfaceAttached = false
        }
        if (cachedVideo != null && playbackPending) {
            showStatus(R.string.video_status_waiting_for_surface)
        }
    }

    override fun onDestroy() {
        surfaceView.holder.removeCallback(this)
        copyTask?.cancel(true)
        fileExecutor.shutdownNow()
        stopProgressUpdates()
        videoPreview.stop()
        if (surfaceAttached) {
            videoPreview.detachSurface()
            surfaceAttached = false
        }
        videoPreview.release()
        cachedVideo?.delete()
        cachedVideo = null
        super.onDestroy()
    }

    private fun prepareSelectedVideo(uri: Uri) {
        selectionGeneration += 1
        val generation = selectionGeneration
        copyTask?.cancel(true)
        copyTask = null

        videoPreview.stop()
        playbackActive = false
        playbackPaused = false
        playbackPending = false
        playbackPositionMs = 0L
        videoDurationMs = 0L
        cachedVideo?.delete()
        cachedVideo = null
        updatePlaybackControlsEnabled(false)
        showPlaybackPosition(0L)
        showStatus(R.string.video_status_copying)

        copyTask = fileExecutor.submit {
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

                mainHandler.post {
                    if (isDestroyed || generation != selectionGeneration) {
                        copiedVideo.delete()
                        return@post
                    }
                    copyTask = null
                    cachedVideo = copiedVideo
                    videoDurationMs = durationMs
                    playbackPositionMs = 0L
                    updatePlaybackControlsEnabled(durationMs > 0L)
                    showPlaybackPosition(0L)
                    playbackPending = true
                    if (surfaceAttached) {
                        showStatus(R.string.video_status_ready)
                    } else {
                        showStatus(R.string.video_status_waiting_for_surface)
                    }
                    tryStartPlayback()
                }
            } catch (_: InterruptedException) {
                destination?.delete()
            } catch (_: Exception) {
                destination?.delete()
                mainHandler.post {
                    if (!isDestroyed && generation == selectionGeneration) {
                        copyTask = null
                        showStatus(R.string.video_status_copy_error)
                    }
                }
            }
        }
    }

    private fun tryStartPlayback() {
        val video = cachedVideo ?: return
        if (!playbackPending || playbackActive || !surfaceAttached || !activityStarted || pickerOpen) {
            return
        }

        val generation = selectionGeneration
        videoPreview.setLooping(loopSwitch.isChecked)
        val accepted = videoPreview.play(
            path = video.absolutePath,
            listener = object : PlaybackListener {
                override fun onPlaybackCompleted() {
                    if (generation != selectionGeneration || isDestroyed) return
                    stopProgressUpdates()
                    playbackActive = false
                    playbackPaused = false
                    playbackPositionMs = videoDurationMs
                    showPlaybackPosition(playbackPositionMs)
                    updatePlayPauseButton()
                    showStatus(R.string.video_status_completed)
                }

                override fun onPlaybackError(error: PlaybackError) {
                    if (generation != selectionGeneration || isDestroyed) return
                    stopProgressUpdates()
                    playbackActive = false
                    playbackPaused = false
                    updatePlayPauseButton()
                    if (error == PlaybackError.RENDER) {
                        videoPreview.detachSurface()
                        surfaceAttached = false
                        if (surfaceView.holder.surface.isValid) {
                            surfaceAttached = videoPreview.attachSurface(surfaceView.holder.surface)
                        }
                    }
                    showStatus(error.statusMessage)
                }
            },
        )
        if (accepted) {
            playbackPending = false
            playbackActive = true
            playbackPaused = false
            playbackPositionMs = 0L
            progressAnchorElapsedMs = SystemClock.elapsedRealtime()
            updatePlayPauseButton()
            showPlaybackPosition(playbackPositionMs)
            startProgressUpdates()
            showStatus(R.string.video_status_playing)
        } else {
            showStatus(R.string.video_status_play_error)
        }
    }

    private val PlaybackError.statusMessage: Int
        get() = when (this) {
            PlaybackError.INPUT_OPEN -> R.string.video_status_input_error
            PlaybackError.UNSUPPORTED_VIDEO -> R.string.video_status_unsupported
            PlaybackError.DECODE -> R.string.video_status_decode_error
            PlaybackError.RENDER -> R.string.video_status_render_error
        }

    private fun showStatus(message: Int) {
        statusView.setText(message)
    }

    private fun configurePlaybackControls() {
        playPauseButton.setOnClickListener {
            when {
                playbackActive && playbackPaused && videoPreview.resume() -> {
                    playbackPaused = false
                    progressAnchorElapsedMs = SystemClock.elapsedRealtime()
                    updatePlayPauseButton()
                    startProgressUpdates()
                    showStatus(R.string.video_status_playing)
                }

                playbackActive && !playbackPaused && videoPreview.pause() -> {
                    updatePositionFromClock()
                    playbackPaused = true
                    stopProgressUpdates()
                    updatePlayPauseButton()
                    showStatus(R.string.video_status_paused)
                }

                !playbackActive && cachedVideo != null -> {
                    playbackPositionMs = 0L
                    playbackPending = true
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
                positionBeforeSeekMs = playbackPositionMs
            }

            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                if (fromUser) {
                    playbackPositionMs = progress.toLong()
                    showPlaybackPosition(playbackPositionMs, updateSeekBar = false)
                }
            }

            override fun onStopTrackingTouch(seekBar: SeekBar) {
                val requestedPositionMs = seekBar.progress.toLong()
                if (playbackActive && videoPreview.seekTo(requestedPositionMs)) {
                    playbackPositionMs = requestedPositionMs
                    progressAnchorElapsedMs = SystemClock.elapsedRealtime()
                } else {
                    playbackPositionMs = positionBeforeSeekMs
                }
                userSeeking = false
                showPlaybackPosition(playbackPositionMs)
            }
        })
    }

    private fun updatePlaybackControlsEnabled(enabled: Boolean) {
        progressView.isEnabled = enabled
        playPauseButton.isEnabled = cachedVideo != null
        progressView.max = videoDurationMs.coerceIn(1L, Int.MAX_VALUE.toLong()).toInt()
        updatePlayPauseButton()
    }

    private fun updatePlayPauseButton() {
        playPauseButton.setText(
            if (playbackActive && !playbackPaused) R.string.video_pause else R.string.video_play,
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
        if (!playbackActive || playbackPaused || userSeeking) return
        val now = SystemClock.elapsedRealtime()
        playbackPositionMs = nextPlaybackPosition(
            playbackPositionMs,
            (now - progressAnchorElapsedMs).coerceAtLeast(0L),
        )
        progressAnchorElapsedMs = now
        showPlaybackPosition(playbackPositionMs)
    }

    private fun nextPlaybackPosition(positionMs: Long, elapsedMs: Long): Long {
        if (videoDurationMs <= 0L) return 0L
        val nextPosition = positionMs + elapsedMs
        return if (loopSwitch.isChecked) {
            nextPosition % videoDurationMs
        } else {
            nextPosition.coerceAtMost(videoDurationMs)
        }
    }

    private fun showPlaybackPosition(positionMs: Long, updateSeekBar: Boolean = true) {
        val boundedPosition = positionMs.coerceIn(0L, videoDurationMs)
        if (updateSeekBar) {
            progressView.progress = boundedPosition.coerceAtMost(Int.MAX_VALUE.toLong()).toInt()
        }
        timeView.text = getString(
            R.string.video_time_format,
            formatDuration(boundedPosition),
            formatDuration(videoDurationMs),
        )
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

    private companion object {
        const val VIDEO_CACHE_DIRECTORY = "video_preview"
        const val VIDEO_CACHE_PREFIX = "selected_"
        const val VIDEO_CACHE_SUFFIX = ".video"
        const val COPY_BUFFER_SIZE = 64 * 1024
        const val PROGRESS_UPDATE_INTERVAL_MS = 250L
    }
}
