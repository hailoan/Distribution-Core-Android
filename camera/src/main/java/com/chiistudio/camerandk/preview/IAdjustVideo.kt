package com.chiistudio.camerandk.preview

import com.chiistudio.camerandk.model.AdjustColorType
import com.chiistudio.camerandk.model.Vec3

interface IAdjustVideo {
    fun changeBrightness(value: Float)
    fun changeExposure(value: Float)
    fun changeContrast(value: Float)
    fun changeSaturation(value: Float)
    fun changeLight(value: Float)
    fun changeHighLight(value: Float)
    fun changeDark(value: Float)
    fun changeClarity(value: Float)
    fun changeShadow(value: Float)
    fun changeTemp(value: Float)
    fun changeHue(value: Float)
    fun changeVignette(value: Float)
    fun changeLevel(x: Float, y: Float, z: Float)
    fun changeVibrant(value: Float)

}

interface IFilter {
    fun applyFilter(pathFilter: String?, opacity: Float, overlayList: List<String>?)
}

interface IColorVideo {
    fun mixColor(mixColor: HashMap<AdjustColorType, Vec3>)
}

/**
 * Sensor-side camera control: tap-to-focus, AF lock, AE exposure compensation.
 *
 * Distinct from [IAdjustVideo.changeBrightness] — that is a post-process shader
 * gain in [-0.5, 0.5]. [setBrightness] here drives the HAL's AE compensation in
 * integer EV steps (each step is the device's `AE_COMPENSATION_STEP`, commonly
 * 1/6 EV). They can be used independently.
 */
interface ICameraControl {
    /**
     * Trigger AF + AE metering at a view-space point. Coordinates are in the
     * preview view's pixel space; the native layer handles sensor-orientation
     * and front-lens mirroring.
     */
    fun focusAt(viewX: Float, viewY: Float)

    /**
     * Lock (or unlock) the focus at its current position. While locked the
     * lens does not hunt; tapping with [focusAt] implicitly unlocks.
     */
    fun lockFocus(locked: Boolean)

    /**
     * Sensor-level exposure compensation in HAL EV steps. Clamped to the
     * device's reported range (see [exposureCompensationRange]).
     */
    fun setBrightness(ev: Int)

    /**
     * `Pair(min, max)` of supported AE compensation steps for the active camera.
     * Both are 0 until the camera has been opened.
     */
    fun exposureCompensationRange(): Pair<Int, Int>
}