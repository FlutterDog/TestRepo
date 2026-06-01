
/**
 * @file EthernetServer.h
 * @brief TCP-сервер W5500.
 */

#pragma once

#include <stdint.h>

#include "Server.h"
#include "EthernetClient.h"

class EthernetServer : public Server
{
public:
    explicit EthernetServer(uint16_t port);

    void begin(void) override;
    EthernetClient available(void);

    size_t write(uint8_t value) override;
    size_t write(const uint8_t* buffer, size_t size) override;

    using Print::write;

private:
    void accept(void);

    uint16_t port_;
};
