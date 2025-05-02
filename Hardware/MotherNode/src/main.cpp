#include <Arduino.h>
#include <SPI.h>

#include "node_shared.hpp" // Includes LoRaPacket, constants, rf95 declaration, externs for globals
#include "SendManager.hpp"   // Include SendManager definition for getInstance()
#include "packet_types.h"    // Contains packet type definitions
#include "bt_impl.hpp"       // Include BLE implementation header
#include <memory>

/// --- Global Variables ---

NodeRecord knownNodes[MAX_NODES];
NodeStatus nodeStatus[MAX_NODES];
uint8_t nodeCount = 0;


// --- Image Reassembly Constants ---
// Timeout for discarding incomplete image assembly (e.g., 2 minutes)
const unsigned long IMAGE_REASSEMBLY_TIMEOUT = 120000;
// How often to check for completed/timed-out images
const unsigned long IMAGE_PROCESS_INTERVAL = 5000; // Check every 5 seconds

// --- Image Reassembly Data Structures ---
struct ImageChunk {
    std::string data;

    // Add timestamp if needed for per-chunk timeout? Usually overall timeout is sufficient.
};

struct ImageReassemblyData {
    std::map<uint16_t, ImageChunk> chunks; // chunkNumber -> chunkData
    uint16_t totalChunks = 0;         // Expected number of chunks
    size_t receivedChars = 0; // Optional: track total characters
    bool complete = false;            // Flag when all chunks are received
    unsigned long lastChunkTime = 0;  // millis() timestamp of the last received chunk
    uint32_t imageIdentifier = 0;     // Store the image ID
    uint8_t sourceNodeID = 0;       // Store the node that sent the image
    uint32_t eventUUID = 0;
};

// Map: Source NodeID -> ImageIdentifier -> ReassemblyData
// Using unique_ptr to manage memory automatically
std::map<uint8_t, std::map<uint32_t, ImageReassemblyData>> imageReassemblyBuffer;

// --- Other Globals ---
// ... (knownNodes, nodeStatus, nodeCount, timers etc. as before) ...
static unsigned long lastImageProcessTime = 0;


volatile bool lora_request_pending = false;
volatile uint8_t lora_req_targetNodeID = 0;
volatile uint8_t lora_req_type = 0;
volatile uint8_t lora_req_subtype = 0;
//#define LORA_REQ_PAYLOAD_MAX_SIZE  lora::MAX_PAYLOAD_SIZE // Use max LoRa payload size
volatile uint8_t lora_req_payload[LORA_REQ_PAYLOAD_MAX_SIZE];
volatile size_t lora_req_payload_len = 0;
volatile bool lora_req_requireAck = false;

// --- Mother Node Configuration ---
#define MOTHER_NODE_ID 0x00
#define MOTHER_HEARTBEAT_INTERVAL 25000 //25 secs
#define NODE_OFFLINE_TIMEOUT 60000      // 60 secs

unsigned long lastMotherHeartbeatSent = 0;
unsigned long lastStatusCheck = 0;

// --- Function Prototypes ---
int findNodeIndexByDeviceID(uint32_t deviceID);
int findNodeIndexByNodeID(uint8_t nodeID);
uint8_t assignNewNodeID(uint32_t deviceID);
void updateNodeStatus(uint8_t nodeID);
void checkNodeTimeouts();
void printAllNodes();
void sendPacketAck(uint8_t targetNodeID, uint8_t seqNumToAck);

// --- Setup ---
void setup() {
    // Radio Reset Sequence
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH); delay(10);
    digitalWrite(RFM95_RST, LOW); delay(10);
    digitalWrite(RFM95_RST, HIGH); delay(10);

    Serial.begin(115200);
    while (!Serial && millis() < 2000);
    Serial.println("\nMother Node Starting...");
   // Serial.print("Mother Node ID: 0x"); Serial.println(MOTHER_NODE_ID, HEX);

    // Initialize LoRa Radio
    //Serial.println("Initializing LoRa Radio...");
    if (!lora::rf95.init()) {
        Serial.println("FATAL: LoRa radio init failed!");
        while (1);
    }
    //Serial.println("Radio init OK.");
    if (!lora::rf95.setFrequency(915.0)) {                 Serial.println("setFrequency failed"); /* handle error */
        while(1);}
    // Note do not set RFO, weird things occur - see docs.

    lora::rf95.setTxPower(23, false);
    // lora::configureRadioSettings();

    // Initialize node tracking arrays
    for (int i = 0; i < MAX_NODES; ++i) {
        knownNodes[i].deviceID = 0;
        knownNodes[i].assignedNodeID = 0xFF;
        nodeStatus[i].nodeID = 0xFF;
        nodeStatus[i].online = false;
        nodeStatus[i].lastSeen = 0;
    }

    // --- Initialize Bluetooth ---
   // Serial.println("Initializing BLE...");
    bt::setup(); // Call the BLE setup function
   // Serial.println("BLE setup complete.");
    // --- End Bluetooth Init ---

    Serial.println("Setup complete. Waiting for Edge Nodes and BLE connections...");
    lastStatusCheck = millis();
    lastMotherHeartbeatSent = millis();
}

// --- Main Loop ---
void loop() {
    unsigned long now = millis();

    // ----- Update Managers -----
    lora::SendManager::getInstance().update(); // Update LoRa sender state machine
    bt::loop(); // Update BLE state (handle outgoing queue)


    if (lora_request_pending) {
        // Then check if sender is ready
        if (lora::SendManager::getInstance().isIdle()) {
            Serial.println("Processing flagged LoRa request...");

            // --- Read parameters and clear flag atomically (optional critical section) ---
            // taskENTER_CRITICAL(&loraRequestMutex);
            uint8_t target = lora_req_targetNodeID;
            uint8_t type = lora_req_type;
            uint8_t subtype = lora_req_subtype;
            size_t len = lora_req_payload_len;
            bool ack = lora_req_requireAck;
            // Copy payload quickly *before* clearing flag
            uint8_t payload_copy[LORA_REQ_PAYLOAD_MAX_SIZE];
            if (len > 0 && len <= LORA_REQ_PAYLOAD_MAX_SIZE) {
                memcpy(payload_copy, (void*)lora_req_payload, len);
            }
            lora_request_pending = false; // Clear flag AFTER reading params
            lora_req_payload_len = 0; // Reset length for safety
            // taskEXIT_CRITICAL(&loraRequestMutex);
            // --- End Read parameters ---

            Serial.print("  Executing send: Type=0x"); Serial.print(type, HEX);
            Serial.print(", Target=0x"); Serial.println(target, HEX);

            // Execute the send command using SendManager
            lora::SendManager::getInstance().send(
                target,
                type,
                subtype,
                len > 0 ? payload_copy : nullptr, // Use copy
                len,
                ack
            );
        }
        // else { Serial.println("LoRa request pending, but sender not idle."); } // Verbose
    }

    // ----- Check for Incoming LoRa Packets -----
    lora::LoRaPacket packet;
    if (lora::receivePacket(&packet)) {
        if (packet.nodeID != MOTHER_NODE_ID || packet.nodeID != 0xFF) {
             updateNodeStatus(packet.nodeID);
        }
        switch(packet.type) {
            // --- Handle JOIN_REQ ---
            case lora::PACKET_TYPE_JOIN_REQ: {
                // ... (JOIN_REQ handling logic as before) ...
                 Serial.print("Received JOIN_REQ from Temp ID 0x"); Serial.println(packet.nodeID, HEX);
                if (packet.payloadSize != sizeof(uint32_t)) { Serial.println("  Invalid JOIN_REQ payload size."); break; }
                uint32_t deviceID; memcpy(&deviceID, packet.payload, sizeof(deviceID)); Serial.print("  Device ID: 0x"); Serial.println(deviceID, HEX);
                int nodeIndex = findNodeIndexByDeviceID(deviceID); uint8_t assignedID;
                if (nodeIndex != -1) { assignedID = knownNodes[nodeIndex].assignedNodeID; Serial.print("  Node already known. Reassigning ID: "); Serial.println(assignedID); updateNodeStatus(assignedID); }
                else { assignedID = assignNewNodeID(deviceID); if (assignedID != 0xFF) { Serial.print("  Assigning new Node ID: "); Serial.println(assignedID); updateNodeStatus(assignedID); } else { Serial.println("  Cannot assign ID, node limit reached."); break; } }
                Serial.print("  Sending JOIN_ACK with Assigned ID: "); Serial.println(assignedID); uint8_t ackPayload[] = { assignedID };
                bool initiated = lora::SendManager::getInstance().send(packet.nodeID, lora::PACKET_TYPE_JOIN_ACK, 0, ackPayload, sizeof(ackPayload), false );
                if (!initiated) { Serial.println("  WARN: Failed to initiate JOIN_ACK send."); } else { Serial.println("  JOIN_ACK send initiated."); }
                break;
            }
            // --- Handle DATA ---
            case lora::PACKET_TYPE_DATA: {
                // Process DATA packet based on subtype
                switch(packet.subtype) {
                    case lora::SUBTYPE_DETECTION: {
                        Serial.print("Received DETECTION from Node 0x"); Serial.println(packet.nodeID, HEX);
                        // --- Parse Payload with UUID ---
                        // Expected: UUID(4) + Mask(1) + Conf(1) + BBox(8) + Timestamp(4) = 18 bytes
                        const size_t EXPECTED_DETECTION_SIZE = 18;
                        if (packet.payloadSize >= EXPECTED_DETECTION_SIZE) { // Check minimum size
                            int offset = 0;
                            // UUID (Network Byte Order)
                            uint32_t eventUUID = ((uint32_t)packet.payload[offset++] << 24) |
                                                    ((uint32_t)packet.payload[offset++] << 16) |
                                                    ((uint32_t)packet.payload[offset++] << 8) |
                                                    ((uint32_t)packet.payload[offset++]);
                            
                            uint32_t packetDeviceID = ((uint32_t)packet.payload[offset++] << 24) | ((uint32_t)packet.payload[offset++] << 16) | ((uint32_t)packet.payload[offset++] << 8) | ((uint32_t)packet.payload[offset++]);

                            // Animal Mask
                            uint8_t animalMask = packet.payload[offset++];
                            // Confidence
                            uint8_t confidence = packet.payload[offset++];
                            // BBox (Placeholder parsing - adjust based on actual needs)
                            uint16_t bbox_x = ((uint16_t)packet.payload[offset++] << 8) | packet.payload[offset++];
                            uint16_t bbox_y = ((uint16_t)packet.payload[offset++] << 8) | packet.payload[offset++];
                            uint16_t bbox_w = ((uint16_t)packet.payload[offset++] << 8) | packet.payload[offset++];
                            uint16_t bbox_h = ((uint16_t)packet.payload[offset++] << 8) | packet.payload[offset++];
                                // Timestamp (Network Byte Order)
                            uint32_t timestamp = ((uint32_t)packet.payload[offset++] << 24) |
                                                    ((uint32_t)packet.payload[offset++] << 16) |
                                                    ((uint32_t)packet.payload[offset++] << 8) |
                                                    ((uint32_t)packet.payload[offset++]);

                            Serial.print("  Event UUID: 0x"); Serial.print(eventUUID, HEX);
                            Serial.print(", DeviceID: 0x"); Serial.print(packetDeviceID, HEX); // Log received device ID
                            Serial.print(", Mask: 0x"); Serial.print(animalMask, HEX);
                            Serial.print(", Conf: "); Serial.println(confidence);

                            // --- Store/Update Node Status with latest UUID (Optional) ---
                            int statusIndex = findNodeIndexByNodeID(packet.nodeID);
                            // if (statusIndex != -1) {
                            //     nodeStatus[statusIndex].latestEventUUID = eventUUID;
                            // }

                            // --- Forward Alert to BLE ---
                            // Include UUID in the message to the app
                            char devIdHex[12]; // Buffer for hex string
                            snprintf(devIdHex, sizeof(devIdHex), "0x%08lX", (long unsigned int)packetDeviceID);
                            std::string bleMsg = "LoRaDetect;Node:" + std::to_string(packet.nodeID);
                            bleMsg += ";EventUUID:" + std::to_string(eventUUID);
                            bleMsg += ";DevID:"; bleMsg += devIdHex; // Add Device ID
                            bleMsg += ";Mask:" + std::to_string(animalMask);
                            bleMsg += ";Conf:" + std::to_string(confidence);
                            bleMsg += ";Time:" + std::to_string(timestamp);
                            bleMsg += ";BBox:" + std::to_string(bbox_x)+","+std::to_string(bbox_y)+","+std::to_string(bbox_w)+","+std::to_string(bbox_h);
                            bleMsg += "\n";

                            bt::queueStringToBLEAndChunk(bleMsg); // Queue for BLE send

                        } else {
                                Serial.print("WARN: Received DETECTION packet with invalid size: "); Serial.println(packet.payloadSize);
                        }

                        // Send ACK if requested
                        if (packet.flags & 0x02) { sendPacketAck(packet.nodeID, packet.seqNum); }
                        break; // Break from SUBTYPE_DETECTION
                    }

                    case lora::SUBTYPE_BATTERY_STATUS: {
                        // ... (Battery status handling as before) ...
                        if (packet.payloadSize == 1) {  
                            nodeStatus[packet.nodeID].batteryPercent = packet.payload[0]; 
                            
                            Serial.print("Received Battery Status from Node 0x"); Serial.print(packet.nodeID, HEX);
                            Serial.print("Battery is: "); Serial.print(packet.payload[0]); 
                            Serial.println("%"); // Log battery status
                        }
                        else { Serial.println("WARN: Received invalid BATTERY_STATUS payload size."); }
                        if (packet.flags & 0x02) { sendPacketAck(packet.nodeID, packet.seqNum); } // ACK if requested
                        break;
                    }
                    // ... (Handle other DATA subtypes) ...
                    default:
                        Serial.print("Received unknown DATA Subtype 0x"); /* ... */
                        if (packet.flags & 0x02) { sendPacketAck(packet.nodeID, packet.seqNum); } // ACK if requested
                        break;
                } // End switch(packet.subtype)
                break; // Break from PACKET_TYPE_DATA
            }
             // --- Handle HEARTBEAT ---
            case lora::PACKET_TYPE_HEARTBEAT: {
                // Serial.print("Received HEARTBEAT from Node 0x"); Serial.println(packet.nodeID, HEX); // Verbose
                // Status already updated
                break;
            }
             // --- Handle ACK ---
            case lora::PACKET_TYPE_ACK: {
                 if (packet.payloadSize >= 1) {
                    uint8_t ackedSeqNum = packet.payload[0];
                    // Serial.print("Received ACK from Node 0x"); Serial.print(packet.nodeID, HEX); // Verbose
                    // Serial.print(" for SeqNum: "); Serial.println(ackedSeqNum); // Verbose
                    lora::SendManager::getInstance().handleAck(packet.nodeID, ackedSeqNum);
                 } else { Serial.println("WARN: Received ACK packet with invalid payload size."); }
                break;
            }
            // --- Handle Default ---
            case lora::PACKET_TYPE_IMAGE_CHUNK: {
                // 1. Send ACK back immediately for this chunk
                if (packet.flags & 0x02) { sendPacketAck(packet.nodeID, packet.seqNum); }
                else { Serial.println("WARN: Received IMAGE_CHUNK without REQ_ACK flag set!"); }

                // 2. Validate payload size
                const size_t CHUNK_METADATA_OVERHEAD = 8; // UUID(4)+Chunk#(2)+TotalChunks(2)
                if (packet.payloadSize < CHUNK_METADATA_OVERHEAD) { /* ... error ... */ break; }

                // 3. Parse Metadata (including UUID)
                // Reconstructing 32-bit UUID from bytes 0, 1, 2, 3
                uint32_t eventUUID = ((uint32_t)packet.payload[0] << 24) |
                    ((uint32_t)packet.payload[1] << 16) |
                    ((uint32_t)packet.payload[2] << 8)  |
                    ((uint32_t)packet.payload[3]);

                // Reconstructing 16-bit Chunk Number from bytes 4, 5
                uint16_t chunkNum = ((uint16_t)packet.payload[4] << 8) |
                  ((uint16_t)packet.payload[5]);

                // Reconstructing 16-bit Total Chunks from bytes 6, 7
                uint16_t totalNumChunks = ((uint16_t)packet.payload[6] << 8) |
                    ((uint16_t)packet.payload[7]);
                size_t chunkDataSize = packet.payloadSize - CHUNK_METADATA_OVERHEAD;

                Serial.print("Received IMAGE_CHUNK from Node 0x"); Serial.print(packet.nodeID, HEX);
                Serial.print(" for EventUUID 0x"); Serial.print(eventUUID, HEX); // Log UUID
                Serial.print(", Chunk "); Serial.print(chunkNum); /* ... */

                // 4. Find or Create Reassembly Entry using EventUUID as key
                auto& nodeBuffer = imageReassemblyBuffer[packet.nodeID];
                auto& reassemblyDataRef = nodeBuffer[eventUUID]; // Use eventUUID

                // Initialize if new
                if (reassemblyDataRef.totalChunks == 0 && reassemblyDataRef.lastChunkTime == 0) {
                    Serial.println("  Creating new reassembly buffer entry.");
                    reassemblyDataRef.totalChunks = totalNumChunks;
                    reassemblyDataRef.eventUUID = eventUUID; // Store UUID
                    reassemblyDataRef.sourceNodeID = packet.nodeID;
                    reassemblyDataRef.receivedChars = 0;
                    reassemblyDataRef.complete = false;
                    reassemblyDataRef.chunks.clear();
                }
                // ... (Check totalChunks consistency) ...

                // 5. Store the Chunk Data (as string)
                if (reassemblyDataRef.chunks.find(chunkNum) == reassemblyDataRef.chunks.end()) {
                    ImageChunk newChunk;
                    const uint8_t* chunkDataStart = packet.payload + CHUNK_METADATA_OVERHEAD;
                    newChunk.data.assign((const char*)chunkDataStart, chunkDataSize);
                    reassemblyDataRef.chunks[chunkNum] = std::move(newChunk);
                    reassemblyDataRef.receivedChars += chunkDataSize;
                    // Serial.print("  Stored chunk string #"); Serial.println(chunkNum); // Verbose
                } // else { Serial.print("  Received duplicate chunk #"); Serial.println(chunkNum); } // Verbose

                // 6. Update Timestamp
                reassemblyDataRef.lastChunkTime = now;

                // 7. Check for Completion
                if (reassemblyDataRef.chunks.size() == reassemblyDataRef.totalChunks && reassemblyDataRef.totalChunks > 0) {
                    Serial.print("All Base64 chunks received for EventUUID 0x"); Serial.println(eventUUID, HEX);
                    reassemblyDataRef.complete = true; // Mark for processing later
                } // else { /* ... log progress ... */ } // Verbose

                break; // Break from IMAGE_CHUNK case
            } // End case IMAGE_CHUNK
            default:
                Serial.print("Received unknown packet type 0x"); Serial.print(packet.type, HEX);
                Serial.print(" from Node 0x"); Serial.println(packet.nodeID, HEX);
                break;
        } // End switch
    } // End if receivePacket


    if (now - lastImageProcessTime > IMAGE_PROCESS_INTERVAL) {
        lastImageProcessTime = now;
        for (auto nodeIt = imageReassemblyBuffer.begin(); nodeIt != imageReassemblyBuffer.end(); /* ... */) {
            uint8_t nodeID = nodeIt->first;
            auto& imageMap = nodeIt->second;
            for (auto imgIt = imageMap.begin(); imgIt != imageMap.end(); /* ... */) {
                uint32_t eventUUID = imgIt->first; // Key is now UUID
                ImageReassemblyData& reassemblyDataRef = imgIt->second;

                if (reassemblyDataRef.complete) {
                    Serial.print("Processing completed Base64 image: Node=0x"); Serial.print(nodeID, HEX);
                    Serial.print(", EventUUID=0x"); Serial.println(eventUUID, HEX);

                    // 1. Reconstruct the full Base64 string
                    std::string fullImageDataString = "";
                    bool reconstructionOk = true;
                    for (uint16_t i = 0; i < reassemblyDataRef.totalChunks; ++i) {
                        if (reassemblyDataRef.chunks.count(i)) {
                            fullImageDataString += reassemblyDataRef.chunks[i].data;
                        } else { /* ... error handling ... */ reconstructionOk = false; break; }
                    }

                    if (reconstructionOk) {
                        Serial.print("  Base64 string reconstructed. Total size: "); Serial.println(fullImageDataString.length());

                        // 2. SKIP Base64 Encoding

                        // 3. Format message for BLE including EventUUID
                        std::string bleMsg = "LoRaImage;Node:";
                        bleMsg += std::to_string(nodeID);
                        bleMsg += ";EventUUID:"; // Use EventUUID instead of ImgID
                        bleMsg += std::to_string(eventUUID);
                        bleMsg += ";Format:jpeg;Encoding:base64;Data:";
                        bleMsg += fullImageDataString;
                        bleMsg += "\n"; // Terminator

                        // 4. Queue for BLE transmission
                        Serial.println("  Queueing Base64 image data for BLE.");
                        bt::queueStringToBLEAndChunk(bleMsg);

                    } else { /* ... handle reconstruction failure ... */ }

                    // 5. Cleanup
                    imgIt = imageMap.erase(imgIt);

                } else if (now - reassemblyDataRef.lastChunkTime > IMAGE_REASSEMBLY_TIMEOUT) {
                    // --- Handle Timeout ---
                    Serial.print("Timeout reassembling Base64 image: Node=0x"); Serial.print(nodeID, HEX);
                    Serial.print(", EventUUID=0x"); Serial.println(eventUUID, HEX);
                    imgIt = imageMap.erase(imgIt);
                } else { ++imgIt; }
            } // End loop through images
            // ... (Cleanup outer map entry if empty) ...
             if (imageMap.empty()) { nodeIt = imageReassemblyBuffer.erase(nodeIt); } else { ++nodeIt; }
        } // End loop through nodes
    } // End periodic image processing check

    // ----- Periodic Tasks -----
    // Send Mother Node Heartbeat periodically
    if (now - lastMotherHeartbeatSent > MOTHER_HEARTBEAT_INTERVAL) {
        lastMotherHeartbeatSent = now;
        lora::SendManager::getInstance().send(0xFF, lora::PACKET_TYPE_HEARTBEAT, 0, 0, 0, false);
    }

    // Check node statuses periodically
    if (now - lastStatusCheck > (NODE_OFFLINE_TIMEOUT / 3)) {
        lastStatusCheck = now;
        checkNodeTimeouts();
        // printAllNodes(); // Optional: Print status periodically
    }

    // delay(1); // Optional small delay
}

// --- Helper Functions ---

// Find index in knownNodes array by device hardware ID
int findNodeIndexByDeviceID(uint32_t deviceID) {
  for (int i = 0; i < nodeCount; i++) {
      if (knownNodes[i].deviceID == deviceID) {
          return i;
      }
  }
  return -1; // Not found
}

// Find index in nodeStatus array by assigned LoRa Node ID
int findNodeIndexByNodeID(uint8_t nodeID) {
   if (nodeID == 0xFF || nodeID == 0x00) return -1; // Invalid/Reserved IDs for tracking edge nodes
   for (int i = 0; i < MAX_NODES; i++) {
      if (nodeStatus[i].nodeID == nodeID) {
          return i;
      }
  }
  return -1; // Not found
}


// Assigns the next available Node ID and registers the deviceID
uint8_t assignNewNodeID(uint32_t deviceID) {
  if (nodeCount >= MAX_NODES) {
      return 0xFF; // No space left
  }
  // Find the next available ID (simple linear assignment for now)
  uint8_t newID = 1; // Start checking from 1
  while (newID < 0xFF) {
      bool id_in_use = false;
      for (int i = 0; i < nodeCount; ++i) {
          if (knownNodes[i].assignedNodeID == newID) {
              id_in_use = true;
              break;
          }
      }
      if (!id_in_use) {
          // Found an unused ID
          knownNodes[nodeCount].deviceID = deviceID;
          knownNodes[nodeCount].assignedNodeID = newID;
          nodeCount++;
          return newID; // Return the assigned ID
      }
      newID++; // Try next ID
  }
  return 0xFF; // No IDs available (shouldn't happen if nodeCount check works)
}

// Update the lastSeen time and online status for a given node ID
void updateNodeStatus(uint8_t nodeID) {
  if (nodeID == 0xFF || nodeID == MOTHER_NODE_ID) return; // Don't track invalid or self

  int statusIndex = -1;
  // Find existing entry
  for (int i = 0; i < MAX_NODES; i++) {
      if (nodeStatus[i].nodeID == nodeID) {
          statusIndex = i;
          break;
      }
  }
  // If not found, find an empty slot
  if (statusIndex == -1) {
      for (int i = 0; i < MAX_NODES; i++) {
          if (nodeStatus[i].nodeID == 0xFF) {
               statusIndex = i;
               nodeStatus[i].nodeID = nodeID; // Assign ID to this slot
               Serial.print("Now tracking status for new Node ID: "); Serial.println(nodeID);
               break;
          }
      }
  }

  if (statusIndex != -1) {
      if (!nodeStatus[statusIndex].online) {
          Serial.print("Node 0x"); Serial.print(nodeID, HEX); Serial.println(" is now ONLINE.");
      }
      nodeStatus[statusIndex].lastSeen = millis();
      nodeStatus[statusIndex].online = true;
  } else {
      // This should only happen if assignNewNodeID failed or MAX_NODES is inconsistent
      Serial.print("WARN: Could not find/create status slot for Node ID: "); Serial.println(nodeID);
  }
}

// Check for nodes that haven't been heard from recently
void checkNodeTimeouts() {
  unsigned long now = millis();
  for (int i = 0; i < MAX_NODES; i++) {
      // Check only active, online nodes
      if (nodeStatus[i].nodeID != 0xFF && nodeStatus[i].online) {
          if (now - nodeStatus[i].lastSeen > NODE_OFFLINE_TIMEOUT) {
              nodeStatus[i].online = false;
              Serial.print("Node 0x"); Serial.print(nodeStatus[i].nodeID, HEX);
              Serial.println(" marked as OFFLINE (Timeout).");
              // Optional: Remove from knownNodes as well? Or just mark offline?
              // If removed, ID can be reused sooner. If kept, deviceID mapping persists.
          }
      }
  }
}

// Print the status of all tracked nodes
void printAllNodes() {
  Serial.println("--- Node Status ---");
  bool anyNodes = false;
  for (int i = 0; i < MAX_NODES; i++) {
      if (nodeStatus[i].nodeID != 0xFF) {
          anyNodes = true;
          Serial.print("  Node 0x"); Serial.print(nodeStatus[i].nodeID, HEX);
          Serial.print(": ");
          Serial.print(nodeStatus[i].online ? "ONLINE" : "OFFLINE");
          if (nodeStatus[i].online) {
               Serial.print(" (Last seen: ");
               Serial.print((millis() - nodeStatus[i].lastSeen)/1000);
               Serial.print("s ago)");
          }
           // Find corresponding deviceID for extra info (optional)
           for(int j=0; j<nodeCount; ++j){
               if(knownNodes[j].assignedNodeID == nodeStatus[i].nodeID){
                   Serial.print(" [DevID: 0x"); Serial.print(knownNodes[j].deviceID, HEX); Serial.print("]");
                   break;
               }
           }
          Serial.println();
      }
  }
  if (!anyNodes) {
      Serial.println("  No edge nodes tracked yet.");
  }
   Serial.print("  Registered Nodes Count: "); Serial.println(nodeCount);
  Serial.println("-------------------");
}

// Helper function to send a standard ACK packet
void sendPacketAck(uint8_t targetNodeID, uint8_t seqNumToAck) {
  // Serial.print("Sending ACK to Node 0x"); Serial.print(targetNodeID, HEX); // Can be verbose
  // Serial.print(" for SeqNum: "); Serial.println(seqNumToAck);
  uint8_t ackPayload[] = { seqNumToAck }; // ACK payload is the sequence number being acknowledged
  // Use SendManager. ACKs don't require an ACK (requireAck = false).
  bool initiated = lora::SendManager::getInstance().send(
      targetNodeID,
      lora::PACKET_TYPE_ACK,
      0,
      ackPayload,
      sizeof(ackPayload),
      false
  );
  // if (!initiated) { Serial.println("  WARN: Failed to initiate ACK send."); }
}

