/**
 * @file x2x_registry.cpp
 * @brief Реализация статического реестра устройств X2X.
 */

#include "x2x_registry.hpp"
#include "x2x_catalog.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace
{
static const size_t X2X_DEVICE_STORAGE_SIZE = sizeof(X2XLct1114_2);
static const uint8_t X2X_REGISTRY_CAPACITY = X2X_MAX_MODULES + 1U;

struct X2XRegistryStorage
{
    alignas(8) uint8_t bytes[X2X_DEVICE_STORAGE_SIZE];
};

X2XRegistryStorage g_device_storage[X2X_REGISTRY_CAPACITY];
X2XDeviceHeader* g_devices[X2X_REGISTRY_CAPACITY];
uint8_t g_module_count = 0U;

static_assert(sizeof(X2XLcp2116) <= X2X_DEVICE_STORAGE_SIZE,
              "LCP storage size mismatch");
static_assert(sizeof(X2XLdo1118) <= X2X_DEVICE_STORAGE_SIZE,
              "LDO storage size mismatch");
static_assert(sizeof(X2XLai1118) <= X2X_DEVICE_STORAGE_SIZE,
              "LAI storage size mismatch");
static_assert(sizeof(X2XLdi1118) <= X2X_DEVICE_STORAGE_SIZE,
              "LDI8 storage size mismatch");
static_assert(sizeof(X2XLdi1116) <= X2X_DEVICE_STORAGE_SIZE,
              "LDI16 storage size mismatch");
static_assert(sizeof(X2XLct1114) <= X2X_DEVICE_STORAGE_SIZE,
              "LCT storage size mismatch");

X2XRegistryResult construct_slot(uint8_t index,
                                 X2XDeviceType type,
                                 uint8_t slave_address,
                                 uint16_t asdu)
{
    const X2XModuleDescriptor* descriptor = x2x_catalog_find(type);

    if (descriptor == 0)
    {
        return X2X_REGISTRY_UNKNOWN_TYPE;
    }

    if (descriptor->object_size > X2X_DEVICE_STORAGE_SIZE)
    {
        return X2X_REGISTRY_STORAGE_TOO_SMALL;
    }

    X2XDeviceHeader* device = descriptor->construct(
        g_device_storage[index].bytes,
        sizeof(g_device_storage[index].bytes),
        slave_address,
        asdu);

    if (device == 0)
    {
        return X2X_REGISTRY_CONSTRUCTION_FAILED;
    }

    g_devices[index] = device;
    return X2X_REGISTRY_OK;
}
}

void x2x_registry_reset(void)
{
    memset(g_device_storage, 0, sizeof(g_device_storage));
    memset(g_devices, 0, sizeof(g_devices));
    g_module_count = 0U;
}

X2XRegistryResult x2x_registry_build(const X2XConfig& config,
                                     uint16_t default_asdu)
{
    if (config.module_count > X2X_MAX_MODULES)
    {
        return X2X_REGISTRY_INVALID_CONFIG;
    }

    x2x_registry_reset();

    X2XRegistryResult result = construct_slot(0U,
                                              X2X_DEVICE_LCP,
                                              0U,
                                              default_asdu);

    if (result != X2X_REGISTRY_OK)
    {
        x2x_registry_reset();
        return result;
    }

    for (uint8_t module_index = 0U;
         module_index < config.module_count;
         ++module_index)
    {
        const uint8_t slave_address = module_index + 1U;
        const X2XModuleDescriptor* descriptor =
            x2x_catalog_find(config.module_types[module_index]);

        if ((descriptor == 0) || (descriptor->allowed_in_x2x_config == 0U))
        {
            x2x_registry_reset();
            return X2X_REGISTRY_UNKNOWN_TYPE;
        }

        result = construct_slot(slave_address,
                                descriptor->type,
                                slave_address,
                                default_asdu);

        if (result != X2X_REGISTRY_OK)
        {
            x2x_registry_reset();
            return result;
        }
    }

    g_module_count = config.module_count;
    return X2X_REGISTRY_OK;
}

uint8_t x2x_registry_module_count(void)
{
    return g_module_count;
}

uint8_t x2x_registry_total_count(void)
{
    return (g_devices[0] != 0) ? static_cast<uint8_t>(g_module_count + 1U) : 0U;
}

X2XDeviceHeader* x2x_registry_controller(void)
{
    return g_devices[0];
}

X2XDeviceHeader* x2x_registry_get_by_slave(uint8_t slave_address)
{
    if ((slave_address == 0U) || (slave_address > g_module_count))
    {
        return 0;
    }

    return g_devices[slave_address];
}

X2XDeviceHeader* x2x_registry_get_by_index(uint8_t index)
{
    if (index >= x2x_registry_total_count())
    {
        return 0;
    }

    return g_devices[index];
}

const char* x2x_registry_result_text(X2XRegistryResult result)
{
    switch (result)
    {
        case X2X_REGISTRY_OK:
            return "ok";

        case X2X_REGISTRY_INVALID_CONFIG:
            return "invalid config";

        case X2X_REGISTRY_UNKNOWN_TYPE:
            return "unknown type";

        case X2X_REGISTRY_STORAGE_TOO_SMALL:
            return "device storage too small";

        case X2X_REGISTRY_CONSTRUCTION_FAILED:
            return "device construction failed";

        default:
            return "unknown";
    }
}
