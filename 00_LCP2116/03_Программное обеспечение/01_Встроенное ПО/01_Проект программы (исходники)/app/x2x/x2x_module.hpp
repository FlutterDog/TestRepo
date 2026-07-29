/**
 * @file x2x_module.hpp
 * @brief Общий интерфейс неблокирующих драйверов модулей X2X.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "x2x_types.hpp"
#include "../../protocol/modbus_rtu/modbus_rtu_master.hpp"

/**
 * @brief Размер одного статического слота реестра устройств X2X.
 *
 * Каждый тип каталога проверяется `static_assert` в функции-конструкторе.
 * Если структура нового модуля больше слота, сборка завершается ошибкой вместо
 * скрытого переполнения статического реестра.
 */
static const size_t X2X_DEVICE_SLOT_BYTES = 256U;

/** @brief Результат одного вызова неблокирующего драйвера модуля. */
enum X2XModulePollResult : uint8_t
{
    X2X_MODULE_POLL_IN_PROGRESS = 0U,   /**< Цикл модуля ещё не завершён. */
    X2X_MODULE_POLL_CYCLE_COMPLETE = 1U /**< Можно переходить к следующему slave. */
};

/**
 * @brief Состояние драйвера отдельного экземпляра модуля.
 *
 * Runtime хранится отдельно от данных устройства, чтобы общая структура
 * X2XDeviceHeader отражала состояние связи, а многошаговая машина драйвера не
 * загрязняла пользовательские данные конкретного типа.
 */
struct X2XModuleRuntime
{
    uint8_t state;                  /**< Текущее состояние драйвера типа. */
    uint8_t waveform_phase;         /**< Текущая фаза A/B/C при чтении LCT. */
    uint16_t transfer_offset;       /**< Смещение очередного блока осциллограммы. */
    uint32_t next_waveform_check_ms;/**< Срок следующей проверки готовности LCT. */
};

/** @brief Общие ресурсы одного вызова драйвера текущего модуля. */
struct X2XModuleContext
{
    ModbusRtuMaster* master;        /**< Единственный master внутренней шины X2X. */
    X2XModuleRuntime* runtime;      /**< Runtime текущего slave. */
    X2XWaveformBuffer* waveform;   /**< Общий буфер последней осциллограммы LCT. */
    uint16_t* register_buffer;     /**< Общий временный буфер регистров. */
    size_t register_capacity;      /**< Размер register_buffer в регистрах. */
    uint32_t now_ms;               /**< Один согласованный снимок millis(). */
};

/**
 * @brief Callback неблокирующего опроса конкретного типа модуля.
 * @param[in,out] device Заголовок и данные конкретного экземпляра.
 * @param[in,out] context Общие ресурсы X2X-сервиса.
 * @return Состояние цикла из X2XModulePollResult.
 */
typedef X2XModulePollResult (*X2XModulePollFunction)(
    X2XDeviceHeader* device,
    X2XModuleContext& context);

/**
 * @brief Callback вывода специфических данных модуля в service console.
 *
 * Общий статус связи печатается диагностикой X2X. Callback выводит только
 * данные типа: дискретные входы, float-поля или выходное значение.
 *
 * @param[in] device Заголовок, фактически являющийся началом структуры типа.
 */
typedef void (*X2XModulePrintFunction)(const X2XDeviceHeader& device);
