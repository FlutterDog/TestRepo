
/**
 * @file Udp.h
 * @brief Базовый интерфейс UDP.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../platform/print.hpp"
#include "IPAddress.h"

class UDP : public Print
{
public:
    virtual uint8_t begin(uint16_t port) = 0;
    virtual void stop(void) = 0;

    virtual int beginPacket(IPAddress ip, uint16_t port) = 0;
    virtual int endPacket(void) = 0;

    virtual size_t write(uint8_t value) = 0;
    virtual size_t write(const uint8_t* buffer, size_t size) = 0;

    virtual int parsePacket(void) = 0;
    virtual int available(void) = 0;
    virtual int read(void) = 0;
    virtual int read(unsigned char* buffer, size_t length) = 0;
    virtual int read(char* buffer, size_t length) = 0;
    virtual int peek(void) = 0;
    virtual void flush(void) = 0;

    virtual IPAddress remoteIP(void) = 0;
    virtual uint16_t remotePort(void) = 0;

    using Print::write;
};
