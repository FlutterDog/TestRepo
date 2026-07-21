/**
 * @file w5500_lite.cpp
 * @brief Реализация минимального polling HAL W5500.
 */

#include "w5500_lite.hpp"
#include "../platform/platform.hpp"

namespace
{
constexpr uint8_t W5500_COMMON_BLOCK = 0x00U;

constexpr uint16_t W5500_REG_MR = 0x0000U;
constexpr uint16_t W5500_REG_GAR = 0x0001U;
constexpr uint16_t W5500_REG_SUBR = 0x0005U;
constexpr uint16_t W5500_REG_SHAR = 0x0009U;
constexpr uint16_t W5500_REG_SIPR = 0x000FU;
constexpr uint16_t W5500_REG_PHYCFGR = 0x002EU;
constexpr uint16_t W5500_REG_VERSIONR = 0x0039U;

constexpr uint8_t W5500_MR_RST = 0x80U;
constexpr uint8_t W5500_VERSION_VALUE = 0x04U;
constexpr uint8_t W5500_PHYCFGR_LINK = 0x01U;

constexpr uint16_t SN_MR = 0x0000U;
constexpr uint16_t SN_CR = 0x0001U;
constexpr uint16_t SN_IR = 0x0002U;
constexpr uint16_t SN_SR = 0x0003U;
constexpr uint16_t SN_PORT = 0x0004U;
constexpr uint16_t SN_TX_FSR = 0x0020U;
constexpr uint16_t SN_TX_WR = 0x0024U;
constexpr uint16_t SN_RX_RSR = 0x0026U;
constexpr uint16_t SN_RX_RD = 0x0028U;

constexpr uint8_t SN_MR_TCP = 0x01U;
constexpr uint8_t SN_CR_OPEN = 0x01U;
constexpr uint8_t SN_CR_LISTEN = 0x02U;
constexpr uint8_t SN_CR_CLOSE = 0x10U;
constexpr uint8_t SN_CR_SEND = 0x20U;
constexpr uint8_t SN_CR_RECV = 0x40U;

constexpr uint8_t SN_SR_CLOSED = 0x00U;
constexpr uint8_t SN_SR_INIT = 0x13U;
constexpr uint8_t SN_SR_LISTEN = 0x14U;
constexpr uint8_t SN_SR_ESTABLISHED = 0x17U;
constexpr uint8_t SN_SR_CLOSE_WAIT = 0x1CU;

constexpr uint8_t W5500_SOCKET_NUMBER = 0U;
constexpr uint16_t SOCKET_COMMAND_GUARD = 1000U;
constexpr uint8_t STABLE_SIZE_READ_ATTEMPTS = 4U;

uint8_t valid_ethernet_id(LcpEthernetId ethernet_id)
{
    return (ethernet_id < LCP_ETHERNET_COUNT) ? 1U : 0U;
}

uint8_t w5500_spi_transfer(uint8_t value)
{
    return SPI.transfer(value);
}

uint8_t w5500_control_byte(uint8_t block, uint8_t write)
{
    return static_cast<uint8_t>((block << 3U) |
                                ((write != 0U) ? 0x04U : 0x00U));
}

void w5500_write_bytes(LcpEthernetId ethernet_id,
                       uint8_t block,
                       uint16_t address,
                       const uint8_t* data,
                       size_t length)
{
    if ((data == 0) || (length == 0U))
    {
        return;
    }

    lcp_ethernet_select(ethernet_id);
    (void)w5500_spi_transfer(static_cast<uint8_t>(address >> 8U));
    (void)w5500_spi_transfer(static_cast<uint8_t>(address & 0x00FFU));
    (void)w5500_spi_transfer(w5500_control_byte(block, 1U));

    for (size_t index = 0U; index < length; ++index)
    {
        (void)w5500_spi_transfer(data[index]);
    }

    lcp_ethernet_deselect(ethernet_id);
}

void w5500_read_bytes(LcpEthernetId ethernet_id,
                      uint8_t block,
                      uint16_t address,
                      uint8_t* data,
                      size_t length)
{
    if ((data == 0) || (length == 0U))
    {
        return;
    }

    lcp_ethernet_select(ethernet_id);
    (void)w5500_spi_transfer(static_cast<uint8_t>(address >> 8U));
    (void)w5500_spi_transfer(static_cast<uint8_t>(address & 0x00FFU));
    (void)w5500_spi_transfer(w5500_control_byte(block, 0U));

    for (size_t index = 0U; index < length; ++index)
    {
        data[index] = w5500_spi_transfer(0xFFU);
    }

    lcp_ethernet_deselect(ethernet_id);
}

void w5500_write8(LcpEthernetId ethernet_id,
                  uint8_t block,
                  uint16_t address,
                  uint8_t value)
{
    w5500_write_bytes(ethernet_id, block, address, &value, 1U);
}

uint8_t w5500_read8(LcpEthernetId ethernet_id,
                    uint8_t block,
                    uint16_t address)
{
    uint8_t value = 0U;
    w5500_read_bytes(ethernet_id, block, address, &value, 1U);
    return value;
}

void w5500_write16(LcpEthernetId ethernet_id,
                   uint8_t block,
                   uint16_t address,
                   uint16_t value)
{
    uint8_t data[2];
    data[0] = static_cast<uint8_t>(value >> 8U);
    data[1] = static_cast<uint8_t>(value & 0x00FFU);
    w5500_write_bytes(ethernet_id, block, address, data, sizeof(data));
}

uint16_t w5500_read16(LcpEthernetId ethernet_id,
                      uint8_t block,
                      uint16_t address)
{
    uint8_t data[2];
    w5500_read_bytes(ethernet_id, block, address, data, sizeof(data));
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8U) |
                                 static_cast<uint16_t>(data[1]));
}

uint16_t w5500_read16_stable(LcpEthernetId ethernet_id,
                             uint8_t block,
                             uint16_t address)
{
    uint16_t previous = w5500_read16(ethernet_id, block, address);

    for (uint8_t attempt = 0U;
         attempt < STABLE_SIZE_READ_ATTEMPTS;
         ++attempt)
    {
        const uint16_t current = w5500_read16(ethernet_id, block, address);

        if (current == previous)
        {
            return current;
        }

        previous = current;
    }

    return previous;
}

uint8_t w5500_socket_register_block(uint8_t socket_number)
{
    return static_cast<uint8_t>((socket_number * 4U) + 1U);
}

uint8_t w5500_socket_tx_block(uint8_t socket_number)
{
    return static_cast<uint8_t>((socket_number * 4U) + 2U);
}

uint8_t w5500_socket_rx_block(uint8_t socket_number)
{
    return static_cast<uint8_t>((socket_number * 4U) + 3U);
}

uint8_t w5500_execute_socket_command(LcpEthernetId ethernet_id,
                                     uint8_t socket_number,
                                     uint8_t command)
{
    const uint8_t block = w5500_socket_register_block(socket_number);
    w5500_write8(ethernet_id, block, SN_CR, command);

    for (uint16_t guard = 0U; guard < SOCKET_COMMAND_GUARD; ++guard)
    {
        if (w5500_read8(ethernet_id, block, SN_CR) == 0U)
        {
            return 1U;
        }
    }

    return 0U;
}

void w5500_socket_close(LcpEthernetId ethernet_id,
                        uint8_t socket_number)
{
    const uint8_t block = w5500_socket_register_block(socket_number);
    (void)w5500_execute_socket_command(ethernet_id,
                                       socket_number,
                                       SN_CR_CLOSE);
    w5500_write8(ethernet_id, block, SN_IR, 0xFFU);
}

void w5500_socket_open_listen(LcpEthernetId ethernet_id,
                              uint8_t socket_number,
                              uint16_t port)
{
    const uint8_t block = w5500_socket_register_block(socket_number);
    w5500_socket_close(ethernet_id, socket_number);
    w5500_write8(ethernet_id, block, SN_MR, SN_MR_TCP);
    w5500_write16(ethernet_id, block, SN_PORT, port);

    if (w5500_execute_socket_command(ethernet_id,
                                     socket_number,
                                     SN_CR_OPEN) == 0U)
    {
        return;
    }

    if (w5500_read8(ethernet_id, block, SN_SR) == SN_SR_INIT)
    {
        (void)w5500_execute_socket_command(ethernet_id,
                                           socket_number,
                                           SN_CR_LISTEN);
    }
}

uint16_t w5500_socket_recv(LcpEthernetId ethernet_id,
                           uint8_t socket_number,
                           uint8_t* buffer,
                           uint16_t capacity)
{
    if ((buffer == 0) || (capacity == 0U))
    {
        return 0U;
    }

    const uint8_t register_block =
        w5500_socket_register_block(socket_number);
    const uint8_t rx_block = w5500_socket_rx_block(socket_number);
    uint16_t read_size = w5500_read16_stable(ethernet_id,
                                             register_block,
                                             SN_RX_RSR);

    if (read_size > capacity)
    {
        read_size = capacity;
    }

    if (read_size == 0U)
    {
        return 0U;
    }

    const uint16_t read_pointer = w5500_read16(ethernet_id,
                                               register_block,
                                               SN_RX_RD);
    w5500_read_bytes(ethernet_id,
                     rx_block,
                     read_pointer,
                     buffer,
                     read_size);
    w5500_write16(ethernet_id,
                  register_block,
                  SN_RX_RD,
                  static_cast<uint16_t>(read_pointer + read_size));

    if (w5500_execute_socket_command(ethernet_id,
                                     socket_number,
                                     SN_CR_RECV) == 0U)
    {
        return 0U;
    }

    return read_size;
}

uint16_t w5500_socket_send(LcpEthernetId ethernet_id,
                           uint8_t socket_number,
                           const uint8_t* buffer,
                           uint16_t length)
{
    if ((buffer == 0) || (length == 0U))
    {
        return 0U;
    }

    const uint8_t register_block =
        w5500_socket_register_block(socket_number);
    const uint8_t tx_block = w5500_socket_tx_block(socket_number);
    const uint16_t free_size = w5500_read16_stable(ethernet_id,
                                                   register_block,
                                                   SN_TX_FSR);

    /* Modbus TCP ADU нельзя фрагментировать из-за временно малого TX_FSR. */
    if (free_size < length)
    {
        return 0U;
    }

    const uint16_t write_pointer = w5500_read16(ethernet_id,
                                                register_block,
                                                SN_TX_WR);
    w5500_write_bytes(ethernet_id,
                      tx_block,
                      write_pointer,
                      buffer,
                      length);
    w5500_write16(ethernet_id,
                  register_block,
                  SN_TX_WR,
                  static_cast<uint16_t>(write_pointer + length));

    if (w5500_execute_socket_command(ethernet_id,
                                     socket_number,
                                     SN_CR_SEND) == 0U)
    {
        return 0U;
    }

    return length;
}

uint8_t w5500_ensure_server_state(LcpEthernetId ethernet_id,
                                  uint16_t port)
{
    const uint8_t block = w5500_socket_register_block(W5500_SOCKET_NUMBER);
    const uint8_t status = w5500_read8(ethernet_id, block, SN_SR);

    if ((status == SN_SR_CLOSED) || (status == SN_SR_CLOSE_WAIT))
    {
        w5500_socket_open_listen(ethernet_id, W5500_SOCKET_NUMBER, port);
        return 0U;
    }

    if (status == SN_SR_INIT)
    {
        (void)w5500_execute_socket_command(ethernet_id,
                                           W5500_SOCKET_NUMBER,
                                           SN_CR_LISTEN);
        return 0U;
    }

    return (status == SN_SR_ESTABLISHED) ? 1U : 0U;
}
}

uint8_t w5500_lite_begin(LcpEthernetId ethernet_id,
                         const W5500NetworkConfig& config)
{
    if (valid_ethernet_id(ethernet_id) == 0U)
    {
        return 0U;
    }

    SPI.begin();
    lcp_ethernet_reset(ethernet_id);
    w5500_write8(ethernet_id,
                  W5500_COMMON_BLOCK,
                  W5500_REG_MR,
                  W5500_MR_RST);
    delay(10U);
    w5500_write_bytes(ethernet_id,
                       W5500_COMMON_BLOCK,
                       W5500_REG_GAR,
                       config.gateway.octet,
                       4U);
    w5500_write_bytes(ethernet_id,
                       W5500_COMMON_BLOCK,
                       W5500_REG_SUBR,
                       config.subnet.octet,
                       4U);
    w5500_write_bytes(ethernet_id,
                       W5500_COMMON_BLOCK,
                       W5500_REG_SHAR,
                       config.mac.octet,
                       6U);
    w5500_write_bytes(ethernet_id,
                       W5500_COMMON_BLOCK,
                       W5500_REG_SIPR,
                       config.ip.octet,
                       4U);
    return (w5500_lite_version(ethernet_id) == W5500_VERSION_VALUE) ? 1U : 0U;
}

uint8_t w5500_lite_version(LcpEthernetId ethernet_id)
{
    return (valid_ethernet_id(ethernet_id) != 0U) ?
        w5500_read8(ethernet_id, W5500_COMMON_BLOCK, W5500_REG_VERSIONR) : 0U;
}

uint8_t w5500_lite_phy_status(LcpEthernetId ethernet_id)
{
    return (valid_ethernet_id(ethernet_id) != 0U) ?
        w5500_read8(ethernet_id, W5500_COMMON_BLOCK, W5500_REG_PHYCFGR) : 0U;
}

uint8_t w5500_lite_link_up(LcpEthernetId ethernet_id)
{
    return ((w5500_lite_phy_status(ethernet_id) & W5500_PHYCFGR_LINK) != 0U) ?
        1U : 0U;
}

void w5500_lite_tcp_server_begin(LcpEthernetId ethernet_id, uint16_t port)
{
    if ((valid_ethernet_id(ethernet_id) == 0U) || (port == 0U))
    {
        return;
    }

    w5500_socket_open_listen(ethernet_id, W5500_SOCKET_NUMBER, port);
}

uint16_t w5500_lite_tcp_server_receive(LcpEthernetId ethernet_id,
                                       uint16_t port,
                                       uint8_t* buffer,
                                       uint16_t capacity)
{
    if ((valid_ethernet_id(ethernet_id) == 0U) ||
        (buffer == 0) || (capacity == 0U) || (port == 0U) ||
        (w5500_ensure_server_state(ethernet_id, port) == 0U))
    {
        return 0U;
    }

    return w5500_socket_recv(ethernet_id,
                             W5500_SOCKET_NUMBER,
                             buffer,
                             capacity);
}

uint16_t w5500_lite_tcp_server_send(LcpEthernetId ethernet_id,
                                    const uint8_t* buffer,
                                    uint16_t length)
{
    if ((valid_ethernet_id(ethernet_id) == 0U) ||
        (w5500_lite_tcp_server_status(ethernet_id) != SN_SR_ESTABLISHED))
    {
        return 0U;
    }

    return w5500_socket_send(ethernet_id,
                             W5500_SOCKET_NUMBER,
                             buffer,
                             length);
}

uint8_t w5500_lite_tcp_server_status(LcpEthernetId ethernet_id)
{
    if (valid_ethernet_id(ethernet_id) == 0U)
    {
        return SN_SR_CLOSED;
    }

    return w5500_read8(ethernet_id,
                       w5500_socket_register_block(W5500_SOCKET_NUMBER),
                       SN_SR);
}
