
/**
 * @file Client.h
 * @brief Базовый интерфейс сетевого клиента.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../platform/print.hpp"
#include "IPAddress.h"

class Client : public Print
{
public:
    virtual int connect(IPAddress ip, uint16_t port) = 0;
    virtual int connect(const char* host, uint16_t port) = 0;
    virtual size_t write(uint8_t value) = 0;
    virtual size_t write(const uint8_t* buffer, size_t size) = 0;
    virtual int available(void) = 0;
    virtual int read(void) = 0;
    virtual int read(uint8_t* buffer, size_t size) = 0;
    virtual int peek(void) = 0;
    virtual void flush(void) = 0;
    virtual void stop(void) = 0;
    virtual uint8_t connected(void) = 0;
    virtual operator bool() = 0;

    using Print::write;
};
