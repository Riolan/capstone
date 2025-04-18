package com.example.myapplication

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.content.SharedPreferences
import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.*
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity
import androidx.viewpager2.adapter.FragmentStateAdapter
import androidx.viewpager2.widget.ViewPager2
import com.example.myapplication.databinding.ActivityCameraSettingsBinding
import com.example.myapplication.ui.BleConnectionListener
import com.example.myapplication.ui.BleDataListener
import com.example.myapplication.ui.BleManager
import com.google.android.material.tabs.TabLayout
import com.google.android.material.tabs.TabLayoutMediator

// Node data class for storing information about nodes
data class Node(
    val id: String,
    val name: String,
    val uuid: String,
    val capabilities: MutableList<String> = mutableListOf("cat", "dog", "squirrel", "bird") // Default capabilities
)

class MyPagerAdapter(fragmentActivity: FragmentActivity, private val items: List<String>) : FragmentStateAdapter(fragmentActivity) {
    override fun getItemCount(): Int = items.size

    override fun createFragment(position: Int): Fragment {
        val fragment = MyFragment()
        fragment.arguments = Bundle().apply {
            putString("data", items[position])
        }
        return fragment
    }
}

class MyFragment : Fragment() {
    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.fragment_camera_activity2, container, false)
        val data = arguments?.getString("data")
        view.findViewById<TextView>(R.id.section_label).text = data
        return view
    }
}

class CameraActivity : AppCompatActivity(), BleConnectionListener, BleDataListener {

    // Declare UI component variables but initialize them in onCreate
    private lateinit var backButton: Button
    private lateinit var syncButton: Button
    private lateinit var motherNodeLabel: TextView
    private lateinit var nodeSelectorSpinner: Spinner
    private lateinit var nodeDetailsContainer: LinearLayout
    private lateinit var nodeDetailsText: TextView
    private lateinit var resetAnimalsButton: Button

    private var receivedNodes: MutableList<Node> = mutableListOf()
    private var isReceivingNodeInfo = false
    private val nodeInfoBuilder = StringBuilder()
    private var currentSelectedNode: Node? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.test_device_viewer)

        // Initialize UI components after setContentView
        backButton = findViewById(R.id.backButton)
        syncButton = findViewById(R.id.syncButton)
        motherNodeLabel = findViewById(R.id.motherNodeLabel)
        nodeSelectorSpinner = findViewById(R.id.nodeSelectorSpinner)
        nodeDetailsContainer = findViewById(R.id.nodeDetailsContainer)
        nodeDetailsText = findViewById(R.id.nodeDetailsText)
        resetAnimalsButton = findViewById(R.id.resetAnimalsButton)

        // Register listeners
        BleManager.getInstance().registerConnectionListener(this)
        BleManager.getInstance().registerDataListener(this)

        // Set click listeners
        syncButton.setOnClickListener {
            val success = BleManager.getInstance().sendDataChunks("REQUEST_NODES\n".toByteArray(Charsets.UTF_8))
            Log.d("BLE", if (success) "Request for nodes sent" else "Failed to send node request")
        }

        // Back button functionality
        backButton.setOnClickListener {
            finish() // Close the activity
        }

        // Reset animals button functionality (placeholder)
        resetAnimalsButton.setOnClickListener {
            // Add your reset functionality here
            Toast.makeText(this, "Resetting animals data...", Toast.LENGTH_SHORT).show()
        }
    }

    override fun onBleDataReceived(data: ByteArray) {
        val text = data.toString(Charsets.UTF_8).trim()
        Log.i("BLE", "Received: $text")

        when {
            text.startsWith("START_NODE_INFO;") -> {
                isReceivingNodeInfo = true
                nodeInfoBuilder.clear()
            }
            text.startsWith("END_NODE_INFO;") -> {
                if (isReceivingNodeInfo) {
                    processCompleteNodeInfo(nodeInfoBuilder.toString())
                    isReceivingNodeInfo = false
                }
            }
            isReceivingNodeInfo -> nodeInfoBuilder.append(text)
        }
    }

    @SuppressLint("MissingPermission")
    private fun processCompleteNodeInfo(data: String) {
        receivedNodes.clear()
        val entries = data.split("NODE;")
        for (entry in entries) {
            if (entry.isEmpty()) continue

            val parts = entry.split(";")
            if (parts.size >= 3) {
                val node = Node(parts[0], parts[1], parts[2])
                receivedNodes.add(node)
            }
        }

        runOnUiThread {
            BleManager.getInstance().getCurrentDevice()?.let { device ->
                if (receivedNodes.isNotEmpty()) {
                    val spinnerAdapter = ArrayAdapter(this, android.R.layout.simple_spinner_item, receivedNodes.map { it.name })
                    spinnerAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
                    nodeSelectorSpinner.adapter = spinnerAdapter

                    nodeSelectorSpinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
                        override fun onItemSelected(parent: AdapterView<*>, view: View?, position: Int, id: Long) {
                            currentSelectedNode = receivedNodes[position]
                            updateNodeDetails(receivedNodes[position])
                        }

                        override fun onNothingSelected(parent: AdapterView<*>) {
                            nodeDetailsContainer.removeAllViews()
                            currentSelectedNode = null
                        }
                    }

                }
            }
        }
    }

    private fun updateNodeDetails(node: Node) {
        nodeDetailsContainer.removeAllViews()

        // Inflate the node details layout
        val inflater = LayoutInflater.from(this)
        val nodeView = inflater.inflate(R.layout.node_details_layout, nodeDetailsContainer, false)

        // Set node name in the TextView at the top
        val nodeNameTextView = nodeView.findViewById<TextView>(R.id.editTextText)
        nodeNameTextView.text = "${node.name} (${node.id})"

        // Find the ScrollView and then get the LinearLayout inside it
        val scrollView = nodeView.findViewById<ScrollView>(R.id.scrollView)
        // Get the first child of the ScrollView which is our LinearLayout
        val switchesContainer = scrollView.getChildAt(0) as LinearLayout

        // Clear the default switches
        switchesContainer.removeAllViews()

        // Add capability switches
        for (capability in node.capabilities) {
            val switch = Switch(this).apply {
                text = capability
                textSize = 25f
                isChecked = true  // Default to checked
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                )
            }
            switchesContainer.addView(switch)
        }

        // Set up the save button
        val saveButton = nodeView.findViewById<Button>(R.id.saveButton)
        saveButton.setOnClickListener {
            // Collect the capabilities that are enabled
            val enabledCapabilities = mutableListOf<String>()
            for (i in 0 until switchesContainer.childCount) {
                val view = switchesContainer.getChildAt(i)
                if (view is Switch && view.isChecked) {
                    enabledCapabilities.add(view.text.toString())
                }
            }

            // Send the updated capabilities to the node
            sendNodeCapabilities(node, enabledCapabilities)

            // Show confirmation
            Toast.makeText(this, "Capabilities saved for ${node.name}", Toast.LENGTH_SHORT).show()
        }


        // Set up the save button
        val requestImageButton = nodeView.findViewById<Button>(R.id.requestImageButton)
        requestImageButton.setOnClickListener {
            // Collect the capabilities that are enabled

            val command = StringBuilder().apply {
                append("REQUEST_IMAGE\n")
                append("${node.id};")
                append("\n")
            }.toString()

            val success = BleManager.getInstance().sendDataChunks(command.toByteArray(Charsets.UTF_8))
            Log.d("BLE", if (success) "Request Image from ${node.name}" else "Failed to Request Image from ${node.name}")
        }

        // Add the view to the container
        nodeDetailsContainer.addView(nodeView)
    }

    private fun sendNodeCapabilities(node: Node, enabledCapabilities: List<String>) {
        // Format the capabilities as a command to send via BLE
        val allAnimals = listOf("cat", "dog", "squirrel", "bird")

        val command = StringBuilder().apply {
            append("CHANGE_ANIMALS\n")
            append("${node.id};")
            append(
                allAnimals.joinToString(",") { animal ->
                    if (animal in enabledCapabilities) "1" else "0"
                }
            )
            append("\n")
        }.toString()

        val success = BleManager.getInstance().sendDataChunks(command.toByteArray(Charsets.UTF_8))
        Log.d("BLE", if (success) "Capabilities sent for ${node.name}" else "Failed to send capabilities for ${node.name}")
    }

    @SuppressLint("MissingPermission")
    override fun onDeviceConnected(device: BluetoothDevice) {
        Log.d("BLE", "Device CONNECTED CameraActivity")
        runOnUiThread {
            Toast.makeText(this, "Device connected: ${device.name ?: device.address}", Toast.LENGTH_SHORT).show()
            motherNodeLabel.text = "Mother Node: ${device.name}"

        }
    }

    override fun onDeviceDisconnected(device: BluetoothDevice) {
        Log.d("BLE", "Device disconnect CameraActivity")
        runOnUiThread {
            Toast.makeText(this, "Device disconnected", Toast.LENGTH_SHORT).show()
            Toast.makeText(this, "TODO: RESET TO DEFAULT VIEW", Toast.LENGTH_SHORT).show()
            motherNodeLabel.text = "Mother Node: Not Connected"

        }
    }

    override fun onDestroy() {
        super.onDestroy()
        BleManager.getInstance().unregisterConnectionListener(this)
        BleManager.getInstance().unregisterDataListener(this)
    }
}