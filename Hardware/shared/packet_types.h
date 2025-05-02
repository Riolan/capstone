#pragma once
#ifndef PACKET_TYPES_H
#define PACKET_TYPES_H
#include <stdint.h>
namespace lora {
// This file defines the packet type and subtype identifiers for LoRa communication
// in the Smart Trail Camera (STC) system.

//------------------------------------------------------------------------------
// Packet Types (Main Identifier - typically buffer[0])
//------------------------------------------------------------------------------

constexpr uint8_t PACKET_TYPE_JOIN_REQ       = 0x01; // Edge -> Mother: Request to join network
constexpr uint8_t PACKET_TYPE_JOIN_ACK       = 0x02; // Mother -> Edge: Acknowledge join, assign ID
constexpr uint8_t PACKET_TYPE_HEARTBEAT      = 0x03; // Bidirectional: Keepalive and basic status
constexpr uint8_t PACKET_TYPE_ACK            = 0x04; // Bidirectional: Acknowledge reliable packet
constexpr uint8_t PACKET_TYPE_DATA           = 0x05; // Edge -> Mother: Events, Status (requires subtype)
constexpr uint8_t PACKET_TYPE_IMAGE_CHUNK    = 0x06; // Edge -> Mother: Part of an image file
constexpr uint8_t PACKET_TYPE_CONFIG_UPDATE  = 0x07; // Mother -> Edge: Send new configuration
constexpr uint8_t PACKET_TYPE_COMMAND        = 0x08; // Mother -> Edge: Request action (requires subtype)

//------------------------------------------------------------------------------
// Subtypes for PACKET_TYPE_DATA (Edge -> Mother - typically buffer[1])
//------------------------------------------------------------------------------

constexpr uint8_t SUBTYPE_DETECTION          = 0x01; // Animal detection event details
constexpr uint8_t SUBTYPE_BATTERY_STATUS     = 0x02; // Report battery level
constexpr uint8_t SUBTYPE_STORAGE_STATUS     = 0x03; // Report SD card status
constexpr uint8_t SUBTYPE_DIFFICULTY_VIEWING = 0x04; // Report operational issue (e.g., lens obscured)
constexpr uint8_t SUBTYPE_CONFIG_ACK         = 0x05; // Acknowledge receiving/applying config
constexpr uint8_t SUBTYPE_IMAGE_UPLOAD_STATUS= 0x06; // Report status of image upload attempt
constexpr uint8_t SUBTYPE_CURRENT_CONFIG     = 0x07; // Report current configuration (response to request)

//------------------------------------------------------------------------------
// Subtypes for PACKET_TYPE_COMMAND (Mother -> Edge - typically buffer[1])
//------------------------------------------------------------------------------

constexpr uint8_t SUBTYPE_REQUEST_IMAGE      = 0x01; // Request upload of a specific image
// Structure for SUBTYPE_DETECTION payload:
// uint8_t  subtype;           // 1 byte (SUBTYPE_DETECTION)
// uint32_t eventUUID;         // 4 bytes
// uint32_t deviceID;          // 4 bytes <<< ADDED
// uint8_t  animalMask;        // 1 byte
// uint8_t  confidenceLevel;   // 1 byte (0-100)
// uint16_t bbox_x;            // 2 bytes
// uint16_t bbox_y;            // 2 bytes
// uint16_t bbox_width;        // 2 bytes
// uint16_t bbox_height;       // 2 bytes
// uint32_t eventTimestamp;    // 4 bytes (Optional - seconds since epoch/boot)
// TOTAL: ~26 bytes (adjust based on included fields)
// Example structure for SUBTYPE_REQUEST_IMAGE payload:
// uint32_t eventUUID;         // 4 bytes

// Example structure for PACKET_TYPE_IMAGE_CHUNK payload metadata (before chunk data):
// uint32_t eventUUID;         // 4 bytes
// uint16_t chunkNumber;       // 2 bytes
// uint16_t totalChunks;       // 2 bytes
// TOTAL METADATA: 8 bytes



constexpr uint8_t SUBTYPE_REQUEST_STATUS     = 0x02; // Request immediate status report
constexpr uint8_t SUBTYPE_REQUEST_CONFIG     = 0x03; // Request current configuration settings
constexpr uint8_t SUBTYPE_REBOOT             = 0x04; // Command the node to reboot

//------------------------------------------------------------------------------
// Parameter Types for PACKET_TYPE_CONFIG_UPDATE (Used in Payload)
//------------------------------------------------------------------------------
// These define *what* setting is being updated. The packet payload will contain
// this type identifier followed by the actual value(s).

constexpr uint8_t CONFIG_PARAM_DETECTION_THRESHOLD= 0x10; // Single Confidence threshold for all detections (0-100)
// Removed individual animal thresholds:
// constexpr uint8_t CONFIG_PARAM_THRESHOLD_SQUIRREL= 0x11
// constexpr uint8_t CONFIG_PARAM_THRESHOLD_CAT     = 0x12
// constexpr uint8_t CONFIG_PARAM_THRESHOLD_DOG     = 0x13
// constexpr uint8_t CONFIG_PARAM_THRESHOLD_BIRD    = 0x14

constexpr uint8_t CONFIG_PARAM_ALERT_MASK        = 0x20; // Bitmask enabling/disabling alerts per animal class (uses ANIMAL_MASK bits)
constexpr uint8_t CONFIG_PARAM_EVENT_RATE        = 0x30; // Minimum time between sending detection events (e.g., seconds)
constexpr uint8_t CONFIG_PARAM_HEARTBEAT_INTERVAL= 0x40; // Interval for edge node sending heartbeats (e.g., seconds)
constexpr uint8_t CONFIG_PARAM_SYSTEM_IDENTIFIER = 0x50; // Set a user-defined identifier/name (requires string payload)

//------------------------------------------------------------------------------
// Status Codes (Used in Payloads like CONFIG_ACK, IMAGE_UPLOAD_STATUS)
//------------------------------------------------------------------------------
constexpr uint8_t STATUS_CODE_OK             = 0x00;
constexpr uint8_t STATUS_CODE_ERROR_GENERIC  = 0x01;
constexpr uint8_t STATUS_CODE_ERROR_INVALID_PARAM= 0x02;
constexpr uint8_t STATUS_CODE_ERROR_SD_CARD  = 0x03;
constexpr uint8_t STATUS_CODE_ERROR_APPLY    = 0x04; // Failed to apply config/command
constexpr uint8_t STATUS_CODE_ERROR_NOT_FOUND= 0x05; // e.g., Image ID not found

//------------------------------------------------------------------------------
// Error Codes (Used in Payloads like DIFFICULTY_VIEWING, STORAGE_STATUS)
//------------------------------------------------------------------------------
constexpr uint8_t ERROR_CODE_NONE            = 0x00; // No error
constexpr uint8_t ERROR_CODE_LENS_OBSCURED   = 0x01;
constexpr uint8_t ERROR_CODE_SENSOR_FAILURE  = 0x02;
constexpr uint8_t ERROR_CODE_AI_FAILURE      = 0x03;
constexpr uint8_t ERROR_CODE_SD_INIT_FAILED  = 0x10;
constexpr uint8_t ERROR_CODE_SD_WRITE_FAILED = 0x11;
constexpr uint8_t ERROR_CODE_SD_READ_FAILED  = 0x12;
constexpr uint8_t ERROR_CODE_SD_FULL         = 0x13;


//------------------------------------------------------------------------------
// Animal Class Bitmasks (Example - Define based on your AI model output)
// Allows combinations (e.g., ANIMAL_MASK_SQUIRREL | ANIMAL_MASK_BIRD)
//------------------------------------------------------------------------------
constexpr uint8_t ANIMAL_MASK_NONE           = 0x00; // No target animal detected
constexpr uint8_t ANIMAL_MASK_SQUIRREL       = 0x01; // Bit 0
constexpr uint8_t ANIMAL_MASK_BIRD           = 0x02; // Bit 1
constexpr uint8_t ANIMAL_MASK_CAT            = 0x04; // Bit 2
constexpr uint8_t ANIMAL_MASK_DOG            = 0x08; // Bit 3
// Add others using the next bits (0x10,= 0x20,= 0x40,= 0x80)

}; // end namespace lora
#endif; // PACKET_TYPES_H