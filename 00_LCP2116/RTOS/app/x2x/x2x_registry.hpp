/**
 * @file x2x_registry.hpp
 * @brief Статический реестр экземпляров устройств X2X.
 */

#pragma once

#include <stdint.h>

#include "x2x_config.hpp"
#include "x2x_types.hpp"

/** @brief Результат построения статического реестра X2X. */
enum X2XRegistryResult : uint8_t
{
    X2X_REGISTRY_OK = 0U,                    /**< Реестр полностью построен. */
    X2X_REGISTRY_INVALID_CONFIG = 1U,        /**< Конфигурация нарушает ограничения X2X. */
    X2X_REGISTRY_UNKNOWN_TYPE = 2U,          /**< Тип отсутствует в каталоге. */
    X2X_REGISTRY_STORAGE_TOO_SMALL = 3U,     /**< Runtime-объект не помещается в фиксированный слот. */
    X2X_REGISTRY_CONSTRUCTION_FAILED = 4U   /**< Placement-конструктор вернул null. */
};

/** @brief Сбрасывает реестр и все runtime-данные устройств. */
void x2x_registry_reset(void);

/**
 * @brief Строит реестр из полностью проверенной конфигурации.
 *
 * Элемент 0 всегда содержит центральный LCP. Элементы 1..N соответствуют
 * адресам X2X slave 1..N, поэтому отдельная таблица адресов не требуется.
 * Функция не выделяет динамическую память.
 *
 * @param[in] config Проверенная последовательность типов внешних модулей.
 * @param[in] default_asdu ASDU, назначаемый каждому создаваемому объекту.
 * @return Код результата построения.
 */
X2XRegistryResult x2x_registry_build(const X2XConfig& config,
                                     uint16_t default_asdu);

/** @return Количество внешних модулей без центрального LCP. */
uint8_t x2x_registry_module_count(void);

/** @return Общее количество объектов, включая центральный LCP. */
uint8_t x2x_registry_total_count(void);

/** @return Центральный объект LCP либо null до успешного построения. */
X2XDeviceHeader* x2x_registry_controller(void);

/**
 * @brief Возвращает модуль по физическому адресу slave.
 * @param[in] slave_address Адрес 1..N.
 * @return Заголовок экземпляра либо null.
 */
X2XDeviceHeader* x2x_registry_get_by_slave(uint8_t slave_address);

/**
 * @brief Возвращает устройство по внутреннему индексу.
 * @param[in] index 0 для LCP, 1..N для внешних модулей.
 * @return Заголовок экземпляра либо null.
 */
X2XDeviceHeader* x2x_registry_get_by_index(uint8_t index);

/** @return Указатель на строковое описание результата построения. */
const char* x2x_registry_result_text(X2XRegistryResult result);
