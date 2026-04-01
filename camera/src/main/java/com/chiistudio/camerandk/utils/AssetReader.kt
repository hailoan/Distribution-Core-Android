package com.chiistudio.camerandk.utils

import android.content.Context
import android.content.res.AssetManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import com.chiistudio.camerandk.utils.CropBitmap.Companion.NONE
import java.io.BufferedReader
import java.io.IOException
import java.io.InputStream
import java.io.InputStreamReader
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.charset.StandardCharsets
import kotlin.math.roundToInt


object AssetReader {

    fun readString(context: Context, path: String): String {
        val sb = StringBuilder()
        val input: InputStream = context.assets.open(path)
        val br = BufferedReader(InputStreamReader(input, StandardCharsets.UTF_8))
        var str: String? = br.readLine()
        while (str != null) {
            sb.append(str)
            str = br.readLine()
            sb.append("\n")
        }
        br.close()
        return sb.toString()
    }

    fun getBitmapFromAsset(
        context: Context,
        filePath: String?,
        @CropBitmap typeCrop: Int = NONE,
        width: Int = 0,
        height: Int = 0,
        ratio: Float = 1f
    ): Bitmap? {
        val assetManager: AssetManager = context.assets
        val istr: InputStream
        try {
            istr = assetManager.open(filePath!!)
            val bitmap = BitmapFactory.decodeStream(istr)
            if (typeCrop == CropBitmap.CROP_CENTER) {
                val result: Bitmap = if (width > height) {
                    bitmap.createCropCenter((height * ratio).toInt(), height)
                } else {
                    bitmap.createCropCenter(width, (width / ratio).toInt())
                }
                bitmap.recycle()
                return result
            }
            return bitmap
        } catch (e: IOException) {
            return null
        }
    }

    fun parseACV(input: InputStream): Triple<Curve, Curve, Curve> {
        val buffer = ByteBuffer.wrap(input.readBytes()).order(ByteOrder.BIG_ENDIAN)

        buffer.short // version
        val curvesCount = buffer.short.toInt()

        var rgbCurve = Curve(emptyList())
        var rCurve = Curve(emptyList())
        var gCurve = Curve(emptyList())
        var bCurve = Curve(emptyList())

        repeat(curvesCount) { index ->
            val pointCount = buffer.short.toInt()
            val points = List(pointCount) {
                val y = buffer.short.toInt()
                val x = buffer.short.toInt()
                x to y
            }
            when (index) {
                0 -> rgbCurve = Curve(points)
                1 -> rCurve = Curve(points)
                2 -> gCurve = Curve(points)
                3 -> bCurve = Curve(points)
            }
        }

        return Triple(rCurve, gCurve, bCurve)
    }

    // Linear interpolation helper
    fun interpolateCurve(curve: Curve): IntArray {
        val output = IntArray(256)
        val pts = curve.points

        for (i in 0..255) {
            var p1 = pts.first()
            var p2 = pts.last()

            for (j in 1 until pts.size) {
                if (i <= pts[j].first) {
                    p1 = pts[j - 1]
                    p2 = pts[j]
                    break
                }
            }

            val span = p2.first - p1.first
            val ratio = if (span == 0) 0f else (i - p1.first).toFloat() / span
            output[i] = (p1.second + ratio * (p2.second - p1.second)).roundToInt().coerceIn(0, 255)
        }

        return output
    }

    fun generateACVLUT(context: Context, path: String): ByteBuffer {
        val (rCurve, gCurve, bCurve) = context.assets.open(path).use(::parseACV)

        val r = interpolateCurve(rCurve)
        val g = interpolateCurve(gCurve)
        val b = interpolateCurve(bCurve)

        val buffer = ByteBuffer.allocateDirect(256 * 3)
        for (i in 0 until 256) {
            buffer.put(r[i].toByte())
            buffer.put(g[i].toByte())
            buffer.put(b[i].toByte())
        }
        buffer.flip()
        return buffer
    }
}

annotation class CropBitmap {
    companion object {
        const val NONE = -1
        const val CROP_CENTER = 0
    }
}

data class Curve(val points: List<Pair<Int, Int>>)
