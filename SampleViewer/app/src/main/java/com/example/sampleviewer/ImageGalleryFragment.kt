package com.example.sampleviewer // Adjust package name

import android.os.Bundle
import android.util.Log
import androidx.fragment.app.Fragment
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import android.widget.Toast // Import Toast
// import androidx.lifecycle.ViewModelProvider // Keep commented for now
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.example.myapplication.R // Adjust R import
import com.example.sampleviewer.database.DatabaseHelper // Import DatabaseHelper
import com.example.sampleviewer.bt.BleManager // Import BleManager
// Remove ViewBinding import
// import com.example.myapplication.databinding.FragmentImageGalleryBinding
import kotlinx.coroutines.Dispatchers // Import Dispatchers
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext // Import withContext

class ImageGalleryFragment : Fragment() {

    // Declare views - initialize in onViewCreated
    private lateinit var galleryRecyclerView: RecyclerView
    private lateinit var galleryEmptyText: TextView
    private lateinit var imageAdapter: ImageGalleryAdapter
    private lateinit var databaseHelper: DatabaseHelper // Add DatabaseHelper instance
    private lateinit var bleManager: BleManager // Hold BleManager instance
    // private lateinit var galleryViewModel: GalleryViewModel // TODO: Introduce a ViewModel

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Initialize DatabaseHelper here, requires context
        databaseHelper = DatabaseHelper(requireContext())
        // Get BleManager instance (pass application context)
        bleManager = BleManager.getInstance(requireContext().applicationContext)
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? { // Return nullable View
        // Inflate the layout the traditional way
        return inflater.inflate(R.layout.fragment_image_library, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        // --- Initialize views using findViewById ---
        galleryRecyclerView = view.findViewById(R.id.gallery_recycler_view)
        galleryEmptyText = view.findViewById(R.id.gallery_empty_text)
        // --- End Initialize views ---

        // TODO: Initialize ViewModel
        // galleryViewModel = ViewModelProvider(this).get(GalleryViewModel::class.java)

        setupRecyclerView()
        setupObservers() // Set up data observation

        // --- Load initial data from Database ---
        loadEventsFromDatabase()
    }

    private fun setupRecyclerView() {
        // *** Create adapter instance WITHOUT the click listener lambda ***
        // The download click is handled inside the adapter's ViewHolder now.
        // If you need clicks on the whole item for navigation, you'd pass a different listener here.
        imageAdapter = ImageGalleryAdapter()

        galleryRecyclerView.apply {
            adapter = imageAdapter
            layoutManager = GridLayoutManager(requireContext(), 3) // Adjust spanCount
        }
    }

    // --- Setup Observers ---
    private fun setupObservers() {
        // Observer for the Database Update Signal from BleManager
        viewLifecycleOwner.lifecycleScope.launch {
            bleManager.databaseUpdatedSignal.collectLatest { timestamp ->
                Log.d("ImageGalleryFragment", "Database update signal received (Timestamp: ${timestamp.get()}). Reloading events.")
                // Reload data from the database when signal received
                loadEventsFromDatabase()
            }
        }

        // Observer for the latest single image event (optional, for immediate UI feedback)
        viewLifecycleOwner.lifecycleScope.launch {
            bleManager.latestEventState.collectLatest { latestImageEvent ->
                if (latestImageEvent != null) {
                    Log.d("ImageGalleryFragment", "Latest image event state updated: ${latestImageEvent.id}")
                    // Optional: Find item in adapter and update specifically?
                    // Or just rely on the DB signal observer to refresh the list.
                    // For simplicity, we rely on the DB signal for now.
                }
            }
        }

        // TODO: Add observer for ViewModel data if/when implemented
    }

    // --- Function to load events from the database ---
    private fun loadEventsFromDatabase() {
        viewLifecycleOwner.lifecycleScope.launch {
            Log.d("ImageGalleryFragment", "Starting/Reloading events from database...")
            // Show loading state? Maybe only if adapter is currently empty.
            // if (imageAdapter.itemCount == 0) {
            //    galleryEmptyText.text = getString(R.string.loading_images)
            //    galleryEmptyText.visibility = View.VISIBLE
            //    galleryRecyclerView.visibility = View.GONE
            // }

            val eventListResult: Result<List<Event>> = withContext(Dispatchers.IO) {
                try { Result.success(databaseHelper.getAllDetectionEvents()) }
                catch (e: Exception) { Log.e("ImageGalleryFragment", "Error loading events", e); Result.failure(e) }
            }

            if (eventListResult.isSuccess) {
                val eventList = eventListResult.getOrNull() ?: emptyList()
                Log.d("ImageGalleryFragment", "Loaded ${eventList.size} events from database.")
                imageAdapter.submitList(eventList) // Update the adapter
                updateEmptyState(eventList.isEmpty()) // Update empty text visibility
            } else {
                Log.e("ImageGalleryFragment", "Failed to load events.")
                Toast.makeText(requireContext(), "Error loading events", Toast.LENGTH_SHORT).show()
                updateEmptyState(true) // Show empty state on error
            }
        }
    }

    // --- Helper to manage empty state visibility ---
    private fun updateEmptyState(isEmpty: Boolean) {
        if (view == null) return // Check if view is available
        galleryEmptyText.text = ("No images found.") // Ensure correct text
        galleryEmptyText.visibility = if (isEmpty) View.VISIBLE else View.GONE
        galleryRecyclerView.visibility = if (isEmpty) View.GONE else View.VISIBLE
    }

}