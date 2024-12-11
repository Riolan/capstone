#include <SPI.h>
#include <LoRa.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// LoRa configuration
#define MAX_PACKET_SIZE 245
#define LORA_BAND 915E6 // Adjust according to region

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

#include <SPI.h>
#include <LoRa.h>

// Pin definitions
#define CS_PIN 6
#define RESET_PIN 10
#define IRQ_PIN 9

void sendBBoxData(const uint8_t* boundingBoxes, size_t bboxLength);
void storeIncomingPackets(int packetSize);
// BLE configuration
#define SERVICE_UUID "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-210987654321"
#define MAX_CHUNK_SIZE 20  // Adjust based on your needs

//uint8_t* receivedImage; // Dynamically allocated array for image
String imageData;
uint16_t sizeOfImage;



// Define a BLEPacket structure of 20 bytes
struct BLEPacket {
    uint8_t data[20]; // Array to hold 20 bytes of data
};

// Define the circular buffer size
const size_t BUFFER_SIZE_BLE = 30; // Adjust as needed
BLEPacket circularBufferBLE[BUFFER_SIZE_BLE];
size_t headBLE = 0; // Index for the next write position
size_t tailBLE = 0; // Index for the next read position
bool isFullBLE = false; // Flag to indicate if the buffer is full

// Function to enqueue a packet into the circular buffer
void enqueuePacketBLE(const BLEPacket& packet) {
    circularBufferBLE[headBLE] = packet; // Store the packet at the head position

    // Update head index
    headBLE = (headBLE + 1) % BUFFER_SIZE_BLE;

    // Check if buffer is full
    if (isFullBLE) {
        tailBLE = (tailBLE + 1) % BUFFER_SIZE_BLE; // Move tail if overwriting
    }

    // Set the full flag
    isFullBLE = (headBLE == tailBLE);
}

// Function to dequeue a packet from the circular buffer
bool dequeuePacketBLE(BLEPacket& packet) {
    if (headBLE == tailBLE && !isFullBLE) {
        return false; // Buffer is empty
    }

    packet = circularBufferBLE[tailBLE]; // Retrieve the packet from the tail position
    tailBLE = (tailBLE + 1) % BUFFER_SIZE_BLE; // Update tail index

    // Clear the full flag if we just removed a packet
    isFullBLE = false;

    return true; // Successfully dequeued a packet
}


// Define the packet structure
struct Packet {
    uint8_t type;
    uint8_t data[MAX_PACKET_SIZE - sizeof(uint8_t)]; // does this need to be -2??
    uint8_t index; // Current index for data access
    uint8_t size;  // Size of the actual data in the packet

    // Constructor to initialize the packet
    Packet() : index(0), size(0) {
        // Initialize data to zero
        memset(data, 0, sizeof(data));
    }

    // Function to get a single byte from the data array
    uint8_t getByte() {
        if (index <= size) {
            return data[index++];
        }
        // Handle out of bounds (optional)
        return 0; // Or throw an error
    }

    // Function to get multiple bytes and return as an array
    void getBytes(uint8_t numBytes, uint8_t* outBuffer) {
        for (uint8_t i = 0; i < numBytes; i++) {
            if (index < size) {
                outBuffer[i] = getByte(); // Fill the output buffer
            } else {
                // Handle out of bounds (optional)
                Serial.println("ERROR IN GET BYTES");
                outBuffer[i] = 0; // Or handle error
            }
        }
    }

    // Function to get a uint16_t value from the next two bytes (Big-endian)
    uint16_t getShort() {
        uint16_t value = 0;
        value |= (static_cast<uint16_t>(getByte()) << 8);  // High byte
        value |= (static_cast<uint16_t>(getByte()) << 0);  // Low byte
        return value;
    }

    // Function to put a single byte into the data array
    void putByte(uint8_t byte) {
        if (index < sizeof(data)) {
            data[index++] = byte;
            size = index; // Update the size
        }
        // Handle out of bounds (optional)
    }

    // Function to put multiple bytes into the data array
    void putBytes(const uint8_t* bytes, uint8_t numBytes) {
        for (uint8_t i = 0; i < numBytes; i++) {
            putByte(bytes[i]);
        }
    }

    // Function to reset the index to allow reading from the beginning
    void resetIndex() {
        index = 0;
    }

    // Function to get the size of the data
    uint8_t getSize() const {
        return size;
    }

    void prepareForTransmission(uint8_t *buffer, size_t &length) {
        length = 0;
        buffer[length++] = type;           // Send the type
        buffer[length++] = size;           // Send the size
        memcpy(&buffer[length], data, size); // Send the data
        length += size;                    // Update the total length
    }


  // Function to convert uint16_t to a byte array
  static void shortToByteArray(uint16_t value, uint8_t *byteArray) {
      byteArray[0] = (value >> 8) & 0xFF; // High byte
      byteArray[1] = value & 0xFF;        // Low byte
  }

};

void enqueuePacket(Packet packet);
Packet dequeuePacket();
void handlePacket(Packet* receivedPacket);

#define BUFFER_SIZE 10
Packet packetBuffer[BUFFER_SIZE];
int bufferHead = 0; // Next write slot
int bufferTail = 0; // Next read slot

bool isBufferFull() {
    return ((bufferHead + 1) % BUFFER_SIZE) == bufferTail;
}

bool isBufferEmpty() {
    return bufferHead == bufferTail;
}

void enqueuePacket(Packet packet) {
    if (!isBufferFull()) {
        packetBuffer[bufferHead] = packet;
        bufferHead = (bufferHead + 1) % BUFFER_SIZE;
    }
}

Packet dequeuePacket() {
    Packet packet = {};
    if (!isBufferEmpty()) {
        packet = packetBuffer[bufferTail];
        bufferTail = (bufferTail + 1) % BUFFER_SIZE;
    }
    return packet;
}


struct BoundingBox {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint8_t score;
    uint8_t target;

    void serialize(uint8_t* buffer, uint8_t& index) const {
        buffer[index++] = (x >> 8) & 0xFF; buffer[index++] = x & 0xFF;
        buffer[index++] = (y >> 8) & 0xFF; buffer[index++] = y & 0xFF;
        buffer[index++] = (w >> 8) & 0xFF; buffer[index++] = w & 0xFF;
        buffer[index++] = (h >> 8) & 0xFF; buffer[index++] = h & 0xFF;
        buffer[index++] = score;
        buffer[index++] = target;
    }

    void deserialize(const uint8_t* buffer, uint8_t& index) {
        Serial.print("Deserialized buffer: ");
        for (int i = 0; i < index; ++i) {
            Serial.print(buffer[i], HEX);
            Serial.print(" ");
        }
        Serial.println();

        x = (buffer[index++] << 8) | buffer[index++];
        y = (buffer[index++] << 8) | buffer[index++];
        w = (buffer[index++] << 8) | buffer[index++];
        h = (buffer[index++] << 8) | buffer[index++];
        score = buffer[index++];
        target = buffer[index++];
    }
};

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Device connected");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected");
        pServer->startAdvertising();  // Restart advertising
    }

    void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue();
      Serial.print("Received data: ");
      for (size_t i = 0; i < value.length(); i++) {
          Serial.print((uint8_t)value[i], HEX); // Print data in HEX
          Serial.print(" ");
      }
        Serial.println();
    }

    void onWrite(BLECharacteristic *pCharacteristic, esp_ble_gatts_cb_param_t* param) {
      Serial.println("Characteristic Write 1");
      onWrite(pCharacteristic);
    }



    /*void onWrite(BLECharacteristic *pCharacteristic)  {
        String value = pCharacteristic->getValue();

        Serial.println("Got an input!");
        Serial.println(value);
        uint8_t packetId = value[0]; // Read the packet ID
        bool areAnimalsEnabled[4] = {0};

        if (packetId == 0x20) {
          // Read the bytes and update the animal enable flags
          areAnimalsEnabled[0] = (value[1] != 0); // bBird
          areAnimalsEnabled[1] = (value[2] != 0); // bCat
          areAnimalsEnabled[2] = (value[3] != 0); // bDog
          areAnimalsEnabled[3] = (value[4] != 0); // bSquirrel

          Serial.println("Animals enabled are:" + String(areAnimalsEnabled[0]) + ", "   + String(areAnimalsEnabled[1]) + ", "  + String(areAnimalsEnabled[2])  + ", "  + String(areAnimalsEnabled[3]));

          Packet packet;
          packet.type = 0x20;
          packet.putByte(areAnimalsEnabled[0]);
          packet.putByte(areAnimalsEnabled[1]);
          packet.putByte(areAnimalsEnabled[2]);
          packet.putByte(areAnimalsEnabled[3]);


          uint8_t sendBuffer[MAX_PACKET_SIZE];
          size_t sendLengthImage = 0;
          packet.prepareForTransmission(sendBuffer, sendLengthImage);

          LoRa.beginPacket();
          LoRa.write(sendBuffer, sendLengthImage); // Send header without data
          LoRa.endPacket();
        }

    }*/

};


/**
Performs set up for BLE, LORA, and SERIAL
*/
void setup() {

  Serial.begin(9600);
  while (!Serial);

  Serial.println("LoRa Receiver Initializing...");

  // Initialize LoRa module
  LoRa.setPins(CS_PIN, RESET_PIN, IRQ_PIN);
  if (!LoRa.begin(915E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  Serial.println("LoRa initialized and ready to receive.");


  // Initialize BLE
  BLEDevice::init("ESP32 Receiver");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());


  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY |
      BLECharacteristic::PROPERTY_WRITE
  );
   //   pCharacteristic->addDescriptor(new BLE2902());

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->start();
  Serial.println("BLE started, ready to receive data.");

}

void loop() {
    // Check if there is a packet available
  if (LoRa.parsePacket()) {
    handlePacket(nullptr);
  }
  // ------
  if (!isBufferEmpty()) {
      Packet packet = dequeuePacket();
      handlePacket(&packet);
  }

}


void handlePacket(Packet* receivedPacket) {
    bool createdPacket = false;
    if (receivedPacket == nullptr) {
      createdPacket = true;
      // Create a packet instance to store the received data
      receivedPacket = new Packet();
      // Read the packet data into the struct
      receivedPacket->type = LoRa.read(); // Read the packet type
      receivedPacket->size = LoRa.read(); // Read the size of the data

    }

  Serial.print(" receivedPacket.type: ");
  Serial.println(receivedPacket->type, HEX);
  Serial.print(" receivedPacket.size: ");
  Serial.println(receivedPacket->size, HEX);

  if (receivedPacket->size > 0 && receivedPacket->size <= (MAX_PACKET_SIZE - sizeof(uint8_t) * 2)) {
    LoRa.readBytes(receivedPacket->data, receivedPacket->size); // Read the data into the packet
    // Now you can process the packet.data as needed
    Serial.print("Received packet with type: ");
    Serial.println(receivedPacket->type, HEX);

    Serial.print("Received packet with size: ");
    Serial.println(receivedPacket->size, HEX);

    // Control packet
    if (receivedPacket->type == 0x10) {
      uint16_t numOfPackets = receivedPacket->getShort();
      Serial.print("Number of packets: ");
      Serial.println(numOfPackets);
      uint16_t sizeOfImage = receivedPacket->getShort();
      Serial.print("Size of image: ");
      Serial.println(sizeOfImage);        
      // create buffer for image.
      //receivedImage = (uint8_t*)malloc(sizeOfImage);
      sizeOfImage = 0;
    } else if (receivedPacket->type == 0x11) {
      uint16_t currentPacketNum = receivedPacket->getShort();
      Serial.print("Current packet: ");
      Serial.println(currentPacketNum);
      Serial.println(receivedPacket->size - sizeof(uint16_t));
      uint8_t outBuffer[receivedPacket->size - sizeof(uint16_t) + 1] = {0};
      outBuffer[receivedPacket->size - sizeof(uint16_t) + 1] = '\0';
      receivedPacket->getBytes(241, outBuffer);      
      Serial.print("Image data packet: ");
      Serial.println(((char*)outBuffer));
      imageData += ((char*)outBuffer);
      //memcpy(&receivedImage[sizeOfImage], outBuffer, size);

    } else if (receivedPacket->type == 0x12) {
      uint16_t currentPacketNum = receivedPacket->getShort();
      Serial.print("IMG Current packet: ");
      Serial.println(currentPacketNum);
      // account for currentpacketnum

      // Current work around buffer, need to make a dynamic buffer created by control
      uint8_t outBuffer[MAX_PACKET_SIZE];
      receivedPacket->getBytes(MAX_PACKET_SIZE, outBuffer);      
      Serial.print("Image data packet: ");
      Serial.println(((char*)outBuffer));
      
      Serial.println("Recieved last packet!");
      //memcpy(&receivedImage[sizeOfImage], outBuffer, size);
      imageData += ((char*)outBuffer);


      Serial.println("Full image data is: ");
      //Serial.println(((char*)receivedImage));
      Serial.println(imageData);

      Serial.println("Holding off sending imageData until we get bbox. ");

      //sendImageData(imageData);
      //free(receivedImage);
      //imageData = "";
      //sizeOfImage = 0;
    } else if (receivedPacket->type == 0x13) {
      uint16_t numOfPackets = receivedPacket->getShort();
      Serial.print("Number of packets: ");
      Serial.println(numOfPackets);
      uint16_t sizeOfBBoxBuffer = receivedPacket->getShort();
      Serial.print("Size of sizeOfBBoxBuffer: ");
      Serial.println(sizeOfBBoxBuffer);       
    } else if (receivedPacket->type == 0x14) {
      uint16_t currentPacketNum = receivedPacket->getShort();
      Serial.print("BBOX Current packet: ");
      Serial.println(currentPacketNum);
      // currently only getting the first one. Likely want to send over the actual size
      // so we can iterate over it, but the single for now should be enough. 
      uint8_t outBuffer[MAX_PACKET_SIZE];
      receivedPacket->getBytes(10, outBuffer);
      Serial.print("BBox data packet: ");
      Serial.println(((char*)outBuffer));
      BoundingBox deserialized_bbox;
      uint8_t index = 0;
      std::vector<BoundingBox> boundingBoxes;
      deserialized_bbox.deserialize(outBuffer, index);


      Serial.println("Deserialized BoundingBox:");
      Serial.println(deserialized_bbox.x);
      Serial.println(deserialized_bbox.y);
      Serial.println(deserialized_bbox.w);
      Serial.println(deserialized_bbox.h);
      Serial.println(deserialized_bbox.score);
      Serial.println(deserialized_bbox.target);


      Serial.println("Sending image data:");

      sendImageData(imageData);
      imageData = "";
      sizeOfImage = 0;
      Serial.println("Sending bbox data:");

      sendBBoxData(outBuffer, 10);
    } 
  }

  if (createdPacket) {
    delete receivedPacket; //free
  }
}

void logBytes(const uint8_t* buffer, size_t length) {
    Serial.print("Raw Bytes: ");
    for (size_t i = 0; i < length; ++i) {
        Serial.print(buffer[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

void sendImageData(String img) {
    if (!deviceConnected) {
        Serial.println("No device connected. Aborting image transfer.");
        return;
    }

    // Create the control packet (START) with only image ID
    String controlPacket = "START;";
    pCharacteristic->setValue(controlPacket.c_str());
    pCharacteristic->notify();
    delay(20);  // Small delay to ensure notification is sent

    // Send the Base64 image data in chunks
    uint8_t* imgData = (uint8_t*) img.c_str();
    size_t bytesSent = 0;
    while (bytesSent < img.length()) {
        size_t chunkSize = (img.length() - bytesSent) < MAX_CHUNK_SIZE ? (img.length() - bytesSent) : MAX_CHUNK_SIZE;
        pCharacteristic->setValue(&imgData[bytesSent], chunkSize);
        pCharacteristic->notify();
        bytesSent += chunkSize;
        delay(20);  // Add delay to ensure notifications are sent correctly
    }

    // Create the end packet (END)
    String endPacket = "END;";
    pCharacteristic->setValue(endPacket.c_str());
    pCharacteristic->notify();
    delay(20);  // Small delay to ensure notification is sent

    Serial.println("Image transfer completed.");
}


void sendBBoxData(const uint8_t* boundingBoxes, size_t bboxLength) {
    if (!deviceConnected) {
        Serial.println("No device connected. Aborting bounding box transfer.");
        return;
    }

    logBytes(boundingBoxes, bboxLength);

    // Create the control packet (START_BBOX)
    String controlPacket = "START_BBOX;";
    pCharacteristic->setValue(controlPacket.c_str());
    pCharacteristic->notify();
    delay(40);  // Small delay to ensure notification is sent

    // Send the bounding box data in chunks
    size_t bytesSent = 0;
    while (bytesSent < bboxLength) {
        size_t chunkSize = (bboxLength - bytesSent) < MAX_CHUNK_SIZE ? (bboxLength - bytesSent) : MAX_CHUNK_SIZE;
        pCharacteristic->setValue((uint8_t*)&boundingBoxes[bytesSent], chunkSize);
        pCharacteristic->notify();
        bytesSent += chunkSize;
        delay(40);  // Add delay to ensure notifications are sent correctly
    }

    // Create the end packet (END_BBOX)
    String endPacket = "END_BBOX;";
    pCharacteristic->setValue(endPacket.c_str());
    pCharacteristic->notify();
    delay(40);  // Small delay to ensure notification is sent

    Serial.println("Bounding box transfer completed.");
}

// Calling inbetween loops of our image sending or BBOX sending
// to ensure that we don't miss a packet that we need to read.
// TODO: implement a packet priority handling system.
void storeIncomingPackets(int packetSize) {
    Packet packet = {};
    while (LoRa.parsePacket()) {
      Packet receivedPacket; // Create an instance of Packet
      
      // Read packet type (1 byte)
      receivedPacket.type = LoRa.read(); 
      
      // Read the size of the data (remaining bytes)
      receivedPacket.size = LoRa.available(); // Get the number of bytes available
      receivedPacket.index = 0; // Reset index

      // Check if the size exceeds the buffer length
      if (receivedPacket.size > sizeof(receivedPacket.data)) {
          Serial.println("Error: Packet size exceeds buffer size.");
          return; // Handle error (you might want to skip this packet or log an error)
      }

      // Read data into the packet
      while (LoRa.available() && receivedPacket.index < sizeof(receivedPacket.data)) {
          receivedPacket.data[receivedPacket.index++] = LoRa.read(); // Fill data
      }

      // After the loop, you may have received a full packet
      Serial.print("Received packet type: ");
      Serial.println(receivedPacket.type);
      Serial.print("Packet size: ");
      Serial.println(receivedPacket.size);
      enqueuePacket(packet);
    }
    // I.E priority 
    //packet.priority = LoRa.read(); // Read packet priority
    /*if (packet.priority == 0) {
        // Process high-priority packet immediately
        processPacket(packet);
    } else {
        // Buffer lower-priority packets
        enqueuePacket(packet);
    }*/
}


