package com.chiistudio.camerandk.model

enum class AdjustType(val glShader: String) {
    BRIGHTNESS("color = brightnessAdjust(color, u_brightness);\n"),
    CONTRAST("color = contrastAdjust(color, u_contrast);\n"),
    SATURATION("color = saturationAdjust(color, u_saturation);\n"),
    EXPOSURE("color = exposureAdjust(color, u_exposure);\n"),
    DARK("color = darkAdjust(color, u_dark);\n"),
    LEVEL("color.rgb = finalLevels(color.rgb, u_level.x, u_level.y, u_level.z);\n"),
    VIGNETTE(""),
    VIBRANCE("color.rgb = vibranceAdjust(color.rgb, u_vibrance);\n"),
    TEMPERATURE("color = temperatureAdjust(color, u_temp);\n"),
    HUE("color = hueAdjust(color, u_hue);\n"),
    HIGHLIGHTS("color = highlightAdjust(color, u_highlight);\n"),
    SHADOW("color = shadowAdjust(color, u_shadow);\n"),
    LIGHTS("color = lightAdjust(color, u_light);\n"),
    CLARITY(""),
}