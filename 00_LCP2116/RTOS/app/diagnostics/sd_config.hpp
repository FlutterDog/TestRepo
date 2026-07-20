/**
 * @file sd_config.hpp
 * @brief Общий парсер числовых конфигурационных файлов microSD.
 *
 * Функции этого файла используются всеми baseline-сервисами, которым требуется
 * совместимый числовой TXT-формат. Парсер не выделяет динамическую память и
 * вызывается последовательно из одной прикладной задачи FreeRTOS.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Результат операции с числовым конфигурационным файлом.
 */
enum SdConfigResult
{
    SD_CONFIG_OK = 0,                 /**< Файл корректно прочитан или записан. */
    SD_CONFIG_CARD_NOT_READY = 1,     /**< microSD ещё не смонтирована. */
    SD_CONFIG_FILE_NOT_FOUND = 2,     /**< Файл с указанным именем отсутствует. */
    SD_CONFIG_FILE_OPEN_FAILED = 3,   /**< Файл найден, но не открыт. */
    SD_CONFIG_EMPTY_FILE = 4,         /**< Не найдена строка с количеством. */
    SD_CONFIG_INVALID_COUNT = 5,      /**< Количество отсутствует или не помещается в буфер. */
    SD_CONFIG_INVALID_VALUE = 6,      /**< Обнаружено значение вне числового формата или диапазона типа. */
    SD_CONFIG_INCOMPLETE_FILE = 7,    /**< Значений меньше, чем объявлено первой строкой. */
    SD_CONFIG_WRITE_FAILED = 8,       /**< Выходной текст не помещается во внутренний буфер. */
    SD_CONFIG_LINE_TOO_LONG = 9,      /**< Физическая строка превышает внутренний предел парсера. */
    SD_CONFIG_STORAGE_ERROR = 10      /**< Низкоуровневая операция хранилища завершилась ошибкой. */
};

/**
 * @brief Загружает знаковые 16-битные значения из числового TXT-файла.
 *
 * Формат файла:
 *
 * @code
 * N
 * value_1
 * ...
 * value_N
 * fin
 * @endcode
 *
 * Маркер `fin` необязателен. Пустые строки, UTF-8 BOM в начале файла и
 * комментарии после `//` или `#` игнорируются.
 *
 * @param[in] file_name Каноническое имя файла в формате FAT 8.3.
 * @param[out] output Буфер результата; `output[0]` получает количество N,
 *                    значения размещаются в `output[1..N]`.
 * @param[in] capacity Количество элементов в `output`, включая `output[0]`.
 * @param[out] loaded_count Фактическое количество значений без `output[0]`.
 * @return Код результата операции.
 */
SdConfigResult sd_config_load_int16(const char* file_name,
                                    int16_t* output,
                                    size_t capacity,
                                    uint8_t* loaded_count);

/**
 * @brief Загружает беззнаковые 32-битные значения из числового TXT-файла.
 *
 * Использует тот же формат, правила комментариев и необязательный маркер `fin`,
 * что и sd_config_load_int16(). Функция предназначена, в частности, для
 * реальных значений baudrate, которые не обязаны помещаться в int16_t.
 *
 * @param[in] file_name Каноническое имя файла в формате FAT 8.3.
 * @param[out] output Буфер результата; `output[0]` получает количество N,
 *                    значения размещаются в `output[1..N]`.
 * @param[in] capacity Количество элементов в `output`, включая `output[0]`.
 * @param[out] loaded_count Фактическое количество значений без `output[0]`.
 * @return Код результата операции.
 */
SdConfigResult sd_config_load_uint32(const char* file_name,
                                     uint32_t* output,
                                     size_t capacity,
                                     uint8_t* loaded_count);

/**
 * @brief Сохраняет int16-конфигурацию в числовой TXT-файл.
 *
 * @param[in] file_name Имя файла в формате FAT 8.3.
 * @param[in] input Буфер; значения берутся из `input[1..size]`.
 * @param[in] size Количество сохраняемых значений.
 * @return Код результата операции.
 *
 * @note Функция записывает количество и значения. Маркер `fin` не обязателен
 *       для чтения и поэтому автоматически не добавляется.
 */
SdConfigResult sd_config_save_int16(const char* file_name,
                                    const int16_t* input,
                                    uint8_t size);

/**
 * @brief Возвращает неизменяемую строку с описанием результата.
 * @param[in] result Код операции.
 * @return Указатель на строковый литерал.
 */
const char* sd_config_result_text(SdConfigResult result);
