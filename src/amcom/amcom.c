#include "amcom.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/// Start of packet character
uint8_t const AMCOM_SOP = 0xA1;
uint16_t const AMCOM_INITIAL_CRC = 0xFFFF;

static uint16_t AMCOM_UpdateCRC(uint8_t byte, uint16_t crc) {
  // NOLINTBEGIN(readability-magic-numbers)
  byte ^= (uint8_t)(crc & 0x00ff);
  byte ^= (uint8_t)(byte << 4);
  return ((((uint16_t)byte << 8) | (uint8_t)(crc >> 8)) ^ (uint8_t)(byte >> 4) ^ ((uint16_t)byte << 3));
  // NOLINTEND(readability-magic-numbers)
}

void AMCOM_InitReceiver(AMCOM_Receiver *receiver, AMCOM_PacketHandler packetHandlerCallback, void *userContext) {
  receiver->packetHandler = packetHandlerCallback;
  receiver->payloadCounter = 0;
  receiver->receivedPacketState = AMCOM_PACKET_STATE_EMPTY;
  receiver->userContext = userContext;
}

static uint16_t calculateCrc(uint8_t type, uint8_t length, void const *payload) {
  uint16_t crc = AMCOM_INITIAL_CRC;
  crc = AMCOM_UpdateCRC(type, crc);
  crc = AMCOM_UpdateCRC(length, crc);
  for (size_t i = 0; i < length; i++) {
    crc = AMCOM_UpdateCRC(((const uint8_t *)payload)[i], crc);
  }
  return crc;
}

size_t AMCOM_Serialize(uint8_t packetType, void const *payload, size_t payloadSize, uint8_t *destinationBuffer) {
  if (destinationBuffer == NULL || payloadSize > 200 || (payloadSize > 0 && payload == NULL)) {
    return 0;
  }

  AMCOM_PacketHeader header;
  header.sop = AMCOM_SOP;
  header.type = packetType;
  header.length = payloadSize;
  header.crc = calculateCrc(packetType, payloadSize, payload);

  memcpy(destinationBuffer, &header, sizeof(AMCOM_PacketHeader));
  if (payloadSize > 0 && payload != NULL) {
    memcpy(destinationBuffer + sizeof(AMCOM_PacketHeader), payload, payloadSize);
  }
  return sizeof(AMCOM_PacketHeader) + payloadSize;
}


static void processGotWholePacket(AMCOM_Receiver *receiver) {
  // receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_WHOLE_PACKET;
  uint16_t crc = calculateCrc(receiver->receivedPacket.header.type,
                              receiver->receivedPacket.header.length,
                              receiver->receivedPacket.payload);
  if (crc == receiver->receivedPacket.header.crc) {
    receiver->packetHandler(&receiver->receivedPacket, receiver->userContext);
  }
  receiver->receivedPacketState = AMCOM_PACKET_STATE_EMPTY;
}

void AMCOM_Deserialize(AMCOM_Receiver *receiver, void const *data, size_t dataSize) {
  for (size_t i = 0; i < dataSize; i++) {
    uint8_t byte = ((const uint8_t *)data)[i];
    switch (receiver->receivedPacketState) {
      case AMCOM_PACKET_STATE_EMPTY: {
        if (byte == AMCOM_SOP) {
          receiver->receivedPacket.header.sop = byte;
          receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_SOP;
        }
        break;
      }
      case AMCOM_PACKET_STATE_GOT_SOP: {
        receiver->receivedPacket.header.type = byte;
        receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_TYPE;
        break;
      }
      case AMCOM_PACKET_STATE_GOT_TYPE: {
        if (byte > 200) {
          receiver->receivedPacketState = AMCOM_PACKET_STATE_EMPTY;
        } else {
          receiver->receivedPacket.header.length = byte;
          receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_LENGTH;
        }
        break;
      }
      case AMCOM_PACKET_STATE_GOT_LENGTH: {
        receiver->receivedPacket.header.crc = byte;
        receiver->receivedPacketState = AMCOM_PACKET_STATE_GOT_CRC_LO;
        break;
      }
      case AMCOM_PACKET_STATE_GOT_CRC_LO: {
        receiver->receivedPacket.header.crc |= byte << 8;
        if (receiver->receivedPacket.header.length == 0) {
          processGotWholePacket(receiver);
        } else {
          receiver->payloadCounter = 0;
          receiver->receivedPacketState = AMCOM_PACKET_STATE_GETTING_PAYLOAD;
        }
        break;
      }
      case AMCOM_PACKET_STATE_GETTING_PAYLOAD: {
        receiver->receivedPacket.payload[receiver->payloadCounter] = byte;
        receiver->payloadCounter++;

        if (receiver->payloadCounter == receiver->receivedPacket.header.length) {
          processGotWholePacket(receiver);
        }
        break;
      }
      default: {
        break;
      }
      // case AMCOM_PACKET_STATE_GOT_WHOLE_PACKET: {
      //   break;
      // }
    }
  }
}
