package com.chiistudio.camerandk.utils

import android.annotation.TargetApi
import android.content.ContentValues
import android.content.Context
import android.database.Cursor
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.ColorMatrix
import android.graphics.ColorMatrixColorFilter
import android.graphics.Matrix
import android.graphics.Paint
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import android.renderscript.Allocation
import android.renderscript.Element
import android.renderscript.RenderScript
import android.renderscript.ScriptIntrinsicBlur
import java.io.FileOutputStream
import java.io.IOException
import java.nio.ByteBuffer
import kotlin.math.roundToInt


internal fun Bitmap.rotateAndCropCenter(flipX: Float, flipY: Float, ratioImage: Float): Bitmap {
    val matrix = Matrix()
    matrix.postScale(flipX, flipY, this.width / 2f, this.height / 2f)
    val sourceWidth = this.width.toFloat()
    val sourceHeight = this.height.toFloat()
    val sourceRatio = sourceWidth / sourceHeight
    val width: Float
    val height: Float
    var x = 0f
    var y = 0f
    if (sourceRatio > ratioImage) {
        height = sourceHeight
        width = (sourceHeight * ratioImage)
        x = kotlin.math.abs(sourceWidth - width) / 2f
    } else {
        width = sourceWidth
        height = (sourceWidth / ratioImage)
        y = kotlin.math.abs(sourceHeight - height) / 2f
    }
    return Bitmap.createBitmap(
        this,
        x.toInt(),
        y.toInt(),
        width.toInt(),
        height.toInt(),
        matrix,
        true
    )

}

internal fun Bitmap.createCropCenter(maxWidth: Int, maxHeight: Int): Bitmap {
    val sourceWidth = this.width.toFloat()
    val sourceHeight = this.height.toFloat()
    val sourceRatio = sourceWidth / sourceHeight
    val ratio = maxWidth / maxHeight.toFloat()
    val width: Float
    val height: Float
    var x = 0f
    var y = 0f
    if (sourceRatio > ratio) {
        height = sourceHeight
        width = (sourceHeight * ratio)
        x = kotlin.math.abs(sourceWidth - width) / 2f
    } else {
        width = sourceWidth
        height = (sourceWidth / ratio)
        y = kotlin.math.abs(sourceHeight - height) / 2f
    }
    return Bitmap.createBitmap(
        this,
        x.toInt(),
        y.toInt(),
        width.toInt(),
        height.toInt()
    )
}

fun Bitmap.rotation(rotation: Float, flipX: Float, flipY: Float): Bitmap {
    val matrix = Matrix()
    matrix.postRotate(rotation)
    matrix.postScale(flipX, flipY, this.width / 2f, this.height / 2f)
    val result = Bitmap.createBitmap(this, 0, 0, this.width, this.height, matrix, true)
    this.recycle()
    return result
}

@TargetApi(Build.VERSION_CODES.Q)
fun Bitmap.saveToFile(context: Context, name: String): String? {
    val values = ContentValues().apply {
        put(MediaStore.MediaColumns.DISPLAY_NAME, name)
        put(MediaStore.MediaColumns.MIME_TYPE, "image/jpg")
        put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DCIM)
    }

    val resolver = context.contentResolver
    var uri: Uri? = null

    try {
        uri = resolver.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values)
            ?: throw IOException("Failed to create new MediaStore record.")

        resolver.openOutputStream(uri)?.use {
            if (!this.compress(Bitmap.CompressFormat.JPEG, 100, it))
                throw IOException("Failed to save bitmap.")
        } ?: throw IOException("Failed to open output stream.")

    } catch (e: Exception) {

    }
    this.recycle()
    return uri?.toString()
}

fun Bitmap.saveToFileLower(path: String): String? {
    return try {
        FileOutputStream(path).use { out ->
            this.compress(Bitmap.CompressFormat.JPEG, 100, out) // bmp is your Bitmap instance
        }
        this.recycle()
        path
    } catch (e: IOException) {
        e.printStackTrace()
        null
    }
}

fun Uri.getRealPathFromURI(context: Context): String? {
    var cursor: Cursor? = null
    return try {
        val proj = arrayOf(MediaStore.Images.Media.DATA)
        cursor = context.contentResolver.query(this, proj, null, null, null)
        val indexColumn = cursor!!.getColumnIndexOrThrow(MediaStore.Images.Media.DATA)
        cursor.moveToFirst()
        cursor.getString(indexColumn)
    } catch (e: java.lang.Exception) {
        null
    } finally {
        cursor?.close()
    }
}

fun ByteArray.saveToFile(context: Context, name: String): String? {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
        val values = ContentValues().apply {
            put(MediaStore.MediaColumns.DISPLAY_NAME, name)
            put(MediaStore.MediaColumns.MIME_TYPE, "image/jpg")
            put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DCIM)
        }

        val resolver = context.contentResolver
        var uri: Uri? = null

        try {
            uri = resolver.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values)
                ?: throw IOException("Failed to create new MediaStore record.")

            resolver.openOutputStream(uri)?.use {
                it.write(this)
            } ?: throw IOException("Failed to open output stream.")

        } catch (e: Exception) {

        }
        return uri.toString()
    }
    return null

}

fun Bitmap.blur(context: Context, scale: Float = 0.4f, blur: Float = 0.7f): Bitmap {
    val widthB = (width * scale).roundToInt()
    val heightB = (height * scale).roundToInt()

    val inputBitmap = Bitmap.createBitmap(widthB, heightB, Bitmap.Config.ARGB_8888)
    val canvas = Canvas(inputBitmap)
    val ma = ColorMatrix()
    ma.setSaturation(0f)
    val paint = Paint()
    paint.colorFilter = ColorMatrixColorFilter(ma)
    canvas.drawBitmap(this, 0f, 0f, paint)


    val outputBitmap = Bitmap.createBitmap(inputBitmap)

    val rs = RenderScript.create(context)
    val theIntrinsic = ScriptIntrinsicBlur.create(rs, Element.U8_4(rs))
    val tmpIn = Allocation.createFromBitmap(rs, inputBitmap)
    val tmpOut = Allocation.createFromBitmap(rs, outputBitmap)
    theIntrinsic.setRadius(blur)
    theIntrinsic.setInput(tmpIn)
    theIntrinsic.forEach(tmpOut)
    tmpOut.copyTo(outputBitmap)
    return outputBitmap
}

fun Bitmap.convertBitmapToByteBuffer(): ByteBuffer {
    val buffer = ByteBuffer.allocateDirect(byteCount)
    copyPixelsToBuffer(buffer)
    buffer.flip()
    this.recycle()
    return buffer
}
