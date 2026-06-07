#ifndef GAME_H__
#define GAME_H__

#include <stdint.h>

// update params after receiving NEW_GAME.request packet
void game_set_params(uint8_t playerNumber, uint8_t numberOfPlayers, float mapWidth, float mapHeight);

// update the state of an object
void game_update_object(uint8_t objectType, uint16_t objectNo, int8_t hp, float x, float y);

// for AMCOM_MoveResponsePayload respose packet
void game_get_action(float *angle, uint8_t *action);


#endif /* GAME_H__ */