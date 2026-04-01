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
    fun changeFilter(pathFilter: String?, opacity: Float, overlayList: List<String>?)
}

interface IColorVideo {
    fun mixColor(mixColor: HashMap<AdjustColorType, Vec3>)
}