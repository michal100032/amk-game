#undef UNICODE
#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "amcom.h"
#include "amcom_packets.h"

#include "game.h"

void amcomPacketHandler(AMCOM_Packet const *packet, void *userContext) {
  uint8_t buf[AMCOM_MAX_PACKET_SIZE];              // buffer used to serialize outgoing packets
  size_t toSend = 0;                               // size of the outgoing packet
  SOCKET ConnectSocket = *((SOCKET *)userContext); // socket used for communication with the server

  switch(packet->header.type) {
    case AMCOM_IDENTIFY_REQUEST: {
      printf("Got IDENTIFY.request. Responding with IDENTIFY.response\n");
      AMCOM_IdentifyResponsePayload identifyResponse;
      (void)sprintf(identifyResponse.playerName, "mniAM player");
      toSend = AMCOM_Serialize(AMCOM_IDENTIFY_RESPONSE, &identifyResponse, sizeof(identifyResponse), buf);
      break;
    }
    case AMCOM_NEW_GAME_REQUEST: {
      AMCOM_NewGameRequestPayload const *newGameRequest = (AMCOM_NewGameRequestPayload const *)packet->payload;
      game_set_params(newGameRequest->playerNumber, newGameRequest->numberOfPlayers, newGameRequest->mapWidth, newGameRequest->mapHeight);
      printf("Got NEW_GAME.request. Responding with NEW_GAME.response\n");
      AMCOM_NewGameResponsePayload newGameResponse;
      (void)sprintf(newGameResponse.helloMessage, "Hello, I'm player %d", newGameRequest->playerNumber);
      toSend = AMCOM_Serialize(AMCOM_NEW_GAME_RESPONSE, &newGameResponse, sizeof(newGameResponse), buf);
      break;
    }
    case AMCOM_OBJECT_UPDATE_REQUEST: {
      AMCOM_ObjectUpdateRequestPayload const *objectUpdateRequest = (AMCOM_ObjectUpdateRequestPayload const *)packet->payload;
      for(size_t i = 0; i < packet->header.length / sizeof(AMCOM_ObjectState); i++) {
        game_update_object(objectUpdateRequest->objectState[i].objectType, objectUpdateRequest->objectState[i].objectNo, objectUpdateRequest->objectState[i].hp, objectUpdateRequest->objectState[i].x, objectUpdateRequest->objectState[i].y);
      }
      break;
    }
    case AMCOM_MOVE_REQUEST: {
      float angle;
      uint8_t action;
      game_get_action(&angle, &action);
      AMCOM_MoveResponsePayload moveResponse = {
          .angle = angle,
          .action = action
      };
      toSend = AMCOM_Serialize(AMCOM_MOVE_RESPONSE, &moveResponse, sizeof(moveResponse), buf);
      break;
    }
    case AMCOM_GAME_OVER_REQUEST: {
      AMCOM_GameOverResponsePayload gameOverResponse = {
          .endMessage = "Goodbye, cruel world!"
      };
      toSend = AMCOM_Serialize(AMCOM_GAME_OVER_RESPONSE, &gameOverResponse, sizeof(gameOverResponse), buf);
      break;
    }
    default: {
      printf("Got unknown packet type %d. Ignoring.\n", packet->header.type);
      break;
    }
  }

  // if there is something to send back - do it
  if(toSend > 0) {
    int bytesSent = send(ConnectSocket, (char const *)buf, (int)toSend, 0);
    if(bytesSent == SOCKET_ERROR) {
      printf("Socket send failed with error: %d\n", WSAGetLastError());
      closesocket(ConnectSocket);
      return;
    }
  }
}

#define GAME_SERVER "localhost"
#define GAME_SERVER_PORT "2001"

int main(int argc, char **argv) {
  (void)argc; // avoid unused parameter warning
  (void)argv; // avoid unused parameter warning

  printf("This is mniAM player. Let's eat some transistors! \n");

  WSADATA wsaData;
  int iResult;

  // Initialize Winsock library (windows sockets)
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if(iResult != 0) {
    printf("WSAStartup failed with error: %d\n", iResult);
    return 1;
  }

  // Prepare temporary data
  SOCKET ConnectSocket = INVALID_SOCKET;
  struct addrinfo *result = NULL;
  struct addrinfo *ptr = NULL;
  struct addrinfo hints;
  char recvbuf[512];
  int recvbuflen = sizeof(recvbuf);

  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;

  // Resolve the game server address and port
  iResult = getaddrinfo(GAME_SERVER, GAME_SERVER_PORT, &hints, &result);
  if(iResult != 0) {
    printf("getaddrinfo failed with error: %d\n", iResult);
    WSACleanup();
    return 1;
  }

  printf("Connecting to game server...\n");
  // Attempt to connect to an address until one succeeds
  for(ptr = result; ptr != NULL; ptr = ptr->ai_next) {

    // Create a SOCKET for connecting to server
    ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if(ConnectSocket == INVALID_SOCKET) {
      printf("Socket failed with error: %d\n", WSAGetLastError());
      WSACleanup();
      return 1;
    }

    // Connect to server
    iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if(iResult == SOCKET_ERROR) {
      closesocket(ConnectSocket);
      ConnectSocket = INVALID_SOCKET;
      continue;
    }
    break;
  }
  // Free some used resources
  freeaddrinfo(result);

  // Check if we connected to the game server
  if(ConnectSocket == INVALID_SOCKET) {
    printf("Unable to connect to the game server!\n");
    WSACleanup();
    return 1;
  } else {
    printf("Connected to game server\n");
  }

  // Initialize AMCOM receiver with the packet handler and the socket as user context
  AMCOM_Receiver amReceiver;
  AMCOM_InitReceiver(&amReceiver, amcomPacketHandler, &ConnectSocket);

  // Receive until the peer closes the connection
  do {

    iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
    if(iResult > 0) {
      AMCOM_Deserialize(&amReceiver, recvbuf, (size_t)iResult);
    } else if(iResult == 0) {
      printf("Connection closed\n");
    } else {
      printf("recv failed with error: %d\n", WSAGetLastError());
    }

  } while(iResult > 0);

  // No longer need the socket
  closesocket(ConnectSocket);
  // Clean up
  WSACleanup();

  return 0;
}
