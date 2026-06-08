
/**
 * @file lcp_sd_storage.hpp
 * @brief Минимальный SPI/FAT слой microSD для диагностической прошивки LCP.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Результат операции с microSD.
 */
enum LcpSdStorageResult
{
    LCP_SD_STORAGE_OK = 0,
    LCP_SD_STORAGE_NO_CARD = 1,
    LCP_SD_STORAGE_CARD_INIT_FAILED = 2,
    LCP_SD_STORAGE_VOLUME_UNSUPPORTED = 3,
    LCP_SD_STORAGE_FILE_NOT_FOUND = 4,
    LCP_SD_STORAGE_FILE_OPEN_FAILED = 5,
    LCP_SD_STORAGE_FILE_EXISTS_ERROR = 6,
    LCP_SD_STORAGE_DIRECTORY_FULL = 7,
    LCP_SD_STORAGE_NO_FREE_CLUSTER = 8,
    LCP_SD_STORAGE_READ_FAILED = 9,
    LCP_SD_STORAGE_WRITE_FAILED = 10,
    LCP_SD_STORAGE_INVALID_NAME = 11,
    LCP_SD_STORAGE_NOT_READY = 12
};

/**
 * @brief Открытый файл для последовательного чтения.
 */
struct LcpSdReadFile
{
    uint32_t first_cluster;
    uint32_t current_cluster;
    uint32_t current_cluster_index;
    uint32_t file_size;
    uint32_t position;
    uint32_t loaded_sector_lba;
    uint8_t sector_loaded;
    uint8_t sector[512];
};

/**
 * @brief Инициализирует карту и монтирует FAT16/FAT32.
 */
LcpSdStorageResult lcp_sd_storage_begin(void);

/**
 * @brief Сбрасывает состояние готовности файловой системы.
 */
void lcp_sd_storage_reset(void);

/**
 * @brief Возвращает признак готовности файловой системы.
 */
uint8_t lcp_sd_storage_ready(void);

/**
 * @brief Возвращает текстовое описание результата.
 */
const char* lcp_sd_storage_result_text(LcpSdStorageResult result);

/**
 * @brief Проверяет наличие файла в корневом каталоге.
 */
uint8_t lcp_sd_storage_exists(const char* file_name);

/**
 * @brief Открывает файл из корневого каталога для чтения.
 */
LcpSdStorageResult lcp_sd_storage_open_read(const char* file_name, LcpSdReadFile* file);

/**
 * @brief Читает один байт из открытого файла.
 */
int lcp_sd_storage_read_byte(LcpSdReadFile* file);

/**
 * @brief Закрывает файл чтения.
 */
void lcp_sd_storage_close_read(LcpSdReadFile* file);

/**
 * @brief Записывает файл в корневой каталог с заменой существующего файла.
 */
LcpSdStorageResult lcp_sd_storage_write_file(const char* file_name, const uint8_t* data, size_t length);

/**
 * @brief Возвращает тип FAT: 16 или 32.
 */
uint8_t lcp_sd_storage_fat_type(void);

/**
 * @brief Возвращает количество секторов в кластере.
 */
uint8_t lcp_sd_storage_sectors_per_cluster(void);
