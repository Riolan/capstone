package com.example.sampleviewer // Adjust package name

import android.content.Context
import android.os.Bundle
import android.util.Log
import androidx.fragment.app.Fragment
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.example.myapplication.R // Adjust R import
import com.example.sampleviewer.bt.BleConnectionListener
import com.example.sampleviewer.bt.BleManager
import com.example.sampleviewer.database.DatabaseHelper // Import DB Helper
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.nio.charset.Charset

class NodesFragment : Fragment() {

    private lateinit var nodesRecyclerView: RecyclerView
    private lateinit var disconnectedMessage: TextView
    private lateinit var nodeAdapter: NodeAdapter // Use the corrected NodeAdapter
    private lateinit var refreshButton: Button
    private lateinit var bleManager: BleManager
    private lateinit var dbHelper: DatabaseHelper

    // Store UI state, including expansion, in a map (Device ID -> Node object)
    // Using deviceId as the key ensures persistence across LoRa ID changes
    private val nodeUiStateMap = mutableMapOf<String, Node>()

    // Connection Listener (keep as before)
    private val connectionListener = object : BleConnectionListener {
        override fun onDeviceConnected(device: android.bluetooth.BluetoothDevice) {
            activity?.runOnUiThread {
                Log.d("NodesFragment", "Connection Listener: Connected")
                updateConnectionStatusDisplay(true)
                requestNodeDataFromDevice() // Request data on connect
            }
        }
        override fun onDeviceDisconnected(device: android.bluetooth.BluetoothDevice) {
            activity?.runOnUiThread {
                Log.d("NodesFragment", "Connection Listener: Disconnected")
                updateConnectionStatusDisplay(false)
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val safeContext = requireContext()
        bleManager = BleManager.getInstance(safeContext.applicationContext)
        dbHelper = DatabaseHelper(safeContext)
    }


    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_nodes, container, false)
        nodesRecyclerView = view.findViewById(R.id.nodes_recycler_view)
        disconnectedMessage = view.findViewById(R.id.disconnected_message)
        refreshButton = view.findViewById(R.id.button_sync) // Use correct ID
        setupRecyclerView() // Setup adapter with listeners
        setupObservers()
        setupListeners()
        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        bleManager.registerConnectionListener(connectionListener)
        updateConnectionStatusDisplay(bleManager.isConnected())
        if (bleManager.isConnected()) {
            requestNodeDataFromDevice()
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        bleManager.unregisterConnectionListener(connectionListener)
        // Optional: Cancel fragment-specific coroutines if not using viewLifecycleOwner.lifecycleScope
    }

    private fun setupRecyclerView() {
        // *** Instantiate NodeAdapter with TWO listeners ***
        nodeAdapter = NodeAdapter(
            // 1. Listener for item expansion clicks
            onItemExpanderClicked = { clickedNode, _ -> // Position might not be needed if using deviceId
                val deviceId = clickedNode.deviceId
                if (deviceId != null) {
                    val currentState = nodeUiStateMap[deviceId]
                    if (currentState != null) {
                        val updatedState = currentState.copy(isExpanded = !currentState.isExpanded)
                        nodeUiStateMap[deviceId] = updatedState // Update the map
                        // Resubmit the list derived from the map values
                        submitCurrentNodeStateToAdapter() // Use helper function
                        Log.d("NodesFragment", "Toggled expansion for $deviceId to ${updatedState.isExpanded}")
                    } else {
                        Log.w("NodesFragment", "Clicked node DevID $deviceId not found in state map for expansion.")
                    }
                } else {
                    Log.w("NodesFragment", "Clicked node missing Device ID for expansion toggle.")
                }
            },
            // 2. Listener for settings changes (e.g., checkbox clicks)
            onSettingsChanged = { deviceId, newMask, newThreshold ->
                Log.d("NodesFragment", "Settings changed callback for $deviceId: Mask=$newMask, Threshold=$newThreshold")
                // Update the local map state immediately for visual feedback (optional but good)
                nodeUiStateMap[deviceId]?.let { currentNode ->
                    nodeUiStateMap[deviceId] = currentNode.copy(
                        alertMask = newMask ?: currentNode.alertMask, // Keep old if null
                        detectionThreshold = newThreshold ?: currentNode.detectionThreshold // Keep old if null
                    )
                    // Maybe resubmit list here for instant checkbox update? Or rely on save function?
                    // submitCurrentNodeStateToAdapter()
                }
                // Call function to save locally and send update command via BLE/LoRa
                saveSettingsAndNotifyDevice(deviceId, newMask, newThreshold)
            }
        )
        // *** End Adapter Instantiation ***

        nodesRecyclerView.apply {
            adapter = nodeAdapter
            layoutManager = LinearLayoutManager(context)
            itemAnimator = null
        }
    }

    // Helper function to submit the current map state to the adapter
    private fun submitCurrentNodeStateToAdapter() {
        nodeAdapter.submitList(nodeUiStateMap.values.toList().sortedBy { it.name })
    }


    private fun setupListeners() {
        refreshButton.setOnClickListener {
            Log.d("NodesFragment", "Refresh button clicked.")
            requestNodeDataFromDevice()
        }
    }

    // Function to request node list from ESP32
    private fun requestNodeDataFromDevice() {
        if (bleManager.isConnected()) {
            Log.d("NodesFragment", "Sending REQUEST_NODES command...")
            val command = "REQUEST_NODES\n"
            val success = bleManager.sendData(command.toByteArray(Charsets.UTF_8))
            if (!success) { Log.e("NodesFragment", "Failed to send REQUEST_NODES command.") }
        } else { Log.w("NodesFragment", "Cannot request nodes, not connected.") }
    }

    // Observe the StateFlow from BleManager
    private fun setupObservers() {
        viewLifecycleOwner.lifecycleScope.launch {
            bleManager.nodeListState.collectLatest { nodesFromBle ->
                // This block executes when BleManager provides a new list of LIVE node data
                Log.d("NodesFragment", "Observer received ${nodesFromBle.size} nodes from BleManager.")

                // --- Reconcile BLE data with existing UI state (expansion) ---
                // We use the existing nodeUiStateMap which holds persistent settings & expansion state
                val newMapState = mutableMapOf<String, Node>()
                for (liveNode in nodesFromBle) {
                    val deviceId = liveNode.deviceId
                    if (deviceId != null) {
                        // Get the previous UI state (including expansion and potentially settings)
                        val existingUiNode = nodeUiStateMap[deviceId]
                        // Get persisted settings from DB (could be cached, but fetch for accuracy)
                        // Run DB access in IO, but wait for it here
                        val (persistedMask, persistedThreshold) = withContext(Dispatchers.IO) {
                            dbHelper.getNodeSettings(deviceId)
                        }

                        // Create the new state:
                        // - Use live data for status, battery, lastSeen, loraNodeId
                        // - Use persisted data for name (if available), alertMask, threshold
                        // - Use existing UI state for isExpanded
                        val updatedNode = liveNode.copy(
                            name = existingUiNode?.name ?: liveNode.name, // Prefer existing name if user set one
                            alertMask = persistedMask ?: existingUiNode?.alertMask, // Prefer DB setting
                            detectionThreshold = persistedThreshold ?: existingUiNode?.detectionThreshold, // Prefer DB setting
                            isExpanded = existingUiNode?.isExpanded ?: false // Keep old expansion state
                        )
                        newMapState[deviceId] = updatedNode
                    } else {
                        Log.w("NodesFragment", "Node received from BLE sync missing Device ID: ${liveNode.nodeId}")
                    }
                }

                // Update the Fragment's state map
                nodeUiStateMap.clear()
                nodeUiStateMap.putAll(newMapState)

                // Submit the reconciled list (values from the map) to the adapter
                submitCurrentNodeStateToAdapter() // Use helper
                Log.d("NodesFragment", "Reconciled state and submitted list to adapter.")

                // Update empty state based on the *final* list
                updateEmptyStateUI(nodeUiStateMap.isEmpty())
            }
        }
        // Observer for DB update signal (still useful for gallery fragment, maybe less critical here now)
        viewLifecycleOwner.lifecycleScope.launch {
            bleManager.databaseUpdatedSignal.collectLatest {
                // Optional: Could trigger a selective refresh or full reload here too
                // if settings changed outside this fragment's interaction
                Log.d("NodesFragment", "DB Signal received, could refresh if needed.")
                // requestNodeDataFromDevice() // Example: Force re-sync on any DB change
            }
        }
    }

    // --- Function to handle saving settings and sending LoRa command ---
    private fun saveSettingsAndNotifyDevice(deviceId: String, alertMask: Int?, threshold: Int?) {
        if (!bleManager.isConnected()) {
            Toast.makeText(requireContext(), "Not connected, cannot save settings", Toast.LENGTH_SHORT).show()
            // Revert UI? Fetching fresh data might be best.
            requestNodeDataFromDevice() // Re-sync to get actual state back
            return
        }

        viewLifecycleOwner.lifecycleScope.launch {
            // 1. Save settings locally using BleManager function (which calls DB helper)
            val dbSuccess = bleManager.saveNodeSettings(deviceId, alertMask, threshold)

            if (dbSuccess) {
                Log.d("NodesFragment", "Settings saved to DB via BleManager for $deviceId.")

                //threshold?.let { thresh ->
                //    val command = "CONFIG_UPDATE;$deviceId;${lora.CONFIG_PARAM_DETECTION_THRESHOLD};$thresh\n"
                //    Log.d("NodesFragment", "Sending LoRa command via BLE: $command")
                //    bleManager.sendData(command.toByteArray(Charsets.UTF_8))
               // }
               // Toast.makeText(requireContext(), "Settings update sent", Toast.LENGTH_SHORT).show()

            } else {
                Log.e("NodesFragment", "Failed to save settings to DB for $deviceId.")
                Toast.makeText(requireContext(), "Failed to save settings", Toast.LENGTH_SHORT).show()
                // Reload data to revert UI changes
                requestNodeDataFromDevice()
            }
        }
    } // --- End saveSettingsAndNotifyDevice ---


    // Helper to update UI based on connection status
    private fun updateConnectionStatusDisplay(isConnected: Boolean) {
        if (view == null) return
        refreshButton.isEnabled = isConnected // Enable/disable refresh button
        if (isConnected) {
            disconnectedMessage.visibility = View.GONE
            nodesRecyclerView.visibility = View.VISIBLE
        } else {
            disconnectedMessage.visibility = View.VISIBLE
            disconnectedMessage.text = getString(R.string.connect_main_camera_prompt) // Set appropriate text
            nodesRecyclerView.visibility = View.GONE
            nodeUiStateMap.clear() // Clear local state when disconnected
            nodeAdapter.submitList(emptyList())
        }
        Log.d("NodesFragment", "UI Updated: Connected = $isConnected")
    }

    // Helper to show/hide empty list message
    private fun updateEmptyStateUI(isEmpty: Boolean){
        if (view == null) return
        if (bleManager.isConnected()) {
            disconnectedMessage.visibility = if (isEmpty) View.VISIBLE else View.GONE
            disconnectedMessage.text = if(isEmpty) "No nodes synced yet. Press Sync." else ""
            nodesRecyclerView.visibility = if (isEmpty) View.GONE else View.VISIBLE
        } // else: updateConnectionStatusDisplay handles the disconnected message
    }

}