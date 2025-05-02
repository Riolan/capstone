// File: bt_impl.cpp
#include "bt_impl.hpp"
#include "node_shared.hpp" // Include for LoRa types, SendManager, constants, NodeStatus, lora::getAllNodeStatuses
#include "packet_types.h"  // For LoRa packet types (assumed definitions like PACKET_TYPE_CONFIG_UPDATE, PACKET_TYPE_IMAGE_REQUEST might be here)
#include "SendManager.hpp" // For access to lora::SendManager::getInstance()

// Include necessary Arduino and BLE headers if not pulled in by other includes
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLESecurity.h>
#include <esp_gap_ble_api.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <queue>
#include <string>
#include <vector>
#include <stdexcept> // For stoi exceptions
#include <algorithm> // For std::min
#include <cstdio>    // For snprintf

// Define UUIDs if not defined elsewhere (replace with actual UUIDs)
// Define MAX_BLE_CHUNK_SIZE if not defined elsewhere

namespace bt {

    // --- Global Variable Definitions ---
    bool deviceConnected = false;
    BLECharacteristic *pCharacteristic = nullptr;
    BLEServer *pServer = nullptr;
    BLEService *pService = nullptr;

    std::queue<BLEOutgoingPacket> bleOutgoingQueue;
    std::string incomingBLEBuffer; // Buffer for accumulating incoming BLE data
    unsigned long lastBLESendTime = 0;
    const unsigned long BLE_SEND_INTERVAL = 10; // Min ms between BLE notifications
    const size_t BLE_CHUNK_PAYLOAD_SIZE = 20; // Max data bytes per notification chunk (Adjust based on MTU & testing)





    // --- Callback Classes ---
    class MyServerCallbacks : public BLEServerCallbacks {
        void onConnect(BLEServer* pServerInstance) override { // Use override keyword
            deviceConnected = true;
            Serial.println("BLE Device connected");
            // Request MTU update from client for potentially larger packets
            // Max MTU is 517, but characteristic limit might be lower (check library/stack)
            // Using a reasonable value here, adjust as needed.
           /// pServerInstance->updatePeerMTU(pServerInstance->getConnId(), MAX_BLE_CHUNK_SIZE);
            //Serial.println("Requested MTU update.");
            // Stop advertising once connected? Optional.
            // BLEDevice::getAdvertising()->stop();
        }

        void onDisconnect(BLEServer* pServerInstance) override {
            deviceConnected = false;
            incomingBLEBuffer = ""; // Clear buffer on disconnect
            Serial.println("BLE Device disconnected");
            // Clear the outgoing queue? Optional, prevents sending stale data on reconnect.
             while(!bleOutgoingQueue.empty()) {
                 bleOutgoingQueue.pop();
             }
            delay(100); // Short delay before restarting advertising
            pServerInstance->startAdvertising(); // Use server instance to restart
            // Or use BLEDevice::startAdvertising(); if managing advertising globally
           // Serial.println("BLE Restarted advertising");
        }
    };

    class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {

        /**
         * @brief Called when the connected BLE client writes data to this characteristic.
         * This function buffers incoming data chunks until a newline terminator is received,
         * then processes the complete command.
         * @param pCharacteristicInstance Pointer to the characteristic that was written to.
         */
        void onWrite(BLECharacteristic* pCharacteristicInstance) override { // Use override keyword
            std::string value = pCharacteristicInstance->getValue(); // Get the data written by the client

            if (value.length() > 0) {
                // Log the received chunk (optional, useful for debugging)
                Serial.print("BLE Received Chunk (");
                Serial.print(value.length());
                Serial.print(" bytes): ");
                // Print hex for debugging non-printable chars
                // for(char c : value) { Serial.print((uint8_t)c, HEX); Serial.print(" "); }
                // Serial.println();

                // Append the received chunk to the buffer
                incomingBLEBuffer += value;

                // Check if the buffer now ends with the expected message terminator (newline)
                // The Android app should ensure it sends a newline after each complete command.
                if (incomingBLEBuffer.length() > 0 && incomingBLEBuffer.back() == '\n') {
                    Serial.print("BLE Full message received: ");
                    Serial.print(incomingBLEBuffer.c_str()); // Print the complete message (includes newline)

                    // Process the complete command string (using the member function)
                    processBLECommand(incomingBLEBuffer);

                    // Clear the buffer to be ready for the next message
                    incomingBLEBuffer = "";
                } else {
                    // The message is likely fragmented across multiple BLE writes.
                    // Wait for the next chunk(s) containing the terminator.
                    Serial.println("  Chunk buffered, waiting for terminator...");
                }
            }
        } // End onWrite

        // --- Command Processing Logic (Member Function) ---
        // This is the version called by onWrite
        void processBLECommand(const std::string& command) {
             std::string cmd = command;
             if (!cmd.empty() && cmd.back() == '\n') { cmd.pop_back(); } // Remove trailing newline

             Serial.print("Processing BLE command: ["); Serial.print(cmd.c_str()); Serial.println("]");

             if (cmd == "REQUEST_NODES") {
                 Serial.println("Processing REQUEST_NODES...");
                 sendNodeInformationToBLE(); // Call member helper to send node info

             } else if (cmd.rfind("CHANGE_ANIMALS", 0) == 0) { // Check if command starts with prefix
                 Serial.println("Processing CHANGE_ANIMALS...");
                 // Format expected: CHANGE_ANIMALS;nodeId;cap1,cap2,cap3,cap4
                 size_t firstSemi = cmd.find(';');
                 if (firstSemi != std::string::npos) {
                     size_t secondSemi = cmd.find(';', firstSemi + 1);
                     if (secondSemi != std::string::npos) {
                         try {
                             int nodeIdInt = std::stoi(cmd.substr(firstSemi + 1, secondSemi - firstSemi - 1));
                             if (nodeIdInt < 0 || nodeIdInt > 255) throw std::out_of_range("Node ID out of range 0-255");
                             uint8_t nodeId = static_cast<uint8_t>(nodeIdInt);

                             std::string capsStr = cmd.substr(secondSemi + 1);
                             uint8_t capabilities[4] = {0};
                             size_t currentPos = 0;
                             size_t nextPos = 0;
                             int capIndex = 0;

                             while (capIndex < 4 && (nextPos = capsStr.find(',', currentPos)) != std::string::npos) {
                                 int capVal = std::stoi(capsStr.substr(currentPos, nextPos - currentPos));
                                 if (capVal < 0 || capVal > 255) throw std::out_of_range("Capability out of range 0-255");
                                 capabilities[capIndex++] = static_cast<uint8_t>(capVal);
                                 currentPos = nextPos + 1;
                             }
                             // Get last capability
                             if (capIndex < 4 && currentPos < capsStr.length()) {
                                  int capVal = std::stoi(capsStr.substr(currentPos));
                                  if (capVal < 0 || capVal > 255) throw std::out_of_range("Capability out of range 0-255");
                                  capabilities[capIndex++] = static_cast<uint8_t>(capVal);
                             }

                             if (capIndex != 4) {
                                 Serial.println("Error: Expected 4 capability values.");
                                 queueStringToBLEAndChunk("Error: Invalid CHANGE_ANIMALS format - expected 4 capabilities\n");
                                 return;
                             }

                             // Log and send LoRa packet
                             Serial.print("   Sending config to Node 0x"); Serial.print(nodeId, HEX); Serial.print(": ");
                             for(int i=0; i<4; ++i) { Serial.print(capabilities[i]); Serial.print(" "); }
                             Serial.println();

                             // Construct LoRa packet (using example type 0x20)
                             // TODO: Replace 0x20 with named constant from packet_types.h if available
                             // e.g., PACKET_TYPE_CONFIG_UPDATE
                             lora::SendManager::getInstance().send(
                                 nodeId,
                                 0x20, // Example: PACKET_TYPE_CONFIG_UPDATE
                                 0,    // Subtype if needed
                                 capabilities,
                                 sizeof(capabilities),
                                 true // Require ACK for configuration changes
                             );
                            // Optionally send confirmation back via BLE
                            //queueStringToBLEAndChunk("OK: CHANGE_ANIMALS command sent to LoRa queue\n");

                         } catch (const std::invalid_argument& ia) {
                            // Serial.print("Error parsing CHANGE_ANIMALS numbers: "); Serial.println(ia.what());
                            // queueStringToBLEAndChunk("Error: Invalid number format in CHANGE_ANIMALS\n");
                         } catch (const std::out_of_range& oor) {
                            // Serial.print("Error parsing CHANGE_ANIMALS values out of range: "); Serial.println(oor.what());
                            // queueStringToBLEAndChunk("Error: Value out of range in CHANGE_ANIMALS\n");
                         }
                     } else {
                         // Serial.println("Error: Invalid CHANGE_ANIMALS format (missing second ';').");
                         // queueStringToBLEAndChunk("Error: Invalid CHANGE_ANIMALS format (missing second ';')\n");
                     }
                 } else {
                     // Serial.println("Error: Invalid CHANGE_ANIMALS format (missing first ';').");
                     // queueStringToBLEAndChunk("Error: Invalid CHANGE_ANIMALS format (missing first ';')\n");
                 }

            } else if (cmd.rfind("REQUEST_IMAGE", 0) == 0) { // Check prefix
                 Serial.println("Processing REQUEST_IMAGE...");
                 // Format expected: REQUEST_IMAGE;nodeId;eventUUID
                 size_t firstSemi = cmd.find(';');
                 size_t secondSemi = (firstSemi != std::string::npos) ? cmd.find(';', firstSemi + 1) : std::string::npos; // Find second semicolon

                 // Check if both semicolons were found and there's data after the second one
                 if (firstSemi != std::string::npos && secondSemi != std::string::npos && secondSemi < cmd.length() - 1) {
                      try {
                          // --- Parse Node ID ---
                          std::string nodeIdStr = cmd.substr(firstSemi + 1, secondSemi - firstSemi - 1);
                          int nodeIdInt = std::stoi(nodeIdStr);
                          if (nodeIdInt < 0 || nodeIdInt > 255) throw std::out_of_range("Node ID out of range 0-255");
                          uint8_t nodeId = static_cast<uint8_t>(nodeIdInt);

                          // --- Parse Event UUID ---
                          std::string eventUUIDStr = cmd.substr(secondSemi + 1);
                          // Use stoul for unsigned long (uint32_t fits within unsigned long)
                          unsigned long eventUUID_ul = std::stoul(eventUUIDStr);
                          // Optional: Check if it fits uint32_t if necessary, though stoul might throw out_of_range
                          if (eventUUID_ul > UINT32_MAX) throw std::out_of_range("Event UUID out of range for uint32_t");
                          uint32_t eventUUID = static_cast<uint32_t>(eventUUID_ul);

                         // Serial.print("  Setting flag for LoRa image request - Node: 0x"); Serial.print(nodeId, HEX);
                         // Serial.print(", EventUUID: 0x"); Serial.println(eventUUID, HEX);

                          // --- Set Flag and Params ---
                          // taskENTER_CRITICAL(&loraRequestMutex); // Optional lock
                          if (lora_request_pending) {
                           //  Serial.println("WARN: Overwriting previous pending LoRa request!");
                             queueStringToBLEAndChunk("Error: Previous LoRa command still pending\n");
                          }
                          lora_req_targetNodeID = nodeId;
                          lora_req_type = lora::PACKET_TYPE_COMMAND;
                          lora_req_subtype = lora::SUBTYPE_REQUEST_IMAGE;

                          // *** Pack UUID into payload ***
                          lora_req_payload[0] = (eventUUID >> 24) & 0xFF;
                          lora_req_payload[1] = (eventUUID >> 16) & 0xFF;
                          lora_req_payload[2] = (eventUUID >> 8) & 0xFF;
                          lora_req_payload[3] = eventUUID & 0xFF;
                          lora_req_payload_len = sizeof(uint32_t); // Set length to 4 bytes
                          // *** End Pack UUID ***

                          lora_req_requireAck = true; // Command itself requires ACK
                          lora_request_pending = true; // Set flag LAST
                          // taskEXIT_CRITICAL(&loraRequestMutex); // Optional unlock
                          // --- End Set Flag ---

                         // queueStringToBLEAndChunk("OK: REQUEST_IMAGE command flagged for LoRa\n");

                      } catch (const std::invalid_argument& ia) {
                          //Serial.print("Error parsing REQUEST_IMAGE IDs: "); Serial.println(ia.what());
                        //  queueStringToBLEAndChunk("Error: Invalid number format in REQUEST_IMAGE command\n");
                      } catch (const std::out_of_range& oor) {
                        //  Serial.print("Error parsing REQUEST_IMAGE IDs out of range: "); Serial.println(oor.what());
                       //   queueStringToBLEAndChunk("Error: ID value out of range in REQUEST_IMAGE command\n");
                      }
                 } else {
                    //  Serial.println("Error: Invalid REQUEST_IMAGE format (missing ';nodeId;eventUUID').");
                    //  queueStringToBLEAndChunk("Error: Invalid REQUEST_IMAGE format (missing ';nodeId;eventUUID')\n");
                 }
            } // End REQUEST_IMAGE handling
             else {
              //   Serial.println("Unknown BLE command received.");
                 // Send an error message back via BLE using the chunking helper
                // queueStringToBLEAndChunk("Error: Unknown command\n");
            }
        } // End processBLECommand (Member)


        // --- Send Node Information (Member Function) ---
        // This is the version called by the member processBLECommand
        void sendNodeInformationToBLE() {
           // Serial.println("Gathering node information via accessors to send via BLE...");
            std::string nodeInfoStr = "NODES_INFO_START\n"; // Start marker

            // Use the accessor function to get the current node statuses
            // Assumes NodeStatus struct and getAllNodeStatuses are defined in node_shared.hpp/cpp
            std::vector<NodeStatus> currentStatuses;
            lora::getAllNodeStatuses(currentStatuses); // Fetch data from main logic

            if (currentStatuses.empty()) {
                nodeInfoStr += "No nodes currently tracked.\n";
            } else {
                for (const auto& status : currentStatuses) { // Iterate through the returned vector
                    nodeInfoStr += "NodeID:";
                    nodeInfoStr += std::to_string(status.nodeID);

                    nodeInfoStr += ";Status:";
                    nodeInfoStr += (status.online ? "Online" : "Offline");

                    nodeInfoStr += ";LastSeen:";
                    // Calculate seconds ago, handle potential offline case where lastSeen might be 0
                    unsigned long secondsAgo = (status.lastSeen > 0) ? (millis() - status.lastSeen) / 1000 : 999999; // Use a large number for never/offline
                    nodeInfoStr += std::to_string(secondsAgo);
                    nodeInfoStr += "s"; // Add 's' suffix for clarity

                    // Add DeviceID (assuming it's part of NodeStatus now)
                    if (status.deviceID != 0) { // Only add if valid
                        char hexBuf[12]; // Buffer for hex string ("0x" + 8 hex chars + null)
                        snprintf(hexBuf, sizeof(hexBuf), "0x%08lX", (long unsigned int)status.deviceID); // Use %lX for uint32_t, cast needed for some compilers
                        nodeInfoStr += ";DevID:";
                        nodeInfoStr += hexBuf;
                    }

                    // Add Battery Percentage (assuming it's part of NodeStatus)
                    nodeInfoStr += ";Battery:";
                    if (status.batteryPercent <= 100) { // Check if battery value is valid (0-100)
                        nodeInfoStr += std::to_string(status.batteryPercent);
                        nodeInfoStr += "%"; // Add '%' suffix
                    } else {
                        nodeInfoStr += "N/A"; // Indicate unknown/invalid battery level
                    }

                    // Add any other relevant fields from NodeStatus here...
                    // nodeInfoStr += ";RSSI:";
                    // nodeInfoStr += std::to_string(status.lastRSSI);
                    // nodeInfoStr += ";SNR:";
                    // nodeInfoStr += std::to_string(status.lastSNR);


                    nodeInfoStr += "\n"; // Newline for each node entry
                }
            }

            nodeInfoStr += "NODES_INFO_END\n"; // End marker

            // Queue the potentially large string for sending using the chunking helper
            queueStringToBLEAndChunk(nodeInfoStr);
            //Serial.print("Node information queued for BLE. Total Size: "); Serial.println(nodeInfoStr.length());
           // Serial.print("Node information: "); Serial.println(nodeInfoStr.c_str()); // Print the complete message (for debugging)

        } // End sendNodeInformationToBLE (Member)

    }; // End MyCharacteristicCallbacks class


    class MySecurityCallbacks : public BLESecurityCallbacks {


        bool onConfirmPIN(uint32_t pin) override {
            // N/A for Just Works or NoInputNoOutput devices
            Serial.println("onConfirmPIN: " + String(pin));
            return true;
        }
    
 
        uint32_t onPassKeyRequest() override {
            // N/A for Just Works or NoInputNoOutput device
            return 0;
        }
    

        void onPassKeyNotify(uint32_t pass_key) override {
            Serial.println("onPassKeyNotify: " + String(pass_key));
            // Cannot display, so just log if needed.
        }

        // If further security implementation is needed, likely
        // looking to use this.
        void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
            Serial.print("Authentication Complete: ");
            if (cmpl.success) {
                Serial.println("Success!");
                Serial.print("Bonded: ");

            } else {
                Serial.print("Failure! Reason: 0x");
                Serial.println(cmpl.fail_reason, HEX);
            }
        }
    

        bool onSecurityRequest() override {
            Serial.println("onSecurityRequest");
            return true; // Accept pairing requests
        }
    
    };
    
    // --- Public Function Implementations ---

    void setup() {
        // Initialize BLE Device
        BLEDevice::init("MotherNode_LoRa_GW (ESP32)"); // Set device name
        // Optional: Set power level
        // BLEDevice::setPower(ESP_PWR_LVL_P9); // Example: Max power (+9dBm)
        BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());

        BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);


        esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;

        esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE,  &iocap, sizeof(uint8_t));
        int8_t auth_req = ESP_LE_AUTH_REQ_SC_BOND; // Secure Connections + Bonding (NO MITM possible)
        esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));


        // Set Key Distribution & Size
        uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
        uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
        uint8_t key_size = 16;
        esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_MIN_KEY_SIZE, &key_size, sizeof(uint8_t)); // Enforce max size


        // Create the BLE Server
        pServer = BLEDevice::createServer();

        pServer->setCallbacks(new MyServerCallbacks()); // Set server callbacks

        // Create the BLE Service
        pService = pServer->createService(SERVICE_UUID);

        // Create a BLE Characteristic
        pCharacteristic = pService->createCharacteristic(
                                        CHARACTERISTIC_UUID,
                                        BLECharacteristic::PROPERTY_READ   | // Allow reading current value (optional)
                                        BLECharacteristic::PROPERTY_WRITE  | // Allow writing commands from client
                                        BLECharacteristic::PROPERTY_NOTIFY   // Allow sending notifications (data to client)
                                        // Use PROPERTY_WRITE_NR for Write Without Response if app supports it & ACKs aren't needed for writes
                                      );

        // Add descriptor for notifications (Client Characteristic Configuration Descriptor - CCCD)
        // This is necessary for the client to enable notifications.
        pCharacteristic->addDescriptor(new BLE2902());

        // Set Characteristic callbacks (handles incoming writes and potentially read requests)
        pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

        // Set an initial value (optional)
        // pCharacteristic->setValue("Ready");

        // Start the service
        pService->start();

        // Start Advertising
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(SERVICE_UUID); // Advertise the service
        pAdvertising->setScanResponse(true); // Allows clients to scan for more info before connecting
        // Min/max advertising interval settings influence connection time and power consumption
        // Example values (units are 0.625 ms)
        // pAdvertising->setMinPreferred(0x06); // Default: 30-60ms range -> 0x30 - 0x60 -> 48 - 96
        // pAdvertising->setMaxPreferred(0x12); // Default: 100-1000ms range -> 0x64 - 0x400 -> 100 - 1024 approx? Check ESP-IDF docs
        pAdvertising->setMinInterval(160); // units of 0.625ms -> 160*0.625 = 100ms
        pAdvertising->setMaxInterval(320); // units of 0.625ms -> 320*0.625 = 200ms

        BLEDevice::startAdvertising();
        Serial.println("BLE Service Started. Waiting for client connection...");
    }

    void loop() {
        unsigned long now = millis();

        // Handle outgoing BLE queue only if connected
        if (deviceConnected && !bleOutgoingQueue.empty()) {
            // Rate limit notifications to avoid overwhelming BLE stack or client
            if (now - lastBLESendTime >= BLE_SEND_INTERVAL) {
                // Process one packet from the queue
                BLEOutgoingPacket& packet = bleOutgoingQueue.front(); // Get reference

                // Send data via notification
                // Ensure packet size doesn't exceed negotiated MTU - characteristic size limit.
                // The stack *might* handle fragmentation, but it's safer to chunk manually.
                // Our BLE_CHUNK_PAYLOAD_SIZE should be conservative.
                pCharacteristic->setValue(packet.data.data(), packet.data.size());
                pCharacteristic->notify(); // Send the notification

                // Debug print (optional)
                // Serial.print("BLE Sent chunk: "); Serial.print(packet.data.size()); Serial.println(" bytes.");

                bleOutgoingQueue.pop(); // Remove packet from queue *after* sending attempt
                lastBLESendTime = now; // Update last send time
            }
        }

        // Note: Incoming data is handled entirely by the onWrite callback.
        // No need for explicit checking/polling for incoming BLE data here.

        // Other non-BLE tasks for the main loop could go here
    }

    // --- Helper function to queue raw data in chunks ---
    bool queueDataForBLEInChunks(const uint8_t* data, size_t length) {
        if (!data || length == 0) {
            Serial.println("queueDataForBLEInChunks: No data or zero length.");
            return false;
        }
        Serial.print("Queueing "); Serial.print(length); Serial.println(" bytes for BLE in chunks...");

        size_t offset = 0;
        while (offset < length) {
            size_t chunkSize = std::min(BLE_CHUNK_PAYLOAD_SIZE, length - offset);
            BLEOutgoingPacket chunkPacket;
            // Use vector's constructor that takes iterators/pointers for efficiency
            chunkPacket.data.assign(data + offset, data + offset + chunkSize); // Copy chunk data

            bleOutgoingQueue.push(std::move(chunkPacket)); // Add this chunk packet to the queue efficiently
            // Serial.print("  Queued chunk: size="); Serial.println(chunkSize); // Verbose

            offset += chunkSize;
        }
        // Serial.println("All chunks queued.");
        return true;
    }

    // --- Helper function to queue a string using the chunking helper ---
    bool queueStringToBLEAndChunk(const std::string& message) {
        if (message.empty()) {
            Serial.println("queueStringToBLEAndChunk: Empty message.");
            return false;
        }
        // Call the raw data helper function to handle chunking and queueing
        return queueDataForBLEInChunks(reinterpret_cast<const uint8_t*>(message.c_str()), message.length());
    }

    // Function to queue a string for sending TO the BLE client
    bool queueStringToBLE(const std::string& message) {
        if (message.empty()) {
            return false;
        }
        // Queue the string data (including null terminator if needed by receiver?)
        // Usually null terminator isn't sent over BLE unless protocol requires it.
        return queueDataForBLE(reinterpret_cast<const uint8_t*>(message.c_str()), message.length());
    }


    // --- Optional: Keep original queueDataForBLE if needed for single, guaranteed small packets ---
    // Consider if this is truly needed or if all sends should use chunking for consistency.
    bool queueDataForBLE(const uint8_t* data, size_t length) {
         if (!data || length == 0) {
             return false;
         }
         // Check if data is too large for a single chunk according to our defined limit
         if (length > BLE_CHUNK_PAYLOAD_SIZE) {
             //Serial.print("WARN: queueDataForBLE called with large packet (");
           //  Serial.print(length);
           //  Serial.print(" bytes > ");
           //  Serial.print(BLE_CHUNK_PAYLOAD_SIZE);
          //   Serial.println("). Use queueDataForBLEInChunks instead, or increase BLE_CHUNK_PAYLOAD_SIZE.");
             // Option 1: Fail
             // return false;
             // Option 2: Automatically use chunking (might be unexpected behavior)
              return queueDataForBLEInChunks(data, length);
             // Option 3: Send anyway and hope the underlying stack handles it (risky)
             // Fall through to send... (Not recommended without understanding stack limits)
         }

         // If size is acceptable, queue as a single packet
         BLEOutgoingPacket packet;
         packet.data.assign(data, data + length);
         bleOutgoingQueue.push(std::move(packet));
         // Serial.print("Queued single packet: size="); Serial.println(length); // Verbose
         return true;
     }

} // namespace bt