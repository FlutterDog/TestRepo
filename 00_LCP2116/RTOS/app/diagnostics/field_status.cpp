/**
 * @file field_status.cpp
 * @brief Реализация service console для FieldSensor S1..S4.
 *
 * Отчёт показывает отдельно конфигурацию, качество связи, последние значения
 * и счётчики каждого физического порта. При добавлении новых полей в
 * FieldSensorConfig или FieldSensorPortState их следует вывести в
 * print_port(), сохраняя разделение на группы `Configuration`, `State`,
 * `Values` и `Counters`.
 */

#include "field_status.hpp"
#include "diagnostic_output.hpp"

#include "../field/field_sensor_service.hpp"
#include "../../board/lcp_field_ports.hpp"
#include "../../platform/platform.hpp"

#include <string.h>

namespace
{
void print_port(LcpFieldPortId port_id)
{
    const FieldSensorPortState& port = field_sensor_service_port(port_id);
    const LcpFieldPortConfig& serial = lcp_field_port_config(port_id);
    const uint32_t now_ms = millis();

    diagnostic_print_group(lcp_field_port_name(port_id));

    SerialUSB.print("configuration: role=");
    SerialUSB.print(field_sensor_role_text(port.config.role));
    SerialUSB.print(", present=");
    SerialUSB.print((port.port_present != 0U) ? "yes" : "no");
    SerialUSB.print(", serial=");
    SerialUSB.print(static_cast<unsigned long>(serial.baudrate));
    SerialUSB.print(" ");
    SerialUSB.print(diagnostic_uart_frame_text(serial.frame));
    SerialUSB.print("\r\n");

    SerialUSB.print("request: slave=");
    SerialUSB.print(static_cast<int>(port.config.slave_address));
    SerialUSB.print(", function=0x03, start_register=");
    SerialUSB.print(static_cast<unsigned long>(port.config.start_register));
    SerialUSB.print(", register_count=");
    SerialUSB.print(static_cast<unsigned long>(port.config.register_count));
    SerialUSB.print(", period_ms=");
    SerialUSB.print(static_cast<unsigned long>(port.config.poll_period_ms));
    SerialUSB.print(", timeout_ms=");
    SerialUSB.print(static_cast<unsigned long>(port.config.timeout_ms));
    SerialUSB.print("\r\n");

    SerialUSB.print("state: connection=");
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

    SerialUSB.print("values: register[0]=");
    SerialUSB.print(static_cast<unsigned long>(port.registers[0]));
    SerialUSB.print(", register[1]=");
    SerialUSB.print(static_cast<unsigned long>(port.registers[1]));
    SerialUSB.print("\r\n");

    SerialUSB.print("counters: success=");
    SerialUSB.print(static_cast<unsigned long>(port.successful_poll_count));
    SerialUSB.print(", failed=");
    SerialUSB.print(static_cast<unsigned long>(port.failed_poll_count));
    SerialUSB.print(", consecutive_failures=");
    SerialUSB.print(static_cast<int>(port.consecutive_failures));
    SerialUSB.print(", uart_errors=");
    SerialUSB.print(static_cast<unsigned long>(
        lcp_field_port_error_count(port_id)));
    SerialUSB.print("\r\n");

    SerialUSB.print("last_update_ms=");
    SerialUSB.print(static_cast<unsigned long>(port.last_update_ms));

    if (port.last_update_ms != 0U)
    {
        const uint32_t age_ms = now_ms - port.last_update_ms;
        SerialUSB.print(", age_ms=");
        SerialUSB.print(static_cast<unsigned long>(age_ms));
        SerialUSB.print(", age_human=");
        diagnostic_print_duration(age_ms);
    }
    else
    {
        SerialUSB.print(", age=never");
    }

    SerialUSB.print("\r\n");
}
}

void field_status_print_report(void)
{
    const FieldSerialConfigReport& config_report =
        field_sensor_service_serial_config_report();

    diagnostic_print_section("FIELDSENSOR S1..S4");

    SerialUSB.print("service=");
    SerialUSB.print((field_sensor_service_paused() != 0U) ?
                    "paused" : "running");
    SerialUSB.print(", config_reload=");
    SerialUSB.print((field_sensor_service_config_reload_pending() != 0U) ?
                    "pending" : "idle");
    SerialUSB.print("\r\n");
    SerialUSB.print("device_model=hardcoded example; edit set_default_configs() in field_sensor_service.cpp\r\n");

    SerialUSB.print("serial_file_baud=baud.TXT, result=");
    SerialUSB.print(sd_config_result_text(config_report.baud_result));
    SerialUSB.print("\r\nserial_file_parity=Parity.TXT, result=");
    SerialUSB.print(sd_config_result_text(config_report.parity_result));
    SerialUSB.print("\r\nany_serial_value_loaded_from_sd=");
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
        SerialUSB.print("FieldSensor pause requested: active requests may finish, no new requests will start\r\n");
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
