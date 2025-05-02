#pragma once
#ifndef LORA_MANAGER_HPP
#define LORA_MANAGER_HPP

#include <RH_RF95.h>
#include <stdint.h>
#include <stddef.h> // For size_t
#include "lora_config.h" // Include the configuration

// Forward declaration for Arduino.h dependency (if needed, e.g., for Serial)
// Or include Arduino.h directly if it's always used
#if defined(ARDUINO) && ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif


namespace lora {

// --- Global Radio Object Declaration ---
// IMPORTANT: Define this object ONCE in your main .ino file or a dedicated .cpp file
// Example: RH_RF95 rf95(RFM95_CS_PIN, RFM95_INT_PIN);
extern RH_RF95 rf95;

// --- Packet Structure Definition ---
// Basic structure for data being sent or received
struct LoRaPacket {
    uint8_t type;
    uint8_t subtype;
    uint8_t seqNum;
    uint8_t flags;
    uint8_t nodeID;      // Sender's ID (or intended recipient in some contexts)
    uint8_t payloadSize;
    uint8_t payload[MAX_PAYLOAD_SIZE];
};

// --- State Machine for Send Buffer Slots ---
enum class SendState {
    FREE,            // Slot is available
    PENDING_SEND,    // Data stored, waiting for radio to be free to start sending
    WAITING_TX_DONE, // rf95.send() called, waiting for hardware TX completion signal
    WAITING_ACK,     // TX complete, waiting for ACK from recipient (if requested)
    FAILED           // Max retries reached or fatal error occurred for this packet
};

// --- Structure for Tracking Packets in the Send Buffer ---
struct SendBufferSlot {
    // Packet Content
    uint8_t nodeID;      // Destination Node ID
    uint8_t seqNum;      // Sequence number of this packet
    uint8_t type;
    uint8_t subtype;
    uint8_t flags;       // Includes whether ACK is required
    uint8_t payload[MAX_PAYLOAD_SIZE];
    uint8_t payloadSize;

    // State Machine Control
    SendState state = SendState::FREE;
    unsigned long lastActivityTime; // Timestamp for timeouts (send start or ACK wait start)
    uint8_t retryCount = 0;         // Resend attempts counter

    // Helper to check if ACK was requested for this packet
    bool isAckRequired() const { return (flags & flags::REQ_ACK); }
};


// --- Lora Manager Class ---
class LoraManager {
public:
    LoraManager();

    // Initialization
    bool begin(); // Initialize radio hardware and manager state

    // Configuration (Call after begin if needed)
    // Example: bool configureRadio(float freq = 915.0, int8_t power = 23);

    // --- Sending ---
    // Attempts to queue a packet for sending. Returns true if successfully queued, false if buffer is full.
    // This function is NON-BLOCKING. Actual transmission happens during update().
    bool send(uint8_t destinationNodeID, uint8_t type, uint8_t subtype,
              const uint8_t *payload, uint8_t length, bool requireAck = true);

    // --- Receiving ---
    // Checks if a complete packet has been received. If yes, decodes it into 'packet' and returns true.
    // Call this frequently in your main loop.
    bool receive(LoRaPacket& packet); // Pass a LoRaPacket struct to be filled

    // --- Main Loop Processing ---
    // MUST be called frequently in the main Arduino loop().
    // Manages state machine (sending, timeouts, retries), and checks for incoming packets.
    void update();

    // --- Utility ---
    int getLastRssi() const; // Get RSSI of the last received packet

private:
    SendBufferSlot sendBuffer[MAX_BUFFERED_PACKETS];
    uint8_t nextSeqNum = 0; // Sequence number for outgoing packets
    uint8_t consecutiveTxTimeouts = 0; // Counter for consecutive TX hardware timeouts
    int lastRssi = 0; // Store RSSI of last received packet

    // --- Internal Helper Functions ---
    int findFreeSendSlot() const;
    bool encodePacket(const SendBufferSlot& slot, uint8_t* buffer, size_t& encodedLength) const;
    bool decodePacket(uint8_t* buffer, uint8_t bufferLen, LoRaPacket& packet) const;
    void printPacketInfo(const char* prefix, const LoRaPacket& packet, bool isOutgoing) const;
    void printPayload(const uint8_t* payload, size_t length) const;
    bool attemptRadioReset(); // Logic to try and reset/reinit the radio

    // --- State Machine Handlers (called by update()) ---
    void handlePendingSend(SendBufferSlot& slot, unsigned long now);
    void handleWaitingTxDone(SendBufferSlot& slot, unsigned long now);
    void handleWaitingAck(SendBufferSlot& slot, unsigned long now);
    void processIncomingAck(uint8_t senderNodeID, uint8_t ackedSeqNum);

};

} // namespace lora

#endif // LORA_MANAGER_HPP