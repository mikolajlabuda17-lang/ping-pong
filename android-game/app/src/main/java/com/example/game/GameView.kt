package com.example.game

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView

class GameView(context: Context) : SurfaceView(context), Runnable {
    private val surfaceHolder: SurfaceHolder = holder
    private var gameThread: Thread? = null
    @Volatile private var isRunning: Boolean = false

    private val backgroundPaint = Paint().apply { color = Color.WHITE }

    private val player: Player = Player(
        startX = 200f,
        startY = 200f,
        radiusPx = 40f,
        color = Color.BLUE,
        speedPxPerSecond = 400f
    )

    private var targetX: Float = player.x
    private var targetY: Float = player.y

    fun resume() {
        if (isRunning) return
        isRunning = true
        gameThread = Thread(this, "GameThread").also { it.start() }
    }

    fun pause() {
        isRunning = false
        gameThread?.joinSafely()
        gameThread = null
    }

    override fun run() {
        var previousTimeNs = System.nanoTime()
        val targetFps = 60.0
        val targetFrameTimeNs = (1_000_000_000.0 / targetFps).toLong()

        while (isRunning) {
            val currentTimeNs = System.nanoTime()
            val deltaTimeSec = ((currentTimeNs - previousTimeNs).coerceAtMost(100_000_000L)).toDouble() / 1_000_000_000.0
            previousTimeNs = currentTimeNs

            update(deltaTimeSec)
            drawFrame()

            val frameDurationNs = System.nanoTime() - currentTimeNs
            val sleepNs = targetFrameTimeNs - frameDurationNs
            if (sleepNs > 0) {
                try {
                    Thread.sleep(sleepNs / 1_000_000L, (sleepNs % 1_000_000L).toInt())
                } catch (_: InterruptedException) {
                    // ignore
                }
            } else {
                Thread.yield()
            }
        }
    }

    private fun update(deltaSeconds: Double) {
        player.updateTowards(targetX, targetY, deltaSeconds)
        clampPlayerInsideBounds()
    }

    private fun clampPlayerInsideBounds() {
        if (width == 0 || height == 0) return
        val clampedX = player.x.coerceIn(player.radius, width.toFloat() - player.radius)
        val clampedY = player.y.coerceIn(player.radius, height.toFloat() - player.radius)
        player.setPosition(clampedX, clampedY)
    }

    private fun drawFrame() {
        var canvas: Canvas? = null
        try {
            canvas = surfaceHolder.lockCanvas()
            if (canvas != null) {
                canvas.drawColor(Color.WHITE)
                player.draw(canvas)

                // Draw target indicator
                val targetPaint = Paint().apply {
                    color = Color.RED
                    style = Paint.Style.STROKE
                    strokeWidth = 3f
                }
                canvas.drawCircle(targetX, targetY, 16f, targetPaint)
            }
        } finally {
            if (canvas != null) {
                surfaceHolder.unlockCanvasAndPost(canvas)
            }
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE -> {
                targetX = event.x
                targetY = event.y
                return true
            }
        }
        return super.onTouchEvent(event)
    }

    private fun Thread?.joinSafely(timeoutMs: Long = 2_000) {
        try {
            this?.join(timeoutMs)
        } catch (_: InterruptedException) {
            // ignore
        }
    }
}