package com.chiistudio.camerandk.utils

import android.content.Context
import android.content.res.AssetManager
import android.graphics.Point
import android.graphics.PointF
import java.io.BufferedReader
import java.io.IOException
import java.util.Arrays
import kotlin.math.pow
import kotlin.math.roundToInt
import kotlin.math.sqrt

class CurveFilter {
    private var rgbCompositeControlPoints: Array<PointF>?= null
    private var redControlPoints: Array<PointF>?= null
    private var greenControlPoints: Array<PointF>?= null
    private var blueControlPoints: Array<PointF>?= null

    var rgbCompositeCurve: ArrayList<Float>? = null
    var redCurve: ArrayList<Float>? = null
    var greenCurve: ArrayList<Float>? = null
    var blueCurve: ArrayList<Float>? = null

    fun loadCurve(context: Context, path: String): CurveFilter {
        val defaultCurvePoints = arrayOf(PointF(0.0f, 0.0f), PointF(0.5f, 0.5f), PointF(1.0f, 1.0f))

        val curves = readCurve(context, path)
        if (curves.isEmpty()) return this
        rgbCompositeControlPoints = curves[0].toTypedArray()
        redControlPoints = curves[1].toTypedArray()
        greenControlPoints = curves[2].toTypedArray()
        blueControlPoints = curves[3].toTypedArray()

        rgbCompositeCurve = createSplineCurve(rgbCompositeControlPoints)
        redCurve = createSplineCurve(redControlPoints)
        greenCurve = createSplineCurve(greenControlPoints)
        blueCurve = createSplineCurve(blueControlPoints)
        return this
    }


    private fun readCurve(context: Context, filePath: String): ArrayList<ArrayList<PointF>> {
        val assetManager: AssetManager = context.assets
        val curves = ArrayList<ArrayList<PointF>>()
        try {
            assetManager.open(filePath).bufferedReader().use {
                val totalCurve = readShort(it)
                val pointRate = 1.0f / 255f
                for (i in 0 until totalCurve) {
                    val pointCount = readShort(it)
                    val points = arrayListOf<PointF>()
                    for (j in 0 until pointCount) {
                        val y = readShort(it)
                        val x = readShort(it)
                        points.add(PointF(x + pointRate, y + pointRate))
                    }
                    curves.add(points)
                }
                it.close()
            }

        } catch (e: Exception) {

        }
        return curves
    }

    
    private fun createSplineCurve(points: Array<PointF>?): ArrayList<Float>? {
        if (points == null || points.isEmpty()) {
            return null
        }

        // Sort the array
        val pointsSorted = points.clone()
        Arrays.sort(
            pointsSorted
        ) { point1, point2 ->
            when {
                point1.x < point2.x -> {
                    -1
                }
                point1.x > point2.x -> {
                    1
                }
                else -> {
                    0
                }
            }
        }

        // Convert from (0, 1) to (0, 255).
        val convertedPoints = arrayOfNulls<Point>(pointsSorted.size)
        for (i in points.indices) {
            val point = pointsSorted[i]
            convertedPoints[i] = Point(
                (point.x * 255).toInt(),
                (point.y * 255).toInt()
            )
        }
        val splinePoints = createSplineCurve2(convertedPoints)

        // If we have a first point like (0.3, 0) we'll be missing some points at the beginning
        // that should be 0.
        val firstSplinePoint = splinePoints!![0]
        if (firstSplinePoint!!.x > 0) {
            for (i in firstSplinePoint.x downTo 0) {
                splinePoints.add(0, Point(i, 0))
            }
        }

        // Insert points similarly at the end, if necessary.
        val lastSplinePoint = splinePoints[splinePoints.size - 1]
        if (lastSplinePoint!!.x < 255) {
            for (i in lastSplinePoint.x + 1..255) {
                splinePoints.add(Point(i, 255))
            }
        }

        // Prepare the spline points.
        val preparedSplinePoints = ArrayList<Float>(
            splinePoints.size
        )
        for (newPoint in splinePoints) {
            val origPoint = Point(newPoint!!.x, newPoint.x)
            var distance =
                sqrt(
                    (origPoint.x - newPoint.x).toDouble().pow(2.0) + (origPoint.y - newPoint.y).toDouble()
                        .pow(2.0)
                )
                    .toFloat()
            if (origPoint.y > newPoint.y) {
                distance = -distance
            }
            preparedSplinePoints.add(distance)
        }
        return preparedSplinePoints
    }

    private fun createSplineCurve2(points: Array<Point?>): ArrayList<Point?>? {
        val sdA = createSecondDerivative(points)

        val n = sdA!!.size
        if (n < 1) {
            return null
        }
        val sd = DoubleArray(n)

        for (i in 0 until n) {
            sd[i] = sdA[i]
        }
        val output = ArrayList<Point?>(n + 1)
        for (i in 0 until n - 1) {
            val cur = points[i]
            val next = points[i + 1]
            for (x in cur!!.x until next!!.x) {
                val t = (x - cur.x).toDouble() / (next.x - cur.x)
                val a = 1 - t
                val h = (next.x - cur.x).toDouble()
                var y =
                    a * cur.y + t * next.y + h * h / 6 * ((a * a * a - a) * sd[i] + (t * t * t - t) * sd[i + 1])
                if (y > 255.0) {
                    y = 255.0
                } else if (y < 0.0) {
                    y = 0.0
                }
                output.add(Point(x, y.roundToInt().toInt()))
            }
        }

        // If the last point is (255, 255) it doesn't get added.
        if (output.size == 255) {
            output.add(points[points.size - 1])
        }
        return output
    }

    private fun createSecondDerivative(points: Array<Point?>): ArrayList<Double>? {
        val n = points.size
        if (n <= 1) {
            return null
        }
        val matrix = Array(n) {
            DoubleArray(
                3
            )
        }
        val result = DoubleArray(n)
        matrix[0][1] = 1.0
        // What about matrix[0][1] and matrix[0][0]? Assuming 0 for now (Brad L.)
        matrix[0][0] = 0.0
        matrix[0][2] = 0.0
        for (i in 1 until n - 1) {
            val P1 = points[i - 1]
            val P2 = points[i]
            val P3 = points[i + 1]
            matrix[i][0] = (P2!!.x - P1!!.x).toDouble() / 6.0
            matrix[i][1] = (P3!!.x - P1.x).toDouble() / 3.0
            matrix[i][2] = (P3.x - P2.x).toDouble() / 6.0
            result[i] =
                (P3.y - P2.y).toDouble() / (P3.x - P2.x) - (P2.y - P1.y).toDouble() / (P2.x - P1.x)
        }

        // What about result[0] and result[n-1]? Assuming 0 for now (Brad L.)
        result[0] = 0.0
        result[n - 1] = 0.0
        matrix[n - 1][1] = 1.0
        // What about matrix[n-1][0] and matrix[n-1][2]? For now, assuming they are 0 (Brad L.)
        matrix[n - 1][0] = 0.0
        matrix[n - 1][2] = 0.0

        // solving pass1 (up->down)
        for (i in 1 until n) {
            val k = matrix[i][0] / matrix[i - 1][1]
            matrix[i][1] -= k * matrix[i - 1][2]
            matrix[i][0] = 0.0
            result[i] -= k * result[i - 1]
        }
        // solving pass2 (down->up)
        for (i in n - 2 downTo 0) {
            val k = matrix[i][2] / matrix[i + 1][1]
            matrix[i][1] -= k * matrix[i + 1][0]
            matrix[i][2] = 0.0
            result[i] -= k * result[i + 1]
        }
        val output = ArrayList<Double>(n)
        for (i in 0 until n) output.add(result[i] / matrix[i][1])
        return output
    }
    
    @Throws(IOException::class)
    private fun readShort(input: BufferedReader): Short {
        return (input.read() shl 8 or input.read()).toShort()
    }
}