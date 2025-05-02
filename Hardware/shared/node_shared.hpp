#pragma once
#ifndef NODE_SHARED_HPP
#define NODE_SHARED_HPP
/**
 * @file node_shared.hpp
 * @brief Shared definitions and structures for LoRa communication
 * Author: Rio Laney
 * Date: 2025-04 (Updated for non-blocking)
 */
#include <stdint.h>
//#include "RadioHead.h"
#include "RH_RF95.h"    // Must be included before declaring rf95
#include "packet_types.h" // Include packet type definitions (Make sure this exists)
#include <Arduino.h>    // Include for millis(), Serial, etc.
// In node_shared.hpp (or a new globals.hpp)

#include <vector>
#define MAX_NODES 4

// --- Node Tracking Structures ---
struct NodeRecord {
    uint32_t deviceID;
    uint8_t assignedNodeID;
};

struct NodeStatus {
    uint8_t nodeID = 0xFF;
    unsigned long lastSeen = 0;
    bool online = false;
    uint32_t deviceID = 0; // Optional: Store device ID for easy lookup


    uint8_t batteryPercent = 255;
};


// Extern declarations for the node tracking arrays (defined in Mother Node's main.cpp)
extern NodeRecord knownNodes[MAX_NODES]; // Uses NodeRecord struct defined above
extern NodeStatus nodeStatus[MAX_NODES]; // Uses NodeStatus struct defined above
extern uint8_t nodeCount;                // Number of nodes currently tracked





namespace lora {

// --- Pin Definitions (Confirm these match your board!) ---
#define RFM95_CS 15
#define RFM95_RST 33 // Or 27 depending on your V1/V2 Feather board and wiring
#define RFM95_INT 32

extern RH_RF95 rf95;  // Declaration of the single radio object


// --- Constants ---
#define ACK_TIMEOUT 5000       // Increased ACK timeout (adjust as needed)
#define SEND_TIMEOUT_MS 2000   // Timeout waiting for TX Done signal from radio
constexpr uint8_t MAX_PAYLOAD_SIZE =200 ;  // Max application payload size
#define HEADER_SIZE 6          // Size of your header (type, subtype, seqNum, flags, nodeID, len)
#define MAX_BUFFERED_PACKETS 10// Max packets SendManager can track for ACKs
#define MAX_RESEND_ATTEMPTS 3  // Max times to resend a packet if ACK not received
#define MAX_CONSECUTIVE_SEND_TIMEOUTS 5 // Threshold for radio reset on TX Done failure

// --- Packet Structure ---
typedef struct {
    uint8_t type;
    uint8_t subtype;
    uint8_t seqNum;
    uint8_t flags;        // e.g., 0x01 = LAST_PACKET, 0x02 = REQ_ACK
    uint8_t nodeID;
    uint8_t payloadSize;
    uint8_t payload[MAX_PAYLOAD_SIZE];
} LoRaPacket;




// --- Utility Function Prototypes ---
void printPayload(const uint8_t* payload, size_t length);
void encodePacket(const lora::LoRaPacket *packet, uint8_t *buffer); // Changed to const pointer
int decodePacket(uint8_t *buffer, size_t bufferLen, lora::LoRaPacket *packet);
bool receivePacket(lora::LoRaPacket *packet); // Non-blocking check and receive
void configureRadioSettings(); // Declare your radio config function if available
void getAllNodeStatuses(std::vector<NodeStatus>& statusVector); // Accessor to get all node statuses



} // endnamespace lora


// --- *** NEW: Extern Declarations for Pending LoRa Request Vars *** ---
// These are defined as volatile in main.cpp
extern volatile bool lora_request_pending;
extern volatile uint8_t lora_req_targetNodeID;
extern volatile uint8_t lora_req_type;
extern volatile uint8_t lora_req_subtype;
// Need the size definition here to declare the array extern
constexpr uint8_t  LORA_REQ_PAYLOAD_MAX_SIZE =lora::MAX_PAYLOAD_SIZE; // Use max LoRa payload size
extern volatile uint8_t lora_req_payload[lora::MAX_PAYLOAD_SIZE];
extern volatile size_t lora_req_payload_len;
extern volatile bool lora_req_requireAck;


#endif // NODE_SHARED_HPP