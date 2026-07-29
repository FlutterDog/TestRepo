/**
 * @file sam3x_internal_flash.cpp
 * @brief Реализация записи зарезервированной Flash1 ATSAM3X8E.
 */

#include "sam3x_internal_flash.hpp"
#include "sam3x_device.hpp"

#include <string.h>

extern "C"
{
extern uint8_t _config_flash_start;
extern uint8_t _config_flash_end;
}

#ifndef EEFC_FMR_FWS_Msk
#define EEFC_FMR_FWS_Msk (0xFUL << 8)
#endif

#ifndef EEFC_FMR_FWS
#define EEFC_FMR_FWS(value) (((value) & 0xFUL) << 8)
#endif

#ifndef EEFC_FCR_FKEY_PASSWD
#define EEFC_FCR_FKEY_PASSWD (0x5AUL << 24)
#endif

#ifndef EEFC_FCR_FARG
#define EEFC_FCR_FARG(value) (((value) & 0xFFFFUL) << 8)
#endif

#ifndef EEFC_FCR_FCMD
#define EEFC_FCR_FCMD(value) ((value) & 0xFFUL)
#endif

#ifndef EEFC_FSR_FRDY
#define EEFC_FSR_FRDY (1UL << 0)
#endif

#ifndef EEFC_FSR_FCMDE
#define EEFC_FSR_FCMDE (1UL << 1)
#endif

#ifndef EEFC_FSR_FLOCKE
#define EEFC_FSR_FLOCKE (1UL << 2)
#endif

namespace
{
constexpr uintptr_t FLASH1_ORIGIN = 0x000C0000UL;
constexpr uintptr_t FLASH1_END = 0x00100000UL;
constexpr uint32_t FLASH1_PAGE_COUNT = 1024U;
constexpr uint32_t EEFC_COMMAND_EWP = 0x03U;
constexpr uint32_t PROGRAMMING_FWS = 6U;
constexpr uint32_t READY_WAIT_LIMIT = 20000000UL;

struct FlashCommandResult
{
    uint32_t status;
    uint8_t timed_out;
};

/**
 * Выполняется полностью из SRAM. Во время EWP запрещены interrupts, чтобы
 * обработчик не попытался выполнить код или прочитать константу из Flash1.
 */
__attribute__((section(".ramfunc"), noinline))
FlashCommandResult erase_write_page_from_ram(uintptr_t page_address,
                                              const uint32_t* source_words)
{
    FlashCommandResult result = { 0U, 0U };
    const uint32_t previous_primask = __get_PRIMASK();
    __disable_irq();

    const uint32_t previous_mode = EFC1->EEFC_FMR;
    EFC1->EEFC_FMR = (previous_mode & ~EEFC_FMR_FWS_Msk) |
                     EEFC_FMR_FWS(PROGRAMMING_FWS);

    uint32_t wait_count = READY_WAIT_LIMIT;

    while (((EFC1->EEFC_FSR & EEFC_FSR_FRDY) == 0U) &&
           (wait_count != 0U))
    {
        --wait_count;
    }

    if (wait_count == 0U)
    {
        result.timed_out = 1U;
        EFC1->EEFC_FMR = previous_mode;

        if (previous_primask == 0U)
        {
            __enable_irq();
        }

        return result;
    }

    volatile uint32_t* destination =
        reinterpret_cast<volatile uint32_t*>(page_address);

    for (uint32_t word = 0U;
         word < (SAM3X_INTERNAL_FLASH_PAGE_SIZE / sizeof(uint32_t));
         ++word)
    {
        destination[word] = source_words[word];
    }

    const uint32_t page_number = static_cast<uint32_t>(
        (page_address - FLASH1_ORIGIN) / SAM3X_INTERNAL_FLASH_PAGE_SIZE);

    __DSB();
    EFC1->EEFC_FCR = EEFC_FCR_FKEY_PASSWD |
                     EEFC_FCR_FARG(page_number) |
                     EEFC_FCR_FCMD(EEFC_COMMAND_EWP);

    wait_count = READY_WAIT_LIMIT;

    while (((EFC1->EEFC_FSR & EEFC_FSR_FRDY) == 0U) &&
           (wait_count != 0U))
    {
        --wait_count;
    }

    if (wait_count == 0U)
    {
        result.timed_out = 1U;
    }
    else
    {
        result.status = EFC1->EEFC_FSR;
    }

    EFC1->EEFC_FMR = previous_mode;
    __DSB();
    __ISB();

    if (previous_primask == 0U)
    {
        __enable_irq();
    }

    return result;
}

uint8_t range_valid(uintptr_t address, size_t length)
{
    const uintptr_t start = sam3x_internal_flash_config_start();
    const uintptr_t end = sam3x_internal_flash_config_end();

    if ((address < start) || (address > end))
    {
        return 0U;
    }

    if (length > static_cast<size_t>(end - address))
    {
        return 0U;
    }

    return 1U;
}
}

uintptr_t sam3x_internal_flash_config_start(void)
{
    return reinterpret_cast<uintptr_t>(&_config_flash_start);
}

uintptr_t sam3x_internal_flash_config_end(void)
{
    return reinterpret_cast<uintptr_t>(&_config_flash_end);
}

uint32_t sam3x_internal_flash_config_size(void)
{
    const uintptr_t start = sam3x_internal_flash_config_start();
    const uintptr_t end = sam3x_internal_flash_config_end();

    return (end >= start) ? static_cast<uint32_t>(end - start) : 0U;
}

Sam3xInternalFlashResult sam3x_internal_flash_read(uintptr_t address,
                                                   void* output,
                                                   size_t length)
{
    if ((output == 0) && (length != 0U))
    {
        return SAM3X_INTERNAL_FLASH_INVALID_ARGUMENT;
    }

    if (range_valid(address, length) == 0U)
    {
        return SAM3X_INTERNAL_FLASH_OUT_OF_RANGE;
    }

    if (length != 0U)
    {
        memcpy(output, reinterpret_cast<const void*>(address), length);
    }

    return SAM3X_INTERNAL_FLASH_OK;
}

Sam3xInternalFlashResult sam3x_internal_flash_erase_write_page(
    uintptr_t page_address,
    const uint8_t page[SAM3X_INTERNAL_FLASH_PAGE_SIZE])
{
    if (page == 0)
    {
        return SAM3X_INTERNAL_FLASH_INVALID_ARGUMENT;
    }

    if (((page_address & (SAM3X_INTERNAL_FLASH_PAGE_SIZE - 1U)) != 0U) ||
        (page_address < FLASH1_ORIGIN) ||
        (page_address >= FLASH1_END) ||
        (range_valid(page_address, SAM3X_INTERNAL_FLASH_PAGE_SIZE) == 0U))
    {
        return SAM3X_INTERNAL_FLASH_OUT_OF_RANGE;
    }

    const uint32_t page_number = static_cast<uint32_t>(
        (page_address - FLASH1_ORIGIN) / SAM3X_INTERNAL_FLASH_PAGE_SIZE);

    if (page_number >= FLASH1_PAGE_COUNT)
    {
        return SAM3X_INTERNAL_FLASH_OUT_OF_RANGE;
    }

    uint32_t aligned_page[SAM3X_INTERNAL_FLASH_PAGE_SIZE / sizeof(uint32_t)];
    memcpy(aligned_page, page, SAM3X_INTERNAL_FLASH_PAGE_SIZE);

    const FlashCommandResult command =
        erase_write_page_from_ram(page_address, aligned_page);

    if (command.timed_out != 0U)
    {
        return SAM3X_INTERNAL_FLASH_NOT_READY;
    }

    if ((command.status & EEFC_FSR_FLOCKE) != 0U)
    {
        return SAM3X_INTERNAL_FLASH_LOCK_ERROR;
    }

    if ((command.status & EEFC_FSR_FCMDE) != 0U)
    {
        return SAM3X_INTERNAL_FLASH_COMMAND_ERROR;
    }

    if (memcmp(reinterpret_cast<const void*>(page_address),
               aligned_page,
               SAM3X_INTERNAL_FLASH_PAGE_SIZE) != 0)
    {
        return SAM3X_INTERNAL_FLASH_VERIFY_ERROR;
    }

    return SAM3X_INTERNAL_FLASH_OK;
}

const char* sam3x_internal_flash_result_text(Sam3xInternalFlashResult result)
{
    switch (result)
    {
        case SAM3X_INTERNAL_FLASH_OK:
            return "ok";
        case SAM3X_INTERNAL_FLASH_INVALID_ARGUMENT:
            return "invalid argument";
        case SAM3X_INTERNAL_FLASH_OUT_OF_RANGE:
            return "out of range";
        case SAM3X_INTERNAL_FLASH_NOT_READY:
            return "not ready";
        case SAM3X_INTERNAL_FLASH_COMMAND_ERROR:
            return "command error";
        case SAM3X_INTERNAL_FLASH_LOCK_ERROR:
            return "lock error";
        case SAM3X_INTERNAL_FLASH_VERIFY_ERROR:
            return "verify error";
        default:
            return "unknown";
    }
}
