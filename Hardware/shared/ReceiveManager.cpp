#include "ReceiveManager.hpp"

namespace lora {

ReceiveManager::ReceiveManager() : nodeID(0) {}

void ReceiveManager::setNodeID(uint8_t id) {
    nodeID = id;
}

void ReceiveManager::handleIncoming(uint8_t* buf, uint8_t len) {
    LoRaPacket pkt;
    if (!decodePacket(buf, (size_t)len, &pkt)) return;

    if (pkt.flags & 0x01) {
        // Incoming packet is an ACK
        // Should be passed to SendManager externally
        return;
    }

    if (pkt.flags & 0x02) {
        // Requires ACK
        sendAck(pkt.nodeID, pkt.seqNum); 
    }

    // Deliver to application logic
    if (onReceive) {
        onReceive(pkt);
    }
}

void ReceiveManager::sendAck(uint8_t targetNodeID, uint8_t seqNum) {
    LoRaPacket ack = {
        .type = 0,
        .subtype = 0,
        .seqNum = seqNum,
        .flags = 0x01, // ACK flag
        .nodeID = targetNodeID,
        .payloadSize = 0
    };

    uint8_t encoded[HEADER_SIZE];
    encodePacket(&ack, encoded);

    if (rf95.waitCAD()) {
        rf95.send(encoded, HEADER_SIZE);
        rf95.waitPacketSent();
    }
}

} // namespace lora
