/**
 * @file x2x_registry.hpp
 * @brief Статический реестр экземпляров устройств X2X.
 */

#pragma once

#include <stdint.h>

#include "x2x_config.hpp"
#include "x2x_types.hpp"

/** @brief Результат построения реестра X2X. */
enum X2XRegistryResult : uint8_t
{
    X2X_REGISTRY_OK = 0U,
    X2X_REGISTRY_INVALID_CONFIG = 1U,
    X2X_REGISTRY_UNKNOWN_TYPE = 2U,
    X2X_REGISTRY_STORAGE_TOO_SMALL = 3U,
    X2X_REGISTRY_CONSTRUCTION_FAILED = 4U
};

/** @brief Сбрасывает реестр и все runtime-данные устройств. */
void x2x_registry_reset(void);

/**
 * @brief Строит реестр из полностью проверенной конфигурации.
 *
 * Элемент 0 всегда содержит LCP. Элементы 1..N соответствуют адресам
 * X2X slave 1..N, поэтому отдельная таблица адресов не требуется.
 */
X2XRegistryResult x2x_registry_build(const X2XConfig& config,
                                     uint16_t default_asdu);

/** @brief Возвращает количество внешних модулей, без LCP. */
uint8_t x2x_registry_module_count(void);

/** @brief Возвращает общее количество устройств, включая LCP. */
uint8_t x2x_registry_total_count(void);

/** @brief Возвращает центральный модуль LCP или null. */
X2XDeviceHeader* x2x_registry_controller(void);

/** @brief Возвращает модуль по адресу slave 1..N или null. */
X2XDeviceHeader* x2x_registry_get_by_slave(uint8_t slave_address);

/** @brief Возвращает устройство по внутреннему индексу 0..N или null. */
X2XDeviceHeader* x2x_registry_get_by_index(uint8_t index);

/** @brief Возвращает текстовое описание результата построения. */
const char* x2x_registry_result_text(X2XRegistryResult result);
