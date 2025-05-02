#pragma once

#include "node_shared.hpp"
#include <stdint.h>
#include <Arduino.h>
#include <functional>

namespace lora {

class ReceiveManager {
public:
    ReceiveManager();

    // Call when you receive a raw packet
    void handleIncoming(uint8_t* buf, uint8_t len);

    // Set from the main logic
    void setNodeID(uint8_t id);

    // User-defined callback to handle received data packets
    std::function<void(const lora::LoRaPacket&)> onReceive;

private:
    uint8_t nodeID;

    void sendAck(uint8_t targetNodeID, uint8_t seqNum);
};

} // namespace lora
