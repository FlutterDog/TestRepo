
/**
 * @file socket.h
 * @brief Сокетный слой W5500.
 */

#pragma once

#include <stdint.h>

#include "w5500.h"

uint8_t socket(SOCKET socket, uint8_t protocol, uint16_t port, uint8_t flag);
void close(SOCKET socket);
uint8_t connect(SOCKET socket, uint8_t* address, uint16_t port);
void disconnect(SOCKET socket);
uint8_t listen(SOCKET socket);

uint16_t send(SOCKET socket, const uint8_t* buffer, uint16_t length);
int16_t recv(SOCKET socket, uint8_t* buffer, int16_t length);
uint16_t peek(SOCKET socket, uint8_t* buffer);

uint16_t sendto(SOCKET socket, const uint8_t* buffer, uint16_t length, uint8_t* address, uint16_t port);
uint16_t recvfrom(SOCKET socket, uint8_t* buffer, uint16_t length, uint8_t* address, uint16_t* port);

uint16_t bufferData(SOCKET socket, uint16_t offset, const uint8_t* buffer, uint16_t length);
int startUDP(SOCKET socket, uint8_t* address, uint16_t port);
int sendUDP(SOCKET socket);

void flush(SOCKET socket);
