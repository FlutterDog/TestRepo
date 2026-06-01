
/**
 * @file EthernetClient.h
 * @brief TCP-клиент W5500.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Client.h"
#include "IPAddress.h"
#include "utility/w5500.h"

class EthernetClient : public Client
{
public:
    EthernetClient();
    explicit EthernetClient(uint8_t socket);

    uint8_t status(void);

    int connect(IPAddress ip, uint16_t port) override;
    int connect(const char* host, uint16_t port) override;

    size_t write(uint8_t value) override;
    size_t write(const uint8_t* buffer, size_t size) override;

    int available(void) override;
    int read(void) override;
    int read(uint8_t* buffer, size_t size) override;
    int peek(void) override;

    void flush(void) override;
    void stop(void) override;

    uint8_t connected(void) override;
    operator bool() override;

    bool operator==(const EthernetClient& client) const;
    bool operator!=(const EthernetClient& client) const;

    uint8_t getSocketNumber(void);
    IPAddress remoteIP(void);
    uint16_t remotePort(void);

    using Print::write;

private:
    static uint16_t source_port_;
    uint8_t socket_;
};
