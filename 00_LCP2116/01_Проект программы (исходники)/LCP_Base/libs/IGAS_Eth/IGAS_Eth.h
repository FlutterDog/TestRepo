
/**
 * @file IGAS_Eth.h
 * @brief Верхний интерфейс Ethernet для W5500.
 */

#pragma once

#include <stdint.h>

#include "IPAddress.h"
#include "utility/w5500.h"

class EthernetClient;
class EthernetServer;

/**
 * @brief Ethernet-интерфейс проекта.
 */
class EthernetClass
{
public:
    static uint8_t _state[MAX_SOCK_NUM];
    static uint16_t _server_port[MAX_SOCK_NUM];

    void select(uint8_t chip_select);

    void begin(uint8_t* mac_address, IPAddress local_ip);
    void begin(uint8_t* mac_address, IPAddress local_ip, IPAddress dns_server);
    void begin(uint8_t* mac_address, IPAddress local_ip, IPAddress dns_server, IPAddress gateway);
    void begin(uint8_t* mac_address, IPAddress local_ip, IPAddress dns_server, IPAddress gateway, IPAddress subnet);

    int maintain(void);

    IPAddress localIP(void);
    IPAddress subnetMask(void);
    IPAddress gatewayIP(void);
    IPAddress dnsServerIP(void);

    uint8_t hardwareVersion(void);
    uint8_t linkStatus(void);

private:
    IPAddress dns_server_address_;
};

extern EthernetClass Ethernet;
