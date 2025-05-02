#include "LoraManager.hpp"
#include "lora_config.h"
#include <string.h> // For memcpy, memset

// IMPORTANT: Include Arduino only if Serial/millis etc. are needed *within* this file
#if defined(ARDUINO) && ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif


namespace lora {

// --- Radio Object Definition ---
// Define this in your main .ino or a dedicated globals.cpp:
// RH_RF95 rf95(RFM95_CS_PIN, RFM95_INT_PIN);
// ---

LoraManager::LoraManager() : nextSeqNum(0), consecutiveTxTimeouts(0), lastRssi(0) {
    // Initialize send buffer slots to FREE state
    for (int i = 0; i < MAX_BUFFERED_PACKETS; ++i) {
        sendBuffer[i].state = SendState::FREE;
        sendBuffer[i].payloadSize = 0; // Explicitly mark as empty
    }
}

bool LoraManager::begin() {
    // Initialize the radio hardware
    pinMode(RFM95_RST_PIN, OUTPUT);
    digitalWrite(RFM95_RST_PIN, HIGH);
    delay(10);
    digitalWrite(RFM95_RST_PIN, LOW);
    delay(10);
    digitalWrite(RFM95_RST_PIN, HIGH);
    delay(10);

    if (!rf95.init()) {
        Serial.println("ERROR: LoRa radio init failed");
        return false;
    }

    // Configure radio settings (Example - Adjust as needed)
    if (!rf95.setFrequency(915.0)) { // Set frequency to 915 MHz
         Serial.println("ERROR: setFrequency failed");
         return false;
    }
    rf95.setTxPower(23, false); // Set TX power to 23 dBm

    // Further optional configurations:
    // rf95.setSignalBandwidth(125000); // 125kHz
    // rf95.setSpreadingFactor(7);      // SF7
    // rf95.setCodingRate4(5);          // 4/5 coding rate
    // rf95.setPreambleLength(8);

    Serial.println("LoRa radio initialized successfully.");
    consecutiveTxTimeouts = 0; // Reset counter on successful init
    return true;
}

bool LoraManager::send(uint8_t destinationNodeID, uint8_t type, uint8_t subtype,
                      const uint8_t *payload, uint8_t length, bool requireAck) {

    if (length > MAX_PAYLOAD_SIZE) {
        Serial.print("ERROR: Payload size ("); Serial.print(length);
        Serial.print(") exceeds maximum ("); Serial.print(MAX_PAYLOAD_SIZE); Serial.println(")");
        return false;
    }

    int slotIndex = findFreeSendSlot();
    if (slotIndex == -1) {
        Serial.println("WARN: Send buffer full. Packet dropped.");
        // Optional: Implement a strategy for buffer full (e.g., drop oldest, error flag)
        return false;
    }

    SendBufferSlot& slot = sendBuffer[slotIndex];

    slot.nodeID = destinationNodeID;
    slot.type = type;
    slot.subtype = subtype;
    slot.seqNum = nextSeqNum; // Assign current sequence number
    slot.payloadSize = length;
    memcpy(slot.payload, payload, length);
    slot.flags = flags::NONE; // Start with no flags
    if (requireAck) {
        slot.flags |= flags::REQ_ACK; // Set the REQ_ACK flag
    }

    slot.retryCount = 0;
    slot.state = SendState::PENDING_SEND; // Mark as ready to be sent
    slot.lastActivityTime = millis();   // Record time it was queued

    // Don't increment nextSeqNum here. Increment only when TX is confirmed done.

    #ifdef DEBUG_LORA // Optional debug output
    LoRaPacket pkt_info; // Temporary struct for printing
    pkt_info.nodeID = slot.nodeID; pkt_info.type = slot.type; pkt_info.subtype = slot.subtype;
    pkt_info.seqNum = slot.seqNum; pkt_info.flags = slot.flags; pkt_info.payloadSize = slot.payloadSize;
    memcpy(pkt_info.payload, slot.payload, slot.payloadSize);
    printPacketInfo("Queued", pkt_info, true);
    #endif

    return true; // Successfully queued
}

bool LoraManager::receive(LoRaPacket& packet) {
    uint8_t receiveBuffer[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(receiveBuffer);

    if (rf95.available()) {
        if (rf95.recv(receiveBuffer, &len)) {
            lastRssi = rf95.lastRssi(); // Store RSSI

            if (decodePacket(receiveBuffer, len, packet)) {
                #ifdef DEBUG_LORA
                printPacketInfo("Recv'd", packet, false);
                Serial.print("  RSSI: "); Serial.println(lastRssi);
                #endif

                // --- ACK Handling ---
                // Check if the received packet is an ACK for one we sent
                if (packet.type == types::ACK) {
                   processIncomingAck(packet.nodeID, packet.seqNum); // nodeID is sender of ACK
                }
                // Check if the received packet requires an ACK from us
                else if (packet.flags & flags::REQ_ACK) {
                    // Immediately queue an ACK packet back to the sender
                    // Note: ACKs themselves typically don't require ACKs to avoid loops.
                    send(packet.nodeID, types::ACK, 0, nullptr, 0, false); // ACK subtype 0, no payload, no ACK required for ACK
                    Serial.print("  ACK Queued for SeqNum: "); Serial.println(packet.seqNum);
                }
                return true; // Packet received and decoded successfully
            } else {
                Serial.println("ERROR: Failed to decode received packet.");
                // Optional: Dump raw buffer here for debugging
                return false;
            }
        } else {
            Serial.println("ERROR: rf95.recv failed.");
            return false;
        }
    }
    return false; // No packet available
}


void LoraManager::update() {
    unsigned long now = millis();

    // --- Process Send Buffer State Machine ---
    for (int i = 0; i < MAX_BUFFERED_PACKETS; ++i) {
        SendBufferSlot& slot = sendBuffer[i];

        switch (slot.state) {
            case SendState::PENDING_SEND:
                handlePendingSend(slot, now);
                break;
            case SendState::WAITING_TX_DONE:
                handleWaitingTxDone(slot, now);
                break;
            case SendState::WAITING_ACK:
                handleWaitingAck(slot, now);
                break;
            case SendState::FREE:
            case SendState::FAILED:
            default:
                // Do nothing for these states in the loop
                break;
        }
    }

    // --- Check for Incoming Packets ---
    // Note: The main application loop should call manager.receive()
    // If you prefer checking here, uncomment the below lines, but ensure
    // the received packet is handled appropriately (e.g., via callback or queue).
    // LoRaPacket receivedPacket;
    // if (receive(receivedPacket)) {
    //    // Handle the fully received packet here or signal the main app
    // }
}

int LoraManager::getLastRssi() const {
    return lastRssi;
}

// --- Private Helper Functions ---

int LoraManager::findFreeSendSlot() const {
    for (int i = 0; i < MAX_BUFFERED_PACKETS; ++i) {
        if (sendBuffer[i].state == SendState::FREE) {
            return i;
        }
    }
    return -1; // No free slot
}

bool LoraManager::encodePacket(const SendBufferSlot& slot, uint8_t* buffer, size_t& encodedLength) const {
    if (slot.payloadSize > MAX_PAYLOAD_SIZE) return false; // Should not happen if send() checks

    buffer[0] = slot.type;
    buffer[1] = slot.subtype;
    buffer[2] = slot.seqNum;
    buffer[3] = slot.flags;
    buffer[4] = slot.nodeID; // Destination Node ID
    buffer[5] = slot.payloadSize;
    memcpy(buffer + HEADER_SIZE, slot.payload, slot.payloadSize);

    encodedLength = HEADER_SIZE + slot.payloadSize;
    return true;
}

bool LoraManager::decodePacket(uint8_t* buffer, uint8_t bufferLen, LoRaPacket& packet) const {
     if (bufferLen < HEADER_SIZE) {
        Serial.println("ERROR: Decode - Buffer too small for header.");
        return false;
    }

    packet.type        = buffer[0];
    packet.subtype     = buffer[1];
    packet.seqNum      = buffer[2];
    packet.flags       = buffer[3];
    packet.nodeID      = buffer[4]; // Sender Node ID (from perspective of receiver)
    packet.payloadSize = buffer[5];

    if (packet.payloadSize > MAX_PAYLOAD_SIZE) {
         Serial.print("ERROR: Decode - Payload size field invalid (");
         Serial.print(packet.payloadSize); Serial.println(").");
         return false;
    }

    if (bufferLen < HEADER_SIZE + packet.payloadSize) {
         Serial.print("ERROR: Decode - Buffer too small for declared payload (");
         Serial.print(bufferLen); Serial.print(" < ");
         Serial.print(HEADER_SIZE + packet.payloadSize); Serial.println(").");
         return false;
    }

    memcpy(packet.payload, buffer + HEADER_SIZE, packet.payloadSize);
    return true;
}

void LoraManager::processIncomingAck(uint8_t senderNodeID, uint8_t ackedSeqNum) {
    for (int i = 0; i < MAX_BUFFERED_PACKETS; ++i) {
        SendBufferSlot& slot = sendBuffer[i];
        // Check if this slot is waiting for an ACK for the specific packet
        if (slot.state == SendState::WAITING_ACK &&
            slot.nodeID == senderNodeID &&       // ACK came from the expected node
            slot.seqNum == ackedSeqNum)
        {
            Serial.print("ACK Received for SeqNum: "); Serial.print(ackedSeqNum);
            Serial.print(" from Node: "); Serial.println(senderNodeID);
            slot.state = SendState::FREE; // Mark slot as free
            slot.payloadSize = 0;         // Clear size to be sure
            // Optional: Add callback here to notify application of successful delivery
            return; // Found and handled
        }
    }
     Serial.print("WARN: Received unexpected/duplicate ACK for SeqNum: "); Serial.print(ackedSeqNum);
     Serial.print(" from Node: "); Serial.println(senderNodeID);
}


bool LoraManager::attemptRadioReset() {
    Serial.println("Attempting LoRa radio reset...");
    // Implement hardware reset sequence (if RST pin is connected)
    if (RFM95_RST_PIN != 0) { // Use 0 or -1 if RST pin is not used/connected
         pinMode(RFM95_RST_PIN, OUTPUT);
         digitalWrite(RFM95_RST_PIN, LOW);
         delay(10);
         digitalWrite(RFM95_RST_PIN, HIGH);
         delay(10);
    }

    if (rf95.init()) {
        // IMPORTANT: Re-apply essential configurations!
        if (rf95.setFrequency(915.0)) { // Re-set frequency
             rf95.setTxPower(23, false);  // Re-set power
             // Re-apply other custom settings (BW, SF, CR, Preamble) if you changed them
             Serial.println("LoRa radio re-initialized successfully after reset.");
             consecutiveTxTimeouts = 0; // Reset counter
             return true;
        } else {
             Serial.println("FATAL ERROR: LoRa radio reset OK, but re-config failed!");
        }
    } else {
        Serial.println("FATAL ERROR: LoRa radio re-initialization failed!");
    }
     // Consider more drastic recovery: reboot device via watchdog, deep sleep?
    return false;
}

// --- State Machine Handler Implementations ---

void LoraManager::handlePendingSend(SendBufferSlot& slot, unsigned long now) {
    // Check if radio is ready (idle and not receiving)
    // Note: RH_RF95::mode() might require checking specific modes like IDLE or RX.
    // A simple check is often if it's NOT transmitting.
    // if (rf95.mode() == RH_RF95::RHModeIdle) { // More precise check if needed
    if (!(rf95.mode() == RH_RF95::RHModeTx)) { // Check if NOT currently transmitting

        uint8_t encodedBuffer[HEADER_SIZE + MAX_PAYLOAD_SIZE];
        size_t encodedLength = 0;

        if (!encodePacket(slot, encodedBuffer, encodedLength)) {
             Serial.print("ERROR: Failed to encode packet SeqNum: "); Serial.println(slot.seqNum);
             slot.state = SendState::FAILED; // Mark as failed
             return;
        }

        // Start transmission
        if (rf95.send(encodedBuffer, encodedLength)) {
            slot.state = SendState::WAITING_TX_DONE;
            slot.lastActivityTime = now; // Record send start time for TX timeout
            #ifdef DEBUG_LORA
            Serial.print("TX Started SeqNum: "); Serial.println(slot.seqNum);
            #endif
        } else {
            // Immediate send failure (rare if mode check passes, but possible)
            Serial.print("ERROR: rf95.send() failed immediately for SeqNum: "); Serial.println(slot.seqNum);
            // Keep state as PENDING_SEND to retry later? Or mark as FAILED?
            // Let's retry next cycle for now. Could add a counter here too.
        }
    }
    // Else: Radio is busy (transmitting or maybe receiving), do nothing, wait for next update cycle
}

void LoraManager::handleWaitingTxDone(SendBufferSlot& slot, unsigned long now) {
    // How to check TX Done non-blockingly with RadioHead?
    // Option 1: Interrupt Pin (Best): Configure an ISR on RFM95_INT_PIN for TxDone interrupt.
    //           The ISR sets a flag. This handler checks the flag.
    // Option 2: Polling Mode: Check if the radio mode has returned to IDLE or RX.
    // Option 3: Polling Status Register: Directly read radio registers (more complex).

    // Let's use Option 2 (Polling Mode) for simplicity here.
    // Note: This assumes rf95.send() puts the radio in TX mode and it returns
    //       to IDLE or RX when done. Verify this behavior for your library version.
    if (rf95.mode() != RH_RF95::RHModeTx) {
         // TX is complete!
         #ifdef DEBUG_LORA
         Serial.print("TX Done SeqNum: "); Serial.println(slot.seqNum);
         #endif

         consecutiveTxTimeouts = 0; // Reset timeout counter on success

         // *** Increment sequence number ONLY AFTER successful TX confirmation ***
         nextSeqNum++;

         if (slot.isAckRequired()) {
            slot.state = SendState::WAITING_ACK;
            slot.lastActivityTime = now; // Start ACK timer
            slot.retryCount = 0;         // Reset retry count for ACK phase
         } else {
            // No ACK needed, packet successfully sent
            slot.state = SendState::FREE; // Mark slot as free
            slot.payloadSize = 0;        // Clear size
            // Optional: Add callback here to notify application of success (no ACK)
         }
    } else if (now - slot.lastActivityTime > SEND_TIMEOUT_MS) {
        // TX confirmation timed out
        Serial.print("ERROR: TX confirmation timeout for SeqNum: "); Serial.println(slot.seqNum);
        consecutiveTxTimeouts++;

        if (consecutiveTxTimeouts > MAX_CONSECUTIVE_TX_TIMEOUTS) {
             if (attemptRadioReset()) {
                 // Radio reset successful, retry sending later
                 slot.state = SendState::PENDING_SEND; // Revert state to try again
                 slot.retryCount = 0; // Reset retries as it might have been a radio issue
             } else {
                 // Radio reset failed, mark packet as terminally failed
                 Serial.print("FATAL: Radio reset failed, marking SeqNum ");
                 Serial.print(slot.seqNum); Serial.println(" as FAILED.");
                 slot.state = SendState::FAILED;
             }
        } else {
             // Less than max consecutive timeouts, just retry sending
             Serial.println(" -> Retrying TX later.");
             slot.state = SendState::PENDING_SEND; // Revert state to try again
             // Keep retryCount as is, or link TX timeouts to ACK retries? For now, independent.
        }
         // We need to force the radio back to a known state (like IDLE) if possible
         // rf95.setModeIdle(); // Or equivalent function to stop any stuck TX attempt
    }
    // Else: Still transmitting, wait longer.
}

void LoraManager::handleWaitingAck(SendBufferSlot& slot, unsigned long now) {
    if (now - slot.lastActivityTime > ACK_TIMEOUT_MS) {
        // ACK Timeout
        slot.retryCount++;
        Serial.print("ACK Timeout for SeqNum: "); Serial.print(slot.seqNum);
        Serial.print(" (Retry "); Serial.print(slot.retryCount);
        Serial.print("/"); Serial.print(MAX_SEND_RETRIES); Serial.println(")");

        if (slot.retryCount > MAX_SEND_RETRIES) {
             // Max retries reached for this packet
             Serial.print(" -> Max ACK retries failed. Giving up on SeqNum: "); Serial.println(slot.seqNum);
             slot.state = SendState::FAILED; // Mark as failed
             // Optional: Callback to notify application of failure
        } else {
             // Retry sending the packet
             Serial.println(" -> Retrying send.");
             slot.state = SendState::PENDING_SEND; // Go back to PENDING_SEND state
             // lastActivityTime will be updated when PENDING_SEND starts the TX again
        }
    }
    // Else: Still waiting for ACK, within timeout period.
}

// --- Debug Printing Helpers ---

void LoraManager::printPayload(const uint8_t* payload, size_t length) const {
    if (length == 0) {
        Serial.println(" <No Payload>");
        return;
    }
    Serial.print(" Payld["); Serial.print(length); Serial.print("]: ");
    // Print HEX
    for (size_t i = 0; i < length; ++i) {
        if (payload[i] < 0x10) Serial.print("0");
        Serial.print(payload[i], HEX);
        Serial.print(" ");
        if (i > 16 && length > 20) { // Limit long payloads
             Serial.print("...");
             break;
        }
    }
    // Print ASCII representation if useful (optional)
    /*
    Serial.print(" | '");
    for (size_t i = 0; i < length; ++i) {
        char c = (payload[i] >= 32 && payload[i] <= 126) ? (char)payload[i] : '.';
        Serial.print(c);
         if (i > 16 && length > 20) {
             Serial.print("...");
             break;
        }
    }
    Serial.print("'");
    */
    Serial.println();
}

void LoraManager::printPacketInfo(const char* prefix, const LoRaPacket& packet, bool isOutgoing) const {
    Serial.print(prefix); Serial.print(" | ");
    if (isOutgoing) {
         Serial.print("To: "); Serial.print(packet.nodeID); // In outgoing, nodeID is Destination
    } else {
         Serial.print("From: "); Serial.print(packet.nodeID); // In incoming, nodeID is Sender
    }
    Serial.print(" | Seq: "); Serial.print(packet.seqNum);
    Serial.print(" | Typ:0x"); if(packet.type < 0x10) Serial.print("0"); Serial.print(packet.type, HEX);
    Serial.print("/0x"); if(packet.subtype < 0x10) Serial.print("0"); Serial.print(packet.subtype, HEX);
    Serial.print(" | Flg:0x"); if(packet.flags < 0x10) Serial.print("0"); Serial.print(packet.flags, HEX);
    printPayload(packet.payload, packet.payloadSize);
}


} // namespace lora