/**
 * @file lcp_sd_storage.hpp
 * @brief Минимальный SPI/FAT16/FAT32 слой microSD контроллера LCP.
 *
 * Слой работает только с корневым каталогом и короткими именами FAT 8.3. Он не
 * выделяет динамическую память и вызывается последовательно из одной
 * прикладной задачи FreeRTOS.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/** @brief Результат низкоуровневой операции с microSD. */
enum LcpSdStorageResult
{
    LCP_SD_STORAGE_OK = 0,                 /**< Операция завершена успешно. */
    LCP_SD_STORAGE_NO_CARD = 1,            /**< Карта не отвечает на SPI. */
    LCP_SD_STORAGE_CARD_INIT_FAILED = 2,   /**< SD initialization sequence не завершена. */
    LCP_SD_STORAGE_VOLUME_UNSUPPORTED = 3, /**< Раздел или FAT не поддерживается. */
    LCP_SD_STORAGE_FILE_NOT_FOUND = 4,     /**< Файл отсутствует в корневом каталоге. */
    LCP_SD_STORAGE_FILE_OPEN_FAILED = 5,   /**< Directory entry найден, но файл не открыт. */
    LCP_SD_STORAGE_FILE_EXISTS_ERROR = 6,  /**< Не удалось корректно заменить существующий файл. */
    LCP_SD_STORAGE_DIRECTORY_FULL = 7,     /**< В корневом каталоге нет свободной записи. */
    LCP_SD_STORAGE_NO_FREE_CLUSTER = 8,    /**< FAT не содержит свободного кластера. */
    LCP_SD_STORAGE_READ_FAILED = 9,        /**< Ошибка чтения блока или цепочки FAT. */
    LCP_SD_STORAGE_WRITE_FAILED = 10,      /**< Ошибка записи данных, FAT или directory entry. */
    LCP_SD_STORAGE_INVALID_NAME = 11,      /**< Имя не соответствует поддерживаемому FAT 8.3. */
    LCP_SD_STORAGE_NOT_READY = 12          /**< Файловая система не смонтирована. */
};

/** @brief Контекст последовательного чтения одного открытого файла. */
struct LcpSdReadFile
{
    uint32_t first_cluster;         /**< Первый кластер файла. */
    uint32_t current_cluster;       /**< Текущий кластер цепочки FAT. */
    uint32_t current_cluster_index; /**< Номер текущего кластера относительно начала файла. */
    uint32_t file_size;             /**< Размер файла в байтах. */
    uint32_t position;              /**< Текущая позиция чтения. */
    uint32_t loaded_sector_lba;     /**< LBA сектора, находящегося в sector[]. */
    uint8_t sector_loaded;          /**< Ненулевое значение, если sector[] валиден. */
    uint8_t sector[512];            /**< Внутренний секторный кэш открытого файла. */
};

/**
 * @brief Инициализирует карту и монтирует поддерживаемый FAT-раздел.
 * @return Код результата; при успехе lcp_sd_storage_ready() возвращает 1.
 */
LcpSdStorageResult lcp_sd_storage_begin(void);

/** @brief Сбрасывает runtime монтирования без изменения содержимого карты. */
void lcp_sd_storage_reset(void);

/** @return 1 после успешного mount, иначе 0. */
uint8_t lcp_sd_storage_ready(void);

/**
 * @brief Возвращает текстовое описание низкоуровневого результата.
 * @param[in] result Код операции.
 * @return Указатель на строковый литерал.
 */
const char* lcp_sd_storage_result_text(LcpSdStorageResult result);

/**
 * @brief Проверяет наличие файла в корневом каталоге.
 * @param[in] file_name Короткое имя FAT 8.3.
 * @return 1, если обычный файл найден, иначе 0.
 */
uint8_t lcp_sd_storage_exists(const char* file_name);

/**
 * @brief Открывает обычный файл корневого каталога для последовательного чтения.
 * @param[in] file_name Короткое имя FAT 8.3.
 * @param[out] file Контекст, принадлежащий вызывающему коду.
 * @return Код результата.
 */
LcpSdStorageResult lcp_sd_storage_open_read(const char* file_name,
                                            LcpSdReadFile* file);

/**
 * @brief Читает следующий байт открытого файла.
 * @param[in,out] file Контекст из lcp_sd_storage_open_read().
 * @return Байт 0..255 либо -1 при EOF или ошибке.
 */
int lcp_sd_storage_read_byte(LcpSdReadFile* file);

/**
 * @brief Закрывает контекст последовательного чтения.
 * @param[in,out] file Контекст файла; указатель может быть null.
 */
void lcp_sd_storage_close_read(LcpSdReadFile* file);

/**
 * @brief Записывает файл в корневой каталог с заменой существующего.
 * @param[in] file_name Короткое имя FAT 8.3.
 * @param[in] data Буфер данных.
 * @param[in] length Размер в байтах.
 * @return Код результата.
 */
LcpSdStorageResult lcp_sd_storage_write_file(const char* file_name,
                                             const uint8_t* data,
                                             size_t length);

/** @return Тип смонтированной FAT: 16, 32 или 0 до mount. */
uint8_t lcp_sd_storage_fat_type(void);

/** @return Количество секторов в кластере либо 0 до mount. */
uint8_t lcp_sd_storage_sectors_per_cluster(void);
