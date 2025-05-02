#pragma once

#include "node_shared.hpp" // Includes RH_RF95, LoRaPacket, constants, etc.
#include <stdint.h>
#include <string.h>
#include <Arduino.h>

namespace lora {

// Forward declaration if needed (already in node_shared.hpp)
// extern RH_RF95 rf95;

// Structure stored in the SendManager's internal buffer for ACK tracking
struct BufferedPacketInfo {
    uint8_t nodeID;
    uint8_t seqNum;
    uint8_t type;
    uint8_t subtype;
    uint8_t flags;      // Original flags (needed for resend?) - REQ_ACK is important
    uint8_t payload[MAX_PAYLOAD_SIZE];
    uint8_t payloadSize;
    unsigned long timestamp; // Time of last successful TX confirmation (used for ACK timeout)
    bool active;         // Is this slot holding a packet waiting for ACK? (false if free or ACKed)
    uint8_t ackRetries;    // Number of resend attempts due to ACK timeout
};

class SendManager {
public:
    static SendManager& getInstance() {
        static SendManager instance; // Created once on first use (thread-safe in C++11+)
        return instance;
    }

    // Initiates sending. Returns true if send command accepted, false if radio busy or error. Non-blocking.
    bool send(uint8_t nodeID, uint8_t type, uint8_t subtype, uint8_t *payload, uint8_t length, bool requireAck);
    bool send(const lora::LoRaPacket& packetToSend);

    /**
     * @brief Checks if the SendManager is currently idle (not waiting for TX Done).
     * @return true if idle, false otherwise.
     */
    bool isIdle() const { return radioState == ManagerState::IDLE; }

    /**
     * @brief Gets the sequence number that will be assigned to the *next* packet
     * sent via an initial call to send().
     * @return The next sequence number (uint8_t).
     */
    uint8_t getNextSeqNum() const { return nextSeqNum; }

    // Main update loop - CALL THIS FREQUENTLY from Arduino loop()
    // Handles TX confirmation checks, ACK timeouts, and initiates resends.
    void update();

    // Call this when an ACK packet is received for a packet sent by this manager.
    void handleAck(uint8_t nodeID, uint8_t seqNum);

private:
    SendManager();
    ~SendManager() = default; // Prevent copying
    SendManager(const SendManager&) = delete; // Prevent copying


    // States for the SendManager's radio operation
    enum class ManagerState {
        IDLE,                   // Radio is free / ready to send
        WAITING_TX_DONE         // rf95.send() called, waiting for hardware confirmation
    };

    ManagerState radioState;    // Current state of the radio operations managed here
    uint8_t nextSeqNum;         // Sequence number for the *next* initial transmission
    BufferedPacketInfo sendBuffer[MAX_BUFFERED_PACKETS]; // Internal buffer for ACK tracking
    uint8_t lora_send_timeouts; // Track consecutive TX_DONE timeouts

    // --- Temporary storage during WAITING_TX_DONE state ---
    LoRaPacket packetBeingSent; // Holds details of the packet currently in flight
    uint8_t encodedPacketBuffer[HEADER_SIZE + MAX_PAYLOAD_SIZE]; // Buffer for encoded packet
    size_t encodedPacketLength; // Length of the data in encodedPacketBuffer
    unsigned long sendStartTime;  // Time rf95.send() was called (for TX_DONE timeout)
    int packetBeingSentIndex;   // Index in sendBuffer if it's a resend (-1 if initial send)

    // --- Helper Methods ---
    int findFreeSlot();         // Find available slot in internal sendBuffer
    void storeSentPacket(const LoRaPacket& packet); // Add packet to internal sendBuffer after TX Done
    void attemptResend(int index); // Non-blocking initiation of a resend
    void resetRadioState();     // Helper to reset state machine variables
    void attemptRadioReset();   // Handles the radio reset procedure
};

} // namespace lora