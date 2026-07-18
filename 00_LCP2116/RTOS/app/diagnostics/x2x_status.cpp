/**
 * @file x2x_status.cpp
 * @brief Реализация USB-диагностики X2X.
 */

#include "x2x_status.hpp"

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
    SerialUSB.print("slave=");
    SerialUSB.print(static_cast<int>(device.slave_address));
    SerialUSB.print(", id=");
    SerialUSB.print(static_cast<int>(device.type));
    SerialUSB.print(", type=");
    SerialUSB.print(descriptor.name);
    SerialUSB.print(", connection=");
    SerialUSB.print((device.connection_lost != 0U) ? "lost" : "online");
    SerialUSB.print(", fault=");
    SerialUSB.print((device.device_fault != 0U) ? "yes" : "no");
    SerialUSB.print("\r\n");

    SerialUSB.print("  success=");
    SerialUSB.print(static_cast<unsigned long>(device.successful_poll_count));
    SerialUSB.print(", failed=");
    SerialUSB.print(static_cast<unsigned long>(device.failed_poll_count));
    SerialUSB.print(", consecutive_failures=");
    SerialUSB.print(static_cast<int>(device.consecutive_failures));
    SerialUSB.print(", last_update_ms=");
    SerialUSB.print(static_cast<unsigned long>(device.last_update_ms));
    SerialUSB.print("\r\n");

    SerialUSB.print("  communication_error=");
    SerialUSB.print(communication_error_text(device.last_communication_error));
    SerialUSB.print(", exception_code=");
    SerialUSB.print(static_cast<int>(device.last_exception_code));
    SerialUSB.print(", SP/ME/TF=");
    SerialUSB.print(static_cast<int>(descriptor.sp_count));
    SerialUSB.print("/");
    SerialUSB.print(static_cast<int>(descriptor.me_count));
    SerialUSB.print("/");
    SerialUSB.print(static_cast<int>(descriptor.tf_count));
    SerialUSB.print("\r\n");
}

void print_float_values(const float* values,
                        uint8_t value_count,
                        uint8_t maximum_to_print)
{
    const uint8_t print_count =
        (value_count < maximum_to_print) ? value_count : maximum_to_print;

    SerialUSB.print("  float_values=");

    for (uint8_t index = 0U; index < print_count; ++index)
    {
        if (index != 0U)
        {
            SerialUSB.print(", ");
        }

        SerialUSB.print(static_cast<int>(index));
        SerialUSB.print(":");
        SerialUSB.print(values[index]);
    }

    if (print_count < value_count)
    {
        SerialUSB.print(", ... total=");
        SerialUSB.print(static_cast<int>(value_count));
    }

    SerialUSB.print("\r\n");
}

void print_device_data(const X2XDeviceHeader& device)
{
    switch (device.type)
    {
        case X2X_DEVICE_LDO1118:
        {
            const X2XLdo1118& ldo =
                static_cast<const X2XLdo1118&>(device);
            SerialUSB.print("  output_value=");
            SerialUSB.print(static_cast<int>(ldo.output_value));
            SerialUSB.print(", digital_inputs=");
            SerialUSB.print(static_cast<unsigned long>(ldo.digital_inputs));
            SerialUSB.print("\r\n");
            break;
        }

        case X2X_DEVICE_LAI1118:
        {
            const X2XLai1118& lai =
                static_cast<const X2XLai1118&>(device);
            SerialUSB.print("  digital_inputs=");
            SerialUSB.print(static_cast<unsigned long>(lai.digital_inputs));
            SerialUSB.print("\r\n");
            print_float_values(lai.tf_values, X2XLai1118::TF_COUNT, 8U);
            break;
        }

        case X2X_DEVICE_LDI1118:
        {
            const X2XLdi1118& ldi =
                static_cast<const X2XLdi1118&>(device);
            SerialUSB.print("  digital_inputs=");
            SerialUSB.print(static_cast<unsigned long>(ldi.digital_inputs));
            SerialUSB.print("\r\n");
            break;
        }

        case X2X_DEVICE_LDI1116:
        {
            const X2XLdi1116& ldi =
                static_cast<const X2XLdi1116&>(device);
            SerialUSB.print("  digital_inputs=");
            SerialUSB.print(static_cast<unsigned long>(ldi.digital_inputs));
            SerialUSB.print("\r\n");
            break;
        }

        case X2X_DEVICE_LCT1114:
        {
            const X2XLct1114& lct =
                static_cast<const X2XLct1114&>(device);
            SerialUSB.print("  digital_inputs=");
            SerialUSB.print(static_cast<unsigned long>(lct.digital_inputs));
            SerialUSB.print("\r\n");
            print_float_values(lct.tf_values, X2XLct1114::TF_COUNT, 8U);
            break;
        }

        case X2X_DEVICE_LCT1114_2:
        {
            const X2XLct1114_2& lct =
                static_cast<const X2XLct1114_2&>(device);
            SerialUSB.print("  digital_inputs=");
            SerialUSB.print(static_cast<unsigned long>(lct.digital_inputs));
            SerialUSB.print("\r\n");
            print_float_values(lct.tf_values, X2XLct1114_2::TF_COUNT, 8U);
            break;
        }

        case X2X_DEVICE_LCP:
        default:
            break;
    }
}

uint8_t parse_unsigned(const char*& cursor, uint32_t* value)
{
    uint32_t result = 0U;
    uint8_t digit_found = 0U;

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
        (slave > 255U))
    {
        SerialUSB.print("usage: x2x ldo SLAVE VALUE\r\n");
        return;
    }

    if (x2x_service_set_ldo_output(static_cast<uint8_t>(slave),
                                   static_cast<int16_t>(value)) == 0U)
    {
        SerialUSB.print("x2x ldo failed: slave is not LDO1118\r\n");
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

    SerialUSB.print("X2X status\r\n");
    SerialUSB.print("config_file=");
    SerialUSB.print(X2X_CONFIG_FILE_NAME);
    SerialUSB.print(", loaded=");
    SerialUSB.print((x2x_service_config_loaded() != 0U) ? "yes" : "no");
    SerialUSB.print(", result=");
    SerialUSB.print(x2x_config_result_text(
        x2x_service_last_config_result()));
    SerialUSB.print("\r\n");

    SerialUSB.print("config_error_line=");
    SerialUSB.print(static_cast<int>(config_error.physical_line));
    SerialUSB.print(", value=");
    SerialUSB.print(static_cast<long>(config_error.value));
    SerialUSB.print(", registry=");
    SerialUSB.print(x2x_registry_result_text(
        x2x_service_last_registry_result()));
    SerialUSB.print(", reload_pending=");
    SerialUSB.print((x2x_service_reload_pending() != 0U) ? "yes" : "no");
    SerialUSB.print("\r\n");

    SerialUSB.print("modules=");
    SerialUSB.print(static_cast<int>(x2x_registry_module_count()));
    SerialUSB.print(", current_slave=");
    SerialUSB.print(static_cast<int>(x2x_service_current_slave()));
    SerialUSB.print(", paused=");
    SerialUSB.print((x2x_service_paused() != 0U) ? "yes" : "no");
    SerialUSB.print(", pause_pending=");
    SerialUSB.print((x2x_service_pause_pending() != 0U) ? "yes" : "no");
    SerialUSB.print(", port_owner=");
    SerialUSB.print((x2x_service_owns_port() != 0U) ?
                    "x2x_master" : "echo_test");
    SerialUSB.print("\r\n");

    SerialUSB.print("modbus_state=");
    SerialUSB.print(modbus_rtu_result_text(x2x_service_modbus_result()));
    SerialUSB.print(", crc_self_test=");
    SerialUSB.print((modbus_rtu_crc_self_test() != 0U) ? "ok" : "failed");
    SerialUSB.print(", uart_error_count=");
    SerialUSB.print(static_cast<unsigned long>(lcp_x2x_port_error_count()));
    SerialUSB.print("\r\n");

    for (uint8_t slave = 1U;
         slave <= x2x_registry_module_count();
         ++slave)
    {
        const X2XDeviceHeader* device =
            x2x_registry_get_by_slave(slave);

        if (device == 0)
        {
            continue;
        }

        const X2XModuleDescriptor* descriptor =
            x2x_catalog_find(device->type);

        if (descriptor == 0)
        {
            continue;
        }

        print_common_device_status(*device, *descriptor);
        print_device_data(*device);
    }

    SerialUSB.print("waveform_valid=");
    SerialUSB.print((waveform.valid != 0U) ? "yes" : "no");
    SerialUSB.print(", owner_slave=");
    SerialUSB.print(static_cast<int>(waveform.owner_slave_address));
    SerialUSB.print(", samples_per_phase=");
    SerialUSB.print(static_cast<int>(waveform.samples_per_phase));
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

        if (x2x_service_paused() != 0U)
        {
            SerialUSB.print("x2x polling: paused\r\n");
        }
        else
        {
            SerialUSB.print("x2x polling: pause pending\r\n");
        }

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
