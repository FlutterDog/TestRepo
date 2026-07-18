/**
 * @file x2x_lct.cpp
 * @brief Драйверы модулей коммутационного ресурса LCT1114 по X2X.
 */

#include "x2x_module_drivers.hpp"
#include "x2x_module_common.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace
{
enum LctPollState : uint8_t
{
    LCT_POLL_START_MAIN = 0U,
    LCT_POLL_WAIT_MAIN = 1U,
    LCT_POLL_START_WAVEFORM_FLAG = 2U,
    LCT_POLL_WAIT_WAVEFORM_FLAG = 3U,
    LCT_POLL_START_WAVEFORM_CHUNK = 4U,
    LCT_POLL_WAIT_WAVEFORM_CHUNK = 5U,
    LCT_POLL_START_WAVEFORM_RESET = 6U,
    LCT_POLL_WAIT_WAVEFORM_RESET = 7U
};

struct LctProfile
{
    X2XDeviceType type;
    uint16_t main_register_count;
    uint16_t waveform_register_count;
    uint16_t waveform_start_register[3];
    uint32_t post_capture_delay_ms;
};

static const uint16_t LCT_MAIN_START_REGISTER = 0U;
static const uint16_t LCT_FLOAT_START_REGISTER = 2U;
static const uint16_t LCT_WAVEFORM_FLAG_REGISTER = 850U;
static const uint16_t LCT_WAVEFORM_FLAG_NEW_DATA = 1U;
static const uint16_t LCT_WAVEFORM_FLAG_RESET_VALUE = 0U;

const LctProfile LCT1114_PROFILE =
{
    X2X_DEVICE_LCT1114,
    78U,
    200U,
    { 200U, 400U, 600U },
    0U
};

const LctProfile LCT1114_2_PROFILE =
{
    X2X_DEVICE_LCT1114_2,
    94U,
    195U,
    { 201U, 401U, 601U },
    1000U
};

uint16_t minimum_u16(uint16_t left, uint16_t right)
{
    return (left < right) ? left : right;
}

void reset_cycle(ModbusRtuMaster& master, X2XModuleRuntime& runtime)
{
    modbus_rtu_master_reset(master);
    runtime.state = LCT_POLL_START_MAIN;
    runtime.waveform_phase = 0U;
    runtime.transfer_offset = 0U;
}

X2XModulePollResult fail_cycle(X2XDeviceHeader& device,
                               X2XModuleContext& context,
                               ModbusRtuResult result)
{
    x2x_module_mark_failure(device,
                            result,
                            modbus_rtu_master_exception_code(*context.master));
    reset_cycle(*context.master, *context.runtime);
    return X2X_MODULE_POLL_CYCLE_COMPLETE;
}

void decode_lct_main(X2XDeviceHeader& device,
                     const LctProfile& profile,
                     const uint16_t* registers)
{
    const uint32_t digital_inputs =
        static_cast<uint32_t>(registers[0] & 0x003FU) |
        (static_cast<uint32_t>(registers[1] & 0x1FFFU) << 6U);

    if (profile.type == X2X_DEVICE_LCT1114)
    {
        X2XLct1114& lct = static_cast<X2XLct1114&>(device);
        lct.digital_inputs = digital_inputs;

        /*
         * В старой карте LCT1114 запрашивалось 78 регистров: два DI и
         * 76 регистров, то есть 38 float. Структура содержит 42 канала.
         * Декодер намеренно обнуляет четыре отсутствующих канала, сохраняя
         * совместимость с фактическим запросом до уточнения карты модуля.
         */
        x2x_module_decode_float_array(lct.tf_values,
                                      X2XLct1114::TF_COUNT,
                                      registers,
                                      LCT_FLOAT_START_REGISTER,
                                      profile.main_register_count);
    }
    else
    {
        X2XLct1114_2& lct = static_cast<X2XLct1114_2&>(device);
        lct.digital_inputs = digital_inputs;
        x2x_module_decode_float_array(lct.tf_values,
                                      X2XLct1114_2::TF_COUNT,
                                      registers,
                                      LCT_FLOAT_START_REGISTER,
                                      profile.main_register_count);
    }
}

X2XModulePollResult poll_lct(X2XDeviceHeader* device,
                            X2XModuleContext& context,
                            const LctProfile& profile)
{
    if ((device == 0) ||
        (device->type != profile.type) ||
        (context.master == 0) ||
        (context.runtime == 0) ||
        (context.waveform == 0) ||
        (context.register_buffer == 0) ||
        (context.register_capacity < MODBUS_RTU_MASTER_MAX_READ_REGISTERS))
    {
        return X2X_MODULE_POLL_CYCLE_COMPLETE;
    }

    switch (context.runtime->state)
    {
        case LCT_POLL_START_MAIN:
            if (modbus_rtu_master_start_read_holding(*context.master,
                                                     device->slave_address,
                                                     LCT_MAIN_START_REGISTER,
                                                     profile.main_register_count,
                                                     context.register_buffer,
                                                     X2X_MODBUS_TRANSACTION_TIMEOUT_MS) == 0U)
            {
                return fail_cycle(*device,
                                  context,
                                  modbus_rtu_master_result(*context.master));
            }

            context.runtime->state = LCT_POLL_WAIT_MAIN;
            return X2X_MODULE_POLL_IN_PROGRESS;

        case LCT_POLL_WAIT_MAIN:
        {
            const ModbusRtuResult result =
                modbus_rtu_master_result(*context.master);

            if (result == MODBUS_RTU_RESULT_BUSY)
            {
                return X2X_MODULE_POLL_IN_PROGRESS;
            }

            if (result != MODBUS_RTU_RESULT_OK)
            {
                return fail_cycle(*device, context, result);
            }

            decode_lct_main(*device, profile, context.register_buffer);
            x2x_module_mark_success(*device, context.now_ms);
            modbus_rtu_master_reset(*context.master);

            if ((profile.post_capture_delay_ms != 0U) &&
                (static_cast<int32_t>(context.now_ms -
                                      context.runtime->next_waveform_check_ms) < 0))
            {
                context.runtime->state = LCT_POLL_START_MAIN;
                return X2X_MODULE_POLL_CYCLE_COMPLETE;
            }

            context.runtime->state = LCT_POLL_START_WAVEFORM_FLAG;
            return X2X_MODULE_POLL_IN_PROGRESS;
        }

        case LCT_POLL_START_WAVEFORM_FLAG:
            if (modbus_rtu_master_start_read_holding(*context.master,
                                                     device->slave_address,
                                                     LCT_WAVEFORM_FLAG_REGISTER,
                                                     1U,
                                                     context.register_buffer,
                                                     X2X_MODBUS_TRANSACTION_TIMEOUT_MS) == 0U)
            {
                return fail_cycle(*device,
                                  context,
                                  modbus_rtu_master_result(*context.master));
            }

            context.runtime->state = LCT_POLL_WAIT_WAVEFORM_FLAG;
            return X2X_MODULE_POLL_IN_PROGRESS;

        case LCT_POLL_WAIT_WAVEFORM_FLAG:
        {
            const ModbusRtuResult result =
                modbus_rtu_master_result(*context.master);

            if (result == MODBUS_RTU_RESULT_BUSY)
            {
                return X2X_MODULE_POLL_IN_PROGRESS;
            }

            if (result != MODBUS_RTU_RESULT_OK)
            {
                return fail_cycle(*device, context, result);
            }

            const uint16_t waveform_flag = context.register_buffer[0];
            modbus_rtu_master_reset(*context.master);

            if (waveform_flag != LCT_WAVEFORM_FLAG_NEW_DATA)
            {
                context.runtime->state = LCT_POLL_START_MAIN;
                return X2X_MODULE_POLL_CYCLE_COMPLETE;
            }

            memset(context.waveform->phase, 0, sizeof(context.waveform->phase));
            context.waveform->valid = 0U;
            context.waveform->owner_slave_address = device->slave_address;
            context.waveform->samples_per_phase = profile.waveform_register_count;
            context.runtime->waveform_phase = 0U;
            context.runtime->transfer_offset = 0U;
            context.runtime->state = LCT_POLL_START_WAVEFORM_CHUNK;
            return X2X_MODULE_POLL_IN_PROGRESS;
        }

        case LCT_POLL_START_WAVEFORM_CHUNK:
        {
            if (context.runtime->waveform_phase >= 3U)
            {
                context.waveform->valid = 1U;
                ++context.waveform->sequence;
                context.runtime->state = LCT_POLL_START_WAVEFORM_RESET;
                return X2X_MODULE_POLL_IN_PROGRESS;
            }

            const uint16_t remaining =
                profile.waveform_register_count - context.runtime->transfer_offset;
            const uint16_t chunk_count =
                minimum_u16(remaining,
                            MODBUS_RTU_MASTER_MAX_READ_REGISTERS);
            const uint16_t start_address =
                profile.waveform_start_register[context.runtime->waveform_phase] +
                context.runtime->transfer_offset;

            if ((chunk_count == 0U) ||
                (modbus_rtu_master_start_read_holding(*context.master,
                                                      device->slave_address,
                                                      start_address,
                                                      chunk_count,
                                                      context.register_buffer,
                                                      X2X_MODBUS_TRANSACTION_TIMEOUT_MS) == 0U))
            {
                return fail_cycle(*device,
                                  context,
                                  modbus_rtu_master_result(*context.master));
            }

            context.runtime->state = LCT_POLL_WAIT_WAVEFORM_CHUNK;
            return X2X_MODULE_POLL_IN_PROGRESS;
        }

        case LCT_POLL_WAIT_WAVEFORM_CHUNK:
        {
            const ModbusRtuResult result =
                modbus_rtu_master_result(*context.master);

            if (result == MODBUS_RTU_RESULT_BUSY)
            {
                return X2X_MODULE_POLL_IN_PROGRESS;
            }

            if (result != MODBUS_RTU_RESULT_OK)
            {
                context.waveform->valid = 0U;
                return fail_cycle(*device, context, result);
            }

            const uint16_t remaining =
                profile.waveform_register_count - context.runtime->transfer_offset;
            const uint16_t chunk_count =
                minimum_u16(remaining,
                            MODBUS_RTU_MASTER_MAX_READ_REGISTERS);

            memcpy(&context.waveform->phase[context.runtime->waveform_phase]
                                            [context.runtime->transfer_offset],
                   context.register_buffer,
                   static_cast<size_t>(chunk_count) * sizeof(uint16_t));

            context.runtime->transfer_offset =
                static_cast<uint16_t>(context.runtime->transfer_offset +
                                      chunk_count);
            modbus_rtu_master_reset(*context.master);

            if (context.runtime->transfer_offset >=
                profile.waveform_register_count)
            {
                context.runtime->transfer_offset = 0U;
                ++context.runtime->waveform_phase;
            }

            context.runtime->state = LCT_POLL_START_WAVEFORM_CHUNK;
            return X2X_MODULE_POLL_IN_PROGRESS;
        }

        case LCT_POLL_START_WAVEFORM_RESET:
        {
            const uint16_t reset_value = LCT_WAVEFORM_FLAG_RESET_VALUE;

            if (modbus_rtu_master_start_write_multiple(*context.master,
                                                       device->slave_address,
                                                       LCT_WAVEFORM_FLAG_REGISTER,
                                                       &reset_value,
                                                       1U,
                                                       X2X_MODBUS_TRANSACTION_TIMEOUT_MS) == 0U)
            {
                return fail_cycle(*device,
                                  context,
                                  modbus_rtu_master_result(*context.master));
            }

            context.runtime->state = LCT_POLL_WAIT_WAVEFORM_RESET;
            return X2X_MODULE_POLL_IN_PROGRESS;
        }

        case LCT_POLL_WAIT_WAVEFORM_RESET:
        {
            const ModbusRtuResult result =
                modbus_rtu_master_result(*context.master);

            if (result == MODBUS_RTU_RESULT_BUSY)
            {
                return X2X_MODULE_POLL_IN_PROGRESS;
            }

            if (result != MODBUS_RTU_RESULT_OK)
            {
                context.waveform->valid = 0U;
                return fail_cycle(*device, context, result);
            }

            if (profile.post_capture_delay_ms != 0U)
            {
                context.runtime->next_waveform_check_ms =
                    context.now_ms + profile.post_capture_delay_ms;
            }

            reset_cycle(*context.master, *context.runtime);
            return X2X_MODULE_POLL_CYCLE_COMPLETE;
        }

        default:
            context.waveform->valid = 0U;
            reset_cycle(*context.master, *context.runtime);
            return X2X_MODULE_POLL_CYCLE_COMPLETE;
    }
}
}

X2XModulePollResult x2x_module_poll_lct1114(X2XDeviceHeader* device,
                                            X2XModuleContext& context)
{
    return poll_lct(device, context, LCT1114_PROFILE);
}

X2XModulePollResult x2x_module_poll_lct1114_2(X2XDeviceHeader* device,
                                              X2XModuleContext& context)
{
    return poll_lct(device, context, LCT1114_2_PROFILE);
}
