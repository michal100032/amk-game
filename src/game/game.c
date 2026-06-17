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

//constants
#define REPULSION_GLUE            (-10.0e6f)
#define REPULSION_SPARK           ( -10.0e6f) 

#define BIGGER_PLAYER_THRESHOLD 150.0f
#define SMALLER_PLAYER_THRESHOLD 300.0f
#define PREDICTION_TIME 5.0f
#define DISTANCE_MOD 100.0f

//variable depending on state
static float foodWeight;
static float smallerPlayerWeight;
static float biggerPlayerWeight;



enum object_type {
    OBJECT_TYPE_PLAYER = 0,
    OBJECT_TYPE_FOOD = 1,
    OBJECT_TYPE_SPARK = 2,
    OBJECT_TYPE_GLUE = 3
};

//enum to set bot state
enum BotState {
    BOT_EAT,
    BOT_HUNT,
    BOT_RUN
};

static enum BotState m_botState = BOT_EAT;


struct GameObject {
    enum object_type objectType; // 0 = player, 1 = food, 2 = spark, 3 = glue
    int8_t hp; // object hp
    float x; // X position on map
    float y; // Y position on map

    float xPrev; // previous X position
    float yPrev; // previous Y position

    bool hasPreviousPosition;
};

static struct GameObject m_players[MAX_PLAYERS];
static struct GameObject m_food[MAX_FOOD];
static struct GameObject m_glue[MAX_GLUE];
static struct GameObject m_sparks[MAX_SPARKS];

//actively track players to set state
static void update_bot_state(void)
{

    bool danger = false;
    bool preyAvailable = false;

    for(size_t i = 0; i < MAX_PLAYERS; i++) {

        if(i == m_playerNumber || m_players[i].hp <= 0) {
            continue;
        }

        float dx = m_players[i].x - m_players[m_playerNumber].x;
        float dy = m_players[i].y - m_players[m_playerNumber].y;
        float dist2 = (dx * dx) + (dy * dy);

        if(m_players[i].hp > m_players[m_playerNumber].hp && dist2 < BIGGER_PLAYER_THRESHOLD)
        {
            danger = true;
        }

        if(m_players[i].hp < m_players[m_playerNumber].hp && dist2 < SMALLER_PLAYER_THRESHOLD)
        {
            preyAvailable = true;
        }
    }

    if(danger) {
        m_botState = BOT_RUN;
    }
    else if(preyAvailable) {
        m_botState = BOT_HUNT;
    }
    else {
        m_botState = BOT_EAT;
    }
}

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
        //update previous position
        if(!object->hasPreviousPosition) {
            object->xPrev = x;
            object->yPrev = y;
        } else {
            object->xPrev = object->x;
            object->yPrev = object->y;
        }

        object->objectType = (enum object_type)objectType;

        object->hp = hp;
        object->x = x;
        object->y = y;
        object->hasPreviousPosition = true;
    }
}


//calculate velocity
static void get_velocity(const struct GameObject *obj, float *vx, float *vy)
{
    if (!obj->hasPreviousPosition) {
        *vx = 0.0f;
        *vy = 0.0f;
        return;
    }

    *vx = obj->x - obj->xPrev;
    *vy = obj->y - obj->yPrev;
}


//predict objects positions

static void predict_position_weighted(const struct GameObject *obj, float *px, float *py)
{
    float vx, vy;
    get_velocity(obj, &vx, &vy);


    float predX = obj->x + vx * PREDICTION_TIME;
    float predY = obj->y + vy * PREDICTION_TIME;

    float dx = obj->x - m_players[m_playerNumber].x;
    float dy = obj->y - m_players[m_playerNumber].y;

    float dist2 = dx*dx + dy*dy;

    float w = dist2 / (DISTANCE_MOD + dist2);


    *px = obj->x + (predX - obj->x) * w;
    *py = obj->y + (predY - obj->y) * w;
}

static void update_weights(void)
{
    switch(m_botState) {

    case BOT_EAT:
        foodWeight          = 1000.0f;
        smallerPlayerWeight = 5000.0f;
        biggerPlayerWeight  = -100000.0f;
        break;

    case BOT_HUNT:
        foodWeight          = 50.0f;
        smallerPlayerWeight = 200000.0f;
        biggerPlayerWeight  = -500000.0f;
        break;

    case BOT_RUN:
        foodWeight          = 50.0f;
        smallerPlayerWeight = 50000.0f;
        biggerPlayerWeight  = -1000000.0f;
        break;
    }
}

//NOT USED RN
//
//get angle at wich the player is pointing
static float get_players_angle(struct GameObject *object) {
    if(OBJECT_TYPE_PLAYER == object->objectType) {
        if(object->hasPreviousPosition) {
            return atan2f(
                object->y - object->yPrev,
                object->x - object->xPrev);
        }
    }
    return 0.0f;
}

//get angle between player and our mcu
static float get_angle_relative(struct GameObject *object) {
    if(OBJECT_TYPE_PLAYER == object->objectType) {
        if(object->hasPreviousPosition) {
            return atan2f(
                m_players[m_playerNumber].y - object->y,
                m_players[m_playerNumber].x - object->x);
        }
    }
    return 0.0f;
}

//are we "in sight" of the other player
static bool is_in_sight(struct GameObject *object) {
    float angleRelative = get_angle_relative(object);
    float angleDerivative = get_players_angle(object);
    float dot = cosf(angleRelative - angleDerivative);

    if (dot > cosf(M_PI / 3.0f)) {
        return true;
    } else {
        return false;
    }
}

//end of NOT USED RN

static uint8_t count_food_nearby(size_t foodIndex, float radius)
{
    uint8_t count = 0;

    float radiusSquared = radius * radius;

    for(size_t i = 0; i < MAX_FOOD; i++) {
        if(i == foodIndex || m_food[i].hp <= 0)
            continue;

        float dx = m_food[i].x - m_food[foodIndex].x;
        float dy = m_food[i].y - m_food[foodIndex].y;

        if(dx * dx + dy * dy < radiusSquared) {
            count++;
        }
    }

    return count;
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
    float objX, objY;
    predict_position_weighted(object, &objX, &objY);
    float distanceX = objX - m_players[m_playerNumber].x;
    float distanceY = objY - m_players[m_playerNumber].y;
    float distanceSquared = distanceX * distanceX + distanceY * distanceY;
    float inverseDistanceSquared = 1.0f / (distanceSquared + 0.0001f); // add small value to avoid division by zero

    switch(object->objectType) {
        case OBJECT_TYPE_PLAYER:
            if(object->hp >= m_players[m_playerNumber].hp) {
                *forceX += biggerPlayerWeight *inverseDistanceSquared *inverseDistanceSquared *distanceX;
                *forceY += biggerPlayerWeight *inverseDistanceSquared *inverseDistanceSquared *distanceY;
            } else{
                *forceX += smallerPlayerWeight *inverseDistanceSquared *distanceX;
                *forceY += smallerPlayerWeight *inverseDistanceSquared *distanceY;
            }
            break;
        case OBJECT_TYPE_FOOD:
            // food attracts the player
            uint8_t nearbyFood = count_food_nearby((size_t)(object - m_food),50.0f);
            float clusterMultiplier = 1.0f + nearbyFood * 5.0f;
            *forceX += foodWeight *clusterMultiplier *inverseDistanceSquared *distanceX;
            *forceY += foodWeight *clusterMultiplier *inverseDistanceSquared *distanceY;
            break;
        case OBJECT_TYPE_SPARK:
            // sparks repel the player
            *forceX += REPULSION_SPARK * inverseDistanceSquared * inverseDistanceSquared * distanceX;
            *forceY += REPULSION_SPARK * inverseDistanceSquared * inverseDistanceSquared * distanceY;
            break;
        case OBJECT_TYPE_GLUE:
            // glue repells the player
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
    for(size_t i = 0; i < MAX_PLAYERS; i++) {
        if(m_players[i].hp > 0 && i != m_playerNumber) { // only consider other players that are still alive
            float objectForceX = 0.0f;
            float objectForceY = 0.0f;
            get_object_force(&m_players[i], &objectForceX, &objectForceY);
            *forceX += objectForceX;
            *forceY += objectForceY;
        }
    }
    for(size_t i = 0; i < MAX_FOOD; i++) {
        if(m_food[i].hp > 0) { // only consider food that is still available
            float objectForceX = 0.0f;
            float objectForceY = 0.0f;
            get_object_force(&m_food[i], &objectForceX, &objectForceY);
            *forceX += objectForceX;
            *forceY += objectForceY;
        }
    }
    for(size_t i = 0; i < MAX_SPARKS; i++) {
        if(m_sparks[i].hp > 0) { // only consider sparks that are still active
            float objectForceX = 0.0f;
            float objectForceY = 0.0f;
            get_object_force(&m_sparks[i], &objectForceX, &objectForceY);
            *forceX += objectForceX;
            *forceY += objectForceY;
        }
    }
    for(size_t i = 0; i < MAX_GLUE; i++) {
        if(m_glue[i].hp > 0) { // only consider glue that is still active
            float objectForceX = 0.0f;
            float objectForceY = 0.0f;
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
            //if(is_in_sight(&m_players[i])) {
                return true;
            //}
        }
    }
    return false;
}

// for AMCOM_MoveResponsePayload respose packet
void game_get_action(float *angle, uint8_t *action)
{
    update_bot_state();
    update_weights();

    float forceX = 0.0f;
    float forceY = 0.0f;

    get_total_force(&forceX, &forceY);

    *angle = atan2f(forceY, forceX);

    *action = (m_botState == BOT_RUN);
}