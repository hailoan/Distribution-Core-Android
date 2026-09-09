package com.chiistudio.library

import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.Button
import android.widget.TextView
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.cii.videolib.PlaybackError
import com.cii.videolib.PlaybackListener
import com.cii.videolib.VideoPreview
import java.io.File
import java.io.IOException
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.Future

class MainActivity2 : AppCompatActivity(), SurfaceHolder.Callback {

    private val mainHandler = Handler(Looper.getMainLooper())
    private val fileExecutor: ExecutorService = Executors.newSingleThreadExecutor()
    private val videoPreview = VideoPreview()

    private lateinit var surfaceView: SurfaceView
    private lateinit var statusView: TextView

    private var copyTask: Future<*>? = null
    private var cachedVideo: File? = null
    private var selectionGeneration = 0
    private var surfaceAttached = false
    private var playbackPending = false
    private var playbackActive = false
    private var activityStarted = false
    private var pickerOpen = false

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
        surfaceView.holder.addCallback(this)
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
            videoPreview.stop()
            playbackActive = false
            playbackPending = true
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
            videoPreview.stop()
            playbackActive = false
            playbackPending = true
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
        playbackPending = false
        cachedVideo?.delete()
        cachedVideo = null
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

                mainHandler.post {
                    if (isDestroyed || generation != selectionGeneration) {
                        copiedVideo.delete()
                        return@post
                    }
                    copyTask = null
                    cachedVideo = copiedVideo
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
        val accepted = videoPreview.play(
            path = video.absolutePath,
            listener = object : PlaybackListener {
                override fun onPlaybackCompleted() {
                    if (generation != selectionGeneration || isDestroyed) return
                    playbackActive = false
                    showStatus(R.string.video_status_completed)
                }

                override fun onPlaybackError(error: PlaybackError) {
                    if (generation != selectionGeneration || isDestroyed) return
                    playbackActive = false
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

    private companion object {
        const val VIDEO_CACHE_DIRECTORY = "video_preview"
        const val VIDEO_CACHE_PREFIX = "selected_"
        const val VIDEO_CACHE_SUFFIX = ".video"
        const val COPY_BUFFER_SIZE = 64 * 1024
    }
}
