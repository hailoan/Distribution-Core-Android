package com.cii.videolib

import androidx.annotation.Keep

sealed interface AppearanceUpdateResult {
    data object Accepted : AppearanceUpdateResult

    data class Rejected(
        val reason: AppearanceRejectionReason,
        val diagnostic: String? = null,
    ) : AppearanceUpdateResult
}

enum class AppearanceRejectionReason {
    INVALID_VALUE,
    INVALID_LEVELS,
    UNSUPPORTED_FILTER_VERSION,
    INVALID_FILTER_SOURCE,
    INVALID_FILTER_OPACITY,
    INVALID_FILTER_TEXTURE,
    SURFACE_UNAVAILABLE,
    RELEASED,
    SHADER_COMPILATION,
    PROGRAM_LINK,
    DEVICE_CAPABILITY,
    RESOURCE_ALLOCATION,
    RENDER_FAILURE,
    UNSUPPORTED_EFFECT_VERSION,
    INVALID_EFFECT_SOURCE,
    INVALID_EFFECT_OPACITY,
    INVALID_EFFECT_SPEED,
}

@Keep
internal data class NativeAppearanceResult(
    val code: Int,
    val diagnostic: String?,
)
