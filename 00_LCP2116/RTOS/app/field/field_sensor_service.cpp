/**
 * @file field_sensor_service.cpp
 * @brief Реализация демонстрационного Modbus RTU master для S1..S4.
 */

#include "field_sensor_service.hpp"
#include "../../libs/lcp_sd_storage/lcp_sd_storage.hpp"
#include "../../platform/platform.hpp"

#include <string.h>

namespace
{
constexpr uint8_t FIELD_SENSOR_CONNECTION_LOSS_THRESHOLD = 5U;
constexpr uint32_t FIELD_SENSOR_DEFAULT_POLL_PERIOD_MS = 300U;
constexpr uint32_t FIELD_SENSOR_DEFAULT_TIMEOUT_MS = 500U;
constexpr uint32_t FIELD_SENSOR_MODBUS_INTERFRAME_GAP_MS = 5U;
constexpr uint32_t FIELD_SENSOR_CONFIG_WAIT_MS = 2000U;

FieldSensorConfig g_configs[LCP_FIELD_PORT_COUNT];
FieldSensorPortState g_ports[LCP_FIELD_PORT_COUNT];
FieldSerialConfigReport g_serial_config_report =
{
    SD_CONFIG_NOT_ATTEMPTED,
    SD_CONFIG_NOT_ATTEMPTED,
    0U
};
uint8_t g_paused = 0U;
uint8_t g_config_reload_pending = 0U;
uint32_t g_config_wait_start_ms = 0U;

LcpFieldPortId normalize_port_id(LcpFieldPortId port_id)
{
    return (port_id < LCP_FIELD_PORT_COUNT) ? port_id : LCP_FIELD_PORT_S1;
}

uint8_t deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (static_cast<int32_t>(now_ms - deadline_ms) >= 0) ? 1U : 0U;
}

void set_default_configs(void)
{
    LcpFieldPortConfig serial[LCP_FIELD_PORT_COUNT];
    field_serial_config_set_defaults(serial);

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        g_configs[index].role = FIELD_PORT_MASTER;
        g_configs[index].slave_address = 1U;
        g_configs[index].start_register = 0U;
        g_configs[index].register_count = FIELD_SENSOR_REGISTER_COUNT;
        g_configs[index].poll_period_ms = FIELD_SENSOR_DEFAULT_POLL_PERIOD_MS;
        g_configs[index].timeout_ms = FIELD_SENSOR_DEFAULT_TIMEOUT_MS;
        g_configs[index].serial = serial[index];
    }
}

uint8_t all_requests_idle(void)
{
    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        if (g_ports[index].request_active != 0U)
        {
            return 0U;
        }
    }

    return 1U;
}

void initialize_runtime(void)
{
    LcpFieldPortConfig serial[LCP_FIELD_PORT_COUNT];

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        serial[index] = g_configs[index].serial;
    }

    lcp_field_ports_init(serial);
    const uint32_t now_ms = millis();

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        FieldSensorPortState& port = g_ports[index];
        memset(&port, 0, sizeof(port));
        port.config = g_configs[index];
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
}

void apply_sd_serial_config(void)
{
    LcpFieldPortConfig serial[LCP_FIELD_PORT_COUNT];

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        serial[index] = g_configs[index].serial;
    }

    field_serial_config_load(serial, &g_serial_config_report);

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        g_configs[index].serial = serial[index];
    }

    initialize_runtime();
    g_config_reload_pending = 0U;
}

void finish_config_wait_with_defaults(void)
{
    g_serial_config_report.baud_result = SD_CONFIG_CARD_NOT_READY;
    g_serial_config_report.parity_result = SD_CONFIG_CARD_NOT_READY;
    g_serial_config_report.loaded_from_sd = 0U;
    g_config_reload_pending = 0U;
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
        (g_config_reload_pending != 0U) ||
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
    set_default_configs();
    initialize_runtime();
    g_paused = 0U;
    g_config_reload_pending = 1U;
    g_config_wait_start_ms = millis();
}

void field_sensor_service_poll(void)
{
    const uint32_t now_ms = millis();

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        poll_port(static_cast<LcpFieldPortId>(index), now_ms);
    }

    if ((g_config_reload_pending == 0U) ||
        (all_requests_idle() == 0U))
    {
        return;
    }

    if (lcp_sd_storage_ready() != 0U)
    {
        apply_sd_serial_config();
        return;
    }

    if ((uint32_t)(now_ms - g_config_wait_start_ms) >=
        FIELD_SENSOR_CONFIG_WAIT_MS)
    {
        finish_config_wait_with_defaults();
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

void field_sensor_service_request_config_reload(void)
{
    g_config_reload_pending = 1U;
    g_config_wait_start_ms = millis();
}

uint8_t field_sensor_service_config_reload_pending(void)
{
    return g_config_reload_pending;
}

const FieldSerialConfigReport& field_sensor_service_serial_config_report(void)
{
    return g_serial_config_report;
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
