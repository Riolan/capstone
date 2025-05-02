package com.example.sampleviewer

import java.util.UUID

/**
 * Data class holding information about a single child node.
 *
 * @param id Unique identifier.
 * @param name User-friendly name of the node.
 * @param status Current status (e.g., "OK", "Low Battery", "Offline").
 * @param lastSeen Timestamp or relative time since last message.
 * @param batteryLevel Optional battery level percentage.
 * @param signalStrength Optional signal strength (e.g., RSSI in dBm).
 * @param isExpanded Flag to track UI expansion state in the list.
 */

data class Node(
    val id: String = UUID.randomUUID().toString(),
    val deviceId: String?,
    val nodeId: String = UUID.randomUUID().toString(),
    val name: String,
    val status: String,
    val lastSeen: String,
    val batteryLevel: Int? = null,
    val signalStrength: Int? = null,
    var isExpanded: Boolean = false, // State for expansion in RecyclerView
    // Animal detection flags
    val detectCat: Boolean = false,
    val detectDog: Boolean = false,
    val detectSquirrel: Boolean = false,
    val detectBird: Boolean = false,
    var alertMask: Int? = null, // Bitmask for which animals trigger alerts

    var detectionThreshold: Int? = null
)