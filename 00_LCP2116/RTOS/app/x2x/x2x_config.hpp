/**
 * @file x2x_config.hpp
 * @brief Чтение и проверка файла X2X.TXT с microSD.
 */

#pragma once

#include <stdint.h>

#include "x2x_types.hpp"

/** @brief Результат загрузки или применения конфигурации X2X. */
enum X2XConfigResult : uint8_t
{
    X2X_CONFIG_OK = 0U,
    X2X_CONFIG_STORAGE_NOT_READY = 1U,
    X2X_CONFIG_FILE_NOT_FOUND = 2U,
    X2X_CONFIG_FILE_OPEN_FAILED = 3U,
    X2X_CONFIG_EMPTY_FILE = 4U,
    X2X_CONFIG_INVALID_COUNT = 5U,
    X2X_CONFIG_LINE_TOO_LONG = 6U,
    X2X_CONFIG_INVALID_VALUE = 7U,
    X2X_CONFIG_INVALID_DEVICE_ID = 8U,
    X2X_CONFIG_INCOMPLETE_FILE = 9U,
    X2X_CONFIG_EXTRA_DATA = 10U,
    X2X_CONFIG_APPLY_PENDING = 11U,
    X2X_CONFIG_REGISTRY_BUILD_FAILED = 12U
};

/** @brief Проверенная конфигурация экземпляров модулей X2X. */
struct X2XConfig
{
    uint8_t module_count;
    X2XDeviceType module_types[X2X_MAX_MODULES];
};

/** @brief Дополнительная информация об ошибке X2X.TXT. */
struct X2XConfigError
{
    X2XConfigResult result;
    uint16_t physical_line;
    int32_t value;
};

/** @brief Обнуляет конфигурацию и структуру ошибки. */
void x2x_config_reset(X2XConfig& config, X2XConfigError& error);

/**
 * @brief Загружает и полностью проверяет X2X.TXT.
 *
 * Формат совместим со старой прошивкой:
 * первая содержательная строка — количество модулей, последующие строки —
 * числовые ID. Поддерживаются пустые строки, комментарии // и необязательная
 * завершающая строка Fin.
 *
 * @param file_name Имя файла в корневом каталоге microSD.
 * @param config Результирующая конфигурация.
 * @param error Подробности ошибки.
 *
 * @return Код результата.
 */
X2XConfigResult x2x_config_load(const char* file_name,
                                X2XConfig& config,
                                X2XConfigError& error);

/** @brief Возвращает текстовое описание результата загрузки. */
const char* x2x_config_result_text(X2XConfigResult result);
