package com.cii.videolib

import android.content.Context
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.media.MediaMuxer
import android.opengl.EGL14
import android.opengl.EGLConfig
import android.opengl.EGLContext
import android.opengl.EGLDisplay
import android.opengl.EGLExt
import android.opengl.EGLSurface
import android.opengl.GLES20
import android.view.Surface
import java.io.File
import java.nio.ByteBuffer

/**
 * Deterministic local media fixtures (C-09) for the [VideoPreview] device suite.
 *
 * The plan named a checked-in `progressive_preview_long.mp4`. A binary MP4 cannot
 * be authored or code-reviewed as source, so the fixture is realised as a runtime
 * generator instead: it encodes a deterministic clip with the platform
 * [MediaCodec]/[MediaMuxer] and writes it to app cache, which the test then hands
 * to [VideoPreview.play] as a readable local path — satisfying the same fixture
 * contract (local, video-only, multi-frame, deterministic, distinguishable
 * frames, long enough to observe a first frame before completion) with more
 * determinism and reviewability than a binary blob, and with a codec (H.264)
 * decodable by the bundled FFmpeg 7.1 build.
 *
 * Each video frame is a solid colour whose RED channel ramps across the clip, so
 * successive presented frames are visually distinguishable at their centre pixel
 * (which is what [PlaybackSurfaceProbe] samples). Audio is never added, so the
 * clip is video-only.
 */
object TestVideoFixture {

    const val WIDTH = 320
    const val HEIGHT = 240
    const val FRAME_RATE = 15
    // 45 frames at 15 fps ≈ 3.0 s of real-time pacing: comfortably long enough to
    // observe a first frame well before completion, with many distinguishable frames.
    const val FRAME_COUNT = 45

    private const val VIDEO_MIME = "video/avc"
    private const val AUDIO_MIME = "audio/mp4a-latm"
    private const val BIT_RATE = 2_000_000
    private const val I_FRAME_INTERVAL = 1
    private const val DEQUEUE_TIMEOUT_US = 10_000L

    /**
     * Generates the progressive video clip into cache and returns the file.
     * Overwrites any previous fixture of the same name.
     */
    fun generateProgressiveVideo(
        context: Context,
        frameCount: Int = FRAME_COUNT,
    ): File {
        val output = File(cacheRoot(context), "progressive_preview_long.mp4")
        if (output.exists()) output.delete()

        val format = MediaFormat.createVideoFormat(VIDEO_MIME, WIDTH, HEIGHT).apply {
            setInteger(
                MediaFormat.KEY_COLOR_FORMAT,
                MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface,
            )
            setInteger(MediaFormat.KEY_BIT_RATE, BIT_RATE)
            setInteger(MediaFormat.KEY_FRAME_RATE, FRAME_RATE)
            setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, I_FRAME_INTERVAL)
        }

        val encoder = MediaCodec.createEncoderByType(VIDEO_MIME)
        encoder.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
        val inputSurface = CodecInputSurface(encoder.createInputSurface())
        encoder.start()

        val muxer = MediaMuxer(output.absolutePath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4)
        val bufferInfo = MediaCodec.BufferInfo()
        var trackIndex = -1
        var muxerStarted = false

        inputSurface.makeCurrent()
        try {
            var frameIndex = 0
            while (true) {
                val endOfStream = frameIndex >= frameCount
                if (!endOfStream) {
                    drainEncoder(encoder, muxer, bufferInfo, endOfStream = false,
                        onTrack = { trackIndex = it; muxerStarted = true },
                        trackIndex = { trackIndex },
                        muxerStarted = { muxerStarted })
                    drawFrame(frameIndex)
                    val presentationNs = frameIndex * 1_000_000_000L / FRAME_RATE
                    inputSurface.setPresentationTime(presentationNs)
                    inputSurface.swapBuffers()
                    frameIndex++
                } else {
                    encoder.signalEndOfInputStream()
                    drainEncoder(encoder, muxer, bufferInfo, endOfStream = true,
                        onTrack = { trackIndex = it; muxerStarted = true },
                        trackIndex = { trackIndex },
                        muxerStarted = { muxerStarted })
                    break
                }
            }
        } finally {
            encoder.stop()
            encoder.release()
            inputSurface.release()
            if (muxerStarted) muxer.stop()
            muxer.release()
        }
        return output
    }

    /**
     * Generates a readable audio-only M4A (no video stream). Opening it succeeds
     * and stream info is found, but there is no video stream, so the native path
     * reports [PlaybackError.UNSUPPORTED_VIDEO].
     */
    fun generateAudioOnly(context: Context): File {
        val output = File(cacheRoot(context), "audio_only.m4a")
        if (output.exists()) output.delete()

        val sampleRate = 44_100
        val channelCount = 1
        val format = MediaFormat.createAudioFormat(AUDIO_MIME, sampleRate, channelCount).apply {
            setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectLC)
            setInteger(MediaFormat.KEY_BIT_RATE, 64_000)
            setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, 16_384)
        }

        val encoder = MediaCodec.createEncoderByType(AUDIO_MIME)
        encoder.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
        encoder.start()

        val muxer = MediaMuxer(output.absolutePath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4)
        val bufferInfo = MediaCodec.BufferInfo()
        var trackIndex = -1
        var muxerStarted = false

        val totalSamples = sampleRate / 2 // ~0.5 s of silence
        val bytesPerSample = 2
        val chunkSamples = 1_024
        val silence = ByteArray(chunkSamples * bytesPerSample * channelCount)
        var samplesSubmitted = 0
        var inputDone = false

        try {
            while (true) {
                if (!inputDone) {
                    val inputIndex = encoder.dequeueInputBuffer(DEQUEUE_TIMEOUT_US)
                    if (inputIndex >= 0) {
                        val input = encoder.getInputBuffer(inputIndex)!!
                        input.clear()
                        val presentationUs = samplesSubmitted * 1_000_000L / sampleRate
                        if (samplesSubmitted >= totalSamples) {
                            encoder.queueInputBuffer(
                                inputIndex, 0, 0, presentationUs,
                                MediaCodec.BUFFER_FLAG_END_OF_STREAM,
                            )
                            inputDone = true
                        } else {
                            input.put(silence)
                            encoder.queueInputBuffer(
                                inputIndex, 0, silence.size, presentationUs, 0,
                            )
                            samplesSubmitted += chunkSamples
                        }
                    }
                }

                val outputIndex = encoder.dequeueOutputBuffer(bufferInfo, DEQUEUE_TIMEOUT_US)
                if (outputIndex == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                    trackIndex = muxer.addTrack(encoder.outputFormat)
                    muxer.start()
                    muxerStarted = true
                } else if (outputIndex >= 0) {
                    val encoded = encoder.getOutputBuffer(outputIndex)!!
                    if (bufferInfo.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG != 0) {
                        bufferInfo.size = 0
                    }
                    if (bufferInfo.size > 0 && muxerStarted) {
                        encoded.position(bufferInfo.offset)
                        encoded.limit(bufferInfo.offset + bufferInfo.size)
                        muxer.writeSampleData(trackIndex, encoded, bufferInfo)
                    }
                    encoder.releaseOutputBuffer(outputIndex, false)
                    if (bufferInfo.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0) break
                }
            }
        } finally {
            encoder.stop()
            encoder.release()
            if (muxerStarted) muxer.stop()
            muxer.release()
        }
        return output
    }

    /**
     * Returns a path inside cache that does not exist, for the missing/unreadable
     * input case ([PlaybackError.INPUT_OPEN]). Ensures the file is absent.
     */
    fun missingLocalPath(context: Context): String {
        val missing = File(cacheRoot(context), "does_not_exist_${System.nanoTime()}.mp4")
        if (missing.exists()) missing.delete()
        return missing.absolutePath
    }

    private fun cacheRoot(context: Context): File =
        File(context.cacheDir, "videolib_test_fixtures").apply { mkdirs() }

    // Drains all currently available encoded output into the muxer. When
    // endOfStream is true, spins until BUFFER_FLAG_END_OF_STREAM is seen.
    private inline fun drainEncoder(
        encoder: MediaCodec,
        muxer: MediaMuxer,
        bufferInfo: MediaCodec.BufferInfo,
        endOfStream: Boolean,
        onTrack: (Int) -> Unit,
        trackIndex: () -> Int,
        muxerStarted: () -> Boolean,
    ) {
        while (true) {
            val outputIndex = encoder.dequeueOutputBuffer(bufferInfo, DEQUEUE_TIMEOUT_US)
            if (outputIndex == MediaCodec.INFO_TRY_AGAIN_LATER) {
                if (!endOfStream) return
                // else keep waiting for EOS
            } else if (outputIndex == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                onTrack(muxer.addTrack(encoder.outputFormat))
                muxer.start()
            } else if (outputIndex >= 0) {
                val encoded = encoder.getOutputBuffer(outputIndex)!!
                if (bufferInfo.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG != 0) {
                    bufferInfo.size = 0
                }
                if (bufferInfo.size > 0 && muxerStarted()) {
                    encoded.position(bufferInfo.offset)
                    encoded.limit(bufferInfo.offset + bufferInfo.size)
                    muxer.writeSampleData(trackIndex(), encoded, bufferInfo)
                }
                encoder.releaseOutputBuffer(outputIndex, false)
                if (bufferInfo.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0) return
            }
        }
    }

    // Solid frame whose RED channel ramps 0..255 across the clip; the probe reads
    // the centre pixel's first (R) byte, so successive frames are distinguishable.
    private fun drawFrame(frameIndex: Int) {
        val level = (frameIndex * 255 / (FRAME_COUNT - 1)).coerceIn(0, 255) / 255f
        GLES20.glClearColor(level, 0f, 0f, 1f)
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT)
    }

    /**
     * Minimal EGL wrapper around a [MediaCodec] input [Surface] so frames can be
     * drawn with GL and timestamped. Standard EGL14 encode-input pattern.
     */
    private class CodecInputSurface(private val surface: Surface) {
        private var eglDisplay: EGLDisplay = EGL14.EGL_NO_DISPLAY
        private var eglContext: EGLContext = EGL14.EGL_NO_CONTEXT
        private var eglSurface: EGLSurface = EGL14.EGL_NO_SURFACE

        init {
            eglDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY)
            check(eglDisplay != EGL14.EGL_NO_DISPLAY) { "eglGetDisplay failed" }
            val version = IntArray(2)
            check(EGL14.eglInitialize(eglDisplay, version, 0, version, 1)) { "eglInitialize failed" }

            val configAttribs = intArrayOf(
                EGL14.EGL_RED_SIZE, 8,
                EGL14.EGL_GREEN_SIZE, 8,
                EGL14.EGL_BLUE_SIZE, 8,
                EGL14.EGL_ALPHA_SIZE, 8,
                EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
                EGLExt.EGL_RECORDABLE_ANDROID, 1,
                EGL14.EGL_NONE,
            )
            val configs = arrayOfNulls<EGLConfig>(1)
            val numConfigs = IntArray(1)
            check(
                EGL14.eglChooseConfig(
                    eglDisplay, configAttribs, 0, configs, 0, configs.size, numConfigs, 0,
                ) && numConfigs[0] > 0,
            ) { "eglChooseConfig failed" }

            val contextAttribs = intArrayOf(EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE)
            eglContext = EGL14.eglCreateContext(
                eglDisplay, configs[0], EGL14.EGL_NO_CONTEXT, contextAttribs, 0,
            )
            check(eglContext != EGL14.EGL_NO_CONTEXT) { "eglCreateContext failed" }

            val surfaceAttribs = intArrayOf(EGL14.EGL_NONE)
            eglSurface = EGL14.eglCreateWindowSurface(
                eglDisplay, configs[0], surface, surfaceAttribs, 0,
            )
            check(eglSurface != EGL14.EGL_NO_SURFACE) { "eglCreateWindowSurface failed" }
        }

        fun makeCurrent() {
            check(EGL14.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
                "eglMakeCurrent failed"
            }
        }

        fun swapBuffers(): Boolean = EGL14.eglSwapBuffers(eglDisplay, eglSurface)

        fun setPresentationTime(nsecs: Long) {
            EGLExt.eglPresentationTimeANDROID(eglDisplay, eglSurface, nsecs)
        }

        fun release() {
            if (eglDisplay != EGL14.EGL_NO_DISPLAY) {
                EGL14.eglMakeCurrent(
                    eglDisplay, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT,
                )
                EGL14.eglDestroySurface(eglDisplay, eglSurface)
                EGL14.eglDestroyContext(eglDisplay, eglContext)
                EGL14.eglTerminate(eglDisplay)
            }
            surface.release()
            eglDisplay = EGL14.EGL_NO_DISPLAY
            eglContext = EGL14.EGL_NO_CONTEXT
            eglSurface = EGL14.EGL_NO_SURFACE
        }
    }
}
