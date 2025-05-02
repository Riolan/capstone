#include <SPI.h>
#include <Seeed_Arduino_SSCMA.h> // Assuming this is needed for AI
//#include "RH_RF95.h"    // Must be included before declaring rf95
//#include "RadioHead.h" // Include RadioHead library for LoRa communication
#include "node_shared.hpp" // Includes LoRaPacket, constants, rf95 declaration
// #include "ReceiveManager.hpp" // Include if receiver object is actually used beyond the callback setup
#include "SendManager.hpp"    // Include SendManager definition for getInstance()
#include "JoinManager.hpp"    // Contains declaration for performJoin
#include "packet_types.h"     // Contains packet type definitions
#include "jpeg_test.hpp" // Include if using JPEG sample data

// --- Function Prototype ---
int estimateBatteryPercentage(float voltage);

#include <map>

std::map<uint32_t, String> uuid_image_map;
String currentImageDataString = ""; 

#define PIR_PIN 33 // Pin for PIR sensor (GPIO34)

/// --- Battery Information ---
const int VBAT_PIN = 35; // ESP32 Huzzah internal battery sense pin (A13) - Verify for your board!
const float ADC_REF_VOLTAGE = 3.3; // Or measure your actual 3.3V rail
const int ADC_RESOLUTION = 4096; // 12-bit ADC
// Check your board's schematic! The divider factor might vary.
// Adafruit Feathers often use two 100k resistors (Factor = 2.0)
// Some other boards might use different values.
const float VOLTAGE_DIVIDER_FACTOR = 2.0;
const float EMA_ALPHA = 0.2; // Smoothing factor for battery percentage

// How often to check the battery voltage (e.g., every 60 seconds)
const unsigned long BATTERY_CHECK_INTERVAL = 60000;
// How often to send the battery status over LoRa (e.g., every 10 minutes)
const unsigned long BATTERY_SEND_INTERVAL = 600000;
float smoothedBatteryVoltage = -1.0; // Use voltage for EMA for better accuracy
uint8_t lastSentBatteryPercent = 255; // Store last sent value (init to invalid)
unsigned long lastBatteryCheck = 0;
unsigned long lastBatterySend = 0;


// --- Image Upload State ---
enum class ImageUploadState {
    IDLE,             // Not currently uploading
    SENDING_CHUNK,    // Ready to send the next chunk (or waiting for SendManager to be idle)
    WAITING_CHUNK_ACK // Chunk sent, waiting for specific ACK from Mother Node
};
ImageUploadState imageUploadState = ImageUploadState::IDLE;
const uint8_t* imageDataSourcePtr = nullptr; // Pointer to image data (e.g., testImageJpegBytes)
size_t totalImageSize = 0;             // Total size of the image being sent
size_t imageBytesSent = 0;             // How many bytes have been successfully ACKed
uint16_t currentChunkNumber = 0;       // Index of the next chunk to *send* (0-based)
uint16_t totalChunks = 0;              // Total number of chunks for the current image
uint32_t currentImageIdentifier = 0;   // ID of the image being uploaded (from request)
uint8_t imageUploadTargetNodeID = 0;  // Who requested the image (Mother Node ID)
uint8_t lastSentChunkSeqNum = 0;      // Track sequence number of the chunk waiting for ACK
unsigned long chunkSendTimestamp = 0;   // Time the chunk waiting for ACK was *successfully initiated* by SendManager
const unsigned long IMAGE_CHUNK_ACK_TIMEOUT = ACK_TIMEOUT + 2000; // Allow more time for chunk ACKs (ACK_TIMEOUT is from node_shared.hpp)
uint8_t chunkResendAttempts = 0;
const uint8_t MAX_CHUNK_RESEND_ATTEMPTS = 3;

// --- Global Variables ---
// lora::ReceiveManager receiver; // Declare if used. Callback is set, but loop handles reception directly?
SSCMA AI;

bool joined = false;         // Flag indicating if node has joined the network
uint8_t myNodeID = 0xFF;     // This node's assigned ID (0xFF = unassigned)
uint32_t deviceID = 0;       // Unique device ID (e.g., from MAC)
unsigned long lastSend = 0;      // Timestamp for last data send attempt
unsigned long lastHeartbeat = 0; // Timestamp for last heartbeat send
unsigned long lastMotherHeartbeat = 0;

// Timeout in milliseconds for Mother Node heartbeat
#define MOTHER_HEARTBEAT_TIMEOUT 90000 // e.g., 30 seconds (adjust as needed)
// Assume Mother Node ID is 0x00
constexpr uint8_t MOTHER_NODE_ID  = 0x00;

// --- Function Implementations ---

// Simple check if channel is free using CAD
bool channelIsClear() {
    // Note: waitCAD() is blocking, but usually very short.
    // For truly non-blocking, a state machine checking CAD results would be needed.
    return lora::rf95.waitCAD();
}



#include <esp_timer.h> // For microsecond timer
// Not a cryptographically secure UUID, but sufficient for our purpose
// Generates a unique event UUID based on time and node ID
uint32_t generateEventUUID() {
    // Get current time in microseconds
    uint64_t micros = esp_timer_get_time();

    // Use lower 24 bits of time and upper 8 bits for Node ID
    // This gives ~16 seconds before the time part wraps around.
    // If events can happen faster than that causing collision risk, use more time bits.
    uint32_t time_part = (uint32_t)(micros & 0x00FFFFFF); // Lower 24 bits
    uint8_t node_part = (myNodeID != 0xFF) ? myNodeID : (uint8_t)(deviceID & 0xFF); // Use assigned ID or part of device ID

    return ((uint32_t)node_part << 24) | time_part;
}


// Function called when AI detects something
void onAnimalDetected(uint8_t detectedAnimalMask, uint8_t confidence, uint16_t x, uint16_t y, uint16_t w, uint16_t h, String image) {
    if (!joined || imageUploadState != ImageUploadState::IDLE) {
        Serial.println("Not joined or busy uploading, skipping detection alert send.");
        return; // Don't send if not joined or busy
    }

    // 1. Generate UUID for this event
    uint32_t eventUUID = generateEventUUID();
    uuid_image_map[eventUUID] = image;

    Serial.print("[HERE] Stored image data for UUID: 0x"); Serial.print(eventUUID, HEX);
    Serial.print("[HERE] (Size: "); Serial.print(image.length()); Serial.println(" chars)");


    // 2. TODO: Capture and Save Image to SD Card using eventUUID in filename
    // bool imageSaved = saveImageToSD(eventUUID, /* camera data */);
    // if (!imageSaved) {
    //     Serial.println("ERROR: Failed to save detection image to SD card!");
    //     // Decide whether to still send the alert without a saved image? Maybe send error code?
    // }

    Serial.print("Detection Event! UUID: 0x"); Serial.print(eventUUID, HEX);
    Serial.print(", Mask: 0x"); Serial.print(detectedAnimalMask, HEX);
    Serial.print(", Conf: "); Serial.println(confidence);

    // 3. Prepare Detection Alert Payload
    // Payload: UUID(4) + Mask(1) + Conf(1) + BBox(8) = 14 bytes
    // Add Timestamp(4) = 18 bytes
    const size_t detectionPayloadSize = 27;
    if (detectionPayloadSize > lora::MAX_PAYLOAD_SIZE) {
        Serial.println("ERROR: Detection payload exceeds MAX_PAYLOAD_SIZE!");
        return;
    }
    uint8_t payload[detectionPayloadSize];
    uint32_t timestamp = (uint32_t)millis() / 1000; // Example: seconds since boot

    // Pack data (Network Byte Order / Big Endian recommended)
    int offset = 0;
    // UUID
    payload[offset++] = (eventUUID >> 24) & 0xFF;
    payload[offset++] = (eventUUID >> 16) & 0xFF;
    payload[offset++] = (eventUUID >> 8) & 0xFF;
    payload[offset++] = eventUUID & 0xFF;
    // *** Device ID ***
    payload[offset++] = (deviceID >> 24) & 0xFF;
    payload[offset++] = (deviceID >> 16) & 0xFF;
    payload[offset++] = (deviceID >> 8) & 0xFF;
    payload[offset++] = deviceID & 0xFF;
    // Animal Mask
    payload[offset++] = detectedAnimalMask;
    // Confidence
    payload[offset++] = confidence;
    // BBox X
    payload[offset++] = (x >> 8) & 0xFF;
    payload[offset++] = x & 0xFF;
    // BBox Y
    payload[offset++] = (y >> 8) & 0xFF;
    payload[offset++] = y & 0xFF;
    // BBox W
    payload[offset++] = (w >> 8) & 0xFF;
    payload[offset++] = w & 0xFF;
    // BBox H
    payload[offset++] = (h >> 8) & 0xFF;
    payload[offset++] = h & 0xFF;
    // Timestamp
    payload[offset++] = (timestamp >> 24) & 0xFF;
    payload[offset++] = (timestamp >> 16) & 0xFF;
    payload[offset++] = (timestamp >> 8) & 0xFF;
    payload[offset++] = timestamp & 0xFF;

    // 4. Send Alert Packet
    Serial.println("Sending Detection Alert packet...");
    bool initiated = lora::SendManager::getInstance().send(
        myNodeID, // WAS MOTHER NODE BEFORE
        lora::PACKET_TYPE_DATA,
        lora::SUBTYPE_DETECTION,
        payload,
        offset, // Use actual size packed
        true // Require ACK for detection alerts
    );

    if (!initiated) {
        Serial.println("  Detection Alert send failed to initiate (radio busy?).");
        // Consider queuing or retrying later? For now, it's lost if busy.
    }
}



std::string coco_classes[] = {"cat", "dog", "squirrel", "bird"};
int coco_ids[] = {1, 2, 3, 4};
// --- Need to trigger onAnimalDetected from your AI logic ---
// Example placeholder in loop() - REPLACE THIS
void checkAIDetection() {
     // Simulate a detection periodically for testing
     static unsigned long lastFakeDetection = 0;
     //if (millis() - lastFakeDetection >110000) { // Every 25 seconds
     if (!AI.invoke(1, false, true)) {

         if (joined && imageUploadState == ImageUploadState::IDLE) {
            //Serial.println("Attempting AI Detection...");
              // Simulate: Squirrel detected with 90% confidence
            //  if (millis() - lastFakeDetection >11000) {
            //    Serial.println("Early return from AI.");
            //    return ;
           // }
              for (int i = 0; i < AI.boxes().size(); i++) {
     
                Serial.print("Box[");
                Serial.print(i);
                Serial.print("] target=");
                Serial.print(AI.boxes()[i].target);
                Serial.print(", score=");
                Serial.print(AI.boxes()[i].score);
                Serial.print(", x=");
                Serial.print(AI.boxes()[i].x);
                Serial.print(", y=");
                Serial.print(AI.boxes()[i].y);
                Serial.print(", w=");
                Serial.print(AI.boxes()[i].w);
                Serial.print(", h=");
                Serial.println(AI.boxes()[i].h);
            }
            if (AI.boxes().size() >= 1) {
                if ( AI.boxes()[0].score > 70) {
                    //lastFakeDetection = millis();
                std::string classes = coco_classes[AI.boxes()[0].target].c_str();
                Serial.print("Detected animal: ");
                Serial.println(String(classes.c_str()));
                 onAnimalDetected(lora::ANIMAL_MASK_SQUIRREL, AI.boxes()[0].score, AI.boxes()[0].x,  AI.boxes()[0].y,  AI.boxes()[0].w,  AI.boxes()[0].h, AI.last_image());
                }
            }
           // Serial.println("Sending AI Detection data!");
             // 
         }
    } else {

        Serial.println("> Invoke failed.");
    }
     //}
}








void sendNextImageChunk();
/*
void sendNextImageChunk() {
    if (imageUploadState != ImageUploadState::SENDING_CHUNK || !lora::SendManager::getInstance().isIdle()) { return; }
    if (imageBytesSent >= totalImageSize) {  return; }

    const size_t CHUNK_METADATA_OVERHEAD = 8;
    size_t payloadCapacity = lora::MAX_PAYLOAD_SIZE - CHUNK_METADATA_OVERHEAD;
    if (payloadCapacity <= 0) {  return; }
    size_t remainingBytes = totalImageSize - imageBytesSent;
    size_t chunkSize = std::min(remainingBytes, payloadCapacity);

    uint8_t chunkPayload[lora::MAX_PAYLOAD_SIZE];

    // --- Fill Metadata (Network Byte Order / Big Endian) ---
    // 1. Event/Image UUID (4 bytes) - Use currentImageIdentifier which holds the requested UUID
    chunkPayload[0] = (currentImageIdentifier >> 24) & 0xFF;
    chunkPayload[1] = (currentImageIdentifier >> 16) & 0xFF;
    chunkPayload[2] = (currentImageIdentifier >> 8) & 0xFF;
    chunkPayload[3] = currentImageIdentifier & 0xFF;
    // 2. Current Chunk Number (2 bytes)
    chunkPayload[4] = (currentChunkNumber >> 8) & 0xFF;
    chunkPayload[5] = currentChunkNumber & 0xFF;
    // 3. Total Chunks (2 bytes)
    chunkPayload[6] = (totalChunks >> 8) & 0xFF;
    chunkPayload[7] = totalChunks & 0xFF;
    // --- Fill Chunk Data ---
    // TODO: If using SD card File object, use file.read(buffer, size) here
    //memcpy(&chunkPayload[CHUNK_METADATA_OVERHEAD], imageDataSourcePtr + imageBytesSent, chunkSize);


     // --- Fill Chunk Data ---
    memcpy(&chunkPayload[CHUNK_METADATA_OVERHEAD],
        currentImageDataString.c_str() + imageBytesSent, // Pointer to start of chunk in String buffer
        chunkSize);

    Serial.print("Sending IMAGE_CHUNK #"); 

    lastSentChunkSeqNum = lora::SendManager::getInstance().getNextSeqNum();
    bool initiated = lora::SendManager::getInstance().send(
        myNodeID, // was imageuploadnodeid
        lora::PACKET_TYPE_IMAGE_CHUNK,
        0,
        chunkPayload,
        CHUNK_METADATA_OVERHEAD + chunkSize, // Total payload size
        true // Require ACK
    );

    if (initiated) {
        Serial.print("  Chunk send initiated with SeqNum: "); Serial.println(lastSentChunkSeqNum);
        imageUploadState = ImageUploadState::WAITING_CHUNK_ACK;
        chunkSendTimestamp = millis();
        chunkResendAttempts = 0;
    } else {
         Serial.println("  Image chunk send failed to initiate (radio busy?). Will retry.");
    }
}
*/

void startImageUpload(uint32_t requestedUUID, uint8_t targetNodeId) {
    if (imageUploadState != ImageUploadState::IDLE) { 
        /* ... return if busy ... */ }

    Serial.print("Image upload requested for UUID: 0x"); Serial.print(requestedUUID, HEX);
    Serial.print(" to Node: 0x"); Serial.println(targetNodeId, HEX);

    // --- Select Image Source ---
    // TODO: Implement logic to FIND and OPEN the image file on SD card
    // using the 'requestedUUID'. Construct filename like "/detections/evt_XXXXXXXX.jpg".
    // If file found:
    //    imageDataSourcePtr = /* pointer to file data buffer or File object */;
    //    totalImageSize = /* file size */;
    // If file not found:
    //    Serial.print("ERROR: Image file for UUID 0x"); Serial.print(requestedUUID, HEX); Serial.println(" not found on SD card!");
    //    // TODO: Send IMAGE_UPLOAD_STATUS (Failure - Not Found) packet?
    //    imageUploadState = ImageUploadState::IDLE;
    //    return;

    // --- Retrieve Image Data from Map ---
    auto it = uuid_image_map.find(requestedUUID); // Use find for safety
    if (it == uuid_image_map.end()) {
        Serial.print("ERROR: Image for UUID 0x"); Serial.print(requestedUUID, HEX); Serial.println(" not found in map!");
        // TODO: Send IMAGE_UPLOAD_STATUS (Failure - Not Found) packet?
        return;
    }

    // *** FIX: Copy data instead of using pointer ***
    currentImageDataString = it->second; // Copy the String content
    totalImageSize = currentImageDataString.length(); // Get length of the copied string
    // *** End Fix ***
    Serial.println("  (Using image data from map)");


    if (totalImageSize == 0) {
        Serial.println("ERROR: Image data in map is empty!");
         // TODO: Send IMAGE_UPLOAD_STATUS (Failure - Empty) packet?
        currentImageDataString = ""; // Clear buffer just in case
        return;
    }


    // Calculate overhead: UUID(4)+Chunk#(2)+TotalChunks(2) = 8 bytes
    const size_t CHUNK_METADATA_OVERHEAD = 8;
    size_t payloadCapacity = lora::MAX_PAYLOAD_SIZE - CHUNK_METADATA_OVERHEAD;
    if (payloadCapacity <= 0 || payloadCapacity > lora::MAX_PAYLOAD_SIZE) { 
        Serial.println("ERROR PAYLOAD CAPACITY ");
        currentImageDataString = ""; // Clear buffer

        return; }
    totalChunks = (totalImageSize + payloadCapacity - 1) / payloadCapacity;

    // Initialize state variables
    imageUploadTargetNodeID = targetNodeId;
    currentImageIdentifier = requestedUUID; // Store the requested UUID
    imageBytesSent = 0;
    currentChunkNumber = 0;
    chunkResendAttempts = 0;
    imageUploadState = ImageUploadState::SENDING_CHUNK;

    // *** ADD LOGGING HERE TO VERIFY ***
    Serial.print("DEBUG startImageUpload: Assigned currentImageIdentifier = 0x");
    Serial.println(currentImageIdentifier, HEX);
    // *** END LOGGING ***


    Serial.print("DEBUG startImageUpload: Assigned currentImageIdentifier = 0x"); Serial.println(currentImageIdentifier, HEX);
    Serial.print("Starting image upload: Size="); Serial.print(totalImageSize);
    Serial.print(" chars, Chunks="); Serial.println(totalChunks);
    sendNextImageChunk(); // Try sending first chunk
}

// --- Helper to send the next image chunk ---
void sendNextImageChunk() {
    if (imageUploadState != ImageUploadState::SENDING_CHUNK || !lora::SendManager::getInstance().isIdle()) { return; }
    // Check based on bytes ACKed (imageBytesSent) vs total size
    if (imageBytesSent >= totalImageSize) {
         Serial.println("INFO: All image bytes ACKed. Upload already complete.");
         imageUploadState = ImageUploadState::IDLE;
         currentImageDataString = ""; // Clear buffer
         return;
    }

    const size_t CHUNK_METADATA_OVERHEAD = 8;
    size_t payloadCapacity = lora::MAX_PAYLOAD_SIZE - CHUNK_METADATA_OVERHEAD;
    if (payloadCapacity <= 0) { /* Error */ imageUploadState = ImageUploadState::IDLE; currentImageDataString = ""; return; }

    // Calculate remaining bytes based on ACKed progress
    size_t remainingBytes = totalImageSize - imageBytesSent;
    size_t chunkSize = std::min(remainingBytes, payloadCapacity);

    uint8_t chunkPayload[lora::MAX_PAYLOAD_SIZE];

    // --- Fill Metadata (as before) ---
    chunkPayload[0] = (currentImageIdentifier >> 24) & 0xFF;
    chunkPayload[1] = (currentImageIdentifier >> 16) & 0xFF;
    chunkPayload[2] = (currentImageIdentifier >> 8) & 0xFF;
    chunkPayload[3] = currentImageIdentifier & 0xFF;
    chunkPayload[4] = (currentChunkNumber >> 8) & 0xFF; // Use currentChunkNumber (index of next chunk)
    chunkPayload[5] = currentChunkNumber & 0xFF;
    chunkPayload[6] = (totalChunks >> 8) & 0xFF;
    chunkPayload[7] = totalChunks & 0xFF;

    // --- Fill Chunk Data ---
    // *** FIX: Copy from the currentImageDataString buffer using imageBytesSent offset ***
    memcpy(&chunkPayload[CHUNK_METADATA_OVERHEAD],
           currentImageDataString.c_str() + imageBytesSent, // Pointer to start of chunk in String buffer
           chunkSize);
    // *** End Fix ***

    Serial.print("Sending IMAGE_CHUNK #"); Serial.print(currentChunkNumber);
    Serial.print("/"); Serial.print(totalChunks);
    Serial.print(" (Offset: "); Serial.print(imageBytesSent);
    Serial.print(", Size: "); Serial.print(chunkSize); Serial.println(" chars)");

    lastSentChunkSeqNum = lora::SendManager::getInstance().getNextSeqNum();
    bool initiated = lora::SendManager::getInstance().send(
        imageUploadTargetNodeID,
        lora::PACKET_TYPE_IMAGE_CHUNK,
        0,
        chunkPayload,
        CHUNK_METADATA_OVERHEAD + chunkSize, // Total payload size
        true // Require ACK
    );

    if (initiated) {
        Serial.print("  Chunk send initiated with SeqNum: "); Serial.println(lastSentChunkSeqNum);
        imageUploadState = ImageUploadState::WAITING_CHUNK_ACK; // Wait for ACK
        chunkSendTimestamp = millis();
        chunkResendAttempts = 0;
        // DO NOT advance imageBytesSent or currentChunkNumber here
    } else {
         Serial.println("  Image chunk send failed to initiate (radio busy?). Will retry.");
         // Stay in SENDING_CHUNK state
    }
}

// --- handleImageChunkAck (Needs slight adjustment) ---
void handleImageChunkAck(uint8_t ackedSeqNum) {
    if (imageUploadState == ImageUploadState::WAITING_CHUNK_ACK && ackedSeqNum == lastSentChunkSeqNum) {
        Serial.print("ACK received for Image Chunk #"); Serial.println(currentChunkNumber);

        // Calculate how many bytes/chars were in the successfully ACKed chunk
        const size_t CHUNK_METADATA_OVERHEAD = 8;
        size_t payloadCapacity = lora::MAX_PAYLOAD_SIZE - CHUNK_METADATA_OVERHEAD;
        size_t remainingBytes = totalImageSize - imageBytesSent;
        size_t ackedChunkSize = std::min(remainingBytes, payloadCapacity);

        // Advance counters
        imageBytesSent += ackedChunkSize; // Mark these bytes as confirmed
        currentChunkNumber++;             // Move to the index of the next chunk

        // Check if upload is complete
        if (imageBytesSent >= totalImageSize) {
            Serial.println("Image upload complete! All chunks ACKed.");
            imageUploadState = ImageUploadState::IDLE;
            currentImageDataString = ""; // Clear the buffer
            // TODO: Optionally send IMAGE_UPLOAD_STATUS (Success) packet
        } else {
            // More chunks to send
             Serial.print("  Advancing to next chunk. "); Serial.print(imageBytesSent); Serial.print("/"); Serial.print(totalImageSize); Serial.println(" bytes ACKed.");
            imageUploadState = ImageUploadState::SENDING_CHUNK; // Go back to sending state
            chunkResendAttempts = 0;
            sendNextImageChunk(); // Immediately try sending next chunk
        }
    }
    // ... (Ignore irrelevant ACKs as before) ...
}


// Helper function to send a standard ACK packet
void sendPacketAck(uint8_t targetNodeID, uint8_t seqNumToAck) {
    // Serial.print("Sending ACK to Node 0x"); Serial.print(targetNodeID, HEX); // Can be verbose
    // Serial.print(" for SeqNum: "); Serial.println(seqNumToAck);
    uint8_t ackPayload[] = { seqNumToAck }; // ACK payload is the sequence number being acknowledged
    // Use SendManager. ACKs don't require an ACK (requireAck = false).
    bool initiated = lora::SendManager::getInstance().send(
        targetNodeID,
        lora::PACKET_TYPE_ACK,
        0,
        ackPayload,
        sizeof(ackPayload),
        false
    );
    // if (!initiated) { Serial.println("  WARN: Failed to initiate ACK send."); }
  }


void setup() {
    // Generate a unique ID for this device (e.g., from MAC address)
    deviceID = (uint32_t)ESP.getEfuseMac();

    if (!AI.begin()) {
        Serial.println("AI module initialization failed.");
        while (1);
      }
      Serial.println("AI module initialized.");

    // Radio Reset Sequence
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH); delay(10);
    digitalWrite(RFM95_RST, LOW); delay(10);
    digitalWrite(RFM95_RST, HIGH); delay(10);

    pinMode(PIR_PIN, INPUT);  // Set the PIR pin as input


    Serial.begin(115200);
    while (!Serial && millis() < 2000); // Wait for serial console (with timeout)
    Serial.println("\nEdge Node Starting...");
    Serial.print("Device ID: 0x"); Serial.println(deviceID, HEX);

    // Initialize AI module
    if (!AI.begin()) {
        Serial.println("FATAL: AI module initialization failed.");
        while (1); // Halt
    }
    Serial.println("AI module initialized.");

    // Initialize LoRa Radio
    Serial.println("Initializing LoRa Radio...");
    if (!lora::rf95.init()) {
        Serial.println("FATAL: LoRa radio init failed");
        while (1); // Halt
    }
    Serial.println("LoRa radio init OK!");

    // Apply Radio Settings (Frequency, Power, etc.)
    // Consider putting these in lora::configureRadioSettings()
     if (!lora::rf95.setFrequency(915.0)) {
         Serial.println("setFrequency failed"); /* handle error */
         while(1);
    }
    // Note do not set RFO, weird things occur - see docs.
    lora::rf95.setTxPower(23, false); // Adjust power as needed

    // --- Configure ADC for Battery Reading ---
    Serial.println("Configuring ADC for battery monitoring...");
    analogSetPinAttenuation(VBAT_PIN, ADC_11db); // Use 11dB attenuation for full range (0-3.3V+) reading

    // --- Initial Battery Read (Optional but good) ---
    // Perform an initial read to start the EMA
    int rawValue = analogRead(VBAT_PIN);
    float pinVoltage = (float)rawValue / (ADC_RESOLUTION - 1) * ADC_REF_VOLTAGE;
    smoothedBatteryVoltage = pinVoltage * VOLTAGE_DIVIDER_FACTOR; // Initialize smoothed value
    int initialPercent = estimateBatteryPercentage(smoothedBatteryVoltage);
    lastSentBatteryPercent = (uint8_t)initialPercent; // Initialize last sent value
    Serial.print("Initial Battery Voltage: "); Serial.print(smoothedBatteryVoltage, 2);
    Serial.print("V ("); Serial.print(initialPercent); Serial.println("%)");
    lastBatteryCheck = millis();
    lastBatterySend = millis(); // Send initial status soon


    // --- Join Network ---
    // IMPORTANT: lora::performJoin MUST be modified to be non-blocking
    // and use lora::SendManager::getInstance() internally.
    // Alternatively, implement join logic as a state machine in loop().
    Serial.println("Attempting to join network...");
    if (lora::performJoin(myNodeID, deviceID, -1)) { 
        Serial.print("Join successful! Assigned Node ID: "); Serial.println(myNodeID);
        joined = true;
    } else {
        Serial.println("Join failed after retries. System halted.");
        // Consider implementing retry logic or deep sleep here instead of halting
        while (1);
    }

    // --- Setup Complete ---
    Serial.println("Setup complete. Starting main loop.");
    lastSend = millis(); // Initialize timestamps
    lastHeartbeat = millis();
}


void loop() {
    unsigned long now = millis(); // Get time once per loop

    // ----- 1. MANDATORY: Update the SendManager state machine -----
    // Handles TX Done confirmations and general ACK timeouts for SendManager's buffer
    lora::SendManager::getInstance().update();

    // ----- 2. Check for Incoming LoRa Packets -----
    lora::LoRaPacket packet;
    if (lora::receivePacket(&packet)) { // Non-blocking check for received packets
        // Update Mother Node heartbeat timer if packet received from it
        // (Counts any valid packet from Mother as proof of connection)
        if (packet.nodeID == MOTHER_NODE_ID) {
            lastMotherHeartbeat = now;
            if (!joined) { // Reconnected via non-join packet
                 Serial.println("Re-established connection with Mother Node.");
                 // If we lost connection, we might need the assigned ID again if it was reset
                 // For now, just mark as joined. Mother should ideally resend JOIN_ACK if needed.
                 joined = true;
            }
        } 
        // TODO: ELSE IF NODEID IS NOT MOTHERNODE RETURN.

        // Process received packet based on type
        switch(packet.type) {
            case lora::PACKET_TYPE_JOIN_ACK:
                // Handle join confirmation (likely redundant if performJoin handles it, but safe)
                if (!joined && packet.payloadSize >= 1) {
                    uint8_t assignedID = packet.payload[0];
                    myNodeID = assignedID;
                    joined = true;
                    lastMotherHeartbeat = now; // Start heartbeat timer upon joining
                    Serial.print("Received JOIN_ACK. Assigned Node ID: "); Serial.println(myNodeID);
                }
                lastMotherHeartbeat = now;
                break;

            case lora::PACKET_TYPE_ACK:
                 // Handle ACK for data we sent
                 if (packet.payloadSize >= 1) {
                    uint8_t ackedSeqNum = packet.payload[0];
                    Serial.print("Received ACK from Node 0x"); Serial.print(packet.nodeID, HEX);
                    Serial.print(" for SeqNum: "); Serial.println(ackedSeqNum);

                    // Check if this ACK is for the image chunk we are waiting for
                    handleImageChunkAck(ackedSeqNum); // Specific handler for image ACKs

                    // Let SendManager also handle it for its internal buffer cleanup
                    // It might log a warning if handleImageChunkAck already handled it, which is okay.
                    lora::SendManager::getInstance().handleAck(packet.nodeID, ackedSeqNum);
                 } else {
                     Serial.println("WARN: Received ACK packet with invalid payload size.");
                 }
                break;

            case lora::PACKET_TYPE_COMMAND:
                 // ACK the command back to the sender if requested BEFORE processing
                 if (packet.flags & 0x02) {
                     sendPacketAck(packet.nodeID, packet.seqNum);
                 }

                 // Process command subtype
                 switch(packet.subtype) {
                    case lora::SUBTYPE_REQUEST_IMAGE: { // <<< Add opening brace
                        Serial.print("Received REQUEST_IMAGE command from Node 0x"); Serial.println(packet.nodeID, HEX);
                         uint32_t requestedImageId = 0; // Default ID is 0

                         // --- Check the received payload size ---
                         Serial.print("  Payload size received: "); Serial.println(packet.payloadSize); // <<< CHECK THIS LOG
                         Serial.print("  Expected size: "); Serial.println(sizeof(uint32_t)); // <<< CHECK THIS LOG

                         if (packet.payloadSize == sizeof(uint32_t)) { // sizeof(uint32_t) is 4
                             // --- Log the raw payload bytes ---
                             Serial.print("  Raw payload bytes (HEX): "); // <<< CHECK THIS LOG
                             for(int i=0; i<packet.payloadSize; ++i) { /* ... print hex ... */ }
                             Serial.println();
                             // --- End Log raw payload ---

                             // Parse the UUID
                             requestedImageId = ((uint32_t)packet.payload[0] << 24) |
                             ((uint32_t)packet.payload[1] << 16) |
                             ((uint32_t)packet.payload[2] << 8) |
                             ((uint32_t)packet.payload[3]);

                             Serial.print("  >>> Parsed Requested Image ID from payload: 0x"); // <<< CHECK THIS LOG
                             Serial.println(requestedImageId, HEX);
                         } else {
                             // This block executes if payload size is NOT 4 bytes
                             Serial.println("  WARN: Invalid payload size for REQUEST_IMAGE. Using default Image ID (0)."); // <<< DID THIS LOG APPEAR?
                         }

                         // Calls startImageUpload with whatever value requestedImageId ended up with
                         startImageUpload(requestedImageId, packet.nodeID);
                         break;
                     }
   
                    case lora::SUBTYPE_REBOOT: { // <<< Add opening brace
                        Serial.print("Received REBOOT command from Node 0x"); Serial.println(packet.nodeID, HEX);
                        if (packet.nodeID != MOTHER_NODE_ID) {
                            Serial.println("  WARN: REBOOT command received from non-Mother Node. Ignoring.");
                            break; // Ignore if not from Mother Node
                        }
                        // TODO: Implement reboot logic if needed
                        // For now, just print a message
                        Serial.println("Rebooting device... (Not implemented)");
                        //Serial.println("Received REBOOT command. Rebooting...");
                        //delay(1000);
                        //ESP.restart();
                        // break; // Unreachable after restart, but technically correct
                    } 
                    case lora::SUBTYPE_REQUEST_STATUS: {
                       Serial.println("Received REQUEST_STATUS command (Not Implemented)");
                       break;
                    }
                    case lora::SUBTYPE_REQUEST_CONFIG: {
                       Serial.println("Received REQUEST_CONFIG command (Not Implemented)");
                       break;
                    }
   
                    default: { // <<< Add opening brace
                        Serial.print("Received unknown COMMAND Subtype 0x"); Serial.println(packet.subtype, HEX);
                        break;
                    } // <<< Add closing brace
                } // End switch(packet.subtype)
                 break; // Break from COMMAND case

            // Handle other packet types (DATA from Mother, HEARTBEAT from Mother)
            case lora::PACKET_TYPE_DATA:
            case lora::PACKET_TYPE_HEARTBEAT:
                 // Already updated lastMotherHeartbeat timer above
                 Serial.print("Received packet type PACKET_TYPE_HEARTBEAT {"); Serial.print(packet.type); Serial.print("} from Mother.\n"); // Verbose
                 lastMotherHeartbeat = now;

                 break;

            default:
                 Serial.print("Received unhandled packet type 0x"); Serial.print(packet.type, HEX);
                 Serial.print(" from Node 0x"); Serial.println(packet.nodeID, HEX);
                 break;
        } // End switch(packet.type)
    } // End if receivePacket

    // ----- 3. Check for Mother Node Heartbeat Timeout -----
    if (joined && (now - lastMotherHeartbeat > MOTHER_HEARTBEAT_TIMEOUT)) {
        Serial.println("!!! Lost connection to Mother Node (Heartbeat Timeout) !!!");
        joined = false; // Mark as disconnected
        myNodeID = 0xFF; // Reset assigned Node ID
        imageUploadState = ImageUploadState::IDLE; // Abort any ongoing upload

        // --- Initiate Rejoin Strategy (Example: Call blocking performJoin) ---
        Serial.println("Attempting to rejoin network immediately...");
        if (lora::performJoin(myNodeID, deviceID, 5)) { // Try 5 times
            Serial.println("Rejoin successful!");
            lastMotherHeartbeat = millis(); // Reset timer immediately
            // joined is set true inside performJoin on success
        } else {
            Serial.println("Immediate rejoin attempt failed. Will rely on periodic retries.");
            // joined remains false
        }
        // --- End Rejoin Strategy ---
    }

    // ----- 4. Periodic Battery Check -----
    if (now - lastBatteryCheck > BATTERY_CHECK_INTERVAL) {
        lastBatteryCheck = now;
        int rawValue = analogRead(VBAT_PIN);
        float pinVoltage = (float)rawValue / (ADC_RESOLUTION - 1) * ADC_REF_VOLTAGE;
        float currentVoltage = pinVoltage * VOLTAGE_DIVIDER_FACTOR;
        if (smoothedBatteryVoltage < 0.0) { smoothedBatteryVoltage = currentVoltage; }
        else { smoothedBatteryVoltage = (currentVoltage * EMA_ALPHA) + (smoothedBatteryVoltage * (1.0 - EMA_ALPHA)); }
        // Serial.print("Smoothed Battery Voltage: "); Serial.println(smoothedBatteryVoltage, 2); // Verbose
    }

    // TODO: our AI detection logic would go here
    // For now, we simulate a detection every 25 seconds for testing purposes
    //Serial.println("Right before CheckAIdetection.");
    int motionDetected = digitalRead(PIR_PIN);
    if (motionDetected == HIGH) {
        checkAIDetection(); // Call AI detection check (replace with actual AI logic)
    }
    // ----- 5. Handle Image Upload State Machine -----
    if (imageUploadState == ImageUploadState::SENDING_CHUNK) {
        // Try to send the next chunk (function checks if sender is idle)
        sendNextImageChunk();
    }
    else if (imageUploadState == ImageUploadState::WAITING_CHUNK_ACK) {
        // Check if we've timed out waiting for the ACK for the last sent chunk
        if (now - chunkSendTimestamp > IMAGE_CHUNK_ACK_TIMEOUT) {
            Serial.print("Timeout waiting for ACK for Image Chunk #"); Serial.print(currentChunkNumber);
            Serial.print(" (SeqNum "); Serial.print(lastSentChunkSeqNum); Serial.println(")");

            if (chunkResendAttempts < MAX_CHUNK_RESEND_ATTEMPTS) {
                // Increment counter and go back to SENDING state to trigger resend logic
                chunkResendAttempts++;
                Serial.print("Attempting resend #"); Serial.println(chunkResendAttempts);
                imageUploadState = ImageUploadState::SENDING_CHUNK;
            } else {
                Serial.println("Max resend attempts reached for chunk. Aborting image upload.");
                imageUploadState = ImageUploadState::IDLE;
                // TODO: Send IMAGE_UPLOAD_STATUS (Failure) packet to Mother Node
                // lora::SendManager::getInstance().send(...DATA, SUBTYPE_IMAGE_UPLOAD_STATUS, ...);
            }
        }
    } // End image upload state handling


    // ----- 6. Perform Periodic Actions Based on Joined Status -----
    if (joined && imageUploadState == ImageUploadState::IDLE) { // Only if joined AND not busy uploading
        // --- Send Periodic Battery Status ---
        if (now - lastBatterySend > BATTERY_SEND_INTERVAL) {
            int currentSmoothedPercent = estimateBatteryPercentage(smoothedBatteryVoltage);
            //if (abs(currentSmoothedPercent - (int)lastSentBatteryPercent) >= 2 || lastSentBatteryPercent == 255) {
                 uint8_t percentPayload = (uint8_t)constrain(currentSmoothedPercent, 0, 100);
                 Serial.print("Attempting to send Battery Status: "); Serial.print(percentPayload); Serial.println("%");
                 bool initiated = lora::SendManager::getInstance().send( myNodeID, lora::PACKET_TYPE_DATA, lora::SUBTYPE_BATTERY_STATUS, &percentPayload, 1, false );
                 if (initiated) { lastBatterySend = now; lastSentBatteryPercent = percentPayload; }
                 // else { Serial.println("  Battery Status send failed to initiate..."); } // Verbose
           // } else { lastBatterySend = now; }
        }

        // --- Send Periodic Data (Example) ---


        if (now - lastSend > 15000) { 
            // 15 seconds interval for data send test sending
            //onAnimalDetected(lora::ANIMAL_MASK_SQUIRREL, 3, 3,  3,  3,  3, String("test.jpg"));
            lastSend = now;
             //if (channelIsClear()) {
                 const char msg[] = "Edge node data payload!";
                //  was MOTHER_NODE_ID
                 bool initiated = lora::SendManager::getInstance().send( myNodeID, lora::PACKET_TYPE_DATA, 1, (uint8_t*)msg, strlen(msg), true);
                 if (initiated) { lastSend = now; }
                 // else { Serial.println("  Data send failed to initiate..."); } // Verbose
            // }
             // else { Serial.println("Channel busy, delaying data send."); } // Verbose
        }

        // --- Send Periodic Heartbeat (From Edge Node to Mother) ---
        if (now - lastHeartbeat > 30000) {
            // was MOTHER_NODE_ID
             bool initiated = lora::SendManager::getInstance().send( myNodeID, lora::PACKET_TYPE_HEARTBEAT, 0, nullptr, 0, false);
             if (initiated) { lastHeartbeat = now; }
             // else { Serial.println("  Heartbeat send failed to initiate..."); } // Verbose
        }

        // --- Perform AI Inference or other tasks ---
        // ...

    } else if (!joined) { // Not currently joined
        // --- Periodic Rejoin Logic ---
        static unsigned long lastJoinAttempt = 0;
        if (now - lastJoinAttempt > 60000) { // Try joining every 60 seconds when disconnected
             lastJoinAttempt = now;
              Serial.println("Attempting periodic rejoin...");
              // Call performJoin with limited retries.
              if (lora::performJoin(myNodeID, deviceID, 2)) { // Try only twice periodically
                    Serial.println("Periodic rejoin successful!");
                    lastMotherHeartbeat = millis(); // Reset timer
                    // joined is set true inside performJoin
              } else {
                   Serial.println("Periodic rejoin attempt failed.");
                   // joined remains false
              }
         }
         // --- End Periodic Rejoin Logic ---

    } // End if(joined && IDLE) / else (!joined)

    // delay(1); // Optional small delay if loop has little work
} // End loop()



// --- Function to Estimate Battery Percentage ---
int estimateBatteryPercentage(float voltage) {
  if (voltage >= 4.20) { return 100; }
  else if (voltage >= 4.10) { return map(voltage * 100, 410, 420, 90, 100); }
  else if (voltage >= 4.00) { return map(voltage * 100, 400, 410, 80, 90); }
  else if (voltage >= 3.90) { return map(voltage * 100, 390, 400, 70, 80); }
  else if (voltage >= 3.80) { return map(voltage * 100, 380, 390, 55, 70); }
  else if (voltage >= 3.70) { return map(voltage * 100, 370, 380, 40, 55); }
  else if (voltage >= 3.60) { return map(voltage * 100, 360, 370, 25, 40); }
  else if (voltage >= 3.50) { return map(voltage * 100, 350, 360, 10, 25); }
  else if (voltage >= 3.30) { return map(voltage * 100, 330, 350, 0, 10); }
  else { return 0; } // Below 3.3V is considered 0%
}