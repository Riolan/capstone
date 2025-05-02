package com.example.sampleviewer

import android.Manifest // Import Manifest
import android.annotation.SuppressLint
import android.app.Activity // For RESULT_OK
import android.app.AlertDialog
import android.bluetooth.* // Import Bluetooth classes
import android.bluetooth.le.ScanCallback // Import ScanCallback
import android.bluetooth.le.ScanResult // Import ScanResult
import android.bluetooth.le.ScanSettings // Import ScanSettings
import android.content.Context // For BluetoothManager
import android.content.Intent // For enabling Bluetooth
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Handler // Import Handler
import android.os.Looper // Import Looper
import android.util.Log // Import Log
import androidx.fragment.app.Fragment
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ArrayAdapter
import android.widget.ListView
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts // Import ActivityResultContracts
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.example.myapplication.R // Make sure R is imported correctly
import com.example.sampleviewer.bt.BleConnectionListener
import com.example.sampleviewer.bt.BleDataListener
import com.example.sampleviewer.bt.BleManager
import com.example.sampleviewer.database.DatabaseHelper
import com.google.android.material.button.MaterialButton
import java.util.UUID

// Implement the listeners
class HomeFragment : Fragment(), BleDataListener, BleConnectionListener {

    // --- UI Elements ---
    private lateinit var connectionStatusLabel: TextView
    private lateinit var batteryStatusLabel: TextView
    private lateinit var signalStatusLabel: TextView
    private lateinit var connectButton: MaterialButton
    private lateinit var eventsRecyclerView: RecyclerView
    private lateinit var eventAdapter: EventAdapter

    // --- Database and Login State ---
    private lateinit var databaseHelper: DatabaseHelper
    private lateinit var sharedPreferences: SharedPreferences // For login state

    // --- BLE Related Variables ---
    private lateinit var bluetoothManager: BluetoothManager
    private var bluetoothAdapter: BluetoothAdapter? = null
    private val bluetoothLeScanner by lazy { bluetoothAdapter?.bluetoothLeScanner }
    private var isScanning = false
    private val scanHandler = Handler(Looper.getMainLooper())
    private val SCAN_PERIOD: Long = 10000 // Stops scanning after 10 seconds.

    // Variables for the Scan Dialog using custom layout
    private var scanDialog: AlertDialog? = null
    private lateinit var devicesListAdapter: ArrayAdapter<String>
    private val discoveredDevicesList = mutableListOf<BluetoothDevice>()
    private var dialogProgressBar: ProgressBar? = null
    private var dialogEmptyTextView: TextView? = null
    private var dialogListView: ListView? = null

    // --- Permission Handling ---
    private val requiredPermissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        arrayOf(
            Manifest.permission.BLUETOOTH_SCAN,
            Manifest.permission.BLUETOOTH_CONNECT
        )
    } else {
        // Needs Location for scanning pre-Android 12
        arrayOf(
            Manifest.permission.BLUETOOTH,
            Manifest.permission.BLUETOOTH_ADMIN,
            Manifest.permission.ACCESS_FINE_LOCATION // IMPORTANT
        )
    }

    private val requestPermissionsLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { permissions ->
            if (permissions.all { it.value }) {
                Log.d("Permissions", "All required permissions granted.")
                // Optional: You could try the action again here, e.g., re-trigger button logic
                // For simplicity, we'll just let the user click again for now.
                Toast.makeText(requireContext(), "Permissions granted. Try connecting again.", Toast.LENGTH_SHORT).show()
            } else {
                Log.e("Permissions", "One or more permissions were denied.")
                Toast.makeText(requireContext(), "Bluetooth permissions are required.", Toast.LENGTH_LONG).show()
            }
        }

    // Launcher for enabling Bluetooth
    private val requestEnableBluetoothLauncher =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
            if (result.resultCode == Activity.RESULT_OK) {
                Toast.makeText(requireContext(), "Bluetooth Enabled", Toast.LENGTH_SHORT).show()
                // Let user press Connect again
            } else {
                Toast.makeText(requireContext(), "Bluetooth is required to connect", Toast.LENGTH_SHORT).show()
            }
        }


    // --- Fragment Lifecycle ---

    // --- Fragment Lifecycle ---

    override fun onAttach(context: Context) {
        super.onAttach(context)
        // Initialize DatabaseHelper and SharedPreferences here
        databaseHelper = DatabaseHelper(requireContext())
        // Use a consistent name for your preferences file
        sharedPreferences = requireActivity().getSharedPreferences("AppPrefs", Context.MODE_PRIVATE)
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        // Inflate the layout for this fragment
        return inflater.inflate(R.layout.fragment_home, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        // --- Initialize UI Elements ---
        connectionStatusLabel = view.findViewById(R.id.connection_status_label)
       // batteryStatusLabel = view.findViewById(R.id.battery_status_label)
       // signalStatusLabel = view.findViewById(R.id.signal_status_label)
        connectButton = view.findViewById(R.id.button_connect)
        eventsRecyclerView = view.findViewById(R.id.events_recycler_view)

        // --- Setup ---
        setupRecyclerView()
        setupBluetooth() // Setup Bluetooth manager/adapter
        setupButtonClickListeners() // Setup button click logic
        updateStatusUI() // Set initial UI based on connection state
        // --- Load initial events based *only* on login state ---
        loadDetectionEvents()
        // loadMockEvents() // Load initial mock events if needed (consider loading on connect)
    }

    override fun onStart() {
        super.onStart()

        val safeContext = requireContext()

        // Register listeners when the fragment becomes visible
        BleManager.getInstance(safeContext).registerDataListener(this)
        BleManager.getInstance(safeContext).registerConnectionListener(this)
        // Update UI in case state changed while fragment was paused
        updateStatusUI()
        loadDetectionEvents()
    }

    override fun onStop() {
        super.onStop()

        val safeContext = requireContext()

        // Unregister listeners when the fragment is no longer visible
        BleManager.getInstance(safeContext).unregisterDataListener(this)
        BleManager.getInstance(safeContext).unregisterConnectionListener(this)
        // Stop scanning if the fragment is stopped
        stopBleScan()
    }

    // --- Helper for Login Status ---
    private fun isUserLoggedIn(): Boolean {
        // Check your SharedPreferences flag (adjust key name as needed)
        return sharedPreferences.getBoolean("isLoggedIn", false)
    }

    // --- Setup Methods ---

    private fun setupBluetooth() {
        bluetoothManager = requireContext().getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        if (bluetoothAdapter == null) {
            Toast.makeText(requireContext(), "Bluetooth not supported", Toast.LENGTH_SHORT).show()
            // Consider disabling connect button or finishing activity if critical
        }
    }

    private fun setupRecyclerView() {
        eventAdapter = EventAdapter { event ->
            Toast.makeText(context, "Clicked event: ${event.description}", Toast.LENGTH_SHORT).show()
            // Handle event click
        }
        eventsRecyclerView.apply {
            adapter = eventAdapter
            layoutManager = LinearLayoutManager(context)
        }
    }

    // --- Button Click Handling ---

    private fun setupButtonClickListeners() {
        connectButton.setOnClickListener {
            // 1. Check Permissions
            if (!hasRequiredPermissions()) {
                checkAndRequestPermissions()
                return@setOnClickListener
            }

            // 2. Check Bluetooth Adapter State
            if (bluetoothAdapter == null) {
                Toast.makeText(requireContext(), "Bluetooth not supported", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            if (!bluetoothAdapter!!.isEnabled) {
                // Request user to enable Bluetooth
                val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
                requestEnableBluetoothLauncher.launch(enableBtIntent)
                return@setOnClickListener
            }

            // 3. Perform Action based on Connection State
            val safeContext = requireContext()

            if (BleManager.getInstance(safeContext).isConnected()) {
                handleDisconnect()
            } else {
                showDeviceScanDialog()
            }
        }
    }

    // --- Permission Handling ---

    private fun hasRequiredPermissions(): Boolean {
        return requiredPermissions.all {
            ContextCompat.checkSelfPermission(requireContext(), it) == PackageManager.PERMISSION_GRANTED
        }
    }

    private fun checkAndRequestPermissions() {
        val missingPermissions = requiredPermissions.filter {
            ContextCompat.checkSelfPermission(requireContext(), it) != PackageManager.PERMISSION_GRANTED
        }

        if (missingPermissions.isNotEmpty()) {
            Log.d("Permissions", "Requesting missing permissions: ${missingPermissions.joinToString()}")
            requestPermissionsLauncher.launch(missingPermissions.toTypedArray())
        } else {
            // This case should ideally not be reached if hasRequiredPermissions() is checked first
            Log.d("Permissions", "All permissions already granted.")
        }
    }

    // --- BLE Scan Dialog ---

    @SuppressLint("MissingPermission") // Permissions checked before calling
    private fun showDeviceScanDialog() {
        if (isScanning) {
            Toast.makeText(requireContext(), "Scan already in progress", Toast.LENGTH_SHORT).show()
            return
        }

        // Inflate the custom layout
        val inflater = requireActivity().layoutInflater
        val dialogView = inflater.inflate(R.layout.dialog_ble_scan, null)

        // Get references to views inside the custom layout
        dialogProgressBar = dialogView.findViewById(R.id.progress_bar_scanning)
        dialogEmptyTextView = dialogView.findViewById(R.id.text_view_empty_scan)
        dialogListView = dialogView.findViewById(R.id.list_view_ble_devices)

        // Setup Adapter and ListView
        // Ensure adapter uses the activity's context
        devicesListAdapter = ArrayAdapter(requireActivity(), android.R.layout.simple_list_item_1, mutableListOf<String>())
        discoveredDevicesList.clear()
        dialogListView?.adapter = devicesListAdapter

        // Set Item Click Listener on the ListView from XML
        dialogListView?.setOnItemClickListener { _, _, position, _ ->
            stopBleScan() // Stop scan on selection
            if (position >= 0 && position < discoveredDevicesList.size) {
                val selectedDevice = discoveredDevicesList[position]
                Log.d("BLE", "Device selected: ${selectedDevice.name ?: selectedDevice.address}")
                connectToDevice(selectedDevice)
                scanDialog?.dismiss() // Dismiss dialog on selection
            } else {
                Log.e("DialogClick", "Invalid position clicked: $position. List size: ${discoveredDevicesList.size}")
            }
        }

        val builder = AlertDialog.Builder(requireContext())
        builder.setTitle("Scanning for Devices...") // Set initial title
        builder.setView(dialogView) // Use the custom layout

        builder.setNegativeButton("Cancel") { dialog, _ ->
            stopBleScan()
            dialog.dismiss()
        }
        builder.setOnDismissListener {
            Log.d("BLE", "Scan Dialog Dismissed")
            stopBleScan() // Ensure scan stops if dismissed otherwise
            // Clean up view references
            dialogProgressBar = null
            dialogEmptyTextView = null
            dialogListView = null
        }

        scanDialog = builder.create()

        // Manage initial Visibility
        dialogProgressBar?.visibility = View.VISIBLE
        dialogEmptyTextView?.visibility = View.GONE
        dialogListView?.visibility = View.VISIBLE // Keep list visible initially

        scanDialog?.show()
        startBleScan() // Start scanning *after* dialog is shown
    }


    // --- BLE Scanning Logic ---

    @SuppressLint("MissingPermission") // Permissions checked before calling startBleScan
    private fun startBleScan() {
        if (bluetoothLeScanner == null) {
            Log.e("BLE", "BluetoothLeScanner not initialized.")
            Toast.makeText(requireContext(), "Cannot scan - BLE Scanner unavailable.", Toast.LENGTH_SHORT).show()
            activity?.runOnUiThread { // Ensure UI runs on main thread
                scanDialog?.dismiss()
            }
            return
        }
        if (isScanning) return // Avoid multiple scans

        Log.d("BLE", "Starting BLE Scan...")

        // Stops scanning after a pre-defined scan period.
        scanHandler.postDelayed({
            if(isScanning) {
                Log.d("BLE", "Scan timeout reached.")
                stopBleScan()
            }
        }, SCAN_PERIOD)

        isScanning = true
        discoveredDevicesList.clear()
        devicesListAdapter.clear()

        // Update custom UI before scan starts
        activity?.runOnUiThread {
            dialogProgressBar?.visibility = View.VISIBLE
            dialogEmptyTextView?.visibility = View.GONE
            dialogListView?.visibility = View.VISIBLE
            devicesListAdapter.notifyDataSetChanged()
            scanDialog?.setTitle("Scanning...")
        }

        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        // Add filters here if needed, e.g., ScanFilter.Builder().setServiceUuid(...)
        bluetoothLeScanner?.startScan(null, settings, scanCallback)
    }

    @SuppressLint("MissingPermission") // Permissions checked before calling stopBleScan
    private fun stopBleScan() {
        if (!isScanning) return // Already stopped

        Log.d("BLE", "Stopping BLE Scan...")
        isScanning = false
        try {
            bluetoothLeScanner?.stopScan(scanCallback)
        } catch (e: IllegalStateException) {
            // Can happen if BT adapter is off
            Log.e("BLE", "Error stopping scan: ${e.message}")
        }
        scanHandler.removeCallbacksAndMessages(null) // Remove timeout callback

        // Update custom UI after scan stops
        activity?.runOnUiThread {
            dialogProgressBar?.visibility = View.GONE
            if (discoveredDevicesList.isEmpty()) {
                dialogEmptyTextView?.visibility = View.VISIBLE
                dialogListView?.visibility = View.GONE
                scanDialog?.setTitle("No devices found")
            } else {
                dialogEmptyTextView?.visibility = View.GONE
                dialogListView?.visibility = View.VISIBLE
                scanDialog?.setTitle("Select Device")
            }
        }
    }

    // Scan Callback (Handles scan results)
    private val scanCallback = object : ScanCallback() {
        @SuppressLint("MissingPermission") // Permissions checked before scan start
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device
            // Use device.name - it might be null initially, Bluetooth stack might update it later
            val deviceName = device.name ?: "Unknown Device"
            val deviceAddress = device.address

            // Log every device found for debugging
            // Log.v("BLE Scan", "Device found: Name: $deviceName, Address: $deviceAddress, RSSI: ${result.rssi}")

            // Filter for devices
            if (!deviceName.contains("ESP32")) { return }
            // Add to list only if it's not already there by address
            if (!discoveredDevicesList.any { it.address == deviceAddress }) {
                Log.i("BLE Scan", "Adding device to list: $deviceName ($deviceAddress)")
                discoveredDevicesList.add(device)
                val deviceInfo = "$deviceName\n$deviceAddress" // Format for display

                activity?.runOnUiThread {
                    // Update custom UI when device found
                    dialogProgressBar?.visibility = View.GONE // Hide progress once devices appear
                    dialogEmptyTextView?.visibility = View.GONE
                    dialogListView?.visibility = View.VISIBLE

                    devicesListAdapter.add(deviceInfo)
                    devicesListAdapter.notifyDataSetChanged()
                }
            }
            // }
        }

        override fun onScanFailed(errorCode: Int) {
            Log.e("BLE", "Scan Failed: Error Code: $errorCode")
            isScanning = false // Ensure scanning flag is reset
            activity?.runOnUiThread {
                Toast.makeText(requireContext(), "BLE Scan Failed: $errorCode", Toast.LENGTH_LONG).show()
                dialogProgressBar?.visibility = View.GONE
                // Check if dialog is still showing before dismissing
                if (scanDialog?.isShowing == true) {
                    scanDialog?.dismiss()
                }
            }
        }
    }

    // --- Connection / Disconnection ---

    @SuppressLint("MissingPermission") // Permissions checked before calling
    private fun connectToDevice(device: BluetoothDevice) {
        val safeContext = requireContext()

        if (BleManager.getInstance(safeContext).isConnected()) {
            Toast.makeText(requireContext(), "Already connected", Toast.LENGTH_SHORT).show()
            return
        }

        Toast.makeText(requireContext(), "Connecting to ${device.name ?: device.address}...", Toast.LENGTH_SHORT).show()
        Log.i("BLE", "Attempting to connect to ${device.name} (${device.address})")

        // Initiate connection - BleManager handles the callbacks
        val gatt = device.connectGatt(requireContext(), false, BleManager.getInstance(safeContext).gattCallback, BluetoothDevice.TRANSPORT_LE)

        // Let BleManager manage the GATT instance and current device

        BleManager.getInstance(safeContext).setGatt(gatt)
        BleManager.getInstance(safeContext).setCurrentDevice(device)

        // UI will be updated via the BleConnectionListener callbacks (onDeviceConnected/onDeviceDisconnected)
    }

    private fun handleDisconnect() {
        val safeContext = requireContext()

        if (!BleManager.getInstance(safeContext).isConnected()) {
            Toast.makeText(requireContext(), "Not connected", Toast.LENGTH_SHORT).show()
            return
        }
        Toast.makeText(requireContext(), "Disconnecting...", Toast.LENGTH_SHORT).show()
        Log.i("BLE", "Disconnect requested by user.")
        BleManager.getInstance(safeContext).disconnect()
        // UI update happens via onDeviceDisconnected callback
    }


    // --- UI Update ---

    @SuppressLint("MissingPermission") // Checks BT Connect on newer APIs indirectly
    private fun updateStatusUI() {
        val context = requireContext() // Use requireContext() safely within fragment lifecycle methods
        val isCurrentlyConnected = BleManager.getInstance(context).isConnected() // Check actual state

        if (isCurrentlyConnected) {
            val deviceName = BleManager.getInstance(context).getCurrentDevice()?.name ?: "Device"
            connectionStatusLabel.text = getString(R.string.status_connected_to, deviceName)
            // Fix: Provide a valid drawable ID for connected state
            connectionStatusLabel.setCompoundDrawablesWithIntrinsicBounds(
                ContextCompat.getDrawable(context, android.R.drawable.presence_online), // Green dot placeholder
                null, null, null
            )

            // Placeholder for Battery Good/Full
            /*batteryStatusLabel.text = getString(R.string.battery_good)
            batteryStatusLabel.setCompoundDrawablesWithIntrinsicBounds(
                ContextCompat.getDrawable(context, android.R.drawable.ic_lock_idle_charging), // Charging icon placeholder
                null, null, null
            )

            // Placeholder for Signal Strong
            signalStatusLabel.text = getString(R.string.signal_strong)
            signalStatusLabel.setCompoundDrawablesWithIntrinsicBounds(
                // Using a generic positive status icon as placeholder
                ContextCompat.getDrawable(context, android.R.drawable.ic_dialog_info), // Info icon placeholder
                null, null, null
            )*/

            // Update Button for Disconnect state
            connectButton.text = getString(R.string.disconnect)
            connectButton.icon = ContextCompat.getDrawable(context, android.R.drawable.ic_menu_close_clear_cancel) // 'X' icon placeholder

        } else {
            connectionStatusLabel.text = getString(R.string.status_disconnected)
            // Placeholder for Disconnected state
            connectionStatusLabel.setCompoundDrawablesWithIntrinsicBounds(
                ContextCompat.getDrawable(context, android.R.drawable.presence_offline), // Grey dot placeholder
                null, null, null
            )

            // Placeholder for Battery Unknown
            /*batteryStatusLabel.text = getString(R.string.battery_unknown)
            batteryStatusLabel.setCompoundDrawablesWithIntrinsicBounds(
                ContextCompat.getDrawable(context, android.R.drawable.ic_lock_idle_low_battery), // Low battery icon placeholder
                null, null, null
            )

            // Placeholder for Signal Unknown
            signalStatusLabel.text = getString(R.string.signal_unknown)
            signalStatusLabel.setCompoundDrawablesWithIntrinsicBounds(
                // Using a generic alert/unknown icon as placeholder
                ContextCompat.getDrawable(context, android.R.drawable.ic_dialog_alert), // Alert icon placeholder
                null, null, null
            )*/

            // Update Button for Connect state
            connectButton.text = getString(R.string.connect)
            connectButton.icon = ContextCompat.getDrawable(context, android.R.drawable.ic_popup_sync) // Sync icon placeholder (like connect)

            // Clear events when disconnected
            eventAdapter.submitList(emptyList())
        }
    }


    // --- Mock Data / Event Handling ---

   /** private fun loadMockEvents() {
        // Only load if connected
        if (!BleManager.getInstance().isConnected()) {
            Log.d("Events", "Not connected, clearing mock events.")
            eventAdapter.submitList(emptyList())
            return
        }
        Log.d("Events", "Loading mock events.")
        // Replace with actual event loading logic based on BLE data later
        val mockEvents = listOf(
            Event(UUID.randomUUID().toString(), "Motion Detected - Deer", "5 minutes ago", android.R.drawable.ic_menu_gallery),
            Event(UUID.randomUUID().toString(), "Image Captured", "5 minutes ago", android.R.drawable.ic_menu_camera),
            Event(UUID.randomUUID().toString(), "Motion Detected - Raccoon", "1 hour ago", android.R.drawable.ic_menu_gallery),
            Event(UUID.randomUUID().toString(), "Image Captured", "1 hour ago", android.R.drawable.ic_menu_camera),
            Event(UUID.randomUUID().toString(), "Low Battery Warning (15%)", "Yesterday 6:00 PM", android.R.drawable.ic_dialog_alert)
        )
        eventAdapter.submitList(mockEvents)
    }*/

    // --- Load Data from Database (Based ONLY on Login State) ---
    private fun loadDetectionEvents() {
        // Check ONLY login status
        if (isUserLoggedIn()) {
            Log.d("HomeFragment", "User logged in. Loading detections from DB.")
            // IMPORTANT: Run database query on a background thread in a real app!
            try {
                // Fetch all historical detections
                val detectionEvents = databaseHelper.getAllDetectionEvents()
                activity?.runOnUiThread { // Ensure UI update is on main thread
                    eventAdapter.submitList(detectionEvents)
                    Log.d("HomeFragment", "Submitted ${detectionEvents.size} historical events to adapter.")
                }
            } catch (e: Exception) {
                Log.e("HomeFragment", "Error loading detection events from DB", e)
                activity?.runOnUiThread {
                    Toast.makeText(context, "Error loading events", Toast.LENGTH_SHORT).show()
                    eventAdapter.submitList(emptyList()) // Clear list on error
                }
            }
        } else {
            // Not logged in, clear the list
            Log.d("HomeFragment", "User not logged in. Clearing events list.")
            activity?.runOnUiThread {
                eventAdapter.submitList(emptyList())
            }
        }
    }

    // --- BleManager Listener Callbacks ---

    override fun onBleDataReceived(data: ByteArray) {
        activity?.runOnUiThread {
            // Process received data, update battery/signal/events UI
            Log.d("BLE Data", "Received data: ${data.joinToString { "%02X".format(it) }}")
            Toast.makeText(context, "Data received: ${data.size} bytes", Toast.LENGTH_SHORT).show()
            // Example: Parse data and update batteryStatusLabel.text, signalStatusLabel.text
            // Example: Create new Event objects and add to eventAdapter


            // val detection = parseDetectionFromBleData(data)
            // if (detection != null) {
            //    databaseHelper.insertDetectionRecord(detection.animal, detection.timestamp, detection.camera, detection.imagePath)
            //    loadDetectionEvents() // Reload list to show the new one
            // }

        }
    }

    @SuppressLint("MissingPermission")
    override fun onDeviceConnected(device: BluetoothDevice) {
        activity?.runOnUiThread {
            Log.i("BLE Callback", "onDeviceConnected: ${device.name ?: device.address}")
            Toast.makeText(context, "Connected to ${device.name ?: "Device"}", Toast.LENGTH_SHORT).show()
            updateStatusUI() // Update button text, status labels

            // Historical data comes from db and is
            // loaded if user is logged in.
            // New data will come via onBleDataReceived.

        }
    }

    @SuppressLint("MissingPermission")
    override fun onDeviceDisconnected(device: BluetoothDevice) {
        activity?.runOnUiThread {
            Log.i("BLE Callback", "onDeviceDisconnected: ${device.name ?: device.address}")
            // Only show Toast if it was the device we thought we were connected to
            // (Avoids toasts if disconnect happens unexpectedly or during connection attempt)
            // if (device.address == BleManager.getInstance().getCurrentDevice()?.address) { // This check might be tricky due to timing
            Toast.makeText(context, "Disconnected", Toast.LENGTH_SHORT).show()
            // }
            updateStatusUI() // Update button text, status labels
            eventAdapter.submitList(emptyList()) // Clear events
        }
    }
}

// --- Dummy Event Data Class (if not already defined) ---
// data class Event(val id: String, val description: String, val timestamp: String, val iconResId: Int)

// --- Dummy Event Adapter (if not already defined) ---
// class EventAdapter(private val onItemClicked: (Event) -> Unit) : ListAdapter<Event, EventAdapter.EventViewHolder>(EventDiffCallback()) {
//     // ... ViewHolder, onCreateViewHolder, onBindViewHolder, DiffCallback implementations
// }