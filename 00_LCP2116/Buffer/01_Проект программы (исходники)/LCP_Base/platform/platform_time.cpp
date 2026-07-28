/**
 * @file platform_time.cpp
 * @brief Платформенные функции времени.
 *
 * Модуль связывает прикладной интерфейс времени с системным таймером HAL.
 */

#include "platform.hpp"
#include "../hal/sam3x_tick.hpp"

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
    hal_delay_ms(ms);
}

void delayMicroseconds(uint32_t us)
{
    hal_delay_us(us);
}
