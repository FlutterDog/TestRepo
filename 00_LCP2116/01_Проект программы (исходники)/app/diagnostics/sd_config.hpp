
/**
 * @file sd_config.hpp
 * @brief Чтение и запись числовых конфигурационных файлов с microSD.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Результат операции с конфигурационным файлом.
 */
enum SdConfigResult
{
    SD_CONFIG_OK = 0,
    SD_CONFIG_CARD_NOT_READY = 1,
    SD_CONFIG_FILE_NOT_FOUND = 2,
    SD_CONFIG_FILE_OPEN_FAILED = 3,
    SD_CONFIG_EMPTY_FILE = 4,
    SD_CONFIG_INVALID_COUNT = 5,
    SD_CONFIG_INVALID_VALUE = 6,
    SD_CONFIG_INCOMPLETE_FILE = 7,
    SD_CONFIG_WRITE_FAILED = 8,
    SD_CONFIG_LINE_TOO_LONG = 9,
    SD_CONFIG_STORAGE_ERROR = 10
};

/**
 * @brief Загружает int16-конфигурацию из файла.
 *
 * Формат файла:
 * строка 1 — количество значений;
 * строки 2..N — числовые значения;
 * комментарий начинается с //.
 *
 * @param file_name Имя файла в формате 8.3.
 * @param output Буфер результата. output[0] получает количество значений.
 * @param capacity Количество элементов в output.
 * @param loaded_count Количество загруженных значений без output[0].
 *
 * @return Код результата.
 */
SdConfigResult sd_config_load_int16(const char* file_name,
                                    int16_t* output,
                                    size_t capacity,
                                    uint8_t* loaded_count);

/**
 * @brief Сохраняет int16-конфигурацию в файл.
 *
 * @param file_name Имя файла в формате 8.3.
 * @param input Буфер, значения берутся из input[1..size].
 * @param size Количество сохраняемых значений.
 *
 * @return Код результата.
 */
SdConfigResult sd_config_save_int16(const char* file_name,
                                    const int16_t* input,
                                    uint8_t size);

/**
 * @brief Возвращает текстовое описание результата.
 */
const char* sd_config_result_text(SdConfigResult result);
