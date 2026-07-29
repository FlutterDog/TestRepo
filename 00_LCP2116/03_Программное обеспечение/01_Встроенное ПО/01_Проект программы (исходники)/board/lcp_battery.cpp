
/**
 * @file lcp_battery.cpp
 * @brief Инициализация входа контроля резервной батареи контроллера LCP.
 */

#include "lcp_battery.hpp"
#include "../hal/sam3x_device.hpp"

void lcp_battery_init_pins(void)
{
    PMC->PMC_PCER0 = (1UL << ID_PIOC);

    PIOC->PIO_PER = PIO_PC13;
    PIOC->PIO_IDR = PIO_PC13;
    PIOC->PIO_ODR = PIO_PC13;
    PIOC->PIO_PUDR = PIO_PC13;
}

uint8_t lcp_battery_raw(void)
{
    return ((PIOC->PIO_PDSR & PIO_PC13) != 0U) ? 1U : 0U;
}

uint8_t lcp_battery_ok(void)
{
    const uint8_t raw = lcp_battery_raw();

    if (LCP_BACKUP_BATTERY_OK_ACTIVE_HIGH != 0U)
    {
        return (raw != 0U) ? 1U : 0U;
    }

    return (raw == 0U) ? 1U : 0U;
}
