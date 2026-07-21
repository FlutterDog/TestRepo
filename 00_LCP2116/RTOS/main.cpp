/**
 * @file main.cpp
 * @brief Точка входа baseline-прошивки LCP под управлением FreeRTOS.
 */

#include "sam.h"
#include "app/app.hpp"

extern "C" void SystemInit(void);

int main(void)
{
    SystemInit();
    app_rtos_start();

    /* app_rtos_start() возвращается только при ошибке запуска планировщика. */
    for (;;)
    {
    }
}
