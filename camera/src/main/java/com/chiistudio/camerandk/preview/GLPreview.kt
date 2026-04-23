package com.chiistudio.camerandk.preview

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Bitmap
import android.os.Handler
import android.os.Looper
import android.view.SurfaceHolder
import android.view.SurfaceView
import com.chiistudio.camerandk.jni.NativeRenderer
import com.chiistudio.camerandk.model.CameraBuilder
import com.chiistudio.camerandk.utils.AssetReader
import com.chiistudio.camerandk.utils.CurveTone
import com.chiistudio.camerandk.utils.ShaderBuilder
import com.chiistudio.camerandk.utils.convertBitmapToByteBuffer
import java.nio.ByteBuffer

@SuppressLint("ViewConstructor")
open class GLPreview(context: Context) : SurfaceView(context), IFilter,
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
        val entries = builder.resolutionEntries()
        NativeRenderer.nativeSetResolutionMap(
            modes = IntArray(entries.size) { entries[it].mode.nativeValue },
            lenses = IntArray(entries.size) { entries[it].lens.nativeValue },
            widths = IntArray(entries.size) { entries[it].preset.width },
            heights = IntArray(entries.size) { entries[it].preset.height },
        )
        NativeRenderer.nativeSetLens(builder.initialLens.nativeValue)
        NativeRenderer.nativeSetMode(builder.initialMode.nativeValue)
        NativeRenderer.startCamera()
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        NativeRenderer.nativeCleanup()
    }

    override fun surfaceRedrawNeeded(holder: SurfaceHolder) {

    }


    override fun applyFilter(pathFilter: String?, opacity: Float, overlayList: List<String>?) {
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

    /**
     * Capture the currently-displayed preview frame (with all active shader
     * effects applied) as an Android [Bitmap].
     *
     * The native layer performs `glReadPixels` on the EGL thread immediately
     * after the next frame is rendered, so the preview is not disrupted and
     * no tearing occurs. The callback is dispatched on the main thread.
     *
     * If capture fails (e.g. zero-sized buffer), [onCapture] is invoked with
     * a null [Bitmap].
     */
    fun captureFrame(onCapture: (Bitmap?) -> Unit) {
        NativeRenderer.captureFrame { rgba, w, h ->
            val bitmap: Bitmap? = if (w > 0 && h > 0 && rgba.size >= w * h * 4) {
                // glReadPixels produces rows with origin at the bottom-left.
                // Flip vertically so the Bitmap matches screen orientation.
                val stride = w * 4
                val flipped = ByteArray(rgba.size)
                for (row in 0 until h) {
                    System.arraycopy(
                        rgba,
                        row * stride,
                        flipped,
                        (h - 1 - row) * stride,
                        stride
                    )
                }
                Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888).apply {
                    copyPixelsFromBuffer(ByteBuffer.wrap(flipped))
                }
            } else {
                null
            }
            mainHandler.post { onCapture(bitmap) }
        }
    }

    companion object {
        private val mainHandler = Handler(Looper.getMainLooper())
    }

}