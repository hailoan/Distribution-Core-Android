package com.chiistudio.camerandk.model

import java.util.EnumMap

class VideoConfigure {
    var scaleX = 1f
    var scaleY = 1f
    var orientation = 0
    var ratioVideo = 1f
    var width: Int = 1
    var height: Int = 1
    var duration: Int = 0

    /**
     * ranger -0.5...0.5
     * default 0
     */
    var brightness = 0.0f

    /**
     * ranger 0...2f
     * default 1
     */
    var contrast = 1.0f

    /**
     * ranger -1...1
     * default 0
     */
    var exposure = 0.0f

    /**
     * ranger -0...2
     * default 1
     */
    var saturation = 1.0f

    /**
     * ranger 0.5...1.5
     * default 1
     */
    var dark = 1f
    var vignette = 0.0f

    /**
     * ranger -1...1
     * default 0
     */
    var vibrance = 0.0f

    /**
     * ranger light 0...2 dark
     * default 1
     */
    var light = 1f

    var highlight = -2.0f

    /**
     * ranger -1...1
     * default 0
     */
    var shadow = 0.0f

    /**
     * ranger -0.5...0.5
     * default 0
     */
    var temperature = 0f

    /**
     * ranger -1...1
     * default 0
     */
    var hue = 0.0f
    var clarity = 0.0f
    var opacityOverlay = 1.0f

    /**
     * x -1...1 default 0
     * y 0.5...1.5 default 1
     * z 0.5...1.5 default 1
     */
    var level: Vec3 = Vec3(0f, 1f, 1f)

    var balanceShadow: Vec3 = Vec3(1f, 1f, 1f)

    /**
     * value max 1.0 min 0.0
     */
    var mixColor: EnumMap<AdjustColorType, Vec3> = EnumMap(AdjustColorType::class.java)
}