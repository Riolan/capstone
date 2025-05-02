#pragma once
#ifndef LORA_CONFIG_H
#define LORA_CONFIG_H

#include <stdint.h>

namespace lora {

// --- Radio Pin Definitions ---
constexpr uint8_t RFM95_CS_PIN  = 15;
constexpr uint8_t RFM95_RST_PIN = 33;
constexpr uint8_t RFM95_INT_PIN = 32;

// --- Protocol & Buffer Settings ---
constexpr uint8_t MAX_PAYLOAD_SIZE = 200; // Max bytes for user data in a packet
constexpr uint8_t HEADER_SIZE = 6;      // Size of the LoRaPacket header
constexpr uint8_t MAX_BUFFERED_PACKETS = 10; // Max packets waiting for ACK or to be sent

// --- Timing Configuration (milliseconds) ---
constexpr unsigned long ACK_TIMEOUT_MS = 2000; // Time to wait for an ACK before retrying
constexpr unsigned long SEND_TIMEOUT_MS = 3000; // Max time to wait for TX_DONE interrupt/signal

// --- Reliability Settings ---
constexpr uint8_t MAX_SEND_RETRIES = 3; // Max attempts to resend a packet if ACK not received
constexpr uint8_t MAX_CONSECUTIVE_TX_TIMEOUTS = 3; // Consecutive TX timeouts before trying radio reset

// --- Packet Flags (Example) ---
namespace flags {
    constexpr uint8_t NONE    = 0x00;
    constexpr uint8_t REQ_ACK = 0x01; // Request an acknowledgment for this packet
    // Add other flags as needed (e.g., LAST_CHUNK, ERROR, etc.)
    // constexpr uint8_t LAST_CHUNK = 0x02;
} // namespace flags

// --- Packet Types/Subtypes (Example - Define yours based on application needs) ---
namespace types {
    constexpr uint8_t DATA = 0x01;
    constexpr uint8_t ACK  = 0x02;
    // Add more types...
} // namespace types

namespace subtypes {
    constexpr uint8_t SENSOR_READING = 0x01;
    constexpr uint8_t STATUS_UPDATE  = 0x02;
    constexpr uint8_t COMMAND        = 0x03;
    // Add more subtypes...
} // namespace subtypes


} // namespace lora

#endif // LORA_CONFIG_H