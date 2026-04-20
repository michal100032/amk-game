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
  // TODO
}

size_t AMCOM_Serialize(uint8_t packetType, void const *payload, size_t payloadSize, uint8_t *destinationBuffer) {
  // TODO
  return 0;
}

void AMCOM_Deserialize(AMCOM_Receiver *receiver, void const *data, size_t dataSize) {
  // TODO
}
