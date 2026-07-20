/**
 * @file field_sensor_service.cpp
 * @brief Реализация демонстрационного Modbus RTU master для S1..S4.
 */

#include "field_sensor_service.hpp"

#include "../../platform/platform.hpp"

#include <string.h>

namespace
{
static const uint8_t FIELD_SENSOR_REGISTER_COUNT = 2U;
static const uint8_t FIELD_SENSOR_CONNECTION_LOSS_THRESHOLD = 5U;
static const uint32_t FIELD_SENSOR_DEFAULT_BAUDRATE = 9600U;
static const uint32_t FIELD_SENSOR_DEFAULT_POLL_PERIOD_MS = 300U;
static const uint32_t FIELD_SENSOR_DEFAULT_TIMEOUT_MS = 500U;
static const uint32_t FIELD_SENSOR_MODBUS_INTERFRAME_GAP_MS = 5U;

static_assert(FIELD_SENSOR_REGISTER_COUNT == 2U,
              "FieldSensor baseline publishes exactly two registers");

const FieldSensorConfig g_default_configs[LCP_FIELD_PORT_COUNT] =
{
    {
        FIELD_PORT_MASTER, 1U, 0U, FIELD_SENSOR_REGISTER_COUNT,
        FIELD_SENSOR_DEFAULT_POLL_PERIOD_MS, FIELD_SENSOR_DEFAULT_TIMEOUT_MS,
        { FIELD_SENSOR_DEFAULT_BAUDRATE, HAL_UART_FRAME_8N1 }
    },
    {
        FIELD_PORT_MASTER, 1U, 0U, FIELD_SENSOR_REGISTER_COUNT,
        FIELD_SENSOR_DEFAULT_POLL_PERIOD_MS, FIELD_SENSOR_DEFAULT_TIMEOUT_MS,
        { FIELD_SENSOR_DEFAULT_BAUDRATE, HAL_UART_FRAME_8N1 }
    },
    {
        FIELD_PORT_MASTER, 1U, 0U, FIELD_SENSOR_REGISTER_COUNT,
        FIELD_SENSOR_DEFAULT_POLL_PERIOD_MS, FIELD_SENSOR_DEFAULT_TIMEOUT_MS,
        { FIELD_SENSOR_DEFAULT_BAUDRATE, HAL_UART_FRAME_8N1 }
    },
    {
        FIELD_PORT_MASTER, 1U, 0U, FIELD_SENSOR_REGISTER_COUNT,
        FIELD_SENSOR_DEFAULT_POLL_PERIOD_MS, FIELD_SENSOR_DEFAULT_TIMEOUT_MS,
        { FIELD_SENSOR_DEFAULT_BAUDRATE, HAL_UART_FRAME_8N1 }
    }
};

FieldSensorPortState g_ports[LCP_FIELD_PORT_COUNT];
uint8_t g_paused = 0U;

LcpFieldPortId normalize_port_id(LcpFieldPortId port_id)
{
    return (port_id < LCP_FIELD_PORT_COUNT) ? port_id : LCP_FIELD_PORT_S1;
}

uint8_t deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (static_cast<int32_t>(now_ms - deadline_ms) >= 0) ? 1U : 0U;
}

void schedule_next_request(FieldSensorPortState& port, uint32_t now_ms)
{
    port.next_poll_ms += port.config.poll_period_ms;

    if (deadline_reached(now_ms, port.next_poll_ms) != 0U)
    {
        port.next_poll_ms = now_ms + port.config.poll_period_ms;
    }
}

void mark_success(FieldSensorPortState& port, uint32_t now_ms)
{
    port.valid = 1U;
    port.connection_lost = 0U;
    port.consecutive_failures = 0U;
    port.last_exception_code = 0U;
    port.last_result = MODBUS_RTU_RESULT_OK;
    port.last_update_ms = now_ms;
    ++port.successful_poll_count;
}

void mark_failure(FieldSensorPortState& port,
                  ModbusRtuResult result,
                  uint8_t exception_code)
{
    if (port.consecutive_failures < 0xFFU)
    {
        ++port.consecutive_failures;
    }

    ++port.failed_poll_count;
    port.last_result = result;
    port.last_exception_code = exception_code;

    if (port.consecutive_failures >= FIELD_SENSOR_CONNECTION_LOSS_THRESHOLD)
    {
        port.connection_lost = 1U;
        port.valid = 0U;
    }
}

void finish_request(FieldSensorPortState& port, uint32_t now_ms)
{
    const ModbusRtuResult result = modbus_rtu_master_result(port.master);

    if (result == MODBUS_RTU_RESULT_OK)
    {
        mark_success(port, now_ms);
    }
    else
    {
        mark_failure(port,
                     result,
                     modbus_rtu_master_exception_code(port.master));
    }

    modbus_rtu_master_reset(port.master);
    port.request_active = 0U;
}

void poll_port(LcpFieldPortId port_id, uint32_t now_ms)
{
    FieldSensorPortState& port = g_ports[port_id];

    if ((port.config.role != FIELD_PORT_MASTER) ||
        (port.port_present == 0U))
    {
        return;
    }

    modbus_rtu_master_poll(port.master);

    if (port.request_active != 0U)
    {
        if (modbus_rtu_master_busy(port.master) != 0U)
        {
            return;
        }

        finish_request(port, now_ms);
    }

    if ((g_paused != 0U) ||
        (deadline_reached(now_ms, port.next_poll_ms) == 0U) ||
        (modbus_rtu_master_ready(port.master) == 0U))
    {
        return;
    }

    schedule_next_request(port, now_ms);

    if (modbus_rtu_master_start_read_holding(
            port.master,
            port.config.slave_address,
            port.config.start_register,
            port.config.register_count,
            port.registers,
            port.config.timeout_ms) == 0U)
    {
        mark_failure(port,
                     modbus_rtu_master_result(port.master),
                     modbus_rtu_master_exception_code(port.master));
        modbus_rtu_master_reset(port.master);
        return;
    }

    port.request_active = 1U;
}
}

void field_sensor_service_init(void)
{
    LcpFieldPortConfig serial_configs[LCP_FIELD_PORT_COUNT];

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        serial_configs[index] = g_default_configs[index].serial;
    }

    lcp_field_ports_init(serial_configs);

    const uint32_t now_ms = millis();

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        FieldSensorPortState& port = g_ports[index];
        memset(&port, 0, sizeof(port));
        port.config = g_default_configs[index];
        port.port_present = lcp_field_port_present(
            static_cast<LcpFieldPortId>(index));
        port.connection_lost = 1U;
        port.valid = 0U;
        port.last_result = (port.port_present != 0U) ?
            MODBUS_RTU_RESULT_IDLE : MODBUS_RTU_RESULT_TRANSPORT_ERROR;
        port.next_poll_ms = now_ms;

        modbus_rtu_master_init(
            port.master,
            lcp_field_port_transport(static_cast<LcpFieldPortId>(index)),
            FIELD_SENSOR_MODBUS_INTERFRAME_GAP_MS);
    }

    g_paused = 0U;
}

void field_sensor_service_poll(void)
{
    const uint32_t now_ms = millis();

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        poll_port(static_cast<LcpFieldPortId>(index), now_ms);
    }
}

void field_sensor_service_pause(void)
{
    g_paused = 1U;
}

void field_sensor_service_resume(void)
{
    const uint32_t now_ms = millis();
    g_paused = 0U;

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        if (g_ports[index].request_active == 0U)
        {
            g_ports[index].next_poll_ms = now_ms;
        }
    }
}

uint8_t field_sensor_service_paused(void)
{
    return g_paused;
}

const FieldSensorPortState& field_sensor_service_port(LcpFieldPortId port_id)
{
    return g_ports[normalize_port_id(port_id)];
}

uint8_t field_sensor_service_port_count(void)
{
    return LCP_FIELD_PORT_COUNT;
}

const char* field_sensor_role_text(FieldPortRole role)
{
    switch (role)
    {
        case FIELD_PORT_MASTER:
            return "master";

        case FIELD_PORT_SLAVE:
            return "slave";

        case FIELD_PORT_DISABLED:
        default:
            return "disabled";
    }
}
