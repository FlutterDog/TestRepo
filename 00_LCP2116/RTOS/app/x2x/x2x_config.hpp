/**
 * @file x2x_config.hpp
 * @brief Строгое чтение и проверка X2X.TXT с microSD.
 */

#pragma once

#include <stdint.h>

#include "x2x_types.hpp"

/** @brief Результат загрузки или применения конфигурации X2X. */
enum X2XConfigResult : uint8_t
{
    X2X_CONFIG_OK = 0U,                    /**< Конфигурация проверена и применима. */
    X2X_CONFIG_STORAGE_NOT_READY = 1U,     /**< microSD ещё не доступна. */
    X2X_CONFIG_FILE_NOT_FOUND = 2U,        /**< X2X.TXT отсутствует. */
    X2X_CONFIG_FILE_OPEN_FAILED = 3U,      /**< Файл найден, но не открыт. */
    X2X_CONFIG_EMPTY_FILE = 4U,            /**< Не найдено количество модулей. */
    X2X_CONFIG_INVALID_COUNT = 5U,         /**< Количество вне диапазона 0..X2X_MAX_MODULES. */
    X2X_CONFIG_LINE_TOO_LONG = 6U,         /**< Физическая строка превышает предел парсера. */
    X2X_CONFIG_INVALID_VALUE = 7U,         /**< Строка не является допустимым целым числом. */
    X2X_CONFIG_INVALID_DEVICE_ID = 8U,     /**< ID отсутствует или запрещён каталогом. */
    X2X_CONFIG_INCOMPLETE_FILE = 9U,       /**< Типов меньше заявленного количества. */
    X2X_CONFIG_EXTRA_DATA = 10U,           /**< После ожидаемых типов найдены лишние данные. */
    X2X_CONFIG_APPLY_PENDING = 11U,        /**< Применение отложено до безопасной точки. */
    X2X_CONFIG_REGISTRY_BUILD_FAILED = 12U /**< Каталог не смог построить runtime-реестр. */
};

/** @brief Полностью проверенная последовательность экземпляров модулей X2X. */
struct X2XConfig
{
    uint8_t module_count; /**< Количество внешних модулей, 0..X2X_MAX_MODULES. */
    X2XDeviceType module_types[X2X_MAX_MODULES]; /**< Типы в порядке slave 1..N. */
};

/** @brief Дополнительная информация о последней ошибке X2X.TXT. */
struct X2XConfigError
{
    X2XConfigResult result; /**< Основной код ошибки. */
    uint16_t physical_line; /**< Номер физической строки файла, начиная с 1. */
    int32_t value;          /**< Проблемное число либо дополнительный код. */
};

/**
 * @brief Обнуляет конфигурацию и структуру ошибки.
 * @param[out] config Конфигурация.
 * @param[out] error Диагностика ошибки.
 */
void x2x_config_reset(X2XConfig& config, X2XConfigError& error);

/**
 * @brief Загружает и полностью проверяет X2X.TXT.
 *
 * Первая содержательная строка — количество модулей, следующие строки —
 * числовые ID. Поддерживаются пустые строки, комментарии `//` и необязательная
 * завершающая строка `Fin`. Порядок ID задаёт slave address.
 *
 * @param[in] file_name Каноническое имя в корневом каталоге microSD.
 * @param[out] config Результирующая конфигурация, изменяемая только при успехе.
 * @param[out] error Подробности результата.
 * @return Код результата.
 */
X2XConfigResult x2x_config_load(const char* file_name,
                                X2XConfig& config,
                                X2XConfigError& error);

/** @return Указатель на строковое описание результата. */
const char* x2x_config_result_text(X2XConfigResult result);
