
/**
 * @file sam3x_watchdog.cpp
 * @brief Реализация HAL watchdog-таймера ATSAM3X8E.
 */

#include "sam3x_watchdog.hpp"
#include "sam3x_device.hpp"

#ifndef WDT_CR_KEY_PASSWD
#define WDT_CR_KEY_PASSWD (0xA5UL << 24)
#endif

#ifndef WDT_CR_WDRSTT
#define WDT_CR_WDRSTT (1UL << 0)
#endif

#ifndef WDT_MR_WDDIS
#define WDT_MR_WDDIS (1UL << 15)
#endif

#ifndef WDT_MR_WDRSTEN
#define WDT_MR_WDRSTEN (1UL << 13)
#endif

#ifndef WDT_MR_WDRPROC
#define WDT_MR_WDRPROC (1UL << 14)
#endif

#ifndef WDT_MR_WDDBGHLT
#define WDT_MR_WDDBGHLT (1UL << 28)
#endif

#ifndef WDT_MR_WDIDLEHLT
#define WDT_MR_WDIDLEHLT (1UL << 29)
#endif

#ifndef WDT_MR_WDV
#define WDT_MR_WDV(value) ((value) & 0xFFFUL)
#endif

#ifndef WDT_MR_WDD
#define WDT_MR_WDD(value) (((value) & 0xFFFUL) << 16)
#endif

#ifndef RSTC_CR_KEY_PASSWD
#define RSTC_CR_KEY_PASSWD (0xA5UL << 24)
#endif

#ifndef RSTC_CR_PROCRST
#define RSTC_CR_PROCRST (1UL << 0)
#endif

#ifndef RSTC_CR_PERRST
#define RSTC_CR_PERRST (1UL << 2)
#endif

static const uint32_t WATCHDOG_RECOVERY_IN_PROGRESS_MAGIC = 0x4C435057UL;
static const uint32_t WATCHDOG_RECOVERY_PERFORMED_MAGIC = 0x4C435052UL;

static Sam3xResetType g_last_reset_type = SAM3X_RESET_UNKNOWN;
static uint32_t g_last_reset_status_register = 0U;

static Sam3xResetType decode_reset_type(uint32_t reset_status_register)
{
    const uint32_t raw_type = (reset_status_register & RSTC_SR_RSTTYP_Msk) >> RSTC_SR_RSTTYP_Pos;

    switch (raw_type)
    {
        case 0U:
            return SAM3X_RESET_GENERAL;

        case 1U:
            return SAM3X_RESET_BACKUP;

        case 2U:
            return SAM3X_RESET_WATCHDOG;

        case 3U:
            return SAM3X_RESET_SOFTWARE;

        case 4U:
            return SAM3X_RESET_USER;

        default:
            return SAM3X_RESET_UNKNOWN;
    }
}

void sam3x_watchdog_boot_recovery_from_watchdog_reset(void)
{
    const uint32_t reset_status_register = RSTC->RSTC_SR;
    const Sam3xResetType reset_type = decode_reset_type(reset_status_register);

    if (reset_type == SAM3X_RESET_WATCHDOG)
    {
        if (GPBR->SYS_GPBR[1] != WATCHDOG_RECOVERY_IN_PROGRESS_MAGIC)
        {
            GPBR->SYS_GPBR[1] = WATCHDOG_RECOVERY_IN_PROGRESS_MAGIC;
            GPBR->SYS_GPBR[2] = WATCHDOG_RECOVERY_PERFORMED_MAGIC;
            sam3x_watchdog_software_reset_processor_and_peripherals();
        }

        return;
    }

    if ((reset_type == SAM3X_RESET_SOFTWARE) &&
        (GPBR->SYS_GPBR[1] == WATCHDOG_RECOVERY_IN_PROGRESS_MAGIC))
    {
        GPBR->SYS_GPBR[1] = 0UL;
        return;
    }

    GPBR->SYS_GPBR[1] = 0UL;
    GPBR->SYS_GPBR[2] = 0UL;
}

void sam3x_watchdog_software_reset_processor_and_peripherals(void)
{
    RSTC->RSTC_CR = RSTC_CR_KEY_PASSWD | RSTC_CR_PROCRST | RSTC_CR_PERRST;

    for (;;)
    {
    }
}

void sam3x_watchdog_capture_boot_status(void)
{
    g_last_reset_status_register = RSTC->RSTC_SR;
    g_last_reset_type = decode_reset_type(g_last_reset_status_register);
}

Sam3xResetType sam3x_watchdog_last_reset_type(void)
{
    return g_last_reset_type;
}

const char* sam3x_watchdog_reset_type_text(Sam3xResetType reset_type)
{
    switch (reset_type)
    {
        case SAM3X_RESET_GENERAL:
            return "general reset";

        case SAM3X_RESET_BACKUP:
            return "backup reset";

        case SAM3X_RESET_WATCHDOG:
            return "watchdog reset";

        case SAM3X_RESET_SOFTWARE:
            return "software reset";

        case SAM3X_RESET_USER:
            return "user reset";

        default:
            return "unknown reset";
    }
}

uint8_t sam3x_watchdog_recovery_performed(void)
{
    return (GPBR->SYS_GPBR[2] == WATCHDOG_RECOVERY_PERFORMED_MAGIC) ? 1U : 0U;
}

uint32_t sam3x_watchdog_boot_count(void)
{
    return GPBR->SYS_GPBR[0];
}

void sam3x_watchdog_increment_boot_count(void)
{
    GPBR->SYS_GPBR[0] = GPBR->SYS_GPBR[0] + 1UL;
}

void sam3x_watchdog_enable(const Sam3xWatchdogConfig* config)
{
    uint32_t mode;
    uint16_t timeout_ticks;

    if (config == 0)
    {
        return;
    }

    timeout_ticks = config->timeout_ticks_256hz;

    if (timeout_ticks == 0U)
    {
        timeout_ticks = 1U;
    }

    if (timeout_ticks > 0x0FFFU)
    {
        timeout_ticks = 0x0FFFU;
    }

    mode = WDT_MR_WDV(timeout_ticks) | WDT_MR_WDD(0x0FFFU);

    if (config->reset_processor != 0U)
    {
        mode |= WDT_MR_WDRPROC;
    }

    if (config->reset_all != 0U)
    {
        mode |= WDT_MR_WDRSTEN;
    }

    if (config->halt_in_debug != 0U)
    {
        mode |= WDT_MR_WDDBGHLT;
    }

    if (config->halt_in_idle != 0U)
    {
        mode |= WDT_MR_WDIDLEHLT;
    }

    WDT->WDT_MR = mode;
    sam3x_watchdog_restart();
}

void sam3x_watchdog_disable_before_enable(void)
{
    WDT->WDT_MR = WDT_MR_WDDIS;
}

void sam3x_watchdog_restart(void)
{
    WDT->WDT_CR = WDT_CR_KEY_PASSWD | WDT_CR_WDRSTT;
}

uint8_t sam3x_watchdog_enabled(void)
{
    return ((WDT->WDT_MR & WDT_MR_WDDIS) == 0U) ? 1U : 0U;
}

uint32_t sam3x_watchdog_mode_register(void)
{
    return WDT->WDT_MR;
}

uint32_t sam3x_watchdog_status_register(void)
{
    return WDT->WDT_SR;
}

uint32_t sam3x_watchdog_reset_status_register(void)
{
    return g_last_reset_status_register;
}

uint32_t sam3x_watchdog_timeout_ms(uint16_t timeout_ticks_256hz)
{
    return (static_cast<uint32_t>(timeout_ticks_256hz) * 1000UL) / 256UL;
}
