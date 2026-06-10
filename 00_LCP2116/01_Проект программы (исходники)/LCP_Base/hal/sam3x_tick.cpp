
/**
 * @file sam3x_tick.cpp
 * @brief Реализация системного таймера через SysTick.
 */

#include "sam3x_tick.hpp"
#include "sam3x_device.hpp"

extern "C" void tickReset(void);

static volatile uint32_t g_ms_ticks = 0U;

static inline void hal_cpu_relax(void)
{
    __asm__ volatile ("nop");
}

/**
 * @brief Обработчик прерывания SysTick.
 *
 * Обработчик увеличивает миллисекундный счётчик системного времени
 * и обслуживает отложенный переход в ROM-загрузчик SAM-BA.
 */
extern "C" void SysTick_Handler(void)
{
    ++g_ms_ticks;
    tickReset();
}

void hal_tick_init(void)
{
    SystemCoreClockUpdate();

    const uint32_t ticks_per_ms = SystemCoreClock / 1000U;

    if ((ticks_per_ms == 0U) || (SysTick_Config(ticks_per_ms) != 0U))
    {
        for (;;)
        {
            hal_cpu_relax();
        }
    }
}

uint32_t hal_millis(void)
{
    return g_ms_ticks;
}

uint32_t hal_micros(void)
{
    uint32_t ms;
    uint32_t systick_value;
    uint32_t systick_load;

    do
    {
        ms = g_ms_ticks;
        systick_value = SysTick->VAL;
        systick_load = SysTick->LOAD + 1U;
    }
    while (ms != g_ms_ticks);

    const uint32_t cycles_per_us = SystemCoreClock / 1000000U;

    if (cycles_per_us == 0U)
    {
        return ms * 1000U;
    }

    const uint32_t elapsed_cycles = systick_load - systick_value;

    return (ms * 1000U) + (elapsed_cycles / cycles_per_us);
}

void hal_delay_ms(uint32_t ms)
{
    const uint32_t started = hal_millis();

    while ((uint32_t)(hal_millis() - started) < ms)
    {
        hal_cpu_relax();
    }
}

void hal_delay_us(uint32_t us)
{
    const uint32_t started = hal_micros();

    while ((uint32_t)(hal_micros() - started) < us)
    {
        hal_cpu_relax();
    }
}
