package com.example.sampleviewer // Adjust package name as needed

import android.content.Context
import android.os.Bundle
import android.util.Log
import androidx.fragment.app.Fragment
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog // Use androidx.appcompat.app.AlertDialog
import androidx.lifecycle.lifecycleScope
import com.example.myapplication.R // Import your R file (adjust if needed)
import com.example.sampleviewer.bt.BleManager
import com.example.sampleviewer.database.DatabaseHelper
import com.google.android.material.button.MaterialButton
import kotlinx.coroutines.* // Import coroutines
import java.util.Random // For generating fake data
import java.util.concurrent.TimeUnit // For timestamps

class DevToolsFragment : Fragment() {

    private lateinit var databaseHelper: DatabaseHelper
    private lateinit var outputTextView: TextView
    private lateinit var clearUsersButton: MaterialButton
    private lateinit var clearDetectionsButton: MaterialButton
    private lateinit var viewUsersButton: MaterialButton
    private lateinit var addFakeDetectionsButton: MaterialButton // Added Button reference
    private lateinit var callTestNotificationButton: MaterialButton // Added Button reference

    private lateinit var callGetImageButton: MaterialButton // Added Button reference

    private lateinit var viewDetectionsButton: MaterialButton

    // Coroutine scope for background tasks
    private val fragmentScope = CoroutineScope(Dispatchers.Main + SupervisorJob())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // TODO: Ensure this fragment is only created for debug builds

        databaseHelper = DatabaseHelper(requireContext())
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        // Inflate the layout for this fragment
        return inflater.inflate(R.layout.fragment_dev_tools, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        outputTextView = view.findViewById(R.id.textview_dev_output)
        clearUsersButton = view.findViewById(R.id.button_clear_users)
        clearDetectionsButton = view.findViewById(R.id.button_clear_detections)
        viewUsersButton = view.findViewById(R.id.button_view_users)
        addFakeDetectionsButton = view.findViewById(R.id.button_add_fake_detections) // Find the new button
        callTestNotificationButton = view.findViewById(R.id.button_call_test_notification) // Find the new button
        callGetImageButton  = view.findViewById(R.id.button_get_image)
        viewDetectionsButton = view.findViewById(R.id.button_view_detections)

        setupButtonClickListeners()
        val safeContext: Context = requireContext()

        // Initial message
        outputTextView.text = "Press a button to perform an action."
    }

    private fun setupButtonClickListeners() {
        clearUsersButton.setOnClickListener {
            showConfirmationDialog("Clear ALL Users?", "This action cannot be undone.") {
                performDbAction(
                    action = { databaseHelper.clearUsersTable() },
                    successMessage = "Users table cleared.",
                    failureMessage = "Failed to clear Users table."
                )
            }
        }

        clearDetectionsButton.setOnClickListener {
            showConfirmationDialog("Clear ALL Detections?", "This action cannot be undone.") {
                performDbAction(
                    action = { databaseHelper.clearDetectionsTable() },
                    successMessage = "Detections table cleared.",
                    failureMessage = "Failed to clear Detections table."
                )
            }
        }

        viewUsersButton.setOnClickListener {
            viewUsers()
        }

        addFakeDetectionsButton.setOnClickListener {
            addFakeDetections() // Call the function to add fake data
        }

        callTestNotificationButton.setOnClickListener {

            val title = "Task Complete"
            val message = "Fake data generation finished successfully."
            Log.i("Notifications", title+message)
            NotificationHelper.sendNotification(requireContext(), title, message)
        }


        callGetImageButton.setOnClickListener {
            val safeContext = requireContext()
            if (BleManager.getInstance(safeContext).isConnected()) {
                val selectedNodeId: String = "1" // Get this from user selection
                val command = "REQUEST_IMAGE;${selectedNodeId}\n"
                val success = BleManager.getInstance(safeContext).sendData(command.toByteArray(Charsets.UTF_8))
                if (!success) {
                    Log.e("NodesFragment", "Failed to send REQUEST_NODES command.")
                }
            } else {
                Log.w("NodesFragment", "Cannot request IMAGE, not connected.")
            }
        }

        viewDetectionsButton.setOnClickListener {
            viewDetections() // Call the function to view detections
        }

    }

    // --- Function to View Detections ---
    private fun viewDetections() {
        viewLifecycleOwner.lifecycleScope.launch {
            outputTextView.text = "Loading detection events..." // Show loading state
            val detectionsResult: Result<List<Event>> = withContext(Dispatchers.IO) {
                try {
                    Result.success(databaseHelper.getAllDetectionEvents()) // Call DB helper
                } catch (e: Exception) {
                    Log.e("DevToolsFragment", "viewDetections: Exception loading events", e)
                    Result.failure(e)
                }
            }

            // Back on Main thread to update UI
            if (detectionsResult.isSuccess) {
                val events = detectionsResult.getOrNull() ?: emptyList()
                if (events.isNotEmpty()) {
                    // Format the list for display in the TextView
                    val formattedText = StringBuilder("Detection Events (${events.size}):\n\n")
                    events.forEach { event ->
                        formattedText.append("ID: ${event.id}\n")
                        formattedText.append("  Desc: ${event.description}\n")
                        formattedText.append("  Time: ${event.timestamp}\n")
                        formattedText.append("  Img Path: ${event.imagePath ?: "N/A"}\n")
                        formattedText.append("--------------------\n")
                    }
                    outputTextView.text = formattedText.toString()
                } else {
                    outputTextView.text = "No detection events found in the database."
                }
            } else {
                val errorMsg = "Error loading detections: ${detectionsResult.exceptionOrNull()?.message}"
                outputTextView.text = errorMsg
                Toast.makeText(requireContext(), errorMsg, Toast.LENGTH_LONG).show()
            }
        }
    }
    // --- End Function to View Detections ---

    // Helper function to show confirmation dialog
    private fun showConfirmationDialog(title: String, message: String, onConfirm: () -> Unit) {
        AlertDialog.Builder(requireContext())
            .setTitle(title)
            .setMessage(message)
            .setPositiveButton("Confirm") { dialog, _ ->
                onConfirm()
                dialog.dismiss()
            }
            .setNegativeButton("Cancel") { dialog, _ ->
                dialog.dismiss()
            }
            .show()
    }

    // Helper function to run DB action using viewLifecycleOwner.lifecycleScope
    private fun performDbAction(action: suspend () -> Boolean, successMessage: String, failureMessage: String) {
        // Ensure viewLifecycleOwner is accessible; launch starts only if view is active
        viewLifecycleOwner.lifecycleScope.launch { // Use launch directly on main thread first for UI feedback
            outputTextView.text = "Processing..." // Show immediate feedback
            val success = withContext(Dispatchers.IO) { // Switch to IO for the action
                try {
                    action() // Execute the suspend function
                } catch (e: Exception) {
                    Log.e("DevToolsFragment", "performDbAction: Exception in IO block", e)
                    false
                }
            }
            // Back on the Main thread automatically after withContext finishes
            val message = if (success) successMessage else failureMessage
            try {
                outputTextView.text = message
                Toast.makeText(requireContext(), message, Toast.LENGTH_SHORT).show()
            } catch (uiException: Exception) {
                Log.e("DevToolsFragment", "performDbAction: Exception during UI update", uiException)
                Toast.makeText(requireContext(), "UI Update Error: ${uiException.message}", Toast.LENGTH_LONG).show()
            }
        }
    }

    // Function to view users using viewLifecycleOwner.lifecycleScope
    private fun viewUsers() {
        viewLifecycleOwner.lifecycleScope.launch {
            outputTextView.text = "Loading users..."
            val usersResult: Result<List<String>> = withContext(Dispatchers.IO) {
                try {
                    Result.success(databaseHelper.getAllUsers())
                } catch (e: Exception) {
                    Log.e("DevToolsFragment", "viewUsers: Exception loading users", e)
                    Result.failure(e)
                }
            }

            // Back on Main thread
            if (usersResult.isSuccess) {
                val users = usersResult.getOrNull()
                if (!users.isNullOrEmpty()) {
                    outputTextView.text = "Registered Users:\n${users.joinToString("\n")}"
                } else {
                    outputTextView.text = "No users found in the database."
                }
            } else {
                outputTextView.text = "Error loading users: ${usersResult.exceptionOrNull()?.message}"
                Toast.makeText(requireContext(), "Error loading users", Toast.LENGTH_SHORT).show()
            }
        }
    }

    private fun addFakeDetections() {
        val numberOfDetections = 5 // Add 5 fake detections

        performDbAction(
            action = {
                // This runs on the IO thread
                val random = Random()
                val possibleAnimals = listOf("Dog", "Cat", "Bird", "Squirrel")
                val possibleCameras = listOf("Front Yard Cam", "Back Porch", "Driveway Cam", "Garden Cam")
                var allSuccessful = true

                // SimpleDateFormat for creating timestamp strings (can use other methods too)
                // Storing as milliseconds string is often easier for sorting in DB
                // val timestampFormat = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())

                for (i in 1..numberOfDetections) {
                    val animal = possibleAnimals[random.nextInt(possibleAnimals.size)]
                    val camera = possibleCameras[random.nextInt(possibleCameras.size)]

                    // Generate timestamp between now and 3 days ago
                    val timestampMillis = System.currentTimeMillis() - TimeUnit.HOURS.toMillis(random.nextInt(72).toLong())

                    // ** Format timestamp as String (e.g., milliseconds) **
                    val timestampString = timestampMillis.toString()
                    // Or if you prefer formatted string (less ideal for sorting):
                    // val timestampString = timestampFormat.format(Date(timestampMillis))

                    // Generate a fake image path
                    val imagePath = "/storage/emulated/0/DCIM/Camera/fake_${animal}_${timestampMillis}.jpg"

                    // Call the correct DatabaseHelper method
                    val success = try {
                        databaseHelper.insertDetectionRecord(i.toString() + timestampString ,animal, timestampString, camera, imagePath, 20, 30, 80, 60, "SAMPLE-DEVICE-ID")
                    } catch (e: Exception) {
                        // Log specific insertion error if needed from fragment side
                        Log.e("DevToolsFragment", "Error calling insertDetectionRecord", e)
                        false
                    }


                    if (!success) {
                        allSuccessful = false
                        Log.w("DevToolsFragment", "Failed to insert fake record for $animal")
                        // Optionally break the loop if one fails
                        // break
                    }
                    // Small delay to potentially ensure different timestamps if needed, though unlikely necessary with millis
                    // Thread.sleep(10)
                }
                allSuccessful // Return true if all insertions were attempted successfully
            },
            successMessage = "Added $numberOfDetections fake detection records.",
            failureMessage = "Failed to add some or all fake detection data. Check Logs."
        )
    }

    override fun onDestroyView() {
        super.onDestroyView()
        // Cancel coroutines when the view is destroyed to prevent leaks
        fragmentScope.cancel()
    }

}