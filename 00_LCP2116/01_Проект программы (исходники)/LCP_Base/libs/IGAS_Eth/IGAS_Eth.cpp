
/**
 * @file IGAS_Eth.cpp
 * @brief Реализация верхнего интерфейса Ethernet для W5500.
 */

#include "IGAS_Eth.h"

uint8_t EthernetClass::_state[MAX_SOCK_NUM] = { 0U };
uint16_t EthernetClass::_server_port[MAX_SOCK_NUM] = { 0U };

EthernetClass Ethernet;

void EthernetClass::select(uint8_t chip_select)
{
    w5500.selectSS(chip_select);
}

void EthernetClass::begin(uint8_t* mac_address, IPAddress local_ip)
{
    IPAddress dns_server(local_ip);
    dns_server[3] = 1U;

    begin(mac_address, local_ip, dns_server);
}

void EthernetClass::begin(uint8_t* mac_address, IPAddress local_ip, IPAddress dns_server)
{
    IPAddress gateway(local_ip);
    gateway[3] = 1U;

    begin(mac_address, local_ip, dns_server, gateway);
}

void EthernetClass::begin(uint8_t* mac_address, IPAddress local_ip, IPAddress dns_server, IPAddress gateway)
{
    IPAddress subnet(255U, 255U, 255U, 0U);

    begin(mac_address, local_ip, dns_server, gateway, subnet);
}

void EthernetClass::begin(uint8_t* mac_address, IPAddress local_ip, IPAddress dns_server, IPAddress gateway, IPAddress subnet)
{
    dns_server_address_ = dns_server;

    w5500.init();
    w5500.setMACAddress(mac_address);
    w5500.setIPAddress(local_ip.raw_address());
    w5500.setGatewayIp(gateway.raw_address());
    w5500.setSubnetMask(subnet.raw_address());

    for (SOCKET socket = 0U; socket < MAX_SOCK_NUM; ++socket)
    {
        _state[socket] = 0U;
        _server_port[socket] = 0U;
    }
}

int EthernetClass::maintain(void)
{
    return 0;
}

IPAddress EthernetClass::localIP(void)
{
    uint8_t address[4];
    w5500.getIPAddress(address);
    return IPAddress(address);
}

IPAddress EthernetClass::subnetMask(void)
{
    uint8_t address[4];
    w5500.getSubnetMask(address);
    return IPAddress(address);
}

IPAddress EthernetClass::gatewayIP(void)
{
    uint8_t address[4];
    w5500.getGatewayIp(address);
    return IPAddress(address);
}

IPAddress EthernetClass::dnsServerIP(void)
{
    return dns_server_address_;
}

uint8_t EthernetClass::hardwareVersion(void)
{
    return w5500.version();
}

uint8_t EthernetClass::linkStatus(void)
{
    return w5500.linkStatus();
}
