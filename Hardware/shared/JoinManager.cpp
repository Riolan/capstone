#include "node_shared.hpp" // For LoRaPacket, constants, encodePacket, receivePacket
#include "SendManager.hpp" // For SendManager::getInstance()
#include "packet_types.h"  // For PACKET_TYPE_JOIN_REQ, PACKET_TYPE_JOIN_ACK
#include <Arduino.h>       // For Serial, millis, delay, random
#include "esp_sleep.h"     // For light sleep

namespace lora { // Assuming this function belongs in the lora namespace

    /**
     * @brief Attempts to join the LoRa network by sending JOIN_REQ and waiting for JOIN_ACK.
     *
     * @param assignedNodeID Reference to store the assigned Node ID upon success.
     * @param deviceID The unique ID of this device (e.g., from MAC address).
     * @param maxRetries Maximum number of join attempts. Set to -1 for infinite retries.
     * @return true if join is successful, false otherwise (only if maxRetries is not -1).
     */
    bool performJoin(uint8_t &assignedNodeID, uint32_t deviceID, int8_t maxRetries) { // Changed maxRetries type to int8_t to clearly hold -1
        uint8_t attempt = 0;
        unsigned long baseDelay = 1000; // Initial delay in ms for backoff

        // Loop condition: Continue if retrying infinitely OR if attempts haven't exceeded maxRetries
        while (maxRetries == -1 || attempt <= maxRetries) {
            // --- Send JOIN_REQ ---
            lora::LoRaPacket joinReq;
            joinReq.type = PACKET_TYPE_JOIN_REQ;
            joinReq.subtype = 0;
            joinReq.flags = 0; // JOIN_REQ usually doesn't need an ACK itself
            joinReq.seqNum = 0; // Sequence number might not be relevant before joining
            joinReq.nodeID = 0xFF; // Use a temporary ID (e.g., 0xFF) or 0 before assignment
            memcpy(joinReq.payload, &deviceID, sizeof(deviceID));
            joinReq.payloadSize = sizeof(deviceID);

            uint8_t buffer[HEADER_SIZE + sizeof(deviceID)]; // Size buffer appropriately
            encodePacket(&joinReq, buffer);

            Serial.print("Sending JOIN_REQ, attempt ");
            // Print attempt number correctly, considering infinite retries
            if (maxRetries != -1) {
                Serial.print(attempt + 1); Serial.print("/"); Serial.print(maxRetries + 1);
            } else {
                Serial.print(attempt + 1); Serial.print(" (Infinite Retries)");
            }
            Serial.println("...");

            // Use SendManager to send non-blockingly.
            // JOIN_REQ itself usually doesn't require an ACK (requireAck = false),
            // we wait for the specific JOIN_ACK response packet.
            bool initiated = lora::SendManager::getInstance().send(
                0x00, // Send to Mother Node (assuming ID 0x00) or Broadcast (0xFF)? Adjust as needed.
                joinReq.type,
                joinReq.subtype,
                joinReq.payload, // Pass payload directly from struct
                joinReq.payloadSize,
                false // JOIN_REQ doesn't need a LoRa ACK, we need the JOIN_ACK packet
            );

            if (!initiated) {
                Serial.println("  Failed to initiate JOIN_REQ send (radio busy?). Retrying after delay...");
                // Apply a short delay before the main backoff if send failed immediately
                delay(500 + random(0, 100)); // Short jittered delay
                // Don't increment attempt count here, just retry the send initiation
                continue; // Go to the start of the while loop
            }

            // --- Wait for JOIN_ACK (Non-blocking style within this function) ---
            lora::LoRaPacket ack;
            unsigned long joinAckTimeout = millis() + 5000; // 5-second timeout for ACK
            bool ackReceived = false;

            while (millis() < joinAckTimeout) {
                // **CRITICAL**: Update the SendManager to handle potential background TX Done
                lora::SendManager::getInstance().update();

                // Check for incoming packet
                if (receivePacket(&ack)) {
                    if (ack.type == PACKET_TYPE_JOIN_ACK && ack.payloadSize >= 1) {
                        // Optional: Verify the target Node ID in the ACK matches our temporary ID or broadcast?
                        // if (ack.nodeID == 0xFF) { ... }
                        assignedNodeID = ack.payload[0];
                        Serial.print("JOIN_ACK Received! Assigned Node ID: ");
                        Serial.println(assignedNodeID);
                        return true; // Join successful!
                    } else {
                        // Received a different packet, log it maybe?
                        Serial.print("Received non-JOIN_ACK packet during join wait. Type: 0x");
                        Serial.println(ack.type, HEX);
                        // Handle other packets if necessary (e.g., ACKs for previous attempts?)
                         if (ack.type == PACKET_TYPE_ACK && ack.payloadSize >= 1) {
                             lora::SendManager::getInstance().handleAck(ack.nodeID, ack.payload[0]);
                         }
                    }
                }
                // Small delay to prevent busy-waiting and allow ESP32 tasks to run
                delay(10); // Reduced delay for more responsive checking
            } // End wait for ACK loop

            // --- Timeout waiting for JOIN_ACK ---
            Serial.println("JOIN_ACK timeout.");

            // Only increment attempt count and sleep if we are not retrying infinitely
            // OR if we are retrying infinitely (always increment and sleep)
            if (maxRetries == -1 || attempt < maxRetries) {
                 attempt++; // Increment attempt counter

                 // Apply exponential backoff with jitter + light sleep
                 // Limit backoff to avoid excessively long sleep times (e.g., max 60 seconds)
                 unsigned long backoffDelay = baseDelay * (1 << (attempt > 6 ? 6 : attempt)); // Limit exponent to prevent overflow/huge delays
                 backoffDelay += random(0, 500); // Add jitter
                 backoffDelay = min(backoffDelay, 60000UL); // Cap max delay (e.g., 60 seconds)

                 Serial.print("  Sleeping for "); Serial.print(backoffDelay); Serial.println(" ms before retry...");

                 esp_sleep_enable_timer_wakeup(backoffDelay * 1000ULL); // Wakeup time in microseconds
                 esp_light_sleep_start(); // Enter light sleep

                 Serial.println("  Woke up, retrying join...");
                 // After waking up, the loop continues

            } else {
                // Max retries reached (and not infinite mode)
                break; // Exit the while loop
            }

        } // End while loop

        // Only reachable if maxRetries is finite and was exceeded
        Serial.println("Failed to join after max retries.");
        return false;
    }

} // end namespace lora