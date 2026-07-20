/**
 * @file x2x_status.cpp
 * @brief Реализация USB-диагностики X2X.
 *
 * Общий отчёт намеренно не знает регистров конкретного модуля. Общие поля
 * печатаются здесь, а специфические значения выводит callback `print` из
 * X2XModuleDescriptor. При добавлении нового типа модуля достаточно добавить
 * callback в `x2x_catalog.cpp`; новый switch в этом файле не требуется.
 */

#include "x2x_status.hpp"
#include "diagnostic_output.hpp"

#include "../x2x/x2x_catalog.hpp"
#include "../x2x/x2x_registry.hpp"
#include "../x2x/x2x_service.hpp"
#include "../../board/lcp_x2x_port.hpp"
#include "../../platform/platform.hpp"
#include "../../protocol/modbus_rtu/modbus_rtu_crc.hpp"

#include <stdint.h>
#include <string.h>

namespace
{
const char* communication_error_text(X2XCommunicationError error)
{
    switch (error)
    {
        case X2X_COMMUNICATION_OK:
            return "ok";
        case X2X_COMMUNICATION_TIMEOUT:
            return "timeout";
        case X2X_COMMUNICATION_CRC_ERROR:
            return "crc error";
        case X2X_COMMUNICATION_PROTOCOL_ERROR:
            return "protocol error";
        case X2X_COMMUNICATION_EXCEPTION:
            return "exception";
        case X2X_COMMUNICATION_TRANSPORT_ERROR:
            return "transport error";
        default:
            return "unknown";
    }
}

void print_common_device_status(const X2XDeviceHeader& device,
                                const X2XModuleDescriptor& descriptor)
{
    const uint32_t now_ms = millis();

    SerialUSB.print("identity: slave=");
    SerialUSB.print(static_cast<int>(device.slave_address));
    SerialUSB.print(", type_id=");
    SerialUSB.print(static_cast<int>(device.type));
    SerialUSB.print(", type_name=");
    SerialUSB.print(descriptor.name);
    SerialUSB.print(", ASDU=");
    SerialUSB.print(static_cast<unsigned long>(device.asdu));
    SerialUSB.print("\r\n");

    SerialUSB.print("state: connection=");
    SerialUSB.print((device.connection_lost != 0U) ? "lost" : "online");
    SerialUSB.print(", device_fault=");
    SerialUSB.print((device.device_fault != 0U) ? "yes" : "no");
    SerialUSB.print(", communication_error=");
    SerialUSB.print(communication_error_text(device.last_communication_error));
    SerialUSB.print(", exception_code=");
    SerialUSB.print(static_cast<int>(device.last_exception_code));
    SerialUSB.print("\r\n");

    SerialUSB.print("counters: success=");
    SerialUSB.print(static_cast<unsigned long>(device.successful_poll_count));
    SerialUSB.print(", failed=");
    SerialUSB.print(static_cast<unsigned long>(device.failed_poll_count));
    SerialUSB.print(", consecutive_failures=");
    SerialUSB.print(static_cast<int>(device.consecutive_failures));
    SerialUSB.print("\r\n");

    SerialUSB.print("last_update_ms=");
    SerialUSB.print(static_cast<unsigned long>(device.last_update_ms));

    if (device.last_update_ms != 0U)
    {
        const uint32_t age_ms = now_ms - device.last_update_ms;
        SerialUSB.print(", age_ms=");
        SerialUSB.print(static_cast<unsigned long>(age_ms));
        SerialUSB.print(", age_human=");
        diagnostic_print_duration(age_ms);
    }
    else
    {
        SerialUSB.print(", age=never");
    }

    SerialUSB.print("\r\npoints: SP=");
    SerialUSB.print(static_cast<unsigned long>(descriptor.sp_count));
    SerialUSB.print(", ME=");
    SerialUSB.print(static_cast<unsigned long>(descriptor.me_count));
    SerialUSB.print(", TF=");
    SerialUSB.print(static_cast<unsigned long>(descriptor.tf_count));
    SerialUSB.print("\r\n");
}

/**
 * @brief Читает беззнаковый аргумент команды и оставляет cursor после числа.
 *
 * Ограничение 65535 выбрано потому, что функция используется для Modbus/X2X
 * аргументов. Полная проверка конца строки выполняется parse_signed().
 */
uint8_t parse_unsigned(const char*& cursor, uint32_t* value)
{
    uint32_t result = 0U;
    uint8_t digit_found = 0U;

    if ((cursor == 0) || (value == 0))
    {
        return 0U;
    }

    while ((*cursor == ' ') || (*cursor == '\t'))
    {
        ++cursor;
    }

    while ((*cursor >= '0') && (*cursor <= '9'))
    {
        digit_found = 1U;
        result = (result * 10U) + static_cast<uint32_t>(*cursor - '0');

        if (result > 65535U)
        {
            return 0U;
        }

        ++cursor;
    }

    if (digit_found == 0U)
    {
        return 0U;
    }

    *value = result;
    return 1U;
}

uint8_t parse_signed(const char*& cursor, int32_t* value)
{
    int32_t sign = 1L;
    uint32_t magnitude = 0U;

    if ((cursor == 0) || (value == 0))
    {
        return 0U;
    }

    while ((*cursor == ' ') || (*cursor == '\t'))
    {
        ++cursor;
    }

    if (*cursor == '-')
    {
        sign = -1L;
        ++cursor;
    }
    else if (*cursor == '+')
    {
        ++cursor;
    }

    if (parse_unsigned(cursor, &magnitude) == 0U)
    {
        return 0U;
    }

    const int32_t signed_value = sign * static_cast<int32_t>(magnitude);

    if ((signed_value < -32768L) || (signed_value > 32767L))
    {
        return 0U;
    }

    while ((*cursor == ' ') || (*cursor == '\t'))
    {
        ++cursor;
    }

    if (*cursor != '\0')
    {
        return 0U;
    }

    *value = signed_value;
    return 1U;
}

void handle_ldo_command(const char* arguments)
{
    const char* cursor = arguments;
    uint32_t slave = 0U;
    int32_t value = 0L;

    if ((parse_unsigned(cursor, &slave) == 0U) ||
        (parse_signed(cursor, &value) == 0U) ||
        (slave == 0U) ||
        (slave > X2X_MAX_MODULES))
    {
        SerialUSB.print("usage: x2x ldo SLAVE VALUE\r\n");
        SerialUSB.print("SLAVE must reference an LDO1118 slot 1..6; VALUE is int16\r\n");
        return;
    }

    if (x2x_service_set_ldo_output(static_cast<uint8_t>(slave),
                                   static_cast<int16_t>(value)) == 0U)
    {
        SerialUSB.print("x2x ldo failed: selected slave is absent or not LDO1118\r\n");
        return;
    }

    SerialUSB.print("x2x ldo updated: slave=");
    SerialUSB.print(static_cast<unsigned long>(slave));
    SerialUSB.print(", value=");
    SerialUSB.print(static_cast<long>(value));
    SerialUSB.print("\r\n");
}
}

void x2x_status_print_report(void)
{
    const X2XConfigError& config_error =
        x2x_service_last_config_error();
    const X2XWaveformBuffer& waveform =
        x2x_service_waveform();
    const uint8_t module_count = x2x_registry_module_count();

    diagnostic_print_section("X2X MODULE BUS");

    diagnostic_print_group("Configuration");
    SerialUSB.print("file=");
    SerialUSB.print(X2X_CONFIG_FILE_NAME);
    SerialUSB.print(", loaded=");
    SerialUSB.print((x2x_service_config_loaded() != 0U) ? "yes" : "no");
    SerialUSB.print(", result=");
    SerialUSB.print(x2x_config_result_text(
        x2x_service_last_config_result()));
    SerialUSB.print(", reload_pending=");
    SerialUSB.print((x2x_service_reload_pending() != 0U) ? "yes" : "no");
    SerialUSB.print("\r\nerror_line=");
    SerialUSB.print(static_cast<unsigned long>(config_error.physical_line));
    SerialUSB.print(", error_value=");
    SerialUSB.print(static_cast<long>(config_error.value));
    SerialUSB.print(", registry=");
    SerialUSB.print(x2x_registry_result_text(
        x2x_service_last_registry_result()));
    SerialUSB.print("\r\n");

    diagnostic_print_group("Runtime");
    SerialUSB.print("module_count=");
    SerialUSB.print(static_cast<int>(module_count));
    SerialUSB.print(", current_slave=");
    SerialUSB.print(static_cast<int>(x2x_service_current_slave()));
    SerialUSB.print(", paused=");
    SerialUSB.print((x2x_service_paused() != 0U) ? "yes" : "no");
    SerialUSB.print(", pause_pending=");
    SerialUSB.print((x2x_service_pause_pending() != 0U) ? "yes" : "no");
    SerialUSB.print(", port_owner=");
    SerialUSB.print((x2x_service_owns_port() != 0U) ?
                    "X2X master" : "echo test");
    SerialUSB.print("\r\nmodbus_state=");
    SerialUSB.print(modbus_rtu_result_text(x2x_service_modbus_result()));
    SerialUSB.print(", crc_self_test=");
    SerialUSB.print((modbus_rtu_crc_self_test() != 0U) ? "ok" : "failed");
    SerialUSB.print(", uart_errors=");
    SerialUSB.print(static_cast<unsigned long>(lcp_x2x_port_error_count()));
    SerialUSB.print("\r\n");

    for (uint8_t slave = 1U; slave <= module_count; ++slave)
    {
        SerialUSB.print("\r\n-- MODULE slave=");
        SerialUSB.print(static_cast<int>(slave));
        SerialUSB.print(" --\r\n");

        const X2XDeviceHeader* device =
            x2x_registry_get_by_slave(slave);

        if (device == 0)
        {
            SerialUSB.print("registry_error=device slot is empty\r\n");
            continue;
        }

        const X2XModuleDescriptor* descriptor =
            x2x_catalog_find(device->type);

        if (descriptor == 0)
        {
            SerialUSB.print("catalog_error=descriptor not found for type_id=");
            SerialUSB.print(static_cast<int>(device->type));
            SerialUSB.print("\r\n");
            continue;
        }

        print_common_device_status(*device, *descriptor);

        if (descriptor->print != 0)
        {
            descriptor->print(*device);
        }
        else
        {
            SerialUSB.print("module_specific_diagnostics=not registered\r\n");
        }
    }

    diagnostic_print_group("LCT waveform buffer");
    SerialUSB.print("valid=");
    SerialUSB.print((waveform.valid != 0U) ? "yes" : "no");
    SerialUSB.print(", owner_slave=");
    SerialUSB.print(static_cast<int>(waveform.owner_slave_address));
    SerialUSB.print(", samples_per_phase=");
    SerialUSB.print(static_cast<unsigned long>(waveform.samples_per_phase));
    SerialUSB.print(", sequence=");
    SerialUSB.print(static_cast<unsigned long>(waveform.sequence));
    SerialUSB.print("\r\n");
}

uint8_t x2x_status_handle_command(const char* command)
{
    if (command == 0)
    {
        return 0U;
    }

    if ((strcmp(command, "x2x") == 0) ||
        (strcmp(command, "x2x status") == 0))
    {
        x2x_status_print_report();
        return 1U;
    }

    if (strcmp(command, "x2x reload") == 0)
    {
        const X2XConfigResult result = x2x_service_reload_config();
        SerialUSB.print("x2x reload: ");
        SerialUSB.print(x2x_config_result_text(result));
        SerialUSB.print("\r\n");
        x2x_status_print_report();
        return 1U;
    }

    if (strcmp(command, "x2x pause") == 0)
    {
        x2x_service_pause();
        SerialUSB.print((x2x_service_paused() != 0U) ?
                        "x2x polling: paused\r\n" :
                        "x2x polling: pause pending until active cycle completes\r\n");
        return 1U;
    }

    if (strcmp(command, "x2x resume") == 0)
    {
        x2x_service_resume();
        SerialUSB.print("x2x polling: running\r\n");
        return 1U;
    }

    static const char LDO_PREFIX[] = "x2x ldo ";

    if (strncmp(command, LDO_PREFIX, sizeof(LDO_PREFIX) - 1U) == 0)
    {
        handle_ldo_command(command + sizeof(LDO_PREFIX) - 1U);
        return 1U;
    }

    return 0U;
}
