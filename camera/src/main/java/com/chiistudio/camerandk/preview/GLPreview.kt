package com.chiistudio.camerandk.preview

import android.annotation.SuppressLint
import android.content.Context
import android.view.SurfaceHolder
import android.view.SurfaceView
import com.chiistudio.camerandk.jni.NativeRenderer
import com.chiistudio.camerandk.model.CameraBuilder
import com.chiistudio.camerandk.utils.AssetReader
import com.chiistudio.camerandk.utils.CurveTone
import com.chiistudio.camerandk.utils.ShaderBuilder
import com.chiistudio.camerandk.utils.convertBitmapToByteBuffer

@SuppressLint("ViewConstructor")
class GLPreview(context: Context) : SurfaceView(context), IFilter,
    SurfaceHolder.Callback2 {


    val builder: CameraBuilder = CameraBuilder()
        .setVertex(AssetReader.readString(context, "glsl/vertex_shader.glsl"))
        .setFragment(
            ShaderBuilder.buildShader(
                context,
                pathMain = "glsl/base_fragment_shade_top.glsl",
                filter = AssetReader.readString(context, "glsl/frag_filter_none.glsl"),
                effect = AssetReader.readString(context, "glsl/frag_effect_none.glsl")
            )
        )

    init {
        holder.addCallback(this)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        NativeRenderer.nativeInit(holder.surface, builder.vertex, builder.fragment)
        NativeRenderer.startCamera()
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        NativeRenderer.nativeCleanup()
    }

    override fun surfaceRedrawNeeded(holder: SurfaceHolder) {

    }


    override fun changeFilter(pathFilter: String?, opacity: Float, overlayList: List<String>?) {
        builder.setFragment(
            ShaderBuilder.buildShader(
                context,
                pathMain = "glsl/base_fragment_shade_top.glsl",
                filter = pathFilter ?: AssetReader.readString(
                    context,
                    "glsl/frag_filter_none.glsl"
                ),
                effect = AssetReader.readString(context, "glsl/frag_effect_none.glsl")
            )
        )
        val dataFilter = overlayList?.map { path ->
            if (path.endsWith(".acv") || path.endsWith("xmp")) {
                val curve = CurveTone()
                curve.loadCurve(context, path)
                Triple(curve.curveData, 256, 1)
            } else {
                Triple(
                    AssetReader.getBitmapFromAsset(context = context, filePath = path)
                        ?.convertBitmapToByteBuffer(), 512, 512
                )

            }
        }
        NativeRenderer.changeFilter(
            resVertex = builder.vertex,
            resShader = builder.fragment,
            opacity = opacity,
            dataFilter = dataFilter?.mapNotNull { it.first } ?: listOf(),
            widths = dataFilter?.map { it.second } ?: listOf(),
            heights = dataFilter?.map { it.third } ?: listOf(),
        )
    }

}