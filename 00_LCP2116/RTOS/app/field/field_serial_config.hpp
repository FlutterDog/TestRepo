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

/** @brief Отчёт о применении baud.TXT и Parity.TXT. */
struct FieldSerialConfigReport
{
    FieldSerialConfigResult baud_result;
    FieldSerialConfigResult parity_result;
    uint8_t loaded_from_sd;
};

/** @brief Заполняет четыре конфигурации значениями 9600 8N1. */
void field_serial_config_set_defaults(
    LcpFieldPortConfig configs[LCP_FIELD_PORT_COUNT]);

/**
 * @brief Загружает baud.TXT и Parity.TXT поверх текущих значений.
 *
 * Первая значимая строка каждого файла содержит количество значений 4. Далее
 * идут значения для S1, S2, S3 и S4. Завершающая строка fin допускается и
 * игнорируется после чтения четырёх значений.
 *
 * baud.TXT содержит фактические значения скорости, например 1200 или 9600.
 * Parity.TXT принимает только штатные коды: 0=None, 1=Odd, 2=Even.
 */
void field_serial_config_load(
    LcpFieldPortConfig configs[LCP_FIELD_PORT_COUNT],
    FieldSerialConfigReport* report);

/** @brief Возвращает текст результата загрузки. */
const char* field_serial_config_result_text(FieldSerialConfigResult result);
