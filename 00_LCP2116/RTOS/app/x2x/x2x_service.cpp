/**
 * @file x2x_service.cpp
 * @brief Реализация конфигурации и циклического опроса X2X.
 */

#include "x2x_service.hpp"
#include "x2x_catalog.hpp"
#include "x2x_module.hpp"
#include "modules/x2x_module_common.hpp"
#include "../../board/lcp_x2x_port.hpp"
#include "../../libs/lcp_sd_storage/lcp_sd_storage.hpp"
#include "../../platform/platform.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace
{
static const uint32_t X2X_CONFIG_RETRY_PERIOD_MS = 1000U;
static const uint32_t X2X_MODULE_CYCLE_GAP_MS = 50U;
static const uint32_t X2X_MODBUS_INTERFRAME_GAP_MS = 5U;

X2XConfig g_active_config;
X2XConfigError g_last_config_error;
X2XConfigResult g_last_config_result = X2X_CONFIG_STORAGE_NOT_READY;
X2XRegistryResult g_last_registry_result = X2X_REGISTRY_INVALID_CONFIG;
ModbusRtuMaster g_modbus_master;
X2XModuleRuntime g_module_runtime[X2X_MAX_MODULES + 1U];
uint16_t g_register_buffer[MODBUS_RTU_MASTER_MAX_READ_REGISTERS];
X2XWaveformBuffer g_waveform;
uint8_t g_config_loaded = 0U;
uint8_t g_paused = 0U;
uint8_t g_pause_requested = 0U;
uint8_t g_reload_requested = 0U;
uint8_t g_current_slave = 0U;
uint8_t g_cycle_in_progress = 0U;
uint32_t g_next_cycle_ms = 0U;
uint32_t g_last_reload_attempt_ms = 0U;

void reset_runtime_state(void)
{
    modbus_rtu_master_reset(g_modbus_master);

    for (uint8_t index = 0U; index <= X2X_MAX_MODULES; ++index)
    {
        x2x_module_runtime_reset(g_module_runtime[index]);
    }

    memset(g_register_buffer, 0, sizeof(g_register_buffer));
    memset(&g_waveform, 0, sizeof(g_waveform));
    g_cycle_in_progress = 0U;
    g_current_slave =
        (x2x_registry_module_count() > 0U) ? 1U : 0U;
    g_next_cycle_ms = millis();
}

X2XConfigResult apply_config_now(void)
{
    X2XConfig candidate_config;
    X2XConfigError candidate_error;

    g_last_reload_attempt_ms = millis();
    g_last_config_result = x2x_config_load(X2X_CONFIG_FILE_NAME,
                                           candidate_config,
                                           candidate_error);
    g_last_config_error = candidate_error;
    g_reload_requested = 0U;

    if (g_last_config_result != X2X_CONFIG_OK)
    {
        return g_last_config_result;
    }

    g_last_registry_result =
        x2x_registry_build(candidate_config, X2X_DEFAULT_ASDU);

    if (g_last_registry_result != X2X_REGISTRY_OK)
    {
        g_config_loaded = 0U;
        reset_runtime_state();
        return g_last_config_result;
    }

    g_active_config = candidate_config;
    g_config_loaded = 1U;
    reset_runtime_state();
    return X2X_CONFIG_OK;
}

void complete_pause_if_safe(void)
{
    if ((g_pause_requested == 0U) ||
        (g_cycle_in_progress != 0U) ||
        (modbus_rtu_master_busy(g_modbus_master) != 0U))
    {
        return;
    }

    g_pause_requested = 0U;
    g_paused = 1U;
    modbus_rtu_master_reset(g_modbus_master);
}

void advance_slave(uint32_t now_ms)
{
    const uint8_t module_count = x2x_registry_module_count();

    if (module_count == 0U)
    {
        g_current_slave = 0U;
    }
    else
    {
        ++g_current_slave;

        if ((g_current_slave == 0U) || (g_current_slave > module_count))
        {
            g_current_slave = 1U;
        }
    }

    g_cycle_in_progress = 0U;
    g_next_cycle_ms = now_ms + X2X_MODULE_CYCLE_GAP_MS;
}

void poll_active_module(uint32_t now_ms)
{
    X2XDeviceHeader* device =
        x2x_registry_get_by_slave(g_current_slave);

    if (device == 0)
    {
        advance_slave(now_ms);
        return;
    }

    const X2XModuleDescriptor* descriptor =
        x2x_catalog_find(device->type);

    if ((descriptor == 0) || (descriptor->poll == 0))
    {
        x2x_module_mark_failure(*device,
                                MODBUS_RTU_RESULT_INVALID_ARGUMENT,
                                0U);
        advance_slave(now_ms);
        return;
    }

    X2XModuleContext context =
    {
        &g_modbus_master,
        &g_module_runtime[g_current_slave],
        &g_waveform,
        g_register_buffer,
        MODBUS_RTU_MASTER_MAX_READ_REGISTERS,
        now_ms
    };

    const X2XModulePollResult result =
        descriptor->poll(device, context);

    if (result == X2X_MODULE_POLL_CYCLE_COMPLETE)
    {
        advance_slave(now_ms);
    }
}
}

void x2x_service_init(void)
{
    x2x_config_reset(g_active_config, g_last_config_error);
    x2x_registry_reset();
    lcp_x2x_port_init();
    modbus_rtu_master_init(g_modbus_master,
                           lcp_x2x_port_transport(),
                           X2X_MODBUS_INTERFRAME_GAP_MS);

    g_config_loaded = 0U;
    g_paused = 0U;
    g_pause_requested = 0U;
    g_reload_requested = 0U;
    g_current_slave = 0U;
    g_cycle_in_progress = 0U;
    g_next_cycle_ms = 0U;
    g_last_reload_attempt_ms = millis() - X2X_CONFIG_RETRY_PERIOD_MS;
    g_last_config_result = X2X_CONFIG_STORAGE_NOT_READY;
    g_last_registry_result = X2X_REGISTRY_INVALID_CONFIG;
    memset(&g_waveform, 0, sizeof(g_waveform));

    if (lcp_sd_storage_ready() != 0U)
    {
        (void)apply_config_now();
    }
}

void x2x_service_poll(void)
{
    const uint32_t now_ms = millis();

    if ((g_config_loaded == 0U) &&
        (g_reload_requested == 0U) &&
        (lcp_sd_storage_ready() != 0U) &&
        ((uint32_t)(now_ms - g_last_reload_attempt_ms) >=
         X2X_CONFIG_RETRY_PERIOD_MS))
    {
        (void)apply_config_now();
    }

    if (g_cycle_in_progress != 0U)
    {
        modbus_rtu_master_poll(g_modbus_master);
        poll_active_module(now_ms);
    }

    if ((g_reload_requested != 0U) &&
        (g_cycle_in_progress == 0U) &&
        (modbus_rtu_master_busy(g_modbus_master) == 0U))
    {
        (void)apply_config_now();
        complete_pause_if_safe();
        return;
    }

    complete_pause_if_safe();

    if ((g_config_loaded == 0U) ||
        (g_paused != 0U) ||
        (g_pause_requested != 0U) ||
        (x2x_registry_module_count() == 0U))
    {
        return;
    }

    if (g_cycle_in_progress == 0U)
    {
        if (static_cast<int32_t>(now_ms - g_next_cycle_ms) < 0)
        {
            return;
        }

        g_cycle_in_progress = 1U;
        poll_active_module(now_ms);
    }
}

X2XConfigResult x2x_service_reload_config(void)
{
    if ((g_cycle_in_progress != 0U) ||
        (modbus_rtu_master_busy(g_modbus_master) != 0U))
    {
        g_reload_requested = 1U;
        return X2X_CONFIG_APPLY_PENDING;
    }

    return apply_config_now();
}

void x2x_service_pause(void)
{
    if (g_paused != 0U)
    {
        return;
    }

    g_pause_requested = 1U;
    complete_pause_if_safe();
}

void x2x_service_resume(void)
{
    g_pause_requested = 0U;
    g_paused = 0U;
    g_next_cycle_ms = millis();
}

uint8_t x2x_service_paused(void)
{
    return g_paused;
}

uint8_t x2x_service_pause_pending(void)
{
    return g_pause_requested;
}

uint8_t x2x_service_reload_pending(void)
{
    return g_reload_requested;
}

uint8_t x2x_service_config_loaded(void)
{
    return g_config_loaded;
}

uint8_t x2x_service_owns_port(void)
{
    return ((g_config_loaded != 0U) &&
            (x2x_registry_module_count() > 0U)) ? 1U : 0U;
}

X2XConfigResult x2x_service_last_config_result(void)
{
    return g_last_config_result;
}

const X2XConfigError& x2x_service_last_config_error(void)
{
    return g_last_config_error;
}

X2XRegistryResult x2x_service_last_registry_result(void)
{
    return g_last_registry_result;
}

uint8_t x2x_service_current_slave(void)
{
    return g_current_slave;
}

ModbusRtuResult x2x_service_modbus_result(void)
{
    return modbus_rtu_master_result(g_modbus_master);
}

const X2XWaveformBuffer& x2x_service_waveform(void)
{
    return g_waveform;
}

uint8_t x2x_service_set_ldo_output(uint8_t slave_address, int16_t value)
{
    X2XDeviceHeader* device =
        x2x_registry_get_by_slave(slave_address);

    if ((device == 0) ||
        (device->type != X2X_DEVICE_LDO1118))
    {
        return 0U;
    }

    X2XLdo1118* ldo = static_cast<X2XLdo1118*>(device);
    ldo->output_value = value;
    return 1U;
}
