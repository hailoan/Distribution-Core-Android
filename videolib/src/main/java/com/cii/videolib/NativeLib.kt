package com.cii.videolib

class NativeLib {

    /**
     * A native method that is implemented by the 'videolib' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String

    /**
     * Returns the FFmpeg version linked into the native library (e.g.
     * "7.1 (avformat 61.7.100)"). Proves the FFmpeg static archives are linked
     * and callable at runtime.
     */
    external fun nativeFFmpegVersion(): String

    companion object {
        // Used to load the 'videolib' library on application startup.
        init {
            System.loadLibrary("videolib")
        }
    }
}