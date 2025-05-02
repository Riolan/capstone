#include <Arduino.h>
#include <unity.h>
#include "lora_packet.h"    // Your packet definition
#include "packet_utils.h"   // encodePacket, decodePacket
#include "node_registry.h"  // findNodeIndexByDeviceID, knownNodes, etc.

void test_join_ack_packet_correctness() {
    uint64_t fakeDeviceID = 0x123456789ABCDEF0;
    resetKnownNodes(); // custom function that clears knownNodes[]

    // Simulate assigning ID
    int nodeIndex = findNodeIndexByDeviceID(fakeDeviceID);
    uint8_t assignedID;

    if (nodeIndex == -1) {
        assignedID = 1;
        knownNodes[0] = { fakeDeviceID, assignedID };
        nodeCount = 1;
    } else {
        assignedID = knownNodes[nodeIndex].assignedNodeID;
    }

    // Create JOIN_ACK
    LoRaPacket ack;
    ack.type = PACKET_TYPE_JOIN_ACK;
    ack.nodeID = assignedID;
    ack.payload[0] = assignedID;
    ack.payloadSize = 1;

    // Encode
    uint8_t buffer[RH_RF95_MAX_MESSAGE_LEN];
    encodePacket(&ack, buffer);

    // Decode
    LoRaPacket decoded;
    TEST_ASSERT_TRUE(decodePacket(buffer, &decoded));

    // Check all fields
    TEST_ASSERT_EQUAL_UINT8(PACKET_TYPE_JOIN_ACK, decoded.type);
    TEST_ASSERT_EQUAL_UINT8(assignedID, decoded.nodeID);
    TEST_ASSERT_EQUAL_UINT8(1, decoded.payloadSize);
    TEST_ASSERT_EQUAL_UINT8(assignedID, decoded.payload[0]);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_join_ack_packet_correctness);
    UNITY_END();
}

void loop() {
    // unused
}
