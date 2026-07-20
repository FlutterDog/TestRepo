/**
 * @file modbus_rtu_slave.cpp
 * @brief Реализация минимального неблокирующего Modbus RTU slave.
 */

#include "modbus_rtu_slave.hpp"

#include "modbus_rtu_crc.hpp"
#include "../../platform/platform.hpp"

#include <string.h>

namespace
{
constexpr uint8_t MODBUS_FUNCTION_READ_HOLDING = 0x03U;
constexpr uint8_t MODBUS_EXCEPTION_ILLEGAL_FUNCTION = 0x01U;
constexpr uint8_t MODBUS_EXCEPTION_ILLEGAL_ADDRESS = 0x02U;
constexpr uint8_t MODBUS_EXCEPTION_ILLEGAL_VALUE = 0x03U;
constexpr uint16_t MODBUS_READ_REQUEST_LENGTH = 8U;
constexpr uint16_t MODBUS_CRC_LENGTH = 2U;
constexpr uint16_t MODBUS_READ_RESPONSE_FIXED_LENGTH = 5U;
constexpr uint16_t MODBUS_MAX_RESPONSE_REGISTERS =
    (MODBUS_RTU_SLAVE_TX_CAPACITY - MODBUS_READ_RESPONSE_FIXED_LENGTH) / 2U;

uint8_t transport_valid(const ModbusRtuTransport* transport)
{
    return ((transport != 0) &&
            (transport->write != 0) &&
            (transport->available != 0) &&
            (transport->read != 0) &&
            (transport->tx_idle != 0) &&
            (transport->clear_rx != 0)) ? 1U : 0U;
}

uint8_t slave_address_valid(uint8_t slave_address)
{
    return ((slave_address >= 1U) &&
            (slave_address <= MODBUS_RTU_MAX_SLAVE_ADDRESS)) ? 1U : 0U;
}

uint8_t deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (static_cast<int32_t>(now_ms - deadline_ms) >= 0) ? 1U : 0U;
}

uint8_t gap_elapsed(uint32_t now_ms, uint32_t then_ms, uint32_t gap_ms)
{
    return ((uint32_t)(now_ms - then_ms) >= gap_ms) ? 1U : 0U;
}

void append_crc(ModbusRtuSlave& slave, uint16_t payload_length)
{
    const uint16_t crc = modbus_rtu_crc16(slave.tx_buffer, payload_length);
    slave.tx_buffer[payload_length] = static_cast<uint8_t>(crc & 0x00FFU);
    slave.tx_buffer[payload_length + 1U] = static_cast<uint8_t>(crc >> 8U);
    slave.tx_length = payload_length + MODBUS_CRC_LENGTH;
}

uint8_t request_crc_valid(const ModbusRtuSlave& slave)
{
    const uint16_t calculated = modbus_rtu_crc16(
        slave.rx_buffer,
        MODBUS_READ_REQUEST_LENGTH - MODBUS_CRC_LENGTH);
    const uint16_t received =
        static_cast<uint16_t>(slave.rx_buffer[MODBUS_READ_REQUEST_LENGTH - 2U]) |
        static_cast<uint16_t>(slave.rx_buffer[MODBUS_READ_REQUEST_LENGTH - 1U] << 8U);

    return (calculated == received) ? 1U : 0U;
}

void begin_transmit(ModbusRtuSlave& slave)
{
    const size_t written = slave.transport->write(slave.tx_buffer,
                                                  slave.tx_length);

    if (written != slave.tx_length)
    {
        ++slave.error_count;
        modbus_rtu_slave_reset(slave);
        return;
    }

    ++slave.response_count;
    slave.tx_deadline_ms = millis() + MODBUS_RTU_SLAVE_TX_TIMEOUT_MS;
    slave.state = MODBUS_RTU_SLAVE_WAIT_TX;
}

void send_exception(ModbusRtuSlave& slave,
                    uint8_t function_code,
                    uint8_t exception_code)
{
    slave.tx_buffer[0] = slave.slave_address;
    slave.tx_buffer[1] = static_cast<uint8_t>(function_code | 0x80U);
    slave.tx_buffer[2] = exception_code;
    append_crc(slave, 3U);
    begin_transmit(slave);
}

void process_request(ModbusRtuSlave& slave)
{
    ++slave.request_count;

    if (request_crc_valid(slave) == 0U)
    {
        ++slave.error_count;
        modbus_rtu_slave_reset(slave);
        return;
    }

    if (slave.rx_buffer[0] != slave.slave_address)
    {
        modbus_rtu_slave_reset(slave);
        return;
    }

    const uint8_t function_code = slave.rx_buffer[1];

    if (function_code != MODBUS_FUNCTION_READ_HOLDING)
    {
        ++slave.error_count;
        send_exception(slave,
                       function_code,
                       MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
        return;
    }

    const uint16_t start_address =
        static_cast<uint16_t>(slave.rx_buffer[2] << 8U) |
        static_cast<uint16_t>(slave.rx_buffer[3]);
    const uint16_t register_count =
        static_cast<uint16_t>(slave.rx_buffer[4] << 8U) |
        static_cast<uint16_t>(slave.rx_buffer[5]);

    if (register_count == 0U)
    {
        ++slave.error_count;
        send_exception(slave,
                       function_code,
                       MODBUS_EXCEPTION_ILLEGAL_VALUE);
        return;
    }

    if ((start_address >= slave.holding_register_count) ||
        (register_count >
         static_cast<uint16_t>(slave.holding_register_count - start_address)))
    {
        ++slave.error_count;
        send_exception(slave,
                       function_code,
                       MODBUS_EXCEPTION_ILLEGAL_ADDRESS);
        return;
    }

    if (register_count > MODBUS_MAX_RESPONSE_REGISTERS)
    {
        ++slave.error_count;
        send_exception(slave,
                       function_code,
                       MODBUS_EXCEPTION_ILLEGAL_VALUE);
        return;
    }

    slave.tx_buffer[0] = slave.slave_address;
    slave.tx_buffer[1] = MODBUS_FUNCTION_READ_HOLDING;
    slave.tx_buffer[2] = static_cast<uint8_t>(register_count * 2U);

    for (uint16_t index = 0U; index < register_count; ++index)
    {
        const uint16_t value = slave.holding_registers[start_address + index];
        const uint16_t byte_index = 3U + (index * 2U);
        slave.tx_buffer[byte_index] = static_cast<uint8_t>(value >> 8U);
        slave.tx_buffer[byte_index + 1U] =
            static_cast<uint8_t>(value & 0x00FFU);
    }

    append_crc(slave,
               static_cast<uint16_t>(3U + (register_count * 2U)));
    begin_transmit(slave);
}
}

void modbus_rtu_slave_init(ModbusRtuSlave& slave,
                           const ModbusRtuTransport& transport,
                           uint8_t slave_address,
                           uint16_t* holding_registers,
                           uint16_t holding_register_count,
                           uint32_t interframe_gap_ms)
{
    memset(&slave, 0, sizeof(slave));
    slave.transport = &transport;
    slave.slave_address = slave_address;
    slave.holding_registers = holding_registers;
    slave.holding_register_count = holding_register_count;
    slave.interframe_gap_ms = interframe_gap_ms;
    slave.state = MODBUS_RTU_SLAVE_RECEIVE;
    slave.last_rx_ms = millis();

    if (transport_valid(slave.transport) != 0U)
    {
        slave.transport->clear_rx();
    }
}

void modbus_rtu_slave_reset(ModbusRtuSlave& slave)
{
    slave.state = MODBUS_RTU_SLAVE_RECEIVE;
    slave.rx_length = 0U;
    slave.tx_length = 0U;
    slave.tx_deadline_ms = 0U;
    slave.last_rx_ms = millis();

    if (transport_valid(slave.transport) != 0U)
    {
        slave.transport->clear_rx();
    }
}

void modbus_rtu_slave_poll(ModbusRtuSlave& slave)
{
    if ((transport_valid(slave.transport) == 0U) ||
        (slave_address_valid(slave.slave_address) == 0U) ||
        (slave.holding_registers == 0) ||
        (slave.holding_register_count == 0U) ||
        (slave.interframe_gap_ms == 0U))
    {
        return;
    }

    const uint32_t now_ms = millis();

    if (slave.state == MODBUS_RTU_SLAVE_WAIT_TX)
    {
        if (slave.transport->tx_idle() != 0U)
        {
            modbus_rtu_slave_reset(slave);
            return;
        }

        if (deadline_reached(now_ms, slave.tx_deadline_ms) != 0U)
        {
            ++slave.error_count;
            modbus_rtu_slave_reset(slave);
        }

        return;
    }

    /* Цикл читает только уже доступные байты и ограничен RX capacity. */
    while (slave.transport->available() > 0U)
    {
        if (slave.rx_length >= MODBUS_RTU_SLAVE_RX_CAPACITY)
        {
            ++slave.error_count;
            modbus_rtu_slave_reset(slave);
            return;
        }

        const int value = slave.transport->read();

        if (value < 0)
        {
            break;
        }

        slave.rx_buffer[slave.rx_length] = static_cast<uint8_t>(value);
        ++slave.rx_length;
        slave.last_rx_ms = now_ms;
    }

    if (slave.rx_length == MODBUS_READ_REQUEST_LENGTH)
    {
        process_request(slave);
        return;
    }

    if (slave.rx_length > MODBUS_READ_REQUEST_LENGTH)
    {
        ++slave.error_count;
        modbus_rtu_slave_reset(slave);
        return;
    }

    if ((slave.rx_length != 0U) &&
        (gap_elapsed(now_ms,
                     slave.last_rx_ms,
                     slave.interframe_gap_ms) != 0U))
    {
        ++slave.error_count;
        modbus_rtu_slave_reset(slave);
    }
}
