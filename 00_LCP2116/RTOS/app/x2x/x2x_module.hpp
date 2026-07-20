/**
 * @file x2x_module.hpp
 * @brief Общий интерфейс драйверов модулей X2X.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "x2x_types.hpp"
#include "../../protocol/modbus_rtu/modbus_rtu_master.hpp"

/**
 * @brief Размер одного статического слота реестра устройств X2X.
 *
 * Каждый тип, зарегистрированный в каталоге, проверяется static_assert в
 * construct_device(). Если структура нового модуля больше этого значения,
 * сборка завершится ошибкой вместо скрытого переполнения реестра.
 */
static const size_t X2X_DEVICE_SLOT_BYTES = 256U;

/** @brief Результат одного вызова неблокирующего драйвера модуля. */
enum X2XModulePollResult : uint8_t
{
    X2X_MODULE_POLL_IN_PROGRESS = 0U,
    X2X_MODULE_POLL_CYCLE_COMPLETE = 1U
};

/**
 * @brief Состояние драйвера отдельного экземпляра модуля.
 *
 * Поля используются для многошаговых операций LCT и для ожидания
 * неблокирующей Modbus RTU транзакции.
 */
struct X2XModuleRuntime
{
    uint8_t state;
    uint8_t waveform_phase;
    uint16_t transfer_offset;
    uint32_t next_waveform_check_ms;
};

/**
 * @brief Ресурсы, передаваемые драйверу текущего модуля.
 */
struct X2XModuleContext
{
    ModbusRtuMaster* master;
    X2XModuleRuntime* runtime;
    X2XWaveformBuffer* waveform;
    uint16_t* register_buffer;
    size_t register_capacity;
    uint32_t now_ms;
};

/** Тип функции неблокирующего опроса конкретного типа модуля. */
typedef X2XModulePollResult (*X2XModulePollFunction)(X2XDeviceHeader* device,
                                                     X2XModuleContext& context);

/**
 * @brief Тип функции вывода специфических данных модуля в service console.
 *
 * Общий статус связи печатается диагностикой X2X. Callback выводит только
 * данные конкретного типа: дискретные входы, float-поля или выходное значение.
 */
typedef void (*X2XModulePrintFunction)(const X2XDeviceHeader& device);
