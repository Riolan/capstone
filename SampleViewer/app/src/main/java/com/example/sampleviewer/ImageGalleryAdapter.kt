package com.example.sampleviewer // Adjust package name

import android.graphics.BitmapFactory
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.ImageView
import android.widget.TextView
import android.widget.Toast
import androidx.lifecycle.findViewTreeLifecycleOwner
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.example.myapplication.R // Adjust R import
import com.example.sampleviewer.bt.BleManager
import com.example.sampleviewer.ui.BoundingBoxView // Import custom view
import java.io.File
import java.io.FileInputStream
import kotlinx.coroutines.*


class ImageGalleryAdapter(): ListAdapter<Event, ImageGalleryAdapter.EventImageViewHolder>(EventDiffCallback()) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): EventImageViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.list_item_event_image, parent, false)
        val bleManager = BleManager.getInstance(parent.context.applicationContext)
        return EventImageViewHolder(view, bleManager)
    }

    override fun onBindViewHolder(holder: EventImageViewHolder, position: Int) {
        val event = getItem(position)
        holder.bind(event)
    }

    // ViewHolder class using findViewById
    class EventImageViewHolder(
        itemView: View,
        private val bleManager: BleManager
    ) : RecyclerView.ViewHolder(itemView) {

        // Find views
        private val imageView: ImageView = itemView.findViewById(R.id.item_image_view)
        private val timestampText: TextView = itemView.findViewById(R.id.item_timestamp_text)
        private val downloadButton: Button = itemView.findViewById(R.id.item_button_download)
        private val animalText: TextView = itemView.findViewById(R.id.item_animal_text) // New
        private val uuidText: TextView = itemView.findViewById(R.id.item_uuid_text)     // New
        private val bboxText: TextView = itemView.findViewById(R.id.item_bbox_text)     // New
        private val bboxView: BoundingBoxView = itemView.findViewById(R.id.item_bbox_view) // New

        private var currentEvent: Event? = null
        private var imageLoadingJob: Job? = null

        init {
            downloadButton.setOnClickListener {
                currentEvent?.let { event ->
                    // ... (Download logic as before) ...
                    if (event.imagePath != null && File(event.imagePath).exists()) {
                        Toast.makeText(itemView.context, "Image already downloaded", Toast.LENGTH_SHORT).show()
                        return@setOnClickListener
                    }
                    itemView.findViewTreeLifecycleOwner()?.lifecycleScope?.launch {
                        Log.d("Adapter", "Requesting image for Node ${event.nodeId}, Event ${event.id}")
                        try {
                            val (_, _, message) = bleManager.requestImageForEvent(event.nodeId, event.id)
                            Toast.makeText(itemView.context, message, Toast.LENGTH_SHORT).show()
                        } catch (e: Exception) { /* ... error handling ... */ }
                    } ?: run { /* ... error handling ... */ }
                } ?: run { Log.w("Adapter", "Download clicked but currentEvent is null") }
            }
        }

        fun bind(event: Event) {
            currentEvent = event

            // --- Bind Text Data ---
            timestampText.text = event.timestamp
            animalText.text = event.description // Assuming description contains animal info
            uuidText.text = "UUID: ${event.id}" // Display UUID
            // Format BBox text (handle nulls)
            val bboxString = if (event.bboxX != null) {
                "BBox: [${event.bboxX}, ${event.bboxY}, ${event.bboxW}, ${event.bboxH}]"
            } else {
                "BBox: N/A"
            }
            bboxText.text = bboxString
            // --- End Bind Text Data ---

            imageLoadingJob?.cancel()

            val imagePath = event.imagePath
            var imageExists = false
            var loadedBitmap: android.graphics.Bitmap? = null // Store loaded bitmap

            if (imagePath != null) {
                val imageFile = File(imagePath)
                imageExists = imageFile.exists()
            }

            // Update download button state
            downloadButton.isEnabled = !imageExists
            downloadButton.text = if (imageExists) "Download" else "DOWNLOADED"

            // --- Manual Image Loading & BBox Drawing ---
            bboxView.clearBoundingBox() // Clear previous box
            if (imageExists) {
                imageView.setImageResource(event.fallbackIconResId) // Set placeholder
                imageLoadingJob = CoroutineScope(Dispatchers.IO).launch {
                    try {
                        val imageFile = File(event.imagePath!!)
                        // Decode bounds first to get dimensions for normalization
                        val options = BitmapFactory.Options().apply { inJustDecodeBounds = true }
                        BitmapFactory.decodeStream(FileInputStream(imageFile), null, options)
                        val imageWidth = options.outWidth
                        val imageHeight = options.outHeight

                        // Now decode the actual bitmap (consider downsampling if needed)
                        // val sampleOptions = BitmapFactory.Options().apply { inSampleSize = calculateInSampleSize(options, reqWidth, reqHeight) } // Implement calculateInSampleSize if needed
                        loadedBitmap = BitmapFactory.decodeStream(FileInputStream(imageFile)) // Decode full bitmap

                        withContext(Dispatchers.Main) {
                            if (loadedBitmap != null) {
                                imageView.setImageBitmap(loadedBitmap)
                                // --- Draw Bounding Box ---
                                if (imageWidth > 0 && imageHeight > 0) { // Check valid dimensions
                                    bboxView.setBoundingBox(
                                        event.bboxX, event.bboxY, event.bboxW, event.bboxH,
                                        imageWidth, imageHeight // Pass original image dimensions
                                    )
                                } else {
                                    bboxView.clearBoundingBox()
                                }
                                // --- End Bounding Box ---
                            } else {
                                imageView.setImageResource(event.fallbackIconResId)
                                bboxView.clearBoundingBox()
                            }
                        }
                    } catch (e: Exception) {
                        Log.e("Adapter", "Error loading image: ${event.imagePath}", e)
                        withContext(Dispatchers.Main) {
                            imageView.setImageResource(R.drawable.baseline_error_outline_24)
                            bboxView.clearBoundingBox()
                        }
                    }
                }
            } else {
                // No image path or file doesn't exist
                imageView.setImageResource(event.fallbackIconResId)
                bboxView.clearBoundingBox() // Ensure box is hidden
            }
            // --- End Manual Image Loading & BBox Drawing ---
        }
    }
    // DiffUtil Callback (same as before)
    class EventDiffCallback : DiffUtil.ItemCallback<Event>() {
        override fun areItemsTheSame(oldItem: Event, newItem: Event): Boolean {
            return oldItem.id == newItem.id // Use Event UUID
        }
        override fun areContentsTheSame(oldItem: Event, newItem: Event): Boolean {
            return oldItem == newItem // Compare all fields
        }
    }
}