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
constexpr uint32_t MAX_DWT_WAIT_CYCLES = 0x7FFFFFFFUL;
constexpr uint32_t MAX_BUSY_WAIT_MS_CHUNK = 1000U;

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
    if (us == 0U)
    {
        return;
    }

    SystemCoreClockUpdate();

    const uint32_t cycles_per_us = SystemCoreClock / 1000000U;
    uint64_t cycles_remaining = static_cast<uint64_t>(cycles_per_us) * us;

    while (cycles_remaining > 0U)
    {
        const uint32_t chunk =
            (cycles_remaining > MAX_DWT_WAIT_CYCLES)
                ? MAX_DWT_WAIT_CYCLES
                : static_cast<uint32_t>(cycles_remaining);

        busy_wait_cycles(chunk);
        cycles_remaining -= chunk;
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
        const uint32_t chunk_ms =
            (ms > MAX_BUSY_WAIT_MS_CHUNK) ? MAX_BUSY_WAIT_MS_CHUNK : ms;

        busy_wait_us(chunk_ms * 1000U);
        ms -= chunk_ms;
    }
}

void delayMicroseconds(uint32_t us)
{
    /* Короткая микросекундная задержка остаётся активным ожиданием. */
    busy_wait_us(us);
}
