#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID "12345678-1234-1234-1234-123456789012" // Replace with your service UUID
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-210987654321" // Replace with your characteristic UUID

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        Serial.println("Client Connected");
    }

    void onDisconnect(BLEServer* pServer) {
        Serial.println("Client Disconnected");
        // Restart the server when a client disconnects
        startBLEServer();
    }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String value = pCharacteristic->getValue();
        if (!value.isEmpty()) {
            Serial.print("Received Message: ");
            Serial.println(value.c_str()); // Print the received message to Serial Monitor
        }
    }
};

BLEServer *pServer = nullptr; // Global server pointer
BLECharacteristic *pCharacteristic = nullptr; // Global characteristic pointer

void startBLEServer() {
    // Create a new BLE server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create a new service
    BLEService *pService = pServer->createService(SERVICE_UUID);
    
    // Create a new characteristic
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
    );

    pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
    pCharacteristic->addDescriptor(new BLE2902()); // For notifications
    pService->start();
    
    // Start advertising
    pServer->getAdvertising()->start();
    Serial.println("BLE Server is Ready. Waiting for a client to connect...");
}

void setup() {
    Serial.begin(115200);
    BLEDevice::init("ESP32_S3_Device"); // Set the BLE device name
    startBLEServer(); // Start the BLE server
}

void loop() {
    // Nothing to do here, everything is handled in callbacks
    delay(5000);
    Serial.println("BLE is still connected (0)...");
}
