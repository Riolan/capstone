package com.example.sampleviewer // Adjust package name

import android.app.Dialog
import android.graphics.Bitmap // Import Bitmap
import android.graphics.BitmapFactory
import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.ImageView
import android.widget.TextView
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.lifecycleScope
import com.example.myapplication.R // Adjust R import
import com.example.sampleviewer.ui.BoundingBoxView // Import custom view
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileInputStream
import java.lang.Exception // Import Exception

class ImageDetailDialogFragment : DialogFragment() {

    // Arguments keys (keep as before)
    companion object {
        private const val ARG_EVENT_ID = "event_id"
        private const val ARG_NODE_ID = "node_id"
        private const val ARG_DESCRIPTION = "description"
        private const val ARG_TIMESTAMP = "timestamp"
        private const val ARG_IMAGE_PATH = "image_path"
        private const val ARG_BBOX_X = "bbox_x"
        private const val ARG_BBOX_Y = "bbox_y"
        private const val ARG_BBOX_W = "bbox_w"
        private const val ARG_BBOX_H = "bbox_h"
        private const val ARG_DEVICE_ID = "device_id"
        private const val ARG_FALLBACK_RES = "fallback_res"

        fun newInstance(event: Event): ImageDetailDialogFragment {
            val fragment = ImageDetailDialogFragment()
            val args = Bundle().apply {
                putString(ARG_EVENT_ID, event.id)
                putString(ARG_NODE_ID, event.nodeId)
                putString(ARG_DESCRIPTION, event.description)
                putString(ARG_TIMESTAMP, event.timestamp)
                putString(ARG_IMAGE_PATH, event.imagePath)
                event.bboxX?.let { putInt(ARG_BBOX_X, it) }
                event.bboxY?.let { putInt(ARG_BBOX_Y, it) }
                event.bboxW?.let { putInt(ARG_BBOX_W, it) }
                event.bboxH?.let { putInt(ARG_BBOX_H, it) }
                putString(ARG_DEVICE_ID, event.deviceId)
                putInt(ARG_FALLBACK_RES, event.fallbackIconResId)
            }
            fragment.arguments = args
            return fragment
        }
    }

    // Views
    private lateinit var imageView: ImageView
    private lateinit var bboxView: BoundingBoxView
    private lateinit var descriptionText: TextView
    private lateinit var timestampText: TextView
    private lateinit var uuidText: TextView
    private lateinit var deviceIdText: TextView
    private lateinit var bboxText: TextView
    private lateinit var closeButton: Button

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        // Inflate the custom layout
        val view = inflater.inflate(R.layout.dialog_image_detail, container, false)

        // Find views
        imageView = view.findViewById(R.id.dialog_image_view)
        bboxView = view.findViewById(R.id.dialog_bbox_view)
        descriptionText = view.findViewById(R.id.dialog_description_text)
        timestampText = view.findViewById(R.id.dialog_timestamp_text)
        uuidText = view.findViewById(R.id.dialog_uuid_text)
        deviceIdText = view.findViewById(R.id.dialog_deviceid_text)
        bboxText = view.findViewById(R.id.dialog_bbox_text)
        closeButton = view.findViewById(R.id.dialog_close_button)

        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        // Retrieve data from arguments (as before)
        val args = requireArguments()
        val eventId = args.getString(ARG_EVENT_ID) ?: "N/A"
        val description = args.getString(ARG_DESCRIPTION) ?: "No description"
        val timestamp = args.getString(ARG_TIMESTAMP) ?: "No timestamp"
        val imagePath = args.getString(ARG_IMAGE_PATH)
        val bboxX = args.getInt(ARG_BBOX_X, -1).takeIf { it != -1 }
        val bboxY = args.getInt(ARG_BBOX_Y, -1).takeIf { it != -1 }
        val bboxW = args.getInt(ARG_BBOX_W, -1).takeIf { it != -1 }
        val bboxH = args.getInt(ARG_BBOX_H, -1).takeIf { it != -1 }
        val deviceId = args.getString(ARG_DEVICE_ID) ?: "N/A"
        val fallbackRes = args.getInt(ARG_FALLBACK_RES, R.drawable.baseline_camera_outdoor_24)

        // Populate TextViews (as before)
        descriptionText.text = description
        timestampText.text = timestamp
        uuidText.text = "UUID: $eventId"
        deviceIdText.text = "DevID: $deviceId"
        val bboxString = if (bboxX != null) { "BBox: [$bboxX, $bboxY, $bboxW, $bboxH]" } else { "BBox: N/A" }
        bboxText.text = bboxString

        // Load image and draw bounding box using manual method
        loadImageAndBboxManually(imagePath, fallbackRes, bboxX, bboxY, bboxW, bboxH)

        // Set close button listener
        closeButton.setOnClickListener {
            dismiss() // Close the dialog
        }
    }

    // *** UPDATED: Manual Image Loading and BBox Drawing ***
    private fun loadImageAndBboxManually(
        imagePath: String?,
        fallbackRes: Int,
        bboxX: Int?, bboxY: Int?, bboxW: Int?, bboxH: Int?
    ) {
        bboxView.clearBoundingBox() // Clear previous box
        imageView.setImageResource(fallbackRes) // Set placeholder immediately

        if (imagePath != null) {
            // Use lifecycleScope tied to the DialogFragment's view lifecycle
            viewLifecycleOwner.lifecycleScope.launch {
                var bitmapToShow: Bitmap? = null
                var imageWidth = 0
                var imageHeight = 0

                // Perform file reading and decoding in background
                withContext(Dispatchers.IO) {
                    try {
                        val imageFile = File(imagePath)
                        if (imageFile.exists()) {
                            Log.d("ImageDetailDialog", "Loading image manually: $imagePath")
                            // 1. Decode bounds first
                            val options = BitmapFactory.Options().apply { inJustDecodeBounds = true }
                            BitmapFactory.decodeStream(FileInputStream(imageFile), null, options)
                            imageWidth = options.outWidth
                            imageHeight = options.outHeight

                            // 2. Decode actual bitmap (consider adding inSampleSize calculation here if needed)
                            // val sampleOptions = BitmapFactory.Options().apply { inSampleSize = 2 } // Example
                            bitmapToShow = BitmapFactory.decodeStream(FileInputStream(imageFile)) // Decode full

                            if (bitmapToShow != null) {
                                Log.d("ImageDetailDialog", "Manual decode success ($imageWidth x $imageHeight)")
                            } else {
                                Log.e("ImageDetailDialog", "Manual decode failed: decodeStream returned null")
                            }
                        } else {
                            Log.w("ImageDetailDialog", "Manual load: Image file not found: $imagePath")
                        }
                    } catch (e: Exception) {
                        Log.e("ImageDetailDialog", "Manual load: Error loading image", e)
                    }
                } // End background context

                // Back on Main thread to update UI
                if (bitmapToShow != null) {
                    imageView.setImageBitmap(bitmapToShow)
                    // Draw bounding box if dimensions are valid
                    if (imageWidth > 0 && imageHeight > 0) {
                        bboxView.setBoundingBox(bboxX, bboxY, bboxW, bboxH, imageWidth, imageHeight)
                    } else {
                        bboxView.clearBoundingBox()
                    }
                } else {
                    // Show error/placeholder if bitmap is null
                    imageView.setImageResource(R.drawable.baseline_error_outline_24) // Use error drawable
                    bboxView.clearBoundingBox()
                }
            } // End coroutine launch
        } else {
            Log.d("ImageDetailDialog", "No image path provided.")
            // Placeholder already set
        }
    }
    // *** End Manual Loading ***


    // Optional: Make the dialog wider
    override fun onStart() {
        super.onStart()
        dialog?.window?.setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
    }

}
