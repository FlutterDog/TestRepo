/**
 * @file Server.h
 * @brief Базовый интерфейс сетевого сервера.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../platform/print.hpp"

/**
 * @brief Базовый сетевой сервер.
 */
class Server : public Print
{
public:
    virtual void begin(void) = 0;

    virtual size_t write(uint8_t value) = 0;
    virtual size_t write(const uint8_t* buffer, size_t size) = 0;

    using Print::write;
};
