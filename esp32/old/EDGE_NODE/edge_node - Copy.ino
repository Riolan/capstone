#include <Wire.h>
#include <Seeed_Arduino_SSCMA.h>
#include <SPI.h>
#include <LoRa.h>

#define CS_PIN 6
#define RESET_PIN 10
#define IRQ_PIN 9
#define PIR_PIN 16     // Define the pin where the PIR sensor OUT is connected

// LoRa settings
#define MAX_PACKET_SIZE 245

// Define the packet structure
struct Packet {
    uint8_t type;
    uint8_t data[MAX_PACKET_SIZE - sizeof(uint8_t)];
    uint8_t index; // Current index for data access
    uint8_t size;  // Size of the actual data in the packet

    // Constructor to initialize the packet
    Packet() : index(0), size(0) {
        // Initialize data to zero
        memset(data, 0, sizeof(data));
    }

    // Function to get a single byte from the data array
    uint8_t getByte() {
        if (index < size) {
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
            // Log buffer for debugging
        Serial.print("Serialized buffer: ");
        for (int i = 0; i < index; ++i) {
            Serial.print(buffer[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }

    void deserialize(const uint8_t* buffer, uint8_t& index) {
        x = (buffer[index++] << 8) | buffer[index++];
        y = (buffer[index++] << 8) | buffer[index++];
        w = (buffer[index++] << 8) | buffer[index++];
        h = (buffer[index++] << 8) | buffer[index++];
        score = buffer[index++];
        target = buffer[index++];
    }
};

struct ClassData {
    uint8_t target;
    uint8_t score;

    void serialize(uint8_t* buffer, uint8_t& index) const {
        buffer[index++] = target;
        buffer[index++] = score;
    }

    void deserialize(const uint8_t* buffer, uint8_t& index) {
        target = buffer[index++];
        score = buffer[index++];
    }
};


// AI Module
SSCMA AI;
/**
Performs set up for LORA and SERIAL
*/
void setup() {
  Serial.begin(9600);
  //  pinMode(PIR_PIN, INPUT);      // Set the PIR pin as an input

  // Initialize AI module
  if (!AI.begin()) {
    Serial.println("AI module initialization failed.");
    while (1);
  }
  Serial.println("AI module initialized.");

  // Initialize LoRa module
  LoRa.setPins(CS_PIN, RESET_PIN, IRQ_PIN);
  if (!LoRa.begin(915E6)) {
    Serial.println("LoRa initialization failed.");
    while (1);
  }
  Serial.println("LoRa initialized.");

  Serial.println("Initialization of Edge Device Succeeded.");

}
unsigned long lastDetectionTime = 0;  // Timestamp of the last detection
const unsigned long detectionCooldown = 10000;  // Cooldown period in milliseconds
  bool imageSent = false;      // Flag to indicate if an image has been sent

 void loop() {
    if (!AI.invoke(1, false, true)) {
      //Serial.println("Fetching AI data...");
      if (AI.boxes().size() > 0) {
        unsigned long currentMillis = millis();
        if (!imageSent || (currentMillis - lastDetectionTime >= detectionCooldown)) {
          Serial.println("Sending AI data via LoRa.");

          String img = AI.last_image();
          if (img.length() > 0){
            sendImage((uint8_t*) img.c_str(), img.length());
          }
          Serial.println("AI data sent successfully.");
          Serial.println("Full image data is: ");
          imageSent = true;
          Serial.println(img);


          if (AI.boxes().size() > 0) {
            std::vector<BoundingBox> boundingBoxes;
            std::vector<ClassData> classData;

            for (int i = 0; i < AI.boxes().size(); i++) {
                BoundingBox box = {
                    AI.boxes()[i].x, AI.boxes()[i].y, AI.boxes()[i].w,
                    AI.boxes()[i].h, AI.boxes()[i].score, AI.boxes()[i].target
                };
                boundingBoxes.push_back(box);
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

            for (int i = 0; i < AI.classes().size(); i++)
            {
                Serial.printf("class %d: target=%d, score=%d\n", i,
                              AI.classes()[i].target, AI.classes()[i].score);
            }
            for (int i = 0; i < AI.points().size(); i++)
            {
                Serial.printf("point %d: x=%d, y=%d, z=%d, score=%d, target=%d\n",
                              i, AI.points()[i].x, AI.points()[i].y,
                              AI.points()[i].z, AI.points()[i].score,
                              AI.points()[i].target);
            }

            for (int i = 0; i < AI.classes().size(); i++) {
                ClassData cls = {AI.classes()[i].target, AI.classes()[i].score};
                classData.push_back(cls);
                Serial.print("Class[");
                Serial.print(i);
                Serial.print("] target=");
                Serial.print(AI.classes()[i].target);
                Serial.print(", score=");
                Serial.println(AI.classes()[i].score);
            }


          sendBoundingBoxesAndClasses(boundingBoxes, classData);
          Serial.println("Bounding box and class data sent successfully.");
        }




          delay(10000);
        }
      } 
    } else {
      Serial.println("Failed to fetch AI data.");
    }

        // Reset the imageSent flag if cooldown has passed
    if (imageSent && (millis() - lastDetectionTime >= detectionCooldown)) {
        imageSent = false;  // Allow for new detections after cooldown
    }
}

void sendBoundingBoxesAndClasses(const std::vector<BoundingBox>& boxes, const std::vector<ClassData>& classes) {
    uint16_t bboxes_count = boxes.size();
    uint16_t bboxes_byte_size = bboxes_count * 10; // 10 bytes, n boxes
    uint8_t bbox_buffer[bboxes_byte_size];


    // Add bounding boxes
    uint8_t index = 0;
    for (const auto& box : boxes) {
        box.serialize(bbox_buffer, index);
    }
    int16_t max_packet_size = (MAX_PACKET_SIZE - sizeof(uint8_t)*2);
    // Get count by divion
    uint16_t totalPackets = bboxes_byte_size/max_packet_size; //(bboxes_byte_size + MAX_PACKET_SIZE - 1) / MAX_PACKET_SIZE;
    if (bboxes_byte_size % max_packet_size != 0) { // account for remainder
      totalPackets += 1;
    }
    
    
    Packet controlPacket;
    controlPacket.type = 0x13;
    // put total packets
    uint8_t totalPacketsArray[2];    // Array to hold the bytes
    Packet::shortToByteArray(totalPackets, totalPacketsArray);
    controlPacket.putBytes(totalPacketsArray, 2);

    // put size of image
    uint8_t sizeOfbboxArray[2];    // Array to hold the bytes
    Packet::shortToByteArray(bboxes_byte_size, sizeOfbboxArray);
    controlPacket.putBytes(sizeOfbboxArray, 2);
    
    uint8_t sendBuffer[MAX_PACKET_SIZE];
    size_t sendLength = 0;
    controlPacket.prepareForTransmission(sendBuffer, sendLength);

    Serial.print("Sending packet with size: ");
    Serial.println(sendLength);

      delay(100); // try to delay so that all packets are recieved.

    LoRa.beginPacket();
    LoRa.write(sendBuffer, sendLength); // Send header without data
    LoRa.endPacket();
    Serial.println("Packet sent!");



    for (int i = 0; i < totalPackets; i++) {
      size_t dataSize = (i == totalPackets - 1) ? (bboxes_byte_size % max_packet_size) : max_packet_size;
      Packet packet;
      packet.type = 0x14; // bboxes

      uint8_t CurrentPacketArray[2];    // Array to hold the bytes
      Packet::shortToByteArray(i+1, CurrentPacketArray);
      packet.putBytes(CurrentPacketArray, 2); 

      uint8_t buffer[MAX_PACKET_SIZE];
      packet.putBytes(&bbox_buffer[i * max_packet_size], dataSize);
      ////////////
      uint8_t sendBuffer[MAX_PACKET_SIZE];
      size_t sendLengthImage = 0;
      packet.prepareForTransmission(sendBuffer, sendLengthImage);
      Serial.println("Printing estimation of what would be sent (weird):");
      Serial.println((char*)sendBuffer);
      //sendLengthImage += -sizeof(uint16_t) - sizeof(uint8_t)*2;  // TODO: dumb work around need a better way to do this
      // Send the packet

      // todo may need to slightly increase this
            delay(100); // try to delay so that all packets are recieved.

      LoRa.beginPacket();
      LoRa.write(sendBuffer, sendLengthImage); // Send header without data
      LoRa.endPacket();
    }

    
    // 

    /*// Add class data
    for (const auto& cls : classes) {
        if (index + 2 > MAX_PACKET_SIZE) {
            LoRa.beginPacket();
            LoRa.write(buffer, index);
            LoRa.endPacket();
            delay(100);
            index = 0;
        }
        cls.serialize(buffer, index);
    }

    // Send any remaining data
    if (index > 0) {
        LoRa.beginPacket();
        LoRa.write(buffer, index);
        LoRa.endPacket();
    }*/
}


void sendImage(uint8_t* imageData, size_t imageSize) {
    // TODO: I think total packets calc is wrong, can't remember why
    // it was done this way originally, likely need to use:
    // int16_t max_packet_size = (MAX_PACKET_SIZE - sizeof(uint16_t) - sizeof(uint8_t)*2);
    uint16_t totalPackets = (imageSize + MAX_PACKET_SIZE - 1) / MAX_PACKET_SIZE;
    Serial.print("Total packets to send: ");
    Serial.println(totalPackets);
    Serial.print("Size of image in bytes (b64 encoded): ");
    Serial.println(imageSize);
    // Send the first packet with the image size
    Packet controlPacket;
    controlPacket.type = 0x10;
    // put total packets
    uint8_t totalPacketsArray[2];    // Array to hold the bytes
    Packet::shortToByteArray(totalPackets, totalPacketsArray);
    controlPacket.putBytes(totalPacketsArray, 2);

    // put size of image
    uint8_t sizeOfImageArray[2];    // Array to hold the bytes
    Packet::shortToByteArray(imageSize, sizeOfImageArray);
    controlPacket.putBytes(sizeOfImageArray, 2);
    
    uint8_t sendBuffer[MAX_PACKET_SIZE];
    size_t sendLength = 0;
    controlPacket.prepareForTransmission(sendBuffer, sendLength);

    Serial.print("Sending packet with size: ");
    Serial.println(sendLength);


    LoRa.beginPacket();
    LoRa.write(sendBuffer, sendLength); // Send header without data
    LoRa.endPacket();
    Serial.println("Packet sent!");

    for (uint16_t i = 0; i < totalPackets; i++) {
      // with this missing other side was getting overwhelmed
      // and dropped half of the packets
      delay(100); // try to delay so that all packets are recieved.
      if (i < totalPackets - 1) {
        Packet image_packet;
        image_packet.type = 0x11;
        //packet.packetNum = i + 1; // Start from 1 since 0 is for image size

        // Make this a macro or function that does all of this
        uint8_t CurrentPacketArray[2];    // Array to hold the bytes
        Packet::shortToByteArray(i+1, CurrentPacketArray);
        image_packet.putBytes(CurrentPacketArray, 2);

        // Copy data into the packet
        int16_t max_packet_size = (MAX_PACKET_SIZE - sizeof(uint16_t) - sizeof(uint8_t)*2);
        size_t dataSize = (i == totalPackets - 1) ? (imageSize % max_packet_size) : max_packet_size;
        image_packet.putBytes(&imageData[i * max_packet_size], dataSize); //memcpy(packet.data, &imageData[i * MAX_PACKET_SIZE], dataSize);

        uint8_t sendBufferImage[MAX_PACKET_SIZE];
        size_t sendLengthImage = 0;
        image_packet.prepareForTransmission(sendBufferImage, sendLengthImage);
        //sendLengthImage += -sizeof(uint16_t) - sizeof(uint8_t)*2;  // TODO: dumb work around need a better way to do this
        // Send the packet
        LoRa.beginPacket();
        LoRa.write(sendBufferImage, sendLengthImage); // Send header without data
        LoRa.endPacket();
        
        Serial.print("Sent packet: ");
        Serial.println(i);
        Serial.print("dataSize: ");
        Serial.println(dataSize);
        Serial.print("max_packet_size: ");
        Serial.println(max_packet_size);
        Serial.print("sendLengthImage: ");
        Serial.println(sendLengthImage);
        Serial.print("&imageData[i * MAX_PACKET_SIZE]: ");

        uint8_t outBuffer[256] = {0};

        memcpy(outBuffer, &imageData[i * max_packet_size], dataSize);
        Serial.println(String((char*)(outBuffer)));
        
      } else {
        // last packet of the image sequence we are going to make slightly different.
        Packet image_packet; // last
        image_packet.type = 0x12;
        // Make this a macro or function that does all of this
        uint8_t CurrentPacketArray[2];    // Array to hold the bytes
        Packet::shortToByteArray(i+1, CurrentPacketArray);
        image_packet.putBytes(CurrentPacketArray, 2);

        // Copy data into the packet
        int16_t max_packet_size = (MAX_PACKET_SIZE - sizeof(uint16_t) - sizeof(uint8_t)*2);
        size_t dataSize = (i == totalPackets - 1) ? (imageSize % max_packet_size) : max_packet_size;
        image_packet.putBytes(&imageData[i * dataSize], dataSize); //memcpy(packet.data, &imageData[i * MAX_PACKET_SIZE], dataSize);

        uint8_t sendBufferImage[MAX_PACKET_SIZE];
        size_t sendLengthImage = 0;
        image_packet.prepareForTransmission(sendBufferImage, sendLengthImage);
        //sendLengthImage += -sizeof(uint16_t) - sizeof(uint8_t)*2; // TODO: dumb work around need a better way to do this

        // Send the packet
        LoRa.beginPacket();
        LoRa.write(sendBufferImage, sendLengthImage); // Send header without data
        LoRa.endPacket();
        
        Serial.print("Sent packet: ");
        Serial.println(i);

        Serial.print("dataSize: ");
        Serial.println(dataSize);
        Serial.print("max_packet_size: ");
        Serial.println(max_packet_size);
        Serial.print("sendLengthImage: ");
        Serial.println(sendLengthImage);
        Serial.print("&imageData[i * MAX_PACKET_SIZE]: ");

        uint8_t outBuffer[256] = {0};

        memcpy(outBuffer, &imageData[i * max_packet_size], dataSize);
        Serial.println(String((char*)(outBuffer)));
      }
    }

    // Now wait for retransmission requests
    /*while (true) {
        if (LoRa.parsePacket()) {
            // Check if it's a retransmission request
            uint8_t requestType = LoRa.read();
            if (requestType == 0b0010) { // Retransmission request
                uint8_t missingPacketNum = LoRa.read();
                Serial.print("Retransmission requested for packet: ");
                Serial.println(missingPacketNum);
                resendPacket(missingPacketNum, totalPackets, imageData);
            }
        }
    }*/
}


/*
void resendPacket(uint8_t packetNum, uint8_t totalPackets, uint8_t* imageData) {
    Packet packet;
    packet.packetNum = packetNum;
    packet.totalPackets = totalPackets;

    // Copy data into the packet
    memcpy(packet.data, &imageData[(packetNum - 1) * MAX_PACKET_SIZE], MAX_PACKET_SIZE);

    // Send the packet
    LoRa.beginPacket();
    LoRa.write((uint8_t*)&packet, sizeof(packet));
    LoRa.endPacket();
    
    Serial.print("Resent packet: ");
    Serial.println(packet.packetNum);
}*/
