
/**
 * @file lcp_sd.cpp
 * @brief Инициализация GPIO интерфейса microSD контроллера LCP.
 */

#include "lcp_sd.hpp"
#include "../hal/sam3x_device.hpp"

static void lcp_sd_enable_peripheral_clock(uint32_t peripheral_id)
{
    if (peripheral_id < 32U)
    {
        PMC->PMC_PCER0 = (1UL << peripheral_id);
    }
    else
    {
        PMC->PMC_PCER1 = (1UL << (peripheral_id - 32U));
    }
}

void lcp_sd_init_pins(void)
{
    lcp_sd_enable_peripheral_clock(ID_PIOA);
    lcp_sd_enable_peripheral_clock(ID_PIOD);
    lcp_sd_enable_peripheral_clock(ID_PIOC);

    PIOA->PIO_PER = PIO_PA14;
    PIOA->PIO_IDR = PIO_PA14;
    PIOA->PIO_OER = PIO_PA14;
    PIOA->PIO_SODR = PIO_PA14;

    PIOD->PIO_PER = PIO_PD0;
    PIOD->PIO_IDR = PIO_PD0;
    PIOD->PIO_ODR = PIO_PD0;
    PIOD->PIO_PUER = PIO_PD0;

    PIOC->PIO_PER = PIO_PC9;
    PIOC->PIO_IDR = PIO_PC9;
    PIOC->PIO_OER = PIO_PC9;
    PIOC->PIO_CODR = PIO_PC9;
}

uint8_t lcp_sd_detect_raw(void)
{
    return ((PIOD->PIO_PDSR & PIO_PD0) != 0U) ? 1U : 0U;
}

uint8_t lcp_sd_card_inserted(void)
{
    const uint8_t raw = lcp_sd_detect_raw();

    if (LCP_SD_DETECT_ACTIVE_LOW != 0U)
    {
        return (raw == 0U) ? 1U : 0U;
    }

    return (raw != 0U) ? 1U : 0U;
}

void lcp_sd_set_ok_led(uint8_t enabled)
{
    if (enabled != 0U)
    {
        PIOC->PIO_SODR = PIO_PC9;
    }
    else
    {
        PIOC->PIO_CODR = PIO_PC9;
    }
}

void lcp_sd_select(void)
{
    PIOA->PIO_CODR = PIO_PA14;
}

void lcp_sd_deselect(void)
{
    PIOA->PIO_SODR = PIO_PA14;
}
