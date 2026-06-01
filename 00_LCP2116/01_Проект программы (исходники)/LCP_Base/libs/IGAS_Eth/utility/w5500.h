
/**
 * @file w5500.h
 * @brief Низкоуровневый драйвер Ethernet-контроллера W5500.
 */

#pragma once

#include <stdint.h>

#include "../../../platform/platform.hpp"

#define MAX_SOCK_NUM 8

typedef uint8_t SOCKET;

class SnMR
{
public:
    static const uint8_t CLOSE  = 0x00U;
    static const uint8_t TCP    = 0x01U;
    static const uint8_t UDP    = 0x02U;
    static const uint8_t IPRAW  = 0x03U;
    static const uint8_t MACRAW = 0x04U;
    static const uint8_t PPPOE  = 0x05U;
    static const uint8_t ND     = 0x20U;
    static const uint8_t MULTI  = 0x80U;
};

enum SockCMD
{
    Sock_OPEN      = 0x01U,
    Sock_LISTEN    = 0x02U,
    Sock_CONNECT   = 0x04U,
    Sock_DISCON    = 0x08U,
    Sock_CLOSE     = 0x10U,
    Sock_SEND      = 0x20U,
    Sock_SEND_MAC  = 0x21U,
    Sock_SEND_KEEP = 0x22U,
    Sock_RECV      = 0x40U
};

class SnIR
{
public:
    static const uint8_t SEND_OK = 0x10U;
    static const uint8_t TIMEOUT = 0x08U;
    static const uint8_t RECV    = 0x04U;
    static const uint8_t DISCON  = 0x02U;
    static const uint8_t CON     = 0x01U;
};

class SnSR
{
public:
    static const uint8_t CLOSED      = 0x00U;
    static const uint8_t INIT        = 0x13U;
    static const uint8_t LISTEN      = 0x14U;
    static const uint8_t SYNSENT     = 0x15U;
    static const uint8_t SYNRECV     = 0x16U;
    static const uint8_t ESTABLISHED = 0x17U;
    static const uint8_t FIN_WAIT    = 0x18U;
    static const uint8_t CLOSING     = 0x1AU;
    static const uint8_t TIME_WAIT   = 0x1BU;
    static const uint8_t CLOSE_WAIT  = 0x1CU;
    static const uint8_t LAST_ACK    = 0x1DU;
    static const uint8_t UDP         = 0x22U;
    static const uint8_t IPRAW       = 0x32U;
    static const uint8_t MACRAW      = 0x42U;
    static const uint8_t PPPOE       = 0x5FU;
};

class W5500Class
{
public:
    static const uint16_t SOCKET_BUFFER_SIZE = 2048U;

    W5500Class();

    void selectSS(uint8_t chip_select);
    void init(void);

    uint8_t version(void);
    uint8_t linkStatus(void);

    void setGatewayIp(const uint8_t* address);
    void getGatewayIp(uint8_t* address);

    void setSubnetMask(const uint8_t* address);
    void getSubnetMask(uint8_t* address);

    void setMACAddress(const uint8_t* address);
    void getMACAddress(uint8_t* address);

    void setIPAddress(const uint8_t* address);
    void getIPAddress(uint8_t* address);

    void setRetransmissionTime(uint16_t timeout);
    void setRetransmissionCount(uint8_t retry);

    void execCmdSn(SOCKET socket, SockCMD command);

    uint16_t getTXFreeSize(SOCKET socket);
    uint16_t getRXReceivedSize(SOCKET socket);

    void send_data_processing(SOCKET socket, const uint8_t* data, uint16_t length);
    void send_data_processing_offset(SOCKET socket, uint16_t data_offset, const uint8_t* data, uint16_t length);
    void recv_data_processing(SOCKET socket, uint8_t* data, uint16_t length, uint8_t peek = 0U);

    uint8_t readSnMR(SOCKET socket);
    void writeSnMR(SOCKET socket, uint8_t value);

    uint8_t readSnCR(SOCKET socket);
    void writeSnCR(SOCKET socket, uint8_t value);

    uint8_t readSnIR(SOCKET socket);
    void writeSnIR(SOCKET socket, uint8_t value);

    uint8_t readSnSR(SOCKET socket);

    uint16_t readSnPORT(SOCKET socket);
    void writeSnPORT(SOCKET socket, uint16_t value);

    void writeSnDIPR(SOCKET socket, const uint8_t* address);
    void readSnDIPR(SOCKET socket, uint8_t* address);

    void writeSnDPORT(SOCKET socket, uint16_t value);
    uint16_t readSnDPORT(SOCKET socket);

    uint16_t readSnTX_FSR(SOCKET socket);
    uint16_t readSnTX_WR(SOCKET socket);
    void writeSnTX_WR(SOCKET socket, uint16_t value);

    uint16_t readSnRX_RSR(SOCKET socket);
    uint16_t readSnRX_RD(SOCKET socket);
    void writeSnRX_RD(SOCKET socket, uint16_t value);

private:
    void initSS(void);
    void setSS(void);
    void resetSS(void);

    uint8_t read(uint16_t address, uint8_t control);
    uint16_t read(uint16_t address, uint8_t control, uint8_t* buffer, uint16_t length);

    uint8_t write(uint16_t address, uint8_t control, uint8_t data);
    uint16_t write(uint16_t address, uint8_t control, const uint8_t* buffer, uint16_t length);

    uint8_t readCommon8(uint16_t address);
    void writeCommon8(uint16_t address, uint8_t value);

    uint16_t readCommon16(uint16_t address);
    void writeCommon16(uint16_t address, uint16_t value);

    void readCommonBuffer(uint16_t address, uint8_t* buffer, uint16_t length);
    void writeCommonBuffer(uint16_t address, const uint8_t* buffer, uint16_t length);

    uint8_t readSn8(SOCKET socket, uint16_t address);
    void writeSn8(SOCKET socket, uint16_t address, uint8_t value);

    uint16_t readSn16(SOCKET socket, uint16_t address);
    void writeSn16(SOCKET socket, uint16_t address, uint16_t value);

    void readSnBuffer(SOCKET socket, uint16_t address, uint8_t* buffer, uint16_t length);
    void writeSnBuffer(SOCKET socket, uint16_t address, const uint8_t* buffer, uint16_t length);

    uint8_t slave_select_;
};

extern W5500Class w5500;
