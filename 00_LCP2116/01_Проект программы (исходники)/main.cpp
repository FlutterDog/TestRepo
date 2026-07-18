
/**
 * @file main.cpp
 * @brief Точка входа диагностической прошивки LCP.
 */

#include "sam.h"
#include "app/app.hpp"
#include "hal/sam3x_tick.hpp"

extern "C" void SystemInit(void);

int main(void)
{
    SystemInit();
    hal_tick_init();

    setup();

    for (;;)
    {
        loop();
    }
}
