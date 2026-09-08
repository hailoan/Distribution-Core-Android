package com.cii.videolib

class NativeLib {

    /**
     * A native method that is implemented by the 'videolib' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String

    companion object {
        // Used to load the 'videolib' library on application startup.
        init {
            System.loadLibrary("videolib")
        }
    }
}