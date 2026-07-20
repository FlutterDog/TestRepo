/**
 * @file field_serial_config.hpp
 * @brief Чтение скорости и чётности S1..S4 из TXT-файлов microSD.
 */

#pragma once

#include <stdint.h>

#include "../../board/lcp_field_ports.hpp"

/** @brief Результат чтения одного файла настроек FieldSensor. */
enum FieldSerialConfigResult : uint8_t
{
    FIELD_SERIAL_CONFIG_NOT_ATTEMPTED = 0U,
    FIELD_SERIAL_CONFIG_OK = 1U,
    FIELD_SERIAL_CONFIG_CARD_NOT_READY = 2U,
    FIELD_SERIAL_CONFIG_FILE_NOT_FOUND = 3U,
    FIELD_SERIAL_CONFIG_FILE_OPEN_FAILED = 4U,
    FIELD_SERIAL_CONFIG_EMPTY_FILE = 5U,
    FIELD_SERIAL_CONFIG_INVALID_COUNT = 6U,
    FIELD_SERIAL_CONFIG_INVALID_VALUE = 7U,
    FIELD_SERIAL_CONFIG_INCOMPLETE_FILE = 8U,
    FIELD_SERIAL_CONFIG_LINE_TOO_LONG = 9U
};

/** @brief Отчёт о применении BAUD.TXT и PARITY.TXT. */
struct FieldSerialConfigReport
{
    FieldSerialConfigResult baud_result;
    FieldSerialConfigResult parity_result;
    uint8_t loaded_from_sd;
};

/**
 * @brief Заполняет четыре конфигурации значениями 9600 8N1.
 */
void field_serial_config_set_defaults(
    LcpFieldPortConfig configs[LCP_FIELD_PORT_COUNT]);

/**
 * @brief Загружает BAUD.TXT и PARITY.TXT поверх текущих значений.
 *
 * Формат обоих файлов совместим с числовыми конфигурационными файлами старой
 * прошивки: первая значимая строка содержит количество значений (4), далее
 * идут значения для S1, S2, S3 и S4.
 *
 * PARITY.TXT принимает N/O/E, NONE/ODD/EVEN, 0/1/2 и ASCII-коды 78/79/69.
 */
void field_serial_config_load(
    LcpFieldPortConfig configs[LCP_FIELD_PORT_COUNT],
    FieldSerialConfigReport* report);

/** @brief Возвращает текст результата загрузки. */
const char* field_serial_config_result_text(FieldSerialConfigResult result);
