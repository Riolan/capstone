package com.example.sampleviewer.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.view.View
import androidx.core.content.ContextCompat
import com.example.myapplication.R // Adjust R import

class BoundingBoxView @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null, defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private val boxPaint = Paint().apply {
        style = Paint.Style.STROKE
        color = ContextCompat.getColor(context, R.color.purple_700) // Define color in colors.xml
        strokeWidth = 4f // Adjust stroke width as needed
        isAntiAlias = true
    }

    // Store normalized coordinates (0.0 to 1.0)
    private var normalizedRect: RectF? = null

    /**
     * Sets the bounding box coordinates.
     * Expects coordinates normalized relative to the view's dimensions (0.0 to 1.0).
     * If any coordinate is null, the box will not be drawn.
     */
    fun setBoundingBox(x: Int?, y: Int?, w: Int?, h: Int?, imageWidth: Int, imageHeight: Int) {
        if (x != null && y != null && w != null && h != null && imageWidth > 0 && imageHeight > 0) {
            // Normalize coordinates based on the original image dimensions
            // Assuming x,y,w,h are relative to the original image size passed from ESP32
            val normX = x.toFloat() / imageWidth
            val normY = y.toFloat() / imageHeight
            val normW = w.toFloat() / imageWidth
            val normH = h.toFloat() / imageHeight

            // Create RectF using normalized values (left, top, right, bottom)
            normalizedRect = RectF(normX, normY, normX + normW, normY + normH)
            visibility = VISIBLE // Make view visible
        } else {
            normalizedRect = null
            visibility = GONE // Hide view if no valid box
        }
        invalidate() // Request redraw
    }

    /** Clears the bounding box and hides the view. */
    fun clearBoundingBox() {
        normalizedRect = null
        visibility = GONE
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        normalizedRect?.let { normRect ->
            // Scale normalized coordinates to the current view's dimensions
            val viewWidth = width.toFloat()
            val viewHeight = height.toFloat()

            val actualRect = RectF(
                normRect.left * viewWidth,
                normRect.top * viewHeight,
                normRect.right * viewWidth,
                normRect.bottom * viewHeight
            )
            canvas.drawRect(actualRect, boxPaint)
        }
    }
}