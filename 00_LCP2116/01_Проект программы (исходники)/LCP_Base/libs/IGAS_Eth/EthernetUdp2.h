
/**
 * @file EthernetUdp2.h
 * @brief UDP-интерфейс W5500.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Udp.h"
#include "IPAddress.h"
#include "utility/w5500.h"

static const uint16_t UDP_TX_PACKET_MAX_SIZE = 512U;

class EthernetUDP : public UDP
{
public:
    EthernetUDP();

    uint8_t begin(uint16_t port) override;
    void stop(void) override;

    int beginPacket(IPAddress ip, uint16_t port) override;
    int endPacket(void) override;

    size_t write(uint8_t value) override;
    size_t write(const uint8_t* buffer, size_t size) override;

    int parsePacket(void) override;
    int available(void) override;
    int read(void) override;
    int read(unsigned char* buffer, size_t length) override;
    int read(char* buffer, size_t length) override;
    int peek(void) override;
    void flush(void) override;

    IPAddress remoteIP(void) override;
    uint16_t remotePort(void) override;

    using Print::write;

private:
    uint8_t socket_;
    uint16_t local_port_;
    IPAddress remote_ip_;
    uint16_t remote_port_;
    uint16_t remaining_;

    uint8_t tx_buffer_[UDP_TX_PACKET_MAX_SIZE];
    uint16_t tx_length_;
};
