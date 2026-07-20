/**
 * @file modbus_tcp_server.cpp
 * @brief Реализация минимального Modbus TCP server.
 */

#include "modbus_tcp_server.hpp"

#include <string.h>

namespace
{
static const uint8_t MODBUS_FUNCTION_READ_HOLDING = 0x03U;
static const uint8_t MODBUS_EXCEPTION_ILLEGAL_FUNCTION = 0x01U;
static const uint8_t MODBUS_EXCEPTION_ILLEGAL_ADDRESS = 0x02U;
static const uint8_t MODBUS_EXCEPTION_ILLEGAL_VALUE = 0x03U;
static const uint16_t MODBUS_TCP_HEADER_WITH_UNIT = 7U;
static const uint16_t MODBUS_TCP_READ_REQUEST_LENGTH = 12U;
static const uint16_t MODBUS_TCP_MAX_READ_REGISTERS = 125U;

uint16_t read_u16_be(const uint8_t* data)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8U) |
                                 static_cast<uint16_t>(data[1]));
}

void write_u16_be(uint8_t* data, uint16_t value)
{
    data[0] = static_cast<uint8_t>(value >> 8U);
    data[1] = static_cast<uint8_t>(value & 0x00FFU);
}

void remove_request(ModbusTcpServer& server, uint16_t request_length)
{
    if (request_length >= server.rx_length)
    {
        server.rx_length = 0U;
        return;
    }

    const uint16_t remaining =
        static_cast<uint16_t>(server.rx_length - request_length);
    memmove(server.rx_buffer,
            &server.rx_buffer[request_length],
            remaining);
    server.rx_length = remaining;
}

void copy_mbap_prefix(ModbusTcpServer& server, uint16_t length_field)
{
    server.tx_buffer[0] = server.rx_buffer[0];
    server.tx_buffer[1] = server.rx_buffer[1];
    server.tx_buffer[2] = 0U;
    server.tx_buffer[3] = 0U;
    write_u16_be(&server.tx_buffer[4], length_field);
    server.tx_buffer[6] = server.rx_buffer[6];
}

void build_exception(ModbusTcpServer& server,
                     uint8_t function_code,
                     uint8_t exception_code)
{
    copy_mbap_prefix(server, 3U);
    server.tx_buffer[7] = static_cast<uint8_t>(function_code | 0x80U);
    server.tx_buffer[8] = exception_code;
    server.tx_length = 9U;
    ++server.exception_count;
    ++server.response_count;
}
}

void modbus_tcp_server_init(ModbusTcpServer& server)
{
    memset(&server, 0, sizeof(server));
}

uint8_t modbus_tcp_server_append(ModbusTcpServer& server,
                                 const uint8_t* data,
                                 uint16_t length)
{
    if ((data == 0) || (length == 0U))
    {
        return 1U;
    }

    if (length > static_cast<uint16_t>(MODBUS_TCP_ADU_CAPACITY -
                                       server.rx_length))
    {
        server.rx_length = 0U;
        ++server.malformed_count;
        return 0U;
    }

    memcpy(&server.rx_buffer[server.rx_length], data, length);
    server.rx_length = static_cast<uint16_t>(server.rx_length + length);
    return 1U;
}

uint8_t modbus_tcp_server_process(ModbusTcpServer& server,
                                  ModbusTcpReadHoldingHandler read_handler,
                                  void* context)
{
    if ((server.tx_length != 0U) || (server.rx_length < MODBUS_TCP_HEADER_WITH_UNIT))
    {
        return 0U;
    }

    const uint16_t protocol_id = read_u16_be(&server.rx_buffer[2]);
    const uint16_t length_field = read_u16_be(&server.rx_buffer[4]);
    const uint32_t expected_length = 6UL + static_cast<uint32_t>(length_field);

    if ((protocol_id != 0U) ||
        (length_field < 2U) ||
        (expected_length > MODBUS_TCP_ADU_CAPACITY))
    {
        server.rx_length = 0U;
        ++server.malformed_count;
        return 0U;
    }

    if (server.rx_length < expected_length)
    {
        return 0U;
    }

    const uint16_t request_length = static_cast<uint16_t>(expected_length);
    const uint8_t function_code = server.rx_buffer[7];
    ++server.request_count;

    if (function_code != MODBUS_FUNCTION_READ_HOLDING)
    {
        build_exception(server,
                        function_code,
                        MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
        remove_request(server, request_length);
        return 1U;
    }

    if ((request_length != MODBUS_TCP_READ_REQUEST_LENGTH) ||
        (length_field != 6U))
    {
        build_exception(server,
                        function_code,
                        MODBUS_EXCEPTION_ILLEGAL_VALUE);
        remove_request(server, request_length);
        return 1U;
    }

    const uint16_t start_address = read_u16_be(&server.rx_buffer[8]);
    const uint16_t register_count = read_u16_be(&server.rx_buffer[10]);

    if ((register_count == 0U) ||
        (register_count > MODBUS_TCP_MAX_READ_REGISTERS))
    {
        build_exception(server,
                        function_code,
                        MODBUS_EXCEPTION_ILLEGAL_VALUE);
        remove_request(server, request_length);
        return 1U;
    }

    uint16_t values[MODBUS_TCP_MAX_READ_REGISTERS];
    uint8_t exception_code = MODBUS_EXCEPTION_ILLEGAL_ADDRESS;

    if (read_handler != 0)
    {
        exception_code = read_handler(context,
                                      start_address,
                                      register_count,
                                      values);
    }

    if (exception_code != 0U)
    {
        build_exception(server, function_code, exception_code);
        remove_request(server, request_length);
        return 1U;
    }

    const uint16_t response_length_field =
        static_cast<uint16_t>(3U + (register_count * 2U));
    const uint16_t response_length =
        static_cast<uint16_t>(6U + response_length_field);

    if (response_length > MODBUS_TCP_ADU_CAPACITY)
    {
        build_exception(server,
                        function_code,
                        MODBUS_EXCEPTION_ILLEGAL_VALUE);
        remove_request(server, request_length);
        return 1U;
    }

    copy_mbap_prefix(server, response_length_field);
    server.tx_buffer[7] = MODBUS_FUNCTION_READ_HOLDING;
    server.tx_buffer[8] = static_cast<uint8_t>(register_count * 2U);

    for (uint16_t index = 0U; index < register_count; ++index)
    {
        write_u16_be(&server.tx_buffer[9U + (index * 2U)], values[index]);
    }

    server.tx_length = response_length;
    ++server.response_count;
    remove_request(server, request_length);
    return 1U;
}

const uint8_t* modbus_tcp_server_response(const ModbusTcpServer& server)
{
    return server.tx_buffer;
}

uint16_t modbus_tcp_server_response_length(const ModbusTcpServer& server)
{
    return server.tx_length;
}

void modbus_tcp_server_response_sent(ModbusTcpServer& server)
{
    server.tx_length = 0U;
}
