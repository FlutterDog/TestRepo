/**
 * @file x2x_ldi.cpp
 * @brief Драйверы модулей дискретных входов LDI1118 и LDI1116 по X2X.
 */

#include "x2x_module_drivers.hpp"
#include "x2x_module_common.hpp"

namespace
{
enum LdiPollState : uint8_t
{
    LDI_POLL_START_READ = 0U,
    LDI_POLL_WAIT_READ = 1U
};

template<typename DeviceType, X2XDeviceType ExpectedType, uint32_t InputMask>
X2XModulePollResult poll_ldi(X2XDeviceHeader* device,
                            X2XModuleContext& context)
{
    if ((device == 0) ||
        (device->type != ExpectedType) ||
        (context.master == 0) ||
        (context.runtime == 0) ||
        (context.register_buffer == 0) ||
        (context.register_capacity < 1U))
    {
        return X2X_MODULE_POLL_CYCLE_COMPLETE;
    }

    DeviceType* ldi = static_cast<DeviceType*>(device);

    switch (context.runtime->state)
    {
        case LDI_POLL_START_READ:
            if (modbus_rtu_master_start_read_holding(*context.master,
                                                     device->slave_address,
                                                     0U,
                                                     1U,
                                                     context.register_buffer,
                                                     X2X_MODBUS_TRANSACTION_TIMEOUT_MS) == 0U)
            {
                x2x_module_mark_failure(*device,
                                        modbus_rtu_master_result(*context.master),
                                        modbus_rtu_master_exception_code(*context.master));
                modbus_rtu_master_reset(*context.master);
                context.runtime->state = LDI_POLL_START_READ;
                return X2X_MODULE_POLL_CYCLE_COMPLETE;
            }

            context.runtime->state = LDI_POLL_WAIT_READ;
            return X2X_MODULE_POLL_IN_PROGRESS;

        case LDI_POLL_WAIT_READ:
        {
            const ModbusRtuResult result =
                modbus_rtu_master_result(*context.master);

            if (result == MODBUS_RTU_RESULT_BUSY)
            {
                return X2X_MODULE_POLL_IN_PROGRESS;
            }

            if (result == MODBUS_RTU_RESULT_OK)
            {
                ldi->digital_inputs =
                    static_cast<uint32_t>(context.register_buffer[0]) & InputMask;
                x2x_module_mark_success(*device, context.now_ms);
            }
            else
            {
                x2x_module_mark_failure(*device,
                                        result,
                                        modbus_rtu_master_exception_code(*context.master));
            }

            modbus_rtu_master_reset(*context.master);
            context.runtime->state = LDI_POLL_START_READ;
            return X2X_MODULE_POLL_CYCLE_COMPLETE;
        }

        default:
            modbus_rtu_master_reset(*context.master);
            context.runtime->state = LDI_POLL_START_READ;
            return X2X_MODULE_POLL_CYCLE_COMPLETE;
    }
}
}

X2XModulePollResult x2x_module_poll_ldi1118(X2XDeviceHeader* device,
                                            X2XModuleContext& context)
{
    return poll_ldi<X2XLdi1118, X2X_DEVICE_LDI1118, 0x000000FFUL>(device,
                                                                  context);
}

X2XModulePollResult x2x_module_poll_ldi1116(X2XDeviceHeader* device,
                                            X2XModuleContext& context)
{
    return poll_ldi<X2XLdi1116, X2X_DEVICE_LDI1116, 0x0000FFFFUL>(device,
                                                                  context);
}
