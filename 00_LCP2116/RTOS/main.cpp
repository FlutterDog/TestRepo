/**
 * @file main.cpp
 * @brief Точка входа специализированной прошивки Lorentz Fixture Bridge.
 */

#include "sam.h"
#include "app/fixture/fixture_firmware.hpp"

extern "C" void SystemInit(void);

int main(void)
{
    SystemInit();
    fixture_firmware::start();

    /* start() возвращается только при ошибке запуска планировщика. */
    for (;;)
    {
    }
}
