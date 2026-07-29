/**
 * @file x2x_catalog.cpp
 * @brief Реализация каталога поддерживаемых модулей X2X.
 */

#include "x2x_catalog.hpp"
#include "modules/x2x_module_drivers.hpp"
#include "../../platform/platform.hpp"
#include "../diagnostics/diagnostic_output.hpp"

#include <new>
#include <string.h>

namespace
{
template<typename DeviceType, X2XDeviceType TypeId>
X2XDeviceHeader* construct_device(void* storage,
                                  size_t storage_size,
                                  uint8_t slave_address,
                                  uint16_t asdu)
{
    static_assert(sizeof(DeviceType) <= X2X_DEVICE_SLOT_BYTES,
                  "X2X device type does not fit static registry slot");

    if ((storage == 0) || (storage_size < sizeof(DeviceType)))
    {
        return 0;
    }

    memset(storage, 0, sizeof(DeviceType));
    DeviceType* device = new (storage) DeviceType;

    device->type = TypeId;
    device->asdu = asdu;
    device->slave_address = slave_address;
    device->connection_lost = (TypeId == X2X_DEVICE_LCP) ? 0U : 1U;
    device->device_fault = 0U;
    device->last_communication_error = X2X_COMMUNICATION_OK;

    return device;
}

void print_float_values(const float* values,
                        uint8_t value_count,
                        uint8_t maximum_to_print)
{
    const uint8_t print_count =
        (value_count < maximum_to_print) ? value_count : maximum_to_print;

    SerialUSB.print("  float_values:");

    for (uint8_t index = 0U; index < print_count; ++index)
    {
        if ((index % 4U) == 0U)
        {
            SerialUSB.print("\r\n    ");
        }
        else
        {
            SerialUSB.print(", ");
        }

        diagnostic_print_index(index);
        SerialUSB.print(values[index]);
    }

    SerialUSB.print("\r\n");

    if (print_count < value_count)
    {
        SerialUSB.print("  ");
        diagnostic_print_assignment("float_values_shown");
        SerialUSB.print(static_cast<int>(print_count));
        SerialUSB.print(", ");
        diagnostic_print_assignment("total");
        SerialUSB.print(static_cast<int>(value_count));
        SerialUSB.print("\r\n");
    }
}

template<typename DeviceType>
void print_digital_inputs(const X2XDeviceHeader& header)
{
    const DeviceType& device = static_cast<const DeviceType&>(header);
    SerialUSB.print("  ");
    diagnostic_print_assignment("digital_inputs");
    SerialUSB.print(static_cast<unsigned long>(device.digital_inputs));
    SerialUSB.print("\r\n");
}

template<typename DeviceType>
void print_digital_inputs_and_floats(const X2XDeviceHeader& header)
{
    const DeviceType& device = static_cast<const DeviceType&>(header);
    SerialUSB.print("  ");
    diagnostic_print_assignment("digital_inputs");
    SerialUSB.print(static_cast<unsigned long>(device.digital_inputs));
    SerialUSB.print("\r\n");
    print_float_values(device.tf_values, DeviceType::TF_COUNT, 8U);
}

void print_ldo1118(const X2XDeviceHeader& header)
{
    const X2XLdo1118& device = static_cast<const X2XLdo1118&>(header);
    SerialUSB.print("  ");
    diagnostic_print_assignment("output_value");
    SerialUSB.print(static_cast<int>(device.output_value));
    SerialUSB.print("\r\n  ");
    diagnostic_print_assignment("digital_inputs");
    SerialUSB.print(static_cast<unsigned long>(device.digital_inputs));
    SerialUSB.print("\r\n");
}

const X2XModuleDescriptor g_x2x_catalog[] =
{
    {
        X2X_DEVICE_LCP,
        "LCP2116",
        sizeof(X2XLcp2116),
        X2XLcp2116::SP_COUNT,
        X2XLcp2116::ME_COUNT,
        X2XLcp2116::TF_COUNT,
        0U,
        construct_device<X2XLcp2116, X2X_DEVICE_LCP>,
        0,
        0
    },
    {
        X2X_DEVICE_LDO1118,
        "LDO1118",
        sizeof(X2XLdo1118),
        X2XLdo1118::SP_COUNT,
        X2XLdo1118::ME_COUNT,
        X2XLdo1118::TF_COUNT,
        1U,
        construct_device<X2XLdo1118, X2X_DEVICE_LDO1118>,
        x2x_module_poll_ldo1118,
        print_ldo1118
    },
    {
        X2X_DEVICE_LAI1118,
        "LAI1118",
        sizeof(X2XLai1118),
        X2XLai1118::SP_COUNT,
        X2XLai1118::ME_COUNT,
        X2XLai1118::TF_COUNT,
        1U,
        construct_device<X2XLai1118, X2X_DEVICE_LAI1118>,
        x2x_module_poll_lai1118,
        print_digital_inputs_and_floats<X2XLai1118>
    },
    {
        X2X_DEVICE_LDI1118,
        "LDI1118",
        sizeof(X2XLdi1118),
        X2XLdi1118::SP_COUNT,
        X2XLdi1118::ME_COUNT,
        X2XLdi1118::TF_COUNT,
        1U,
        construct_device<X2XLdi1118, X2X_DEVICE_LDI1118>,
        x2x_module_poll_ldi1118,
        print_digital_inputs<X2XLdi1118>
    },
    {
        X2X_DEVICE_LCT1114,
        "LCT1114",
        sizeof(X2XLct1114),
        X2XLct1114::SP_COUNT,
        X2XLct1114::ME_COUNT,
        X2XLct1114::TF_COUNT,
        1U,
        construct_device<X2XLct1114, X2X_DEVICE_LCT1114>,
        x2x_module_poll_lct1114,
        print_digital_inputs_and_floats<X2XLct1114>
    },
    {
        X2X_DEVICE_LDI1116,
        "LDI1116",
        sizeof(X2XLdi1116),
        X2XLdi1116::SP_COUNT,
        X2XLdi1116::ME_COUNT,
        X2XLdi1116::TF_COUNT,
        1U,
        construct_device<X2XLdi1116, X2X_DEVICE_LDI1116>,
        x2x_module_poll_ldi1116,
        print_digital_inputs<X2XLdi1116>
    },
    {
        X2X_DEVICE_LCT1114_2,
        "LCT1114_2",
        sizeof(X2XLct1114_2),
        X2XLct1114_2::SP_COUNT,
        X2XLct1114_2::ME_COUNT,
        X2XLct1114_2::TF_COUNT,
        1U,
        construct_device<X2XLct1114_2, X2X_DEVICE_LCT1114_2>,
        x2x_module_poll_lct1114_2,
        print_digital_inputs_and_floats<X2XLct1114_2>
    }
};
}

const X2XModuleDescriptor* x2x_catalog_find(X2XDeviceType type)
{
    for (size_t index = 0U; index < x2x_catalog_size(); ++index)
    {
        if (g_x2x_catalog[index].type == type)
        {
            return &g_x2x_catalog[index];
        }
    }

    return 0;
}

const X2XModuleDescriptor* x2x_catalog_find_by_id(uint16_t type_id)
{
    for (size_t index = 0U; index < x2x_catalog_size(); ++index)
    {
        if (static_cast<uint16_t>(g_x2x_catalog[index].type) == type_id)
        {
            return &g_x2x_catalog[index];
        }
    }

    return 0;
}

size_t x2x_catalog_size(void)
{
    return sizeof(g_x2x_catalog) / sizeof(g_x2x_catalog[0]);
}

const X2XModuleDescriptor* x2x_catalog_at(size_t index)
{
    if (index >= x2x_catalog_size())
    {
        return 0;
    }

    return &g_x2x_catalog[index];
}
