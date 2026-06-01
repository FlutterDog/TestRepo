
/**
 * @file w5500.cpp
 * @brief Реализация низкоуровневого драйвера W5500.
 */

#include "w5500.h"

static const uint32_t W5500_SPI_CLOCK_HZ = 12000000UL;

static const uint8_t W5500_COMMON_READ  = 0x00U;
static const uint8_t W5500_COMMON_WRITE = 0x04U;

static const uint8_t W5500_MODE_VDM = 0x00U;

static const uint16_t REG_MR       = 0x0000U;
static const uint16_t REG_GAR      = 0x0001U;
static const uint16_t REG_SUBR     = 0x0005U;
static const uint16_t REG_SHAR     = 0x0009U;
static const uint16_t REG_SIPR     = 0x000FU;
static const uint16_t REG_RTR      = 0x0019U;
static const uint16_t REG_RCR      = 0x001BU;
static const uint16_t REG_PHYCFGR  = 0x002EU;
static const uint16_t REG_VERSIONR = 0x0039U;

static const uint16_t REG_SN_MR      = 0x0000U;
static const uint16_t REG_SN_CR      = 0x0001U;
static const uint16_t REG_SN_IR      = 0x0002U;
static const uint16_t REG_SN_SR      = 0x0003U;
static const uint16_t REG_SN_PORT    = 0x0004U;
static const uint16_t REG_SN_DIPR    = 0x000CU;
static const uint16_t REG_SN_DPORT   = 0x0010U;
static const uint16_t REG_SN_RXBUF   = 0x001EU;
static const uint16_t REG_SN_TXBUF   = 0x001FU;
static const uint16_t REG_SN_TX_FSR  = 0x0020U;
static const uint16_t REG_SN_TX_WR   = 0x0024U;
static const uint16_t REG_SN_RX_RSR  = 0x0026U;
static const uint16_t REG_SN_RX_RD   = 0x0028U;

static const uint8_t MR_RST = 0x80U;
static const uint8_t PHYCFGR_AUTONEG_ALL = 0xF8U;

static uint8_t socket_register_read_control(SOCKET socket)
{
    return static_cast<uint8_t>((socket << 5) + 0x08U);
}

static uint8_t socket_register_write_control(SOCKET socket)
{
    return static_cast<uint8_t>((socket << 5) + 0x0CU);
}

static uint8_t socket_tx_buffer_write_control(SOCKET socket)
{
    return static_cast<uint8_t>((socket << 5) + 0x14U);
}

static uint8_t socket_rx_buffer_read_control(SOCKET socket)
{
    return static_cast<uint8_t>((socket << 5) + 0x18U);
}

W5500Class w5500;

W5500Class::W5500Class()
    : slave_select_(10U)
{
}

void W5500Class::selectSS(uint8_t chip_select)
{
    slave_select_ = chip_select;
}

void W5500Class::initSS(void)
{
    pinMode(slave_select_, OUTPUT);
    digitalWrite(slave_select_, HIGH);
}

void W5500Class::setSS(void)
{
    digitalWrite(slave_select_, LOW);
}

void W5500Class::resetSS(void)
{
    digitalWrite(slave_select_, HIGH);
}

void W5500Class::init(void)
{
    initSS();
    SPI.begin();

    writeCommon8(REG_MR, MR_RST);

    const uint32_t reset_started = millis();

    while (((readCommon8(REG_MR) & MR_RST) != 0U) && ((uint32_t)(millis() - reset_started) < 100U))
    {
    }

    writeCommon8(REG_PHYCFGR, PHYCFGR_AUTONEG_ALL);

    for (SOCKET socket = 0U; socket < MAX_SOCK_NUM; ++socket)
    {
        writeSn8(socket, REG_SN_RXBUF, 2U);
        writeSn8(socket, REG_SN_TXBUF, 2U);
        execCmdSn(socket, Sock_CLOSE);
        writeSnIR(socket, 0xFFU);
    }

    setRetransmissionTime(2000U);
    setRetransmissionCount(3U);
}

uint8_t W5500Class::version(void)
{
    return readCommon8(REG_VERSIONR);
}

uint8_t W5500Class::linkStatus(void)
{
    return static_cast<uint8_t>(readCommon8(REG_PHYCFGR) & 0x01U);
}

void W5500Class::setGatewayIp(const uint8_t* address)
{
    writeCommonBuffer(REG_GAR, address, 4U);
}

void W5500Class::getGatewayIp(uint8_t* address)
{
    readCommonBuffer(REG_GAR, address, 4U);
}

void W5500Class::setSubnetMask(const uint8_t* address)
{
    writeCommonBuffer(REG_SUBR, address, 4U);
}

void W5500Class::getSubnetMask(uint8_t* address)
{
    readCommonBuffer(REG_SUBR, address, 4U);
}

void W5500Class::setMACAddress(const uint8_t* address)
{
    writeCommonBuffer(REG_SHAR, address, 6U);
}

void W5500Class::getMACAddress(uint8_t* address)
{
    readCommonBuffer(REG_SHAR, address, 6U);
}

void W5500Class::setIPAddress(const uint8_t* address)
{
    writeCommonBuffer(REG_SIPR, address, 4U);
}

void W5500Class::getIPAddress(uint8_t* address)
{
    readCommonBuffer(REG_SIPR, address, 4U);
}

void W5500Class::setRetransmissionTime(uint16_t timeout)
{
    writeCommon16(REG_RTR, timeout);
}

void W5500Class::setRetransmissionCount(uint8_t retry)
{
    writeCommon8(REG_RCR, retry);
}

void W5500Class::execCmdSn(SOCKET socket, SockCMD command)
{
    writeSnCR(socket, static_cast<uint8_t>(command));

    const uint32_t started = millis();

    while ((readSnCR(socket) != 0U) && ((uint32_t)(millis() - started) < 100U))
    {
    }
}

uint16_t W5500Class::getTXFreeSize(SOCKET socket)
{
    uint16_t value;
    uint16_t previous;

    do
    {
        previous = readSnTX_FSR(socket);
        value = readSnTX_FSR(socket);
    }
    while (value != previous);

    return value;
}

uint16_t W5500Class::getRXReceivedSize(SOCKET socket)
{
    uint16_t value;
    uint16_t previous;

    do
    {
        previous = readSnRX_RSR(socket);
        value = readSnRX_RSR(socket);
    }
    while (value != previous);

    return value;
}

void W5500Class::send_data_processing(SOCKET socket, const uint8_t* data, uint16_t length)
{
    send_data_processing_offset(socket, 0U, data, length);
}

void W5500Class::send_data_processing_offset(SOCKET socket, uint16_t data_offset, const uint8_t* data, uint16_t length)
{
    uint16_t pointer = readSnTX_WR(socket);
    pointer = static_cast<uint16_t>(pointer + data_offset);
    write(pointer, socket_tx_buffer_write_control(socket), data, length);
    pointer = static_cast<uint16_t>(pointer + length);
    writeSnTX_WR(socket, pointer);
}

void W5500Class::recv_data_processing(SOCKET socket, uint8_t* data, uint16_t length, uint8_t peek)
{
    uint16_t pointer = readSnRX_RD(socket);
    read(pointer, socket_rx_buffer_read_control(socket), data, length);

    if (peek == 0U)
    {
        pointer = static_cast<uint16_t>(pointer + length);
        writeSnRX_RD(socket, pointer);
    }
}

uint8_t W5500Class::readSnMR(SOCKET socket) { return readSn8(socket, REG_SN_MR); }
void W5500Class::writeSnMR(SOCKET socket, uint8_t value) { writeSn8(socket, REG_SN_MR, value); }
uint8_t W5500Class::readSnCR(SOCKET socket) { return readSn8(socket, REG_SN_CR); }
void W5500Class::writeSnCR(SOCKET socket, uint8_t value) { writeSn8(socket, REG_SN_CR, value); }
uint8_t W5500Class::readSnIR(SOCKET socket) { return readSn8(socket, REG_SN_IR); }
void W5500Class::writeSnIR(SOCKET socket, uint8_t value) { writeSn8(socket, REG_SN_IR, value); }
uint8_t W5500Class::readSnSR(SOCKET socket) { return readSn8(socket, REG_SN_SR); }
uint16_t W5500Class::readSnPORT(SOCKET socket) { return readSn16(socket, REG_SN_PORT); }
void W5500Class::writeSnPORT(SOCKET socket, uint16_t value) { writeSn16(socket, REG_SN_PORT, value); }
void W5500Class::writeSnDIPR(SOCKET socket, const uint8_t* address) { writeSnBuffer(socket, REG_SN_DIPR, address, 4U); }
void W5500Class::readSnDIPR(SOCKET socket, uint8_t* address) { readSnBuffer(socket, REG_SN_DIPR, address, 4U); }
void W5500Class::writeSnDPORT(SOCKET socket, uint16_t value) { writeSn16(socket, REG_SN_DPORT, value); }
uint16_t W5500Class::readSnDPORT(SOCKET socket) { return readSn16(socket, REG_SN_DPORT); }
uint16_t W5500Class::readSnTX_FSR(SOCKET socket) { return readSn16(socket, REG_SN_TX_FSR); }
uint16_t W5500Class::readSnTX_WR(SOCKET socket) { return readSn16(socket, REG_SN_TX_WR); }
void W5500Class::writeSnTX_WR(SOCKET socket, uint16_t value) { writeSn16(socket, REG_SN_TX_WR, value); }
uint16_t W5500Class::readSnRX_RSR(SOCKET socket) { return readSn16(socket, REG_SN_RX_RSR); }
uint16_t W5500Class::readSnRX_RD(SOCKET socket) { return readSn16(socket, REG_SN_RX_RD); }
void W5500Class::writeSnRX_RD(SOCKET socket, uint16_t value) { writeSn16(socket, REG_SN_RX_RD, value); }

uint8_t W5500Class::read(uint16_t address, uint8_t control)
{
    uint8_t value;

    SPI.beginTransaction(SPISettings(W5500_SPI_CLOCK_HZ, MSBFIRST, SPI_MODE0));
    setSS();
    SPI.transfer(static_cast<uint8_t>(address >> 8));
    SPI.transfer(static_cast<uint8_t>(address & 0xFFU));
    SPI.transfer(static_cast<uint8_t>(control | W5500_MODE_VDM));
    value = SPI.transfer(0x00U);
    resetSS();
    SPI.endTransaction();

    return value;
}

uint16_t W5500Class::read(uint16_t address, uint8_t control, uint8_t* buffer, uint16_t length)
{
    SPI.beginTransaction(SPISettings(W5500_SPI_CLOCK_HZ, MSBFIRST, SPI_MODE0));
    setSS();
    SPI.transfer(static_cast<uint8_t>(address >> 8));
    SPI.transfer(static_cast<uint8_t>(address & 0xFFU));
    SPI.transfer(static_cast<uint8_t>(control | W5500_MODE_VDM));

    for (uint16_t index = 0U; index < length; ++index)
    {
        buffer[index] = SPI.transfer(0x00U);
    }

    resetSS();
    SPI.endTransaction();

    return length;
}

uint8_t W5500Class::write(uint16_t address, uint8_t control, uint8_t data)
{
    SPI.beginTransaction(SPISettings(W5500_SPI_CLOCK_HZ, MSBFIRST, SPI_MODE0));
    setSS();
    SPI.transfer(static_cast<uint8_t>(address >> 8));
    SPI.transfer(static_cast<uint8_t>(address & 0xFFU));
    SPI.transfer(static_cast<uint8_t>(control | W5500_MODE_VDM));
    SPI.transfer(data);
    resetSS();
    SPI.endTransaction();

    return 1U;
}

uint16_t W5500Class::write(uint16_t address, uint8_t control, const uint8_t* buffer, uint16_t length)
{
    SPI.beginTransaction(SPISettings(W5500_SPI_CLOCK_HZ, MSBFIRST, SPI_MODE0));
    setSS();
    SPI.transfer(static_cast<uint8_t>(address >> 8));
    SPI.transfer(static_cast<uint8_t>(address & 0xFFU));
    SPI.transfer(static_cast<uint8_t>(control | W5500_MODE_VDM));

    for (uint16_t index = 0U; index < length; ++index)
    {
        SPI.transfer(buffer[index]);
    }

    resetSS();
    SPI.endTransaction();

    return length;
}

uint8_t W5500Class::readCommon8(uint16_t address) { return read(address, W5500_COMMON_READ); }
void W5500Class::writeCommon8(uint16_t address, uint8_t value) { (void)write(address, W5500_COMMON_WRITE, value); }

uint16_t W5500Class::readCommon16(uint16_t address)
{
    uint16_t value = static_cast<uint16_t>(readCommon8(address)) << 8;
    value |= readCommon8(static_cast<uint16_t>(address + 1U));
    return value;
}

void W5500Class::writeCommon16(uint16_t address, uint16_t value)
{
    writeCommon8(address, static_cast<uint8_t>(value >> 8));
    writeCommon8(static_cast<uint16_t>(address + 1U), static_cast<uint8_t>(value & 0xFFU));
}

void W5500Class::readCommonBuffer(uint16_t address, uint8_t* buffer, uint16_t length) { (void)read(address, W5500_COMMON_READ, buffer, length); }
void W5500Class::writeCommonBuffer(uint16_t address, const uint8_t* buffer, uint16_t length) { (void)write(address, W5500_COMMON_WRITE, buffer, length); }
uint8_t W5500Class::readSn8(SOCKET socket, uint16_t address) { return read(address, socket_register_read_control(socket)); }
void W5500Class::writeSn8(SOCKET socket, uint16_t address, uint8_t value) { (void)write(address, socket_register_write_control(socket), value); }

uint16_t W5500Class::readSn16(SOCKET socket, uint16_t address)
{
    uint16_t value = static_cast<uint16_t>(readSn8(socket, address)) << 8;
    value |= readSn8(socket, static_cast<uint16_t>(address + 1U));
    return value;
}

void W5500Class::writeSn16(SOCKET socket, uint16_t address, uint16_t value)
{
    writeSn8(socket, address, static_cast<uint8_t>(value >> 8));
    writeSn8(socket, static_cast<uint16_t>(address + 1U), static_cast<uint8_t>(value & 0xFFU));
}

void W5500Class::readSnBuffer(SOCKET socket, uint16_t address, uint8_t* buffer, uint16_t length) { (void)read(address, socket_register_read_control(socket), buffer, length); }
void W5500Class::writeSnBuffer(SOCKET socket, uint16_t address, const uint8_t* buffer, uint16_t length) { (void)write(address, socket_register_write_control(socket), buffer, length); }
