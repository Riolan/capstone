package com.example.sampleviewer // Adjust package name

import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.CheckBox // Import CheckBox
import android.widget.CompoundButton
import android.widget.ImageButton
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.example.myapplication.R // Adjust R import
// Removed imports not used directly here: DatabaseHelper, lifecycleScope, coroutines

// Define constants matching packet_types.h if not globally accessible
object AnimalMasks {
    const val NONE     = 0x00
    const val SQUIRREL = 0x01 // Bit 0
    const val BIRD     = 0x02 // Bit 1
    const val CAT      = 0x04 // Bit 2
    const val DOG      = 0x08 // Bit 3
}

class NodeAdapter(
    // Listener for expanding/collapsing item
    private val onItemExpanderClicked: (node: Node, position: Int) -> Unit,
    // Listener for when settings *change* in the UI and need saving/sending
    private val onSettingsChanged: (deviceId: String, newMask: Int?, newThreshold: Int?) -> Unit
) : ListAdapter<Node, NodeAdapter.NodeViewHolder>(NodeDiffCallback()) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): NodeViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.list_item_node, parent, false) // Use node item layout
        // Pass both listeners to the ViewHolder
        return NodeViewHolder(view, onItemExpanderClicked, onSettingsChanged)
    }

    override fun onBindViewHolder(holder: NodeViewHolder, position: Int) {
        val node = getItem(position)
        holder.bind(node) // Pass the node object to bind
    }

    // ViewHolder class
    class NodeViewHolder(
        itemView: View,
        private val onItemExpanderClicked: (node: Node, position: Int) -> Unit,
        private val onSettingsChanged: (deviceId: String, newMask: Int?, newThreshold: Int?) -> Unit
    ) : RecyclerView.ViewHolder(itemView) {

        // --- Find Views (Ensure these IDs match your list_item_node.xml) ---
        // Collapsed views
        private val nodeNameTextView: TextView = itemView.findViewById(R.id.node_name) // Changed ID
        private val nodeStatusTextView: TextView = itemView.findViewById(R.id.node_status) // Changed ID
        private val expandButton: ImageButton = itemView.findViewById(R.id.expand_button)

        // Expanded views group
        private val expandedDetailsView: LinearLayout = itemView.findViewById(R.id.expanded_details_view)
        private val nodeDetailBattery: TextView = itemView.findViewById(R.id.node_detail_battery)
        private val nodeDetailLastSeen: TextView = itemView.findViewById(R.id.node_status) // Changed ID
        // Checkboxes (Use correct IDs from your layout)
        private val detectSquirrelCheckBox: CheckBox = itemView.findViewById(R.id.checkbox_detect_squirrel)
        private val detectBirdCheckBox: CheckBox = itemView.findViewById(R.id.checkbox_detect_bird)
        private val detectCatCheckBox: CheckBox = itemView.findViewById(R.id.checkbox_detect_cat)
        private val detectDogCheckBox: CheckBox = itemView.findViewById(R.id.checkbox_detect_dog)
        // Update button (if needed, or trigger save on checkbox change)
        // private val updateNodeButton: MaterialButton = itemView.findViewById(R.id.button_update_node)
        // --- End Find Views ---


        private var currentNode: Node? = null
        private var isBinding = false // Prevent listener loops during binding

        init {
            // --- Set Checkbox Listeners ---
            val checkListener = CompoundButton.OnCheckedChangeListener { buttonView, isChecked ->
                if (isBinding) return@OnCheckedChangeListener // Avoid triggering during bind()

                // Get the currently bound node and its deviceId
                val node = currentNode ?: return@OnCheckedChangeListener
                val deviceId = node.deviceId ?: return@OnCheckedChangeListener // Need deviceId to save settings

                var currentMask = node.alertMask ?: 0 // Get current mask from bound node data
                val maskBit: Int = when (buttonView.id) {
                    R.id.checkbox_detect_squirrel -> AnimalMasks.SQUIRREL
                    R.id.checkbox_detect_bird -> AnimalMasks.BIRD
                    R.id.checkbox_detect_cat -> AnimalMasks.CAT
                    R.id.checkbox_detect_dog -> AnimalMasks.DOG
                    else -> 0
                }

                if (maskBit != 0) {
                    // Update the mask based on the checkbox state
                    currentMask = if (isChecked) {
                        currentMask or maskBit // Set the bit
                    } else {
                        currentMask and maskBit.inv() // Clear the bit
                    }
                    // Update the locally held node's mask immediately for consistency
                    // Note: This doesn't update the list data source directly,
                    // the Fragment needs to handle that if necessary upon saving.
                    currentNode = node.copy(alertMask = currentMask)

                    Log.d("NodeAdapter", "Checkbox changed for DevID $deviceId. New Mask: $currentMask")
                    // Trigger the callback to the Fragment to handle saving/sending
                    onSettingsChanged(deviceId, currentMask, node.detectionThreshold) // Pass new mask
                }
            }

            detectSquirrelCheckBox.setOnCheckedChangeListener(checkListener)
            detectBirdCheckBox.setOnCheckedChangeListener(checkListener)
            detectCatCheckBox.setOnCheckedChangeListener(checkListener)
            detectDogCheckBox.setOnCheckedChangeListener(checkListener)
            // --- End Checkbox Listeners ---

            // --- Set Expander Listener ---
            // Handles clicks on the expand button OR the main item view
            val expandClickListener = View.OnClickListener {
                currentNode?.let { node ->
                    // Use bindingAdapterPosition for reliable position
                    val position = bindingAdapterPosition
                    if (position != RecyclerView.NO_POSITION) {
                        onItemExpanderClicked(node, position) // Notify Fragment to handle expansion toggle
                    }
                }
            }
            expandButton.setOnClickListener(expandClickListener)
            itemView.setOnClickListener(expandClickListener) // Allow clicking whole item
            // --- End Expander Listener ---

            // Remove updateNodeButton listener if saving happens on checkbox change
            // updateNodeButton.setOnClickListener { ... }
        }

        fun bind(node: Node) {
            isBinding = true // Prevent listeners during binding
            currentNode = node

            // Bind basic info (always visible)
            nodeNameTextView.text = node.name
            nodeStatusTextView.text = node.status

            // Bind expanded details (only visible if node.isExpanded is true)
            nodeDetailBattery.text = node.batteryLevel?.let { "Battery: $it%" } ?: "Battery: N/A"
            nodeDetailLastSeen.text = "Last Seen: ${node.lastSeen}"
            // nodeDetailSignal.text = node.signalStrength?.let { "RSSI: $it dBm" } ?: "RSSI: N/A"

            // --- Bind Checkbox States from alertMask ---
            // Assumes node object passed in already has settings loaded from DB by Fragment/ViewModel
            val mask = node.alertMask ?: 0 // Default to 0 (no alerts) if null
            detectSquirrelCheckBox.isChecked = (mask and AnimalMasks.SQUIRREL) != 0
            detectBirdCheckBox.isChecked = (mask and AnimalMasks.BIRD) != 0
            detectCatCheckBox.isChecked = (mask and AnimalMasks.CAT) != 0
            detectDogCheckBox.isChecked = (mask and AnimalMasks.DOG) != 0
            // --- End Bind Checkboxes ---

            // --- Handle Expansion Visibility ---
            expandedDetailsView.visibility = if (node.isExpanded) View.VISIBLE else View.GONE
            // Change expand button icon based on state


            isBinding = false // Re-enable listeners
        }
    }

    // DiffUtil Callback
    class NodeDiffCallback : DiffUtil.ItemCallback<Node>() {
        override fun areItemsTheSame(oldItem: Node, newItem: Node): Boolean {
            // Use the persistent deviceId if available and reliable, otherwise fallback
            return oldItem.deviceId == newItem.deviceId && oldItem.deviceId != null || oldItem.id == newItem.id
        }

        override fun areContentsTheSame(oldItem: Node, newItem: Node): Boolean {
            // Compare all fields that affect the UI representation
            return oldItem == newItem
        }
    }
}