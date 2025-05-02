#include "node_shared.hpp"
#include <cstring> // For memcpy

namespace lora {

    // Definition of the actual global radio object
    RH_RF95 rf95(RFM95_CS, RFM95_INT);

    // --- Removed global sendBuffer and global storeSentPacket ---

    // Helper to print payload in ASCII and HEX
    void printPayload(const uint8_t* payload, size_t length) {
        Serial.print("  ASCII: ");
        for (size_t i = 0; i < length; ++i) {
            char c = (payload[i] >= 32 && payload[i] <= 126) ? payload[i] : '.';
            Serial.print(c);
        }
        Serial.println();

        Serial.print("  HEX  : ");
        for (size_t i = 0; i < length; ++i) {
            if (payload[i] < 0x10) Serial.print("0");
            Serial.print(payload[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }

    // Function to encode a packet (parameter is now const pointer)
    void encodePacket(const lora::LoRaPacket *packet, uint8_t *buffer) {
        buffer[0] = packet->type;
        buffer[1] = packet->subtype;
        buffer[2] = packet->seqNum;
        buffer[3] = packet->flags;
        buffer[4] = packet->nodeID;
        buffer[5] = packet->payloadSize;
        memcpy(buffer + HEADER_SIZE, packet->payload, packet->payloadSize); // Corrected index

        // Optional: Debug output (can be removed for production)
        // Serial.print("Encoding packet - SeqNum: "); Serial.println(packet->seqNum);
        // printPayload(packet->payload, packet->payloadSize);
    }

    // Function to decode a packet (parameter is now const pointer)
    int decodePacket(uint8_t* buffer, size_t bufferLen, lora::LoRaPacket* packet) {
        if (bufferLen < HEADER_SIZE) {
            Serial.println("ERROR: Buffer too small for header");
            return -1;
        }

        packet->type        = buffer[0];
        packet->subtype     = buffer[1];
        packet->seqNum      = buffer[2];
        packet->flags       = buffer[3];
        packet->nodeID      = buffer[4];
        packet->payloadSize = buffer[5];

        // Validate payload size against buffer length received
        if (bufferLen < HEADER_SIZE + packet->payloadSize) {
             Serial.print("ERROR: Buffer too small for declared payload. Len:");
             Serial.print(bufferLen); Serial.print(", Header+Payload:");
             Serial.println(HEADER_SIZE + packet->payloadSize);
            return -1;
        }
        // Also check against max compile-time size
         if (packet->payloadSize > MAX_PAYLOAD_SIZE) {
            Serial.print("ERROR: Declared payload size exceeds MAX_PAYLOAD_SIZE. Declared:");
            Serial.println(packet->payloadSize);
            return -1;
        }

        // Copy payload
        memcpy(packet->payload, buffer + HEADER_SIZE, packet->payloadSize); // Corrected index

        // Optional: Debug output
        // Serial.print("Decoded packet - SeqNum: "); Serial.println(packet->seqNum);
        // printPayload(packet->payload, packet->payloadSize);

        return 0; // Successfully decoded
    }

    // Function to receive a packet (Non-blocking check)
    bool receivePacket(lora::LoRaPacket *packet) {
        if (rf95.available()) {
            uint8_t buffer[RH_RF95_MAX_MESSAGE_LEN];
            uint8_t len = sizeof(buffer);

            if (rf95.recv(buffer, &len)) {
                Serial.print("Received Raw - Len: "); Serial.print(len);
                Serial.print(", RSSI: "); Serial.println(rf95.lastRssi(), DEC);

                // Attempt to decode
                if (decodePacket(buffer, len, packet) == 0) {
                    Serial.print("Successfully Decoded Packet - Type: 0x"); Serial.print(packet->type, HEX);
                    Serial.print(", Subtype: 0x"); Serial.print(packet->subtype, HEX);
                    Serial.print(", Seq: "); Serial.print(packet->seqNum);
                    Serial.print(", Flags: 0x"); Serial.print(packet->flags, HEX);
                    Serial.print(", Node: 0x"); Serial.print(packet->nodeID, HEX);
                    Serial.print(", Len: "); Serial.println(packet->payloadSize);
                    printPayload(packet->payload, packet->payloadSize);
                    return true; // Successfully received and decoded
                } else {
                    Serial.println("Packet decode failed.");
                    // Optionally print raw buffer hex for debugging
                    return false; // Decode failed
                }
            } else {
                Serial.println("rf95.recv failed");
                return false; // Hardware receive failed
            }
        }
        return false; // No packet available
    }

     // Dummy implementation - Replace with your actual radio config if needed
     void configureRadioSettings() {
         Serial.println("INFO: Applying default radio settings (if any)...");
         // Example: Set frequency, TX power, modulation params if not done in setup()
         // rf95.setFrequency(RF95_FREQ); // Already done in setup typically
         // rf95.setTxPower(23, false);  // Already done in setup typically
     }

     void getAllNodeStatuses(std::vector<NodeStatus>& statusVector) {
        statusVector.clear(); // Clear the output vector first
        // Note: nodeCount is the count of *registered* nodes in knownNodes,
        // but nodeStatus array might have entries scattered. We iterate the whole status array.
        // Reserve space based on nodeCount as an estimate.
        statusVector.reserve(nodeCount);

        for (int i = 0; i < MAX_NODES; ++i) {
            // Check active status slots (where nodeID is not 0xFF)
            if (nodeStatus[i].nodeID != 0xFF) {
                 // Create a temporary copy to modify before adding
                 NodeStatus currentStatusCopy = nodeStatus[i];

                 // Find corresponding deviceID from knownNodes and add it
                 currentStatusCopy.deviceID = 0; // Reset just in case
                 for(int j=0; j<nodeCount; ++j) { // Iterate knownNodes up to current count
                     if(knownNodes[j].assignedNodeID == currentStatusCopy.nodeID) {
                         currentStatusCopy.deviceID = knownNodes[j].deviceID;
                         break; // Found the device ID
                     }
                 }
                // Add the completed copy (with deviceID and batteryPercent) to the vector
                statusVector.push_back(currentStatusCopy);
            }
        }
     }


} // end namespace lora