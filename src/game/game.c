#include "game.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static uint8_t m_playerNumber;
static uint8_t m_numberOfPlayers;
static float m_mapWidth;
static float m_mapHeight;

#define MAX_PLAYERS 8
#define MAX_FOOD 100
#define MAX_GLUE 8
#define MAX_SPARKS 24

#define REPULSION_GLUE            (-10.0e6f)
#define REPULSION_SPARK           ( -0.6e6f)
#define REPULSION_BIGGER_PLAYER   ( -1.0e6f)
#define ATTRACTION_FOOD           ( 240.0f)
#define ATTRACTION_SMALLER_PLAYER ( 120.0f)

#define BIGGER_PLAYER_THRESHOLD 100.0f

enum object_type {
    OBJECT_TYPE_PLAYER = 0,
    OBJECT_TYPE_FOOD = 1,
    OBJECT_TYPE_SPARK = 2,
    OBJECT_TYPE_GLUE = 3
};

struct GameObject {
    enum object_type objectType; // 0 = player, 1 = food, 2 = spark, 3 = glue
    int8_t hp; // object hp
    float x; // X position on map
    float y; // Y position on map
};

static struct GameObject m_players[MAX_PLAYERS];
static struct GameObject m_food[MAX_FOOD];
static struct GameObject m_glue[MAX_GLUE];
static struct GameObject m_sparks[MAX_SPARKS];

// update params after receiving NEW_GAME.request packet
void game_set_params(uint8_t playerNumber, uint8_t numberOfPlayers, float mapWidth, float mapHeight) {
    m_playerNumber = playerNumber;
    m_numberOfPlayers = numberOfPlayers;
    m_mapWidth = mapWidth;
    m_mapHeight = mapHeight;
}

static void update_object_state(uint8_t objectType, uint16_t objectNo, int8_t hp, float x, float y) {
    struct GameObject *object = NULL;
    switch((enum object_type)objectType) {
        case OBJECT_TYPE_PLAYER: // player
            if(objectNo < MAX_PLAYERS) {
                object = &m_players[objectNo];
            }
            break;
        case OBJECT_TYPE_FOOD: // food
            if(objectNo < MAX_FOOD) {
                object = &m_food[objectNo];
            }
            break;
        case OBJECT_TYPE_SPARK: // spark
            if(objectNo < MAX_SPARKS) {
                object = &m_sparks[objectNo];
            }
            break;
        case OBJECT_TYPE_GLUE: // glue
            if(objectNo < MAX_GLUE) {
                object = &m_glue[objectNo];
            }
            break;
    }
    if(object != NULL) {
        object->objectType = (enum object_type)objectType;
        object->hp = hp;
        object->x = x;
        object->y = y;
    }
}

static int get_nearest_food_index(float x, float y) {
    int nearestFoodIndex = -1;
    float nearestFoodDistance = 0.0f;
    for(size_t i = 0; i < MAX_FOOD; i++) {
        if(m_food[i].hp > 0) { // only consider food that is still available
            float dx = m_food[i].x - x;
            float dy = m_food[i].y - y;
            float distance = dx * dx + dy * dy; // squared distance
            if(nearestFoodIndex == -1 || distance < nearestFoodDistance) {
                nearestFoodIndex = (int)i;
                nearestFoodDistance = distance;
            }
        }
    }
    return nearestFoodIndex;
}

// update the state of an object
void game_update_object(uint8_t objectType, uint16_t objectNo, int8_t hp, float x, float y) {
    update_object_state(objectType, objectNo, hp, x, y);
}

static void get_object_force(struct GameObject const *object, float *forceX, float *forceY) {
    float distanceX = object->x - m_players[m_playerNumber].x;
    float distanceY = object->y - m_players[m_playerNumber].y;
    float distanceSquared = distanceX * distanceX + distanceY * distanceY;
    float inverseDistanceSquared = 1.0f / (distanceSquared + 0.0001f); // add small value to avoid division by zero

    switch(object->objectType) {
        case OBJECT_TYPE_PLAYER:
            if (object->hp >= m_players[m_playerNumber].hp) {
                // bigger player attracts the player
                *forceX += REPULSION_BIGGER_PLAYER * inverseDistanceSquared * inverseDistanceSquared * distanceX;
                *forceY += REPULSION_BIGGER_PLAYER * inverseDistanceSquared * inverseDistanceSquared * distanceY;
            } else if(object->hp < m_players[m_playerNumber].hp) {
                // smaller player repels the player
                *forceX += ATTRACTION_SMALLER_PLAYER * inverseDistanceSquared * distanceX;
                *forceY += ATTRACTION_SMALLER_PLAYER * inverseDistanceSquared * distanceY;
            }
            break;
        case OBJECT_TYPE_FOOD:
            // food attracts the player
            *forceX += ATTRACTION_FOOD * inverseDistanceSquared * distanceX;
            *forceY += ATTRACTION_FOOD * inverseDistanceSquared * distanceY;
            break;
        case OBJECT_TYPE_SPARK:
            // sparks repel the player
            *forceX += REPULSION_SPARK * inverseDistanceSquared * inverseDistanceSquared * distanceX;
            *forceY += REPULSION_SPARK * inverseDistanceSquared * inverseDistanceSquared * distanceY;
            break;
        case OBJECT_TYPE_GLUE:
            // glue attracts the player but reduces its speed
            *forceX += REPULSION_GLUE * inverseDistanceSquared * inverseDistanceSquared * distanceX;
            *forceY += REPULSION_GLUE * inverseDistanceSquared * inverseDistanceSquared * distanceY;
            break;
        default:
            *forceX = 0.0f;
            *forceY = 0.0f;
            break;
    }
    
}

static void get_total_force(float *forceX, float *forceY) {
    float objectForceX = 0.0f;
    float objectForceY = 0.0f;
    for(size_t i = 0; i < MAX_PLAYERS; i++) {
        if(m_players[i].hp > 0 && i != m_playerNumber) { // only consider other players that are still alive
            get_object_force(&m_players[i], &objectForceX, &objectForceY);
            *forceX += objectForceX;
            *forceY += objectForceY;
        }
    }
    for(size_t i = 0; i < MAX_FOOD; i++) {
        if(m_food[i].hp > 0) { // only consider food that is still available
            get_object_force(&m_food[i], &objectForceX, &objectForceY);
            *forceX += objectForceX;
            *forceY += objectForceY;
        }
    }
    for(size_t i = 0; i < MAX_SPARKS; i++) {
        if(m_sparks[i].hp > 0) { // only consider sparks that are still active
            get_object_force(&m_sparks[i], &objectForceX, &objectForceY);
            *forceX += objectForceX;
            *forceY += objectForceY;
        }
    }
    for(size_t i = 0; i < MAX_GLUE; i++) {
        if(m_glue[i].hp > 0) { // only consider glue that is still active
            get_object_force(&m_glue[i], &objectForceX, &objectForceY);
            *forceX += objectForceX;
            *forceY += objectForceY;
        }
    }
}

static bool is_bigger_player_ahead(void) {
    for(size_t i = 0; i < MAX_PLAYERS; i++) {
        float distanceSquared = (m_players[i].x - m_players[m_playerNumber].x) * (m_players[i].x - m_players[m_playerNumber].x) +
                                (m_players[i].y - m_players[m_playerNumber].y) * (m_players[i].y - m_players[m_playerNumber].y);
        
        if(distanceSquared < BIGGER_PLAYER_THRESHOLD && m_players[i].hp > m_players[m_playerNumber].hp) {
            return true;
        }
    }
    return false;
}

// for AMCOM_MoveResponsePayload respose packet
void game_get_action(float *angle, uint8_t *action) {
    float forceX = 0.0f;
    float forceY = 0.0f;
    get_total_force(&forceX, &forceY);
    printf("Total force: (%f, %f)\n", forceX, forceY);
    *angle = atan2f(forceY, forceX);
    printf("Angle: %f\n", *angle);
    *action = is_bigger_player_ahead() ? 1 : 0; // drop spark if there is a bigger player ahead, otherwise do nothing

    // uint8_t foodIndex = get_nearest_food_index(m_players[m_playerNumber].x, m_players[m_playerNumber].y);
    // if(foodIndex != (uint8_t)-1) {
    //     float dx = m_food[foodIndex].x - m_players[m_playerNumber].x;
    //     float dy = m_food[foodIndex].y - m_players[m_playerNumber].y;
    //     *angle = atan2f(dy, dx);
    //     *action = 0; // move towards the food
    // } else {
    //     *angle = 0.0f;
    //     *action = 0; // no food available, do nothing
    // }
}