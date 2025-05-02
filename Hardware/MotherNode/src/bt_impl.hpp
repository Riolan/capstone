#pragma once
#ifndef BT_IMPL_HPP
#define BT_IMPL_HPP
#undef dump
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <queue>
#include <string> 

namespace bt {

    const char* const SERVICE_UUID = "12345678-1234-1234-1234-123456789012";
    const char* const CHARACTERISTIC_UUID = "87654321-4321-4321-4321-210987654321";
    const size_t MAX_BLE_CHUNK_SIZE = 100; // Max MTU size we might handle

    // Structure for outgoing BLE data queue
    // Using std::vector<uint8_t> for dynamic size without raw new/delete
    struct BLEOutgoingPacket {
        std::vector<uint8_t> data;
    };

    // --- Global Variable Declarations (defined in bt_impl.cpp) ---
    extern bool deviceConnected;
    extern BLECharacteristic *pCharacteristic; // Pointer to the characteristic
    extern std::queue<BLEOutgoingPacket> bleOutgoingQueue; // Queue for data going TO the phone app

    // --- Function Declarations ---

    /**
     * @brief Initializes the BLE server, service, characteristic, and starts advertising.
     * Call this once in the main setup().
     */
    void setup();

    /**
     * @brief Handles BLE processing. Sends queued outgoing data if connected.
     * Call this frequently in the main loop().
     */
    void loop();

    /**
     * @brief Queues data to be sent over BLE notification.
     * Use this function from other parts of your code (e.g., main.cpp)
     * to send data received via LoRa to the connected BLE device.
     * Data might be split into chunks within bt::loop().
     * @param data Pointer to the data buffer.
     * @param length Length of the data in the buffer.
     * @return true if successfully queued, false otherwise (e.g., queue full - though std::queue grows).
     */
    bool queueDataForBLE(const uint8_t* data, size_t length);

    /**
     * @brief Queues a string message to be sent over BLE notification.
     * Convenience function for sending text. Appends a newline.
     * @param message The string message to send.
     * @return true if successfully queued, false otherwise.
     */
    bool queueStringToBLE(const std::string& message);
    // Forward declarations for helper functions used within callbacks
   
   
    bool queueDataForBLEInChunks(const uint8_t* data, size_t length);
    bool queueStringToBLEAndChunk(const std::string& message);
    void sendNodeInformationToBLE();

} // namespace bt

#endif // BT_IMPL_HPP
// End of bt_impl.hpp