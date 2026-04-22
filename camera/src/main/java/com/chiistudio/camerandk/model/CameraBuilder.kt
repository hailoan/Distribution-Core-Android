package com.chiistudio.camerandk.model

class CameraBuilder {
    var vertex: String = ""
    var fragment: String = ""
    var duration: Long = 0

    fun setVertex(vertex: String): CameraBuilder {
        this.vertex = vertex
        return this
    }

    fun setFragment(fragment: String): CameraBuilder {
        this.fragment = fragment
        return this
    }
}
