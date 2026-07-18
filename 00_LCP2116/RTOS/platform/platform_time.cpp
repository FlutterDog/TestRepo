/**
 * @file platform_time.cpp
 * @brief Платформенные функции времени для baseline-прошивки с FreeRTOS.
 */

#include "platform.hpp"
#include "../hal/sam3x_tick.hpp"
#include "../hal/sam3x_device.hpp"

extern "C"
{
#include "FreeRTOS.h"
#include "task.h"
}

namespace
{
void enable_cycle_counter(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U)
    {
        DWT->CYCCNT = 0U;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
}

void busy_wait_cycles(uint32_t cycles)
{
    if (cycles == 0U)
    {
        return;
    }

    enable_cycle_counter();

    const uint32_t started = DWT->CYCCNT;

    while ((uint32_t)(DWT->CYCCNT - started) < cycles)
    {
        __NOP();
    }
}

void busy_wait_us(uint32_t us)
{
    SystemCoreClockUpdate();

    const uint32_t cycles_per_us = SystemCoreClock / 1000000U;

    while (us > 0U)
    {
        busy_wait_cycles(cycles_per_us);
        --us;
    }
}
}

uint32_t millis(void)
{
    return hal_millis();
}

uint32_t micros(void)
{
    return hal_micros();
}

void delay(uint32_t ms)
{
    /* Блокирующая миллисекундная задержка из ISR запрещена. */
    configASSERT(__get_IPSR() == 0U);

    if (ms == 0U)
    {
        return;
    }

    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        TickType_t ticks = pdMS_TO_TICKS(ms);

        if (ticks == 0U)
        {
            ticks = 1U;
        }

        vTaskDelay(ticks);
        return;
    }

    /*
     * До запуска планировщика SysTick ещё принадлежит не HAL, а будущему
     * порту FreeRTOS. Поэтому ранняя задержка выполняется по DWT CYCCNT.
     */
    while (ms > 0U)
    {
        busy_wait_us(1000U);
        --ms;
    }
}

void delayMicroseconds(uint32_t us)
{
    /* Короткая микросекундная задержка остаётся активным ожиданием. */
    busy_wait_us(us);
}
