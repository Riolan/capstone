#include "SendManager.hpp"
#include <cstring> // For memcpy/memset

namespace lora {

// Constructor
SendManager::SendManager() :
    radioState(ManagerState::IDLE),
    nextSeqNum(0),
    lora_send_timeouts(0),
    encodedPacketLength(0),
    sendStartTime(0),
    packetBeingSentIndex(-1)
{
    // Initialize the internal send buffer
    for (int i = 0; i < MAX_BUFFERED_PACKETS; ++i) {
        sendBuffer[i].active = false; // Mark all slots as initially free
        sendBuffer[i].payloadSize = 0; // Ensure size is zero
    }
    memset(&packetBeingSent, 0, sizeof(packetBeingSent)); // Clear temp packet holder
    memset(encodedPacketBuffer, 0, sizeof(encodedPacketBuffer)); // Clear temp encode buffer
}

// Find an inactive slot in the internal buffer
int SendManager::findFreeSlot() {
    for (int i = 0; i < MAX_BUFFERED_PACKETS; ++i) {
        if (!sendBuffer[i].active) { // Slot is free if not active
            return i;
        }
    }
    Serial.println("WARN: Send buffer for ACKs is full!");
    return -1; // No free slot found
}

// Stores packet info into the buffer AFTER successful TX confirmation (called from update)
void SendManager::storeSentPacket(const LoRaPacket& packet) {
    int i = findFreeSlot();
    if (i == -1) {
        Serial.println("ERROR: Could not store packet for ACK - buffer full!");
        return; // Buffer full
    }

    // Copy details from the successfully transmitted packet
    sendBuffer[i].nodeID = packet.nodeID;
    sendBuffer[i].seqNum = packet.seqNum;
    sendBuffer[i].type = packet.type;
    sendBuffer[i].subtype = packet.subtype;
    sendBuffer[i].flags = packet.flags; // Store flags (includes REQ_ACK)
    sendBuffer[i].payloadSize = packet.payloadSize;
    memcpy(sendBuffer[i].payload, packet.payload, packet.payloadSize);

    sendBuffer[i].timestamp = millis(); // Record time of successful TX confirmation for ACK timeout
    sendBuffer[i].ackRetries = 0;       // Reset retry count for this new entry
    sendBuffer[i].active = true;        // Mark slot as active (waiting for ACK)

    Serial.print("Stored packet SeqNum: "); Serial.print(packet.seqNum); Serial.println(" for ACK tracking.");
}

// Reset state machine variables after TX attempt completes or times out
void SendManager::resetRadioState() {
    radioState = ManagerState::IDLE;
    packetBeingSentIndex = -1;
    encodedPacketLength = 0; // Optional: clear buffers if needed
    // memset(&packetBeingSent, 0, sizeof(packetBeingSent));
    // memset(encodedPacketBuffer, 0, sizeof(encodedPacketBuffer));
}

// Non-blocking initiation of sending a packet
bool SendManager::send(uint8_t nodeID, uint8_t type, uint8_t subtype, uint8_t *payload, uint8_t length, bool requireAck) {
    if (radioState != ManagerState::IDLE) {
        Serial.println("WARN: Radio busy (waiting TX Done), cannot send now.");
        return false; // Radio is busy with a previous transmission
    }
    if (length > MAX_PAYLOAD_SIZE) {
        Serial.println("ERROR: Payload too large.");
        return false;
    }

    // Prepare the packet details in the temporary holder
    // Use the *current* nextSeqNum - **do not increment here yet!**
    packetBeingSent = {
        type,
        subtype,
        nextSeqNum, // Use current seq num
        requireAck ? (uint8_t)0x02 : (uint8_t)0x00, // Set REQ_ACK flag
        nodeID,
        length,
        {} // Payload memcpy below
    };
    memcpy(packetBeingSent.payload, payload, length);

    // Encode the packet into the temporary buffer
    encodePacket(&packetBeingSent, encodedPacketBuffer); // Use the global encoder
    encodedPacketLength = length + HEADER_SIZE;

    // Attempt to start sending via the radio hardware
    if (rf95.send(encodedPacketBuffer, encodedPacketLength)) {
        // Send command initiated successfully
        Serial.print("LoRa send initiated. SeqNum: "); Serial.println(packetBeingSent.seqNum);
        radioState = ManagerState::WAITING_TX_DONE; // Update manager state
        sendStartTime = millis();                   // Record start time for TX_DONE timeout
        packetBeingSentIndex = -1;                  // Mark as an initial send

        return true; // Indicate send was initiated
    } else {
        // Immediate hardware failure (e.g., radio busy flag didn't clear)
        Serial.print("ERROR: LoRa rf95.send() failed immediately for SeqNum: "); Serial.println(packetBeingSent.seqNum);
        // State remains IDLE, sequence number not used.
        resetRadioState(); // Ensure clean state
        return false; // Indicate send failed to start
    }
}

bool SendManager::send(const lora::LoRaPacket& packetToSend) {
    if (radioState != ManagerState::IDLE) {
        Serial.println("WARN: Radio busy (waiting TX Done), cannot send now.");
        return false; // Radio is busy with a previous transmission
    }
    if (packetToSend.payloadSize > MAX_PAYLOAD_SIZE) {
        Serial.println("ERROR: Payload size in provided packet exceeds MAX_PAYLOAD_SIZE.");
        return false;
    }

    // --- Prepare the internal packetBeingSent ---
    // Copy most fields directly from the input packet
    packetBeingSent.type = packetToSend.type;
    packetBeingSent.subtype = packetToSend.subtype;
    packetBeingSent.nodeID = packetToSend.nodeID;
    packetBeingSent.payloadSize = packetToSend.payloadSize;
    memcpy(packetBeingSent.payload, packetToSend.payload, packetToSend.payloadSize);

    // ** IMPORTANT: Sequence Number Handling **
    // The SendManager controls the sequence number for reliable delivery tracking.
    // We will assign the *next* sequence number managed by the class,
    // ignoring any seqNum potentially present in packetToSend.
    packetBeingSent.seqNum = nextSeqNum; // Use the manager's next sequence number

    // ** IMPORTANT: Flags / ACK Requirement Handling **
    // Determine if ACK is required based on the flags in the provided packet.
    packetBeingSent.flags = packetToSend.flags; // Copy flags from input
    bool requireAck = (packetToSend.flags & 0x02); // Check the REQ_ACK flag

    // --- Proceed with encoding and sending ---
    encodePacket(&packetBeingSent, encodedPacketBuffer); // Use the global encoder
    encodedPacketLength = packetBeingSent.payloadSize + HEADER_SIZE;

    // Attempt to start sending via the radio hardware
    if (rf95.send(encodedPacketBuffer, encodedPacketLength)) {
        // Send command initiated successfully
        Serial.print("LoRa send initiated (from packet). SeqNum: "); Serial.println(packetBeingSent.seqNum);
        radioState = ManagerState::WAITING_TX_DONE; // Update manager state
        sendStartTime = millis();                   // Record start time for TX_DONE timeout
        packetBeingSentIndex = -1;                  // Mark as an initial send

        // Note: nextSeqNum is incremented in update() upon successful TX_DONE confirmation

        return true; // Indicate send was initiated
    } else {
        // Immediate hardware failure
        Serial.print("ERROR: LoRa rf95.send() failed immediately (from packet) for SeqNum: "); Serial.println(packetBeingSent.seqNum);
        resetRadioState(); // Ensure clean state
        return false; // Indicate send failed to start
    }
}


// Marks a packet in the internal buffer as ACKed
void SendManager::handleAck(uint8_t nodeID, uint8_t seqNum) {
    for (int i = 0; i < MAX_BUFFERED_PACKETS; ++i) {
        // Check if this slot holds the active packet we're looking for
        if (sendBuffer[i].active &&
            sendBuffer[i].nodeID == nodeID &&
            sendBuffer[i].seqNum == seqNum)
        {
            sendBuffer[i].active = false; // Mark as inactive (ACK received)
            Serial.print("ACK processed for SeqNum: "); Serial.println(seqNum);
            // Optional: Clear other fields if needed
            // sendBuffer[i].payloadSize = 0;
            return; // Found and handled
        }
    }
     Serial.print("WARN: Received ACK for unknown or inactive SeqNum: "); Serial.println(seqNum);
}

// Non-blocking initiation of a resend attempt (called from update)
void SendManager::attemptResend(int index) {
     if (radioState != ManagerState::IDLE) {
        // This should ideally not happen if update() logic is correct, but safety check
        Serial.print("WARN: Radio busy, cannot initiate resend for index "); Serial.println(index);
        return;
    }
    if (index < 0 || index >= MAX_BUFFERED_PACKETS || !sendBuffer[index].active) {
        Serial.println("ERROR: Invalid index or state for resend attempt.");
        return; // Invalid index or packet not waiting for ACK
    }

    // Reconstruct the packet to be resent from the buffer
    packetBeingSent = {
        sendBuffer[index].type,
        sendBuffer[index].subtype,
        sendBuffer[index].seqNum,
        static_cast<uint8_t>(sendBuffer[index].flags | 0x02), // Ensure REQ_ACK flag is set for resends
        sendBuffer[index].nodeID,
        sendBuffer[index].payloadSize,
        {}
    };
    memcpy(packetBeingSent.payload, sendBuffer[index].payload, packetBeingSent.payloadSize);

    // Encode into the temporary buffer
    encodePacket(&packetBeingSent, encodedPacketBuffer);
    encodedPacketLength = packetBeingSent.payloadSize + HEADER_SIZE;

    // Attempt to start sending the resend
    if (rf95.send(encodedPacketBuffer, encodedPacketLength)) {
        Serial.print("LoRa RESEND initiated. SeqNum: "); Serial.print(packetBeingSent.seqNum);
        Serial.print(" Attempt: "); Serial.println(sendBuffer[index].ackRetries + 1);

        radioState = ManagerState::WAITING_TX_DONE; // Update manager state
        sendStartTime = millis();                   // Record start time for TX_DONE timeout
        packetBeingSentIndex = index;               // Store the index being resent

        // DO NOT update sendBuffer timestamp or retry count here. Update on TX_DONE success in update().

    } else {
        // Immediate hardware failure during resend attempt
        Serial.print("ERROR: LoRa rf95.send() failed immediately during RESEND for SeqNum: "); Serial.println(packetBeingSent.seqNum);
        // State remains IDLE. Consider if timestamp/retry should be updated here to prevent fast loops?
        // Maybe increment retry count here and update timestamp to avoid hammering?
        // sendBuffer[index].ackRetries++;
        // sendBuffer[index].timestamp = millis(); // Update to delay next attempt slightly
        resetRadioState();
    }
}

// Radio reset procedure
void SendManager::attemptRadioReset() {
    Serial.println("Attempting LoRa radio reset...");
    pinMode(RFM95_RST, OUTPUT); // Ensure pin is output
    digitalWrite(RFM95_RST, LOW);
    delay(10); // Datasheet typically recommends >= 100us, 10ms is plenty
    digitalWrite(RFM95_RST, HIGH);
    delay(10); // Allow time for chip to stabilize

    if (rf95.init()) {
        Serial.println("LoRa radio re-initialized successfully.");
        // IMPORTANT: Re-apply essential configurations!
        configureRadioSettings(); // Call your radio setup function
        lora_send_timeouts = 0; // Reset timeout counter
    } else {
        Serial.println("FATAL ERROR: LoRa radio re-initialization failed!");
        // Consider more drastic recovery: ESP.restart(), watchdog, etc.
    }
    // Ensure radio state is IDLE after reset attempt
    resetRadioState();
}


// Main update function - CALL THIS FREQUENTLY FROM Arduino loop()
void SendManager::update() {
    unsigned long now = millis();
    // --- Part 1: Check status if we are waiting for TX Done ---
    if (radioState == ManagerState::WAITING_TX_DONE) {
        // Check if the radio hardware has signalled completion (returned to IDLE mode)
        int currentMode = rf95.mode();

       /* Serial.print("DEBUG: Waiting TX Done. Current rf95.mode() = ");
        Serial.print(currentMode);
        Serial.print(" (RHModeIdle="); Serial.print(2);
        Serial.print(", RH_RF95_MODE_SLEEP="); Serial.print(RH_RF95_MODE_SLEEP);
        Serial.print(", RH_RF95_MODE_TX="); Serial.println(RH_RF95_MODE_TX); // Add TX mode value for comparison
        Serial.print("  millis() since send start: "); Serial.println(millis() - sendStartTime);
*/
                
        
        if (rf95.mode() == 2) { // TX Done successfully!
            Serial.print("LoRa TX Done confirmation received for SeqNum: "); Serial.println(packetBeingSent.seqNum);

            // Reset consecutive TX failure counter on any success
            lora_send_timeouts = 0;

            bool wasInitialSend = (packetBeingSentIndex == -1);
            bool needsAck = (packetBeingSent.flags & 0x02);

            // Increment sequence number ONLY for successful INITIAL sends
            if (wasInitialSend) {
                nextSeqNum++;
                // Serial.print("Incremented nextSeqNum to: "); Serial.println(nextSeqNum);
            }

            // If the packet required an ACK, store/update its info in the buffer
            if (needsAck) {
                if (wasInitialSend) {
                    // It was an initial send that needs ACK: store it.
                    storeSentPacket(packetBeingSent);
                } else {
                    // It was a resend: update timestamp and retry count for the existing entry.
                    if (packetBeingSentIndex >= 0 && packetBeingSentIndex < MAX_BUFFERED_PACKETS) {
                        sendBuffer[packetBeingSentIndex].timestamp = now; // Reset ACK timer base
                        // Retry count already effectively incremented by the attempt itself
                         Serial.print("Updated timestamp for resent SeqNum: "); Serial.println(packetBeingSent.seqNum);
                    } else {
                         Serial.println("ERROR: Invalid index during resend success handling."); // Should not happen
                    }
                }
            } // end if (needsAck)

            // TX processing complete, return radio state to IDLE
            resetRadioState();

        } // end if (rf95.mode() == RHModeIdle)
        else if (now - sendStartTime > SEND_TIMEOUT_MS) {
            // TX Done Timeout: Radio didn't confirm TX completion in time
            Serial.print("ERROR: LoRa TX Done confirmation TIMEOUT for SeqNum: "); Serial.println(packetBeingSent.seqNum);

            lora_send_timeouts++; // Increment consecutive failure counter
             Serial.print("Consecutive TX Done timeouts: "); Serial.println(lora_send_timeouts);

            // Check if consecutive timeouts exceed the limit -> try radio reset
            if (lora_send_timeouts > MAX_CONSECUTIVE_SEND_TIMEOUTS) {
                 attemptRadioReset(); // This function will reset the counter on success
            }

            // Give up on this specific transmission attempt and return state to IDLE
            // If it was an initial send, the seqNum wasn't incremented and packet wasn't stored.
            // If it was a resend, the timestamp/retry count wasn't updated in the buffer.
            resetRadioState();

        } // end if (timeout)
        // Else: Still waiting for TX Done, within timeout period. Do nothing this cycle.

    } // end if (radioState == WAITING_TX_DONE)


    // --- Part 2: Check for ACK Timeouts (only if radio is IDLE) ---
    // We only attempt resends if the radio isn't busy finishing a previous TX.
    if (radioState == ManagerState::IDLE) {
        for (int i = 0; i < MAX_BUFFERED_PACKETS; ++i) {
            // Check slots that are active (waiting for an ACK)
            if (sendBuffer[i].active) {
                // Check if the ACK timeout has expired since the last successful TX confirmation
                if (now - sendBuffer[i].timestamp > ACK_TIMEOUT) {

                    if (sendBuffer[i].ackRetries >= MAX_RESEND_ATTEMPTS) {
                         Serial.print("ACK Timeout - MAX RETRIES reached for SeqNum: ");
                         Serial.println(sendBuffer[i].seqNum);
                         sendBuffer[i].active = false; // Give up on this packet
                         // Optionally: notify application layer of failure
                    } else {
                         Serial.print("ACK Timeout for SeqNum: "); Serial.print(sendBuffer[i].seqNum);
                         Serial.print(". Attempting resend (Retry "); Serial.print(sendBuffer[i].ackRetries + 1);
                         Serial.print("/"); Serial.print(MAX_RESEND_ATTEMPTS); Serial.println(")...");

                         // Increment retry count *before* attempting resend
                         sendBuffer[i].ackRetries++;
                         // Attempt to initiate the resend (this is non-blocking)
                         attemptResend(i);
                         // If attemptResend starts successfully, radioState will become WAITING_TX_DONE,
                         // preventing other resends this cycle. If it fails immediately, state remains IDLE.
                         break; // Only attempt one resend per update() call to be safe
                    }
                } // end if (ack timeout expired)
            } // end if (slot active)
        } // end for (buffer scan)
    } // end if (radioState == IDLE)

} // End update()

} // namespace lora