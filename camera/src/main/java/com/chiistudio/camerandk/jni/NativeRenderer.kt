package com.chiistudio.camerandk.jni

import android.view.Surface
import java.nio.ByteBuffer

object NativeRenderer {

    external fun startCamera()

    external fun nativeInit(surface: Surface, vertex: String, fragment: String)
    external fun nativeRender()

    external fun nativeCleanup()

    /**
     * filter video
     */
    external fun changeFilter(
        resVertex: String,
        resShader: String,
        dataFilter: List<ByteBuffer>,
        widths: List<Int>,
        heights: List<Int>,
        opacity: Float
    )

}