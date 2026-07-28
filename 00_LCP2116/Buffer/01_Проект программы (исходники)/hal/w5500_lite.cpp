
/**
 * @file w5500_lite.cpp
 * @brief Реализация минимального polling HAL W5500.
 */

#include "w5500_lite.hpp"
#include "../platform/platform.hpp"

static const uint8_t W5500_COMMON_BLOCK = 0x00U;

static const uint8_t W5500_REG_MR = 0x0000U;
static const uint16_t W5500_REG_GAR = 0x0001U;
static const uint16_t W5500_REG_SUBR = 0x0005U;
static const uint16_t W5500_REG_SHAR = 0x0009U;
static const uint16_t W5500_REG_SIPR = 0x000FU;
static const uint16_t W5500_REG_PHYCFGR = 0x002EU;
static const uint16_t W5500_REG_VERSIONR = 0x0039U;

static const uint8_t W5500_MR_RST = 0x80U;
static const uint8_t W5500_VERSION_VALUE = 0x04U;
static const uint8_t W5500_PHYCFGR_LINK = 0x01U;

static const uint16_t SN_MR = 0x0000U;
static const uint16_t SN_CR = 0x0001U;
static const uint16_t SN_IR = 0x0002U;
static const uint16_t SN_SR = 0x0003U;
static const uint16_t SN_PORT = 0x0004U;
static const uint16_t SN_TX_FSR = 0x0020U;
static const uint16_t SN_TX_WR = 0x0024U;
static const uint16_t SN_RX_RSR = 0x0026U;
static const uint16_t SN_RX_RD = 0x0028U;

static const uint8_t SN_MR_TCP = 0x01U;

static const uint8_t SN_CR_OPEN = 0x01U;
static const uint8_t SN_CR_LISTEN = 0x02U;
static const uint8_t SN_CR_CLOSE = 0x10U;
static const uint8_t SN_CR_SEND = 0x20U;
static const uint8_t SN_CR_RECV = 0x40U;

static const uint8_t SN_SR_CLOSED = 0x00U;
static const uint8_t SN_SR_INIT = 0x13U;
static const uint8_t SN_SR_LISTEN = 0x14U;
static const uint8_t SN_SR_ESTABLISHED = 0x17U;
static const uint8_t SN_SR_CLOSE_WAIT = 0x1CU;

static const uint8_t W5500_SOCKET_NUMBER = 0U;
static const uint16_t W5500_ECHO_BUFFER_SIZE = 256U;

static uint8_t g_w5500_rx_buffer[W5500_ECHO_BUFFER_SIZE];

static uint8_t w5500_spi_transfer(uint8_t value)
{
    return SPI.transfer(value);
}

static uint8_t w5500_control_byte(uint8_t block, uint8_t write)
{
    return static_cast<uint8_t>((block << 3U) | ((write != 0U) ? 0x04U : 0x00U));
}

static void w5500_write_bytes(LcpEthernetId ethernet_id, uint8_t block, uint16_t address, const uint8_t* data, size_t length)
{
    lcp_ethernet_select(ethernet_id);

    (void)w5500_spi_transfer(static_cast<uint8_t>(address >> 8U));
    (void)w5500_spi_transfer(static_cast<uint8_t>(address & 0xFFU));
    (void)w5500_spi_transfer(w5500_control_byte(block, 1U));

    for (size_t index = 0U; index < length; ++index)
    {
        (void)w5500_spi_transfer(data[index]);
    }

    lcp_ethernet_deselect(ethernet_id);
}

static void w5500_read_bytes(LcpEthernetId ethernet_id, uint8_t block, uint16_t address, uint8_t* data, size_t length)
{
    lcp_ethernet_select(ethernet_id);

    (void)w5500_spi_transfer(static_cast<uint8_t>(address >> 8U));
    (void)w5500_spi_transfer(static_cast<uint8_t>(address & 0xFFU));
    (void)w5500_spi_transfer(w5500_control_byte(block, 0U));

    for (size_t index = 0U; index < length; ++index)
    {
        data[index] = w5500_spi_transfer(0xFFU);
    }

    lcp_ethernet_deselect(ethernet_id);
}

static void w5500_write8(LcpEthernetId ethernet_id, uint8_t block, uint16_t address, uint8_t value)
{
    w5500_write_bytes(ethernet_id, block, address, &value, 1U);
}

static uint8_t w5500_read8(LcpEthernetId ethernet_id, uint8_t block, uint16_t address)
{
    uint8_t value = 0U;
    w5500_read_bytes(ethernet_id, block, address, &value, 1U);
    return value;
}

static void w5500_write16(LcpEthernetId ethernet_id, uint8_t block, uint16_t address, uint16_t value)
{
    uint8_t data[2];

    data[0] = static_cast<uint8_t>(value >> 8U);
    data[1] = static_cast<uint8_t>(value & 0xFFU);

    w5500_write_bytes(ethernet_id, block, address, data, sizeof(data));
}

static uint16_t w5500_read16(LcpEthernetId ethernet_id, uint8_t block, uint16_t address)
{
    uint8_t data[2];

    w5500_read_bytes(ethernet_id, block, address, data, sizeof(data));

    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8U) | data[1]);
}

static uint8_t w5500_socket_register_block(uint8_t socket_number)
{
    return static_cast<uint8_t>((socket_number * 4U) + 1U);
}

static uint8_t w5500_socket_tx_block(uint8_t socket_number)
{
    return static_cast<uint8_t>((socket_number * 4U) + 2U);
}

static uint8_t w5500_socket_rx_block(uint8_t socket_number)
{
    return static_cast<uint8_t>((socket_number * 4U) + 3U);
}

static void w5500_execute_socket_command(LcpEthernetId ethernet_id, uint8_t socket_number, uint8_t command)
{
    const uint8_t block = w5500_socket_register_block(socket_number);

    w5500_write8(ethernet_id, block, SN_CR, command);

    for (uint16_t guard = 0U; guard < 1000U; ++guard)
    {
        if (w5500_read8(ethernet_id, block, SN_CR) == 0U)
        {
            break;
        }
    }
}

static void w5500_socket_close(LcpEthernetId ethernet_id, uint8_t socket_number)
{
    const uint8_t block = w5500_socket_register_block(socket_number);

    w5500_execute_socket_command(ethernet_id, socket_number, SN_CR_CLOSE);
    w5500_write8(ethernet_id, block, SN_IR, 0xFFU);
}

static void w5500_socket_open_listen(LcpEthernetId ethernet_id, uint8_t socket_number, uint16_t port)
{
    const uint8_t block = w5500_socket_register_block(socket_number);

    w5500_socket_close(ethernet_id, socket_number);

    w5500_write8(ethernet_id, block, SN_MR, SN_MR_TCP);
    w5500_write16(ethernet_id, block, SN_PORT, port);

    w5500_execute_socket_command(ethernet_id, socket_number, SN_CR_OPEN);

    if (w5500_read8(ethernet_id, block, SN_SR) == SN_SR_INIT)
    {
        w5500_execute_socket_command(ethernet_id, socket_number, SN_CR_LISTEN);
    }
}

static uint16_t w5500_socket_recv(LcpEthernetId ethernet_id, uint8_t socket_number, uint8_t* buffer, uint16_t length)
{
    const uint8_t reg_block = w5500_socket_register_block(socket_number);
    const uint8_t rx_block = w5500_socket_rx_block(socket_number);
    const uint16_t received_size = w5500_read16(ethernet_id, reg_block, SN_RX_RSR);

    uint16_t read_size = received_size;

    if (read_size > length)
    {
        read_size = length;
    }

    if (read_size == 0U)
    {
        return 0U;
    }

    const uint16_t rx_read_pointer = w5500_read16(ethernet_id, reg_block, SN_RX_RD);

    w5500_read_bytes(ethernet_id, rx_block, rx_read_pointer, buffer, read_size);
    w5500_write16(ethernet_id, reg_block, SN_RX_RD, static_cast<uint16_t>(rx_read_pointer + read_size));
    w5500_execute_socket_command(ethernet_id, socket_number, SN_CR_RECV);

    return read_size;
}

static uint16_t w5500_socket_send(LcpEthernetId ethernet_id, uint8_t socket_number, const uint8_t* buffer, uint16_t length)
{
    const uint8_t reg_block = w5500_socket_register_block(socket_number);
    const uint8_t tx_block = w5500_socket_tx_block(socket_number);
    const uint16_t free_size = w5500_read16(ethernet_id, reg_block, SN_TX_FSR);

    uint16_t send_size = length;

    if (send_size > free_size)
    {
        send_size = free_size;
    }

    if (send_size == 0U)
    {
        return 0U;
    }

    const uint16_t tx_write_pointer = w5500_read16(ethernet_id, reg_block, SN_TX_WR);

    w5500_write_bytes(ethernet_id, tx_block, tx_write_pointer, buffer, send_size);
    w5500_write16(ethernet_id, reg_block, SN_TX_WR, static_cast<uint16_t>(tx_write_pointer + send_size));
    w5500_execute_socket_command(ethernet_id, socket_number, SN_CR_SEND);

    return send_size;
}

uint8_t w5500_lite_begin(LcpEthernetId ethernet_id, const W5500NetworkConfig& config)
{
    SPI.begin();

    lcp_ethernet_reset(ethernet_id);

    w5500_write8(ethernet_id, W5500_COMMON_BLOCK, W5500_REG_MR, W5500_MR_RST);
    delay(10U);

    w5500_write_bytes(ethernet_id, W5500_COMMON_BLOCK, W5500_REG_GAR, config.gateway.octet, 4U);
    w5500_write_bytes(ethernet_id, W5500_COMMON_BLOCK, W5500_REG_SUBR, config.subnet.octet, 4U);
    w5500_write_bytes(ethernet_id, W5500_COMMON_BLOCK, W5500_REG_SHAR, config.mac.octet, 6U);
    w5500_write_bytes(ethernet_id, W5500_COMMON_BLOCK, W5500_REG_SIPR, config.ip.octet, 4U);

    return (w5500_lite_version(ethernet_id) == W5500_VERSION_VALUE) ? 1U : 0U;
}

uint8_t w5500_lite_version(LcpEthernetId ethernet_id)
{
    return w5500_read8(ethernet_id, W5500_COMMON_BLOCK, W5500_REG_VERSIONR);
}

uint8_t w5500_lite_phy_status(LcpEthernetId ethernet_id)
{
    return w5500_read8(ethernet_id, W5500_COMMON_BLOCK, W5500_REG_PHYCFGR);
}

uint8_t w5500_lite_link_up(LcpEthernetId ethernet_id)
{
    return ((w5500_lite_phy_status(ethernet_id) & W5500_PHYCFGR_LINK) != 0U) ? 1U : 0U;
}

void w5500_lite_tcp_server_begin(LcpEthernetId ethernet_id, uint16_t port)
{
    w5500_socket_open_listen(ethernet_id, W5500_SOCKET_NUMBER, port);
}

void w5500_lite_tcp_echo_poll(LcpEthernetId ethernet_id, uint16_t port)
{
    const uint8_t block = w5500_socket_register_block(W5500_SOCKET_NUMBER);
    const uint8_t status = w5500_read8(ethernet_id, block, SN_SR);

    if ((status == SN_SR_CLOSED) || (status == SN_SR_CLOSE_WAIT))
    {
        w5500_socket_open_listen(ethernet_id, W5500_SOCKET_NUMBER, port);
        return;
    }

    if (status == SN_SR_INIT)
    {
        w5500_execute_socket_command(ethernet_id, W5500_SOCKET_NUMBER, SN_CR_LISTEN);
        return;
    }

    if (status == SN_SR_LISTEN)
    {
        return;
    }

    if (status == SN_SR_ESTABLISHED)
    {
        const uint16_t received = w5500_socket_recv(ethernet_id,
                                                    W5500_SOCKET_NUMBER,
                                                    g_w5500_rx_buffer,
                                                    W5500_ECHO_BUFFER_SIZE);

        if (received != 0U)
        {
            (void)w5500_socket_send(ethernet_id, W5500_SOCKET_NUMBER, g_w5500_rx_buffer, received);
        }
    }
}
