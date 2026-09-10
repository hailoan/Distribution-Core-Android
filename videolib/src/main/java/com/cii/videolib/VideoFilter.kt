package com.cii.videolib

/** Immutable RGBA8888 auxiliary texture supplied to a [VideoFilter]. */
class VideoFilterTexture(
    val width: Int,
    val height: Int,
    rgba8888: ByteArray,
) {
    private val ownedRgba8888 = rgba8888.copyOf()

    /** Returns a copy; caller mutation can never change the retained texture. */
    val rgba8888: ByteArray
        get() = ownedRgba8888.copyOf()

    internal fun copyRgba8888(): ByteArray = ownedRgba8888.copyOf()

    override fun equals(other: Any?): Boolean =
        other is VideoFilterTexture &&
            width == other.width &&
            height == other.height &&
            ownedRgba8888.contentEquals(other.ownedRgba8888)

    override fun hashCode(): Int =
        31 * (31 * width + height) + ownedRgba8888.contentHashCode()
}

/**
 * Trusted GLES 3.0 filter snippet. Version 1 source must define
 * `vec4 addFilter(vec4 inputColor, vec2 uv)` and may sample ordered auxiliary
 * textures named `u_filterTexture0`, `u_filterTexture1`, and so on.
 */
class VideoFilter(
    val version: Int = VERSION_1,
    val source: String,
    val opacity: Float = 1f,
    textures: List<VideoFilterTexture> = emptyList(),
) {
    val textures: List<VideoFilterTexture> = textures.toList()

    override fun equals(other: Any?): Boolean =
        other is VideoFilter &&
            version == other.version &&
            source == other.source &&
            opacity == other.opacity &&
            textures == other.textures

    override fun hashCode(): Int {
        var result = version
        result = 31 * result + source.hashCode()
        result = 31 * result + opacity.hashCode()
        result = 31 * result + textures.hashCode()
        return result
    }

    companion object {
        const val VERSION_1 = 1
    }
}
