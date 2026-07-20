/**
 * @file field_status.cpp
 * @brief Реализация service console для FieldSensor S1..S4.
 */

#include "field_status.hpp"

#include "../field/field_sensor_service.hpp"
#include "../../board/lcp_field_ports.hpp"
#include "../../platform/platform.hpp"

#include <string.h>

namespace
{
const char* frame_text(HalUartFrame frame)
{
    switch (frame)
    {
        case HAL_UART_FRAME_8O1:
            return "8O1";
        case HAL_UART_FRAME_8E1:
            return "8E1";
        case HAL_UART_FRAME_8N1:
        default:
            return "8N1";
    }
}

void print_port(LcpFieldPortId port_id)
{
    const FieldSensorPortState& port = field_sensor_service_port(port_id);
    const LcpFieldPortConfig& serial = lcp_field_port_config(port_id);

    SerialUSB.print(lcp_field_port_name(port_id));
    SerialUSB.print(": role=");
    SerialUSB.print(field_sensor_role_text(port.config.role));
    SerialUSB.print(", present=");
    SerialUSB.print((port.port_present != 0U) ? "yes" : "no");
    SerialUSB.print(", baud=");
    SerialUSB.print(static_cast<unsigned long>(serial.baudrate));
    SerialUSB.print(", format=");
    SerialUSB.print(frame_text(serial.frame));
    SerialUSB.print(", slave=");
    SerialUSB.print(static_cast<int>(port.config.slave_address));
    SerialUSB.print(", FC=03, start=");
    SerialUSB.print(static_cast<int>(port.config.start_register));
    SerialUSB.print(", count=");
    SerialUSB.print(static_cast<int>(port.config.register_count));
    SerialUSB.print(", period_ms=");
    SerialUSB.print(static_cast<unsigned long>(port.config.poll_period_ms));
    SerialUSB.print("\r\n");

    SerialUSB.print("  connection=");
    SerialUSB.print((port.connection_lost != 0U) ? "lost" : "online");
    SerialUSB.print(", valid=");
    SerialUSB.print((port.valid != 0U) ? "yes" : "no");
    SerialUSB.print(", request=");
    SerialUSB.print((port.request_active != 0U) ? "busy" : "idle");
    SerialUSB.print(", result=");
    SerialUSB.print(modbus_rtu_result_text(port.last_result));
    SerialUSB.print(", exception_code=");
    SerialUSB.print(static_cast<int>(port.last_exception_code));
    SerialUSB.print("\r\n");

    SerialUSB.print("  registers=");
    SerialUSB.print(static_cast<unsigned long>(port.registers[0]));
    SerialUSB.print(",");
    SerialUSB.print(static_cast<unsigned long>(port.registers[1]));
    SerialUSB.print(", success=");
    SerialUSB.print(static_cast<unsigned long>(port.successful_poll_count));
    SerialUSB.print(", failed=");
    SerialUSB.print(static_cast<unsigned long>(port.failed_poll_count));
    SerialUSB.print(", consecutive_failures=");
    SerialUSB.print(static_cast<int>(port.consecutive_failures));
    SerialUSB.print(", last_update_ms=");
    SerialUSB.print(static_cast<unsigned long>(port.last_update_ms));
    SerialUSB.print(", uart_error_count=");
    SerialUSB.print(static_cast<unsigned long>(
        lcp_field_port_error_count(port_id)));
    SerialUSB.print("\r\n");
}
}

void field_status_print_report(void)
{
    const FieldSerialConfigReport& config_report =
        field_sensor_service_serial_config_report();

    SerialUSB.print("FieldSensor status\r\n");
    SerialUSB.print("service=");
    SerialUSB.print((field_sensor_service_paused() != 0U) ?
                    "paused" : "running");
    SerialUSB.print(", implementation=hardcoded device example");
    SerialUSB.print(", config_reload=");
    SerialUSB.print((field_sensor_service_config_reload_pending() != 0U) ?
                    "pending" : "idle");
    SerialUSB.print("\r\n");

    SerialUSB.print("serial_config: BAUD.TXT=");
    SerialUSB.print(field_serial_config_result_text(config_report.baud_result));
    SerialUSB.print(", PARITY.TXT=");
    SerialUSB.print(field_serial_config_result_text(config_report.parity_result));
    SerialUSB.print(", any_loaded_from_sd=");
    SerialUSB.print((config_report.loaded_from_sd != 0U) ? "yes" : "no");
    SerialUSB.print("\r\n");

    for (uint8_t index = 0U;
         index < field_sensor_service_port_count();
         ++index)
    {
        print_port(static_cast<LcpFieldPortId>(index));
    }
}

uint8_t field_status_handle_command(const char* command)
{
    if (command == 0)
    {
        return 0U;
    }

    if ((strcmp(command, "field") == 0) ||
        (strcmp(command, "field status") == 0))
    {
        field_status_print_report();
        return 1U;
    }

    if (strcmp(command, "field pause") == 0)
    {
        field_sensor_service_pause();
        SerialUSB.print("FieldSensor polling: paused after active requests\r\n");
        return 1U;
    }

    if (strcmp(command, "field resume") == 0)
    {
        field_sensor_service_resume();
        SerialUSB.print("FieldSensor polling: running\r\n");
        return 1U;
    }

    if (strcmp(command, "field reload") == 0)
    {
        field_sensor_service_request_config_reload();
        SerialUSB.print("FieldSensor serial configuration reload: pending\r\n");
        return 1U;
    }

    return 0U;
}
