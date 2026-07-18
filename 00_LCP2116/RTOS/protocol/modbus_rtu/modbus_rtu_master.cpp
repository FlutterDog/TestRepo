/**
 * @file modbus_rtu_master.cpp
 * @brief Реализация неблокирующего Modbus RTU master.
 */

#include "modbus_rtu_master.hpp"
#include "modbus_rtu_crc.hpp"
#include "../../platform/platform.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace
{
static const uint8_t MODBUS_FUNCTION_READ_HOLDING = 0x03U;
static const uint8_t MODBUS_FUNCTION_WRITE_MULTIPLE = 0x10U;
static const uint16_t MODBUS_REQUEST_BASE_LENGTH = 6U;
static const uint16_t MODBUS_CRC_LENGTH = 2U;
static const uint16_t MODBUS_WRITE_RESPONSE_LENGTH = 8U;
static const uint16_t MODBUS_EXCEPTION_RESPONSE_LENGTH = 5U;

uint8_t deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (static_cast<int32_t>(now_ms - deadline_ms) >= 0) ? 1U : 0U;
}

void finish_transaction(ModbusRtuMaster& master, ModbusRtuResult result)
{
    master.state = MODBUS_RTU_MASTER_STATE_IDLE;
    master.result = result;
    master.next_request_ms = millis() + master.interframe_gap_ms;
}

uint8_t transport_valid(const ModbusRtuTransport* transport)
{
    return ((transport != 0) &&
            (transport->write != 0) &&
            (transport->available != 0) &&
            (transport->read != 0) &&
            (transport->tx_idle != 0) &&
            (transport->clear_rx != 0)) ? 1U : 0U;
}

void append_crc(ModbusRtuMaster& master, uint16_t payload_length)
{
    const uint16_t crc = modbus_rtu_crc16(master.tx_buffer, payload_length);
    master.tx_buffer[payload_length] = static_cast<uint8_t>(crc & 0x00FFU);
    master.tx_buffer[payload_length + 1U] = static_cast<uint8_t>(crc >> 8U);
    master.tx_length = payload_length + MODBUS_CRC_LENGTH;
}

uint8_t start_transmission(ModbusRtuMaster& master, uint32_t timeout_ms)
{
    master.transport->clear_rx();
    master.rx_length = 0U;
    master.exception_code = 0U;
    master.timeout_ms = timeout_ms;
    master.deadline_ms = millis() + timeout_ms;

    const size_t written = master.transport->write(master.tx_buffer,
                                                   master.tx_length);

    if (written != master.tx_length)
    {
        finish_transaction(master, MODBUS_RTU_RESULT_TRANSPORT_ERROR);
        return 0U;
    }

    master.result = MODBUS_RTU_RESULT_BUSY;
    master.state = MODBUS_RTU_MASTER_STATE_WAIT_TX;
    return 1U;
}

uint8_t response_crc_valid(const ModbusRtuMaster& master,
                           uint16_t response_length)
{
    if (response_length < MODBUS_CRC_LENGTH)
    {
        return 0U;
    }

    const uint16_t calculated =
        modbus_rtu_crc16(master.rx_buffer,
                         response_length - MODBUS_CRC_LENGTH);
    const uint16_t received =
        static_cast<uint16_t>(master.rx_buffer[response_length - 2U]) |
        static_cast<uint16_t>(master.rx_buffer[response_length - 1U] << 8U);

    return (calculated == received) ? 1U : 0U;
}

void validate_response(ModbusRtuMaster& master)
{
    uint16_t response_length = master.expected_response_length;

    if ((master.rx_length >= 2U) &&
        (master.rx_buffer[1] ==
         static_cast<uint8_t>(master.function_code | 0x80U)))
    {
        response_length = MODBUS_EXCEPTION_RESPONSE_LENGTH;
    }

    if (master.rx_length < response_length)
    {
        return;
    }

    if (response_crc_valid(master, response_length) == 0U)
    {
        finish_transaction(master, MODBUS_RTU_RESULT_CRC_ERROR);
        return;
    }

    if (master.rx_buffer[0] != master.slave_address)
    {
        finish_transaction(master, MODBUS_RTU_RESULT_INVALID_RESPONSE);
        return;
    }

    if (master.rx_buffer[1] ==
        static_cast<uint8_t>(master.function_code | 0x80U))
    {
        master.exception_code = master.rx_buffer[2];
        finish_transaction(master, MODBUS_RTU_RESULT_EXCEPTION);
        return;
    }

    if (master.rx_buffer[1] != master.function_code)
    {
        finish_transaction(master, MODBUS_RTU_RESULT_INVALID_RESPONSE);
        return;
    }

    if (master.transaction_type == MODBUS_RTU_TRANSACTION_READ_HOLDING)
    {
        const uint16_t expected_byte_count = master.register_count * 2U;

        if ((master.rx_buffer[2] != expected_byte_count) ||
            (master.read_output == 0))
        {
            finish_transaction(master, MODBUS_RTU_RESULT_INVALID_RESPONSE);
            return;
        }

        for (uint16_t register_index = 0U;
             register_index < master.register_count;
             ++register_index)
        {
            const uint16_t byte_index = 3U + (register_index * 2U);
            master.read_output[register_index] =
                static_cast<uint16_t>(master.rx_buffer[byte_index] << 8U) |
                static_cast<uint16_t>(master.rx_buffer[byte_index + 1U]);
        }

        finish_transaction(master, MODBUS_RTU_RESULT_OK);
        return;
    }

    if (master.transaction_type == MODBUS_RTU_TRANSACTION_WRITE_MULTIPLE)
    {
        const uint16_t response_address =
            static_cast<uint16_t>(master.rx_buffer[2] << 8U) |
            static_cast<uint16_t>(master.rx_buffer[3]);
        const uint16_t response_count =
            static_cast<uint16_t>(master.rx_buffer[4] << 8U) |
            static_cast<uint16_t>(master.rx_buffer[5]);

        if ((response_address != master.start_address) ||
            (response_count != master.register_count))
        {
            finish_transaction(master, MODBUS_RTU_RESULT_INVALID_RESPONSE);
            return;
        }

        finish_transaction(master, MODBUS_RTU_RESULT_OK);
        return;
    }

    finish_transaction(master, MODBUS_RTU_RESULT_INVALID_RESPONSE);
}

void receive_available_bytes(ModbusRtuMaster& master)
{
    while (master.transport->available() > 0U)
    {
        if (master.rx_length >= MODBUS_RTU_MASTER_RX_CAPACITY)
        {
            finish_transaction(master, MODBUS_RTU_RESULT_INVALID_RESPONSE);
            return;
        }

        const int value = master.transport->read();

        if (value < 0)
        {
            break;
        }

        master.rx_buffer[master.rx_length] = static_cast<uint8_t>(value);
        ++master.rx_length;

        uint16_t required_length = master.expected_response_length;

        if ((master.rx_length >= 2U) &&
            (master.rx_buffer[1] ==
             static_cast<uint8_t>(master.function_code | 0x80U)))
        {
            required_length = MODBUS_EXCEPTION_RESPONSE_LENGTH;
        }

        if (master.rx_length >= required_length)
        {
            break;
        }
    }
}
}

void modbus_rtu_master_init(ModbusRtuMaster& master,
                            const ModbusRtuTransport& transport,
                            uint32_t interframe_gap_ms)
{
    memset(&master, 0, sizeof(master));
    master.transport = &transport;
    master.state = MODBUS_RTU_MASTER_STATE_IDLE;
    master.result = MODBUS_RTU_RESULT_IDLE;
    master.interframe_gap_ms = interframe_gap_ms;
    master.next_request_ms = millis();
}

void modbus_rtu_master_reset(ModbusRtuMaster& master)
{
    const ModbusRtuTransport* transport = master.transport;
    const uint32_t interframe_gap_ms = master.interframe_gap_ms;
    const uint32_t next_request_ms = master.next_request_ms;

    memset(&master, 0, sizeof(master));
    master.transport = transport;
    master.state = MODBUS_RTU_MASTER_STATE_IDLE;
    master.result = MODBUS_RTU_RESULT_IDLE;
    master.interframe_gap_ms = interframe_gap_ms;
    master.next_request_ms = next_request_ms;

    if (transport_valid(transport) != 0U)
    {
        transport->clear_rx();
    }
}

uint8_t modbus_rtu_master_ready(const ModbusRtuMaster& master)
{
    if ((master.state != MODBUS_RTU_MASTER_STATE_IDLE) ||
        (master.result == MODBUS_RTU_RESULT_BUSY))
    {
        return 0U;
    }

    return deadline_reached(millis(), master.next_request_ms);
}

uint8_t modbus_rtu_master_start_read_holding(ModbusRtuMaster& master,
                                             uint8_t slave_address,
                                             uint16_t start_address,
                                             uint16_t register_count,
                                             uint16_t* output,
                                             uint32_t timeout_ms)
{
    if ((transport_valid(master.transport) == 0U) ||
        (slave_address == 0U) ||
        (register_count == 0U) ||
        (register_count > MODBUS_RTU_MASTER_MAX_READ_REGISTERS) ||
        (output == 0) ||
        (timeout_ms == 0U))
    {
        master.result = MODBUS_RTU_RESULT_INVALID_ARGUMENT;
        return 0U;
    }

    if (modbus_rtu_master_ready(master) == 0U)
    {
        return 0U;
    }

    master.transaction_type = MODBUS_RTU_TRANSACTION_READ_HOLDING;
    master.slave_address = slave_address;
    master.function_code = MODBUS_FUNCTION_READ_HOLDING;
    master.start_address = start_address;
    master.register_count = register_count;
    master.read_output = output;
    master.expected_response_length = 5U + (register_count * 2U);

    master.tx_buffer[0] = slave_address;
    master.tx_buffer[1] = MODBUS_FUNCTION_READ_HOLDING;
    master.tx_buffer[2] = static_cast<uint8_t>(start_address >> 8U);
    master.tx_buffer[3] = static_cast<uint8_t>(start_address & 0x00FFU);
    master.tx_buffer[4] = static_cast<uint8_t>(register_count >> 8U);
    master.tx_buffer[5] = static_cast<uint8_t>(register_count & 0x00FFU);
    append_crc(master, MODBUS_REQUEST_BASE_LENGTH);

    return start_transmission(master, timeout_ms);
}

uint8_t modbus_rtu_master_start_write_multiple(ModbusRtuMaster& master,
                                               uint8_t slave_address,
                                               uint16_t start_address,
                                               const uint16_t* values,
                                               uint16_t register_count,
                                               uint32_t timeout_ms)
{
    const uint16_t payload_length = 7U + (register_count * 2U);

    if ((transport_valid(master.transport) == 0U) ||
        (slave_address == 0U) ||
        (values == 0) ||
        (register_count == 0U) ||
        (register_count > MODBUS_RTU_MASTER_MAX_WRITE_REGISTERS) ||
        ((payload_length + MODBUS_CRC_LENGTH) >
         MODBUS_RTU_MASTER_TX_CAPACITY) ||
        (timeout_ms == 0U))
    {
        master.result = MODBUS_RTU_RESULT_INVALID_ARGUMENT;
        return 0U;
    }

    if (modbus_rtu_master_ready(master) == 0U)
    {
        return 0U;
    }

    master.transaction_type = MODBUS_RTU_TRANSACTION_WRITE_MULTIPLE;
    master.slave_address = slave_address;
    master.function_code = MODBUS_FUNCTION_WRITE_MULTIPLE;
    master.start_address = start_address;
    master.register_count = register_count;
    master.read_output = 0;
    master.expected_response_length = MODBUS_WRITE_RESPONSE_LENGTH;

    master.tx_buffer[0] = slave_address;
    master.tx_buffer[1] = MODBUS_FUNCTION_WRITE_MULTIPLE;
    master.tx_buffer[2] = static_cast<uint8_t>(start_address >> 8U);
    master.tx_buffer[3] = static_cast<uint8_t>(start_address & 0x00FFU);
    master.tx_buffer[4] = static_cast<uint8_t>(register_count >> 8U);
    master.tx_buffer[5] = static_cast<uint8_t>(register_count & 0x00FFU);
    master.tx_buffer[6] = static_cast<uint8_t>(register_count * 2U);

    for (uint16_t register_index = 0U;
         register_index < register_count;
         ++register_index)
    {
        const uint16_t byte_index = 7U + (register_index * 2U);
        master.tx_buffer[byte_index] =
            static_cast<uint8_t>(values[register_index] >> 8U);
        master.tx_buffer[byte_index + 1U] =
            static_cast<uint8_t>(values[register_index] & 0x00FFU);
    }

    append_crc(master, payload_length);
    return start_transmission(master, timeout_ms);
}

void modbus_rtu_master_poll(ModbusRtuMaster& master)
{
    if ((master.state == MODBUS_RTU_MASTER_STATE_IDLE) ||
        (master.result != MODBUS_RTU_RESULT_BUSY))
    {
        return;
    }

    const uint32_t now_ms = millis();

    if (master.state == MODBUS_RTU_MASTER_STATE_WAIT_TX)
    {
        if (master.transport->tx_idle() != 0U)
        {
            master.state = MODBUS_RTU_MASTER_STATE_WAIT_RESPONSE;
            master.deadline_ms = now_ms + master.timeout_ms;
            return;
        }

        if (deadline_reached(now_ms, master.deadline_ms) != 0U)
        {
            finish_transaction(master,
                               MODBUS_RTU_RESULT_TRANSPORT_ERROR);
        }

        return;
    }

    if (master.state == MODBUS_RTU_MASTER_STATE_WAIT_RESPONSE)
    {
        receive_available_bytes(master);

        if (master.state == MODBUS_RTU_MASTER_STATE_IDLE)
        {
            return;
        }

        validate_response(master);

        if (master.state == MODBUS_RTU_MASTER_STATE_IDLE)
        {
            return;
        }

        if (deadline_reached(now_ms, master.deadline_ms) != 0U)
        {
            finish_transaction(master, MODBUS_RTU_RESULT_TIMEOUT);
        }
    }
}

uint8_t modbus_rtu_master_busy(const ModbusRtuMaster& master)
{
    return (master.result == MODBUS_RTU_RESULT_BUSY) ? 1U : 0U;
}

ModbusRtuResult modbus_rtu_master_result(const ModbusRtuMaster& master)
{
    return master.result;
}

uint8_t modbus_rtu_master_exception_code(const ModbusRtuMaster& master)
{
    return master.exception_code;
}

const char* modbus_rtu_result_text(ModbusRtuResult result)
{
    switch (result)
    {
        case MODBUS_RTU_RESULT_IDLE:
            return "idle";

        case MODBUS_RTU_RESULT_BUSY:
            return "busy";

        case MODBUS_RTU_RESULT_OK:
            return "ok";

        case MODBUS_RTU_RESULT_TIMEOUT:
            return "timeout";

        case MODBUS_RTU_RESULT_CRC_ERROR:
            return "crc error";

        case MODBUS_RTU_RESULT_INVALID_RESPONSE:
            return "invalid response";

        case MODBUS_RTU_RESULT_EXCEPTION:
            return "exception";

        case MODBUS_RTU_RESULT_TRANSPORT_ERROR:
            return "transport error";

        case MODBUS_RTU_RESULT_INVALID_ARGUMENT:
            return "invalid argument";

        default:
            return "unknown";
    }
}
