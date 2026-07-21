/**
 * @file x2x_module_common.cpp
 * @brief Реализация общих операций драйверов X2X.
 */

#include "x2x_module_common.hpp"

#include <string.h>

void x2x_module_runtime_reset(X2XModuleRuntime& runtime)
{
    runtime.state = 0U;
    runtime.waveform_phase = 0U;
    runtime.transfer_offset = 0U;
    runtime.next_waveform_check_ms = 0U;
}

void x2x_module_mark_success(X2XDeviceHeader& device, uint32_t now_ms)
{
    device.connection_lost = 0U;
    device.consecutive_failures = 0U;
    device.last_communication_error = X2X_COMMUNICATION_OK;
    device.last_exception_code = 0U;
    device.last_update_ms = now_ms;
    ++device.successful_poll_count;
}

void x2x_module_mark_failure(X2XDeviceHeader& device,
                             ModbusRtuResult result,
                             uint8_t exception_code)
{
    if (device.consecutive_failures < 0xFFU)
    {
        ++device.consecutive_failures;
    }

    ++device.failed_poll_count;
    device.last_exception_code = exception_code;

    switch (result)
    {
        case MODBUS_RTU_RESULT_TIMEOUT:
            device.last_communication_error = X2X_COMMUNICATION_TIMEOUT;
            break;

        case MODBUS_RTU_RESULT_CRC_ERROR:
            device.last_communication_error = X2X_COMMUNICATION_CRC_ERROR;
            break;

        case MODBUS_RTU_RESULT_EXCEPTION:
            device.last_communication_error = X2X_COMMUNICATION_EXCEPTION;
            break;

        case MODBUS_RTU_RESULT_TRANSPORT_ERROR:
            device.last_communication_error = X2X_COMMUNICATION_TRANSPORT_ERROR;
            break;

        case MODBUS_RTU_RESULT_INVALID_RESPONSE:
        case MODBUS_RTU_RESULT_INVALID_ARGUMENT:
        default:
            device.last_communication_error = X2X_COMMUNICATION_PROTOCOL_ERROR;
            break;
    }

    if (device.consecutive_failures >= X2X_CONNECTION_LOSS_THRESHOLD)
    {
        device.connection_lost = 1U;
    }
}

float x2x_module_registers_to_float(uint16_t low_register,
                                    uint16_t high_register)
{
    const uint32_t raw_value =
        (static_cast<uint32_t>(high_register) << 16U) |
        static_cast<uint32_t>(low_register);
    float result = 0.0F;

    memcpy(&result, &raw_value, sizeof(result));
    return result;
}

void x2x_module_decode_float_array(float* destination,
                                   size_t float_count,
                                   const uint16_t* registers,
                                   size_t start_register,
                                   size_t register_count)
{
    if ((destination == 0) || (registers == 0))
    {
        return;
    }

    memset(destination, 0, float_count * sizeof(float));

    for (size_t float_index = 0U; float_index < float_count; ++float_index)
    {
        const size_t low_index = start_register + (float_index * 2U);
        const size_t high_index = low_index + 1U;

        if (high_index >= register_count)
        {
            break;
        }

        destination[float_index] =
            x2x_module_registers_to_float(registers[low_index],
                                          registers[high_index]);
    }
}
