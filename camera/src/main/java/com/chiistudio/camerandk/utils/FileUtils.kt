package com.chiistudio.camerandk.utils

import android.content.Context
import android.net.Uri
import java.io.BufferedOutputStream
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream

object FileUtils {
    fun copyToNewFile(
        context: Context,
        uri: Uri
    ): File {
        return try {
            val idName = uri.pathSegments.lastOrNull() ?: System.currentTimeMillis().toString()
            val pfd = context.contentResolver.openFileDescriptor(uri, "r", null)
            val fd = pfd?.fileDescriptor
            val inputStream = FileInputStream(fd)
            val cacheFile = File(context.cacheDir, "$idName.mp4")
            if (cacheFile.exists() && cacheFile.totalSpace > 0) return cacheFile
            val output = BufferedOutputStream(FileOutputStream(cacheFile))
            val bytes = ByteArray(2048)
            var read = inputStream.read(bytes, 0, bytes.size)
            while (read != -1) {
                output.write(bytes, 0, read)
                output.flush()
                read = inputStream.read(bytes, 0, bytes.size)
            }
            inputStream.close()
            output.close()
            pfd?.close()
            return cacheFile
        } catch (e: Exception) {
            if (uri.path.isNullOrEmpty()) {
                File(uri.toString())
            } else {
                File(uri.path ?: "")
            }
        }
    }

}