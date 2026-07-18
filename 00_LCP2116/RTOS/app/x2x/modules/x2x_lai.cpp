/**
 * @file x2x_lai.cpp
 * @brief Драйвер модуля аналоговых входов LAI1118 по X2X.
 */

#include "x2x_module_drivers.hpp"
#include "x2x_module_common.hpp"

namespace
{
enum LaiPollState : uint8_t
{
    LAI_POLL_START_READ = 0U,
    LAI_POLL_WAIT_READ = 1U
};

static const uint16_t LAI_REGISTER_COUNT = 17U;
static const uint16_t LAI_FLOAT_START_REGISTER = 1U;
}

X2XModulePollResult x2x_module_poll_lai1118(X2XDeviceHeader* device,
                                            X2XModuleContext& context)
{
    if ((device == 0) ||
        (device->type != X2X_DEVICE_LAI1118) ||
        (context.master == 0) ||
        (context.runtime == 0) ||
        (context.register_buffer == 0) ||
        (context.register_capacity < LAI_REGISTER_COUNT))
    {
        return X2X_MODULE_POLL_CYCLE_COMPLETE;
    }

    X2XLai1118* lai = static_cast<X2XLai1118*>(device);

    switch (context.runtime->state)
    {
        case LAI_POLL_START_READ:
            if (modbus_rtu_master_start_read_holding(*context.master,
                                                     device->slave_address,
                                                     0U,
                                                     LAI_REGISTER_COUNT,
                                                     context.register_buffer,
                                                     X2X_MODBUS_TRANSACTION_TIMEOUT_MS) == 0U)
            {
                x2x_module_mark_failure(*device,
                                        modbus_rtu_master_result(*context.master),
                                        modbus_rtu_master_exception_code(*context.master));
                modbus_rtu_master_reset(*context.master);
                context.runtime->state = LAI_POLL_START_READ;
                return X2X_MODULE_POLL_CYCLE_COMPLETE;
            }

            context.runtime->state = LAI_POLL_WAIT_READ;
            return X2X_MODULE_POLL_IN_PROGRESS;

        case LAI_POLL_WAIT_READ:
        {
            const ModbusRtuResult result =
                modbus_rtu_master_result(*context.master);

            if (result == MODBUS_RTU_RESULT_BUSY)
            {
                return X2X_MODULE_POLL_IN_PROGRESS;
            }

            if (result == MODBUS_RTU_RESULT_OK)
            {
                lai->digital_inputs =
                    static_cast<uint32_t>(context.register_buffer[0] & 0x00FFU);
                x2x_module_decode_float_array(lai->tf_values,
                                              X2XLai1118::TF_COUNT,
                                              context.register_buffer,
                                              LAI_FLOAT_START_REGISTER,
                                              LAI_REGISTER_COUNT);
                x2x_module_mark_success(*device, context.now_ms);
            }
            else
            {
                x2x_module_mark_failure(*device,
                                        result,
                                        modbus_rtu_master_exception_code(*context.master));
            }

            modbus_rtu_master_reset(*context.master);
            context.runtime->state = LAI_POLL_START_READ;
            return X2X_MODULE_POLL_CYCLE_COMPLETE;
        }

        default:
            modbus_rtu_master_reset(*context.master);
            context.runtime->state = LAI_POLL_START_READ;
            return X2X_MODULE_POLL_CYCLE_COMPLETE;
    }
}
