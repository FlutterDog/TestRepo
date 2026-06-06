
/**
 * @file main.cpp
 * @brief Точка входа встроенного программного обеспечения.
 */

#include "hal/sam3x_device.hpp"
#include "app/app.hpp"
#include "hal/sam3x_tick.hpp"
#include "hal/sam3x_watchdog.hpp"

int main(void)
{
    SystemInit();

    hal_watchdog_disable();
    hal_tick_init();

    setup();

    for (;;)
    {
        loop();
    }
}
