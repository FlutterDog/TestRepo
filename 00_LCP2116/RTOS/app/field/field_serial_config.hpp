/**
 * @file field_serial_config.hpp
 * @brief Чтение скорости и чётности S1..S4 из microSD.
 */

#pragma once

#include <stdint.h>

#include "../diagnostics/sd_config.hpp"
#include "../../board/lcp_field_ports.hpp"

/**
 * @brief Отчёт о применении канонических файлов baud.TXT и Parity.TXT.
 */
struct FieldSerialConfigReport
{
    SdConfigResult baud_result;   /**< Результат чтения baud.TXT. */
    SdConfigResult parity_result; /**< Результат чтения Parity.TXT. */
    uint8_t loaded_from_sd;       /**< Ненулевое значение, если применён хотя бы один файл. */
};

/**
 * @brief Заполняет четыре конфигурации безопасными значениями 9600 8N1.
 * @param[out] configs Массив конфигураций S1, S2, S3 и S4.
 */
void field_serial_config_set_defaults(
    LcpFieldPortConfig configs[LCP_FIELD_PORT_COUNT]);

/**
 * @brief Загружает baud.TXT и Parity.TXT поверх текущих значений.
 *
 * Первая значимая строка каждого файла содержит количество значений `4`.
 * Далее идут параметры физических портов S1, S2, S3 и S4. Завершающая строка
 * `fin` и комментарии после `//` поддерживаются общим SD-парсером.
 *
 * `baud.TXT` содержит реальные значения скорости. `Parity.TXT` принимает
 * только штатные коды: `0=None`, `1=Odd`, `2=Even`. Файлы применяются
 * независимо: ошибка одного файла не отменяет корректные данные другого.
 *
 * @param[in,out] configs Текущие параметры портов; корректные файлы заменяют
 *                        только соответствующую группу значений.
 * @param[out] report Результаты чтения обоих файлов.
 */
void field_serial_config_load(
    LcpFieldPortConfig configs[LCP_FIELD_PORT_COUNT],
    FieldSerialConfigReport* report);
