package com.chiistudio.camerandk.model

class CameraBuilder {
    var vertex: String = ""
    var fragment: String = ""
    var duration: Long = 0

    var initialMode: CameraMode = CameraMode.PHOTO
        private set
    var initialLens: CameraLens = CameraLens.BACK
        private set

    private val resolutions: MutableMap<Pair<CameraMode, CameraLens>, ResolutionPreset> =
        mutableMapOf(
            CameraMode.PHOTO to CameraLens.BACK to ResolutionPreset.P1080,
            CameraMode.PHOTO to CameraLens.FRONT to ResolutionPreset.P720,
            CameraMode.VIDEO to CameraLens.BACK to ResolutionPreset.P1080,
            CameraMode.VIDEO to CameraLens.FRONT to ResolutionPreset.P720,
        )

    fun setVertex(vertex: String): CameraBuilder {
        this.vertex = vertex
        return this
    }

    fun setFragment(fragment: String): CameraBuilder {
        this.fragment = fragment
        return this
    }

    fun setResolution(
        mode: CameraMode,
        lens: CameraLens,
        preset: ResolutionPreset,
    ): CameraBuilder {
        resolutions[mode to lens] = preset
        return this
    }

    fun setInitialMode(mode: CameraMode): CameraBuilder {
        initialMode = mode
        return this
    }

    fun setInitialLens(lens: CameraLens): CameraBuilder {
        initialLens = lens
        return this
    }

    fun resolutionFor(mode: CameraMode, lens: CameraLens): ResolutionPreset =
        resolutions.getValue(mode to lens)

    fun resolutionEntries(): List<Entry> =
        resolutions.entries.map { (key, preset) -> Entry(key.first, key.second, preset) }

    data class Entry(
        val mode: CameraMode,
        val lens: CameraLens,
        val preset: ResolutionPreset,
    )
}
