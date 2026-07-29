/**
 * @file x2x_ldo.cpp
 * @brief Драйвер опроса и управления релейным модулем LDO1118 по X2X.
 */

#include "x2x_module_drivers.hpp"
#include "x2x_module_common.hpp"

namespace
{
enum LdoPollState : uint8_t
{
    LDO_POLL_START_WRITE = 0U,
    LDO_POLL_WAIT_WRITE = 1U
};
}

X2XModulePollResult x2x_module_poll_ldo1118(X2XDeviceHeader* device,
                                            X2XModuleContext& context)
{
    if ((device == 0) ||
        (device->type != X2X_DEVICE_LDO1118) ||
        (context.master == 0) ||
        (context.runtime == 0))
    {
        return X2X_MODULE_POLL_CYCLE_COMPLETE;
    }

    X2XLdo1118* ldo = static_cast<X2XLdo1118*>(device);

    switch (context.runtime->state)
    {
        case LDO_POLL_START_WRITE:
        {
            if (modbus_rtu_master_ready(*context.master) == 0U)
            {
                return X2X_MODULE_POLL_IN_PROGRESS;
            }

            const uint16_t output_value =
                static_cast<uint16_t>(ldo->output_value);

            if (modbus_rtu_master_start_write_multiple(*context.master,
                                                       device->slave_address,
                                                       0U,
                                                       &output_value,
                                                       1U,
                                                       X2X_MODBUS_TRANSACTION_TIMEOUT_MS) == 0U)
            {
                x2x_module_mark_failure(*device,
                                        modbus_rtu_master_result(*context.master),
                                        modbus_rtu_master_exception_code(*context.master));
                modbus_rtu_master_reset(*context.master);
                context.runtime->state = LDO_POLL_START_WRITE;
                return X2X_MODULE_POLL_CYCLE_COMPLETE;
            }

            context.runtime->state = LDO_POLL_WAIT_WRITE;
            return X2X_MODULE_POLL_IN_PROGRESS;
        }

        case LDO_POLL_WAIT_WRITE:
        {
            const ModbusRtuResult result =
                modbus_rtu_master_result(*context.master);

            if (result == MODBUS_RTU_RESULT_BUSY)
            {
                return X2X_MODULE_POLL_IN_PROGRESS;
            }

            if (result == MODBUS_RTU_RESULT_OK)
            {
                x2x_module_mark_success(*device, context.now_ms);
            }
            else
            {
                x2x_module_mark_failure(*device,
                                        result,
                                        modbus_rtu_master_exception_code(*context.master));
            }

            modbus_rtu_master_reset(*context.master);
            context.runtime->state = LDO_POLL_START_WRITE;
            return X2X_MODULE_POLL_CYCLE_COMPLETE;
        }

        default:
            modbus_rtu_master_reset(*context.master);
            context.runtime->state = LDO_POLL_START_WRITE;
            return X2X_MODULE_POLL_CYCLE_COMPLETE;
    }
}
