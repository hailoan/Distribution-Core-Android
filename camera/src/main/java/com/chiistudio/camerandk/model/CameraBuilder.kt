package com.chiistudio.camerandk.model

import android.util.Size

class CameraBuilder {
    var size: Size? = null
    var vertex: String = ""
    var fragment: String = ""
    var duration: Long = 0

    fun setSize(size: Size): CameraBuilder {
        this.size = size
        return this
    }

    fun setVertex(vertex: String): CameraBuilder {
        this.vertex = vertex
        return this
    }

    fun setFragment(fragment: String): CameraBuilder {
        this.fragment = fragment
        return this
    }
}
