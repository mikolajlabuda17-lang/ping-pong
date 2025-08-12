package com.example.game

import android.graphics.Canvas
import android.graphics.Paint

class Player(
    startX: Float,
    startY: Float,
    val radiusPx: Float,
    color: Int,
    private val speedPxPerSecond: Float
) {
    private val paint: Paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        this.color = color
        style = Paint.Style.FILL
    }

    var x: Float = startX
        private set
    var y: Float = startY
        private set

    val radius: Float get() = radiusPx

    fun setPosition(newX: Float, newY: Float) {
        x = newX
        y = newY
    }

    fun updateTowards(targetX: Float, targetY: Float, deltaSeconds: Double) {
        val dx = targetX - x
        val dy = targetY - y
        val distance = kotlin.math.hypot(dx.toDouble(), dy.toDouble())
        if (distance < 1e-3) return

        val maxStep = speedPxPerSecond * deltaSeconds
        if (distance <= maxStep) {
            x = targetX
            y = targetY
        } else {
            val nx = dx / distance
            val ny = dy / distance
            x = (x + nx * maxStep).toFloat()
            y = (y + ny * maxStep).toFloat()
        }
    }

    fun draw(canvas: Canvas) {
        canvas.drawCircle(x, y, radiusPx, paint)
    }
}