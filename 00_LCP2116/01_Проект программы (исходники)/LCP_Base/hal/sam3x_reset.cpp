
/**
 * @file sam3x_reset.cpp
 * @brief Реализация перехода ATSAM3X8E в ROM-загрузчик SAM-BA.
 */

#include "sam3x_reset.hpp"
#include "sam3x_device.hpp"

static volatile int32_t g_bootloader_ticks = -1;

__attribute__ ((long_call, section (".ramfunc")))
static void sam3x_enter_samba_bootloader(void)
{
    __disable_irq();

    static const uint32_t EEFC_FCMD_CGPB = 0x0CU;
    static const uint32_t EEFC_KEY = 0x5AU;

    while ((EFC0->EEFC_FSR & EEFC_FSR_FRDY) == 0U)
    {
    }

    EFC0->EEFC_FCR = ((EEFC_FCMD_CGPB & 0xFFU) << 0U)
                   | ((1U & 0xFFFFU) << 8U)
                   | ((EEFC_KEY & 0xFFU) << 24U);

    while ((EFC0->EEFC_FSR & EEFC_FSR_FRDY) == 0U)
    {
    }

    static const uint32_t RSTC_KEY_VALUE = 0xA5U;

    RSTC->RSTC_CR = ((RSTC_KEY_VALUE & 0xFFU) << 24U)
                  | RSTC_CR_PROCRST
                  | RSTC_CR_PERRST;

    for (;;)
    {
    }
}

void sam3x_bootloader_request(uint32_t delay_ms)
{
    if (delay_ms == 0U)
    {
        delay_ms = 1U;
    }

    g_bootloader_ticks = static_cast<int32_t>(delay_ms);
}

void sam3x_bootloader_cancel(void)
{
    g_bootloader_ticks = -1;
}

void sam3x_bootloader_tick(void)
{
    if (g_bootloader_ticks < 0)
    {
        return;
    }

    if (g_bootloader_ticks > 0)
    {
        --g_bootloader_ticks;
    }

    if (g_bootloader_ticks == 0)
    {
        sam3x_enter_samba_bootloader();
    }
}
