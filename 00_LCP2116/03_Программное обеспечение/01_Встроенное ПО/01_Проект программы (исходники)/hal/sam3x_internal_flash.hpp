/**
 * @file sam3x_internal_flash.hpp
 * @brief Доступ к зарезервированной области внутренней Flash ATSAM3X8E.
 *
 * Область задаётся linker script символами `_config_flash_start` и
 * `_config_flash_end`. Запись выполняется только полными страницами 256 байт.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

static const uint32_t SAM3X_INTERNAL_FLASH_PAGE_SIZE = 256U;

enum Sam3xInternalFlashResult : uint8_t
{
    SAM3X_INTERNAL_FLASH_OK = 0U,
    SAM3X_INTERNAL_FLASH_INVALID_ARGUMENT = 1U,
    SAM3X_INTERNAL_FLASH_OUT_OF_RANGE = 2U,
    SAM3X_INTERNAL_FLASH_NOT_READY = 3U,
    SAM3X_INTERNAL_FLASH_COMMAND_ERROR = 4U,
    SAM3X_INTERNAL_FLASH_LOCK_ERROR = 5U,
    SAM3X_INTERNAL_FLASH_VERIFY_ERROR = 6U
};

/** @return Первый адрес зарезервированной области конфигурации. */
uintptr_t sam3x_internal_flash_config_start(void);

/** @return Адрес за последним байтом зарезервированной области. */
uintptr_t sam3x_internal_flash_config_end(void);

/** @return Размер зарезервированной области в байтах. */
uint32_t sam3x_internal_flash_config_size(void);

/**
 * @brief Читает байты из memory-mapped Flash.
 */
Sam3xInternalFlashResult sam3x_internal_flash_read(uintptr_t address,
                                                   void* output,
                                                   size_t length);

/**
 * @brief Стирает и записывает одну страницу Flash1, затем проверяет read-back.
 *
 * Операция кратковременно запрещает interrupts. Исполнение команды и ожидание
 * FRDY выполняются из SRAM, поэтому код приложения может находиться в любой
 * части Flash, кроме зарезервированной linker-области.
 */
Sam3xInternalFlashResult sam3x_internal_flash_erase_write_page(
    uintptr_t page_address,
    const uint8_t page[SAM3X_INTERNAL_FLASH_PAGE_SIZE]);

/** @return Строковое описание результата. */
const char* sam3x_internal_flash_result_text(Sam3xInternalFlashResult result);
