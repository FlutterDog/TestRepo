
/**
 * @file sam3x_watchdog.cpp
 * @brief Реализация управления watchdog ATSAM3X8E.
 */

#include "sam3x_watchdog.hpp"
#include "sam3x_device.hpp"

void hal_watchdog_disable(void)
{
    WDT->WDT_MR = WDT_MR_WDDIS;
}
