/**
 * @file sam3x_tick.hpp
 * @brief Системный таймер проекта на базе SysTick Cortex-M3.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Инициализирует SysTick с периодом одна миллисекунда.
 *
 * Функция использует текущее значение SystemCoreClock.
 */
void hal_tick_init(void);

/**
 * @brief Возвращает время с момента старта прошивки в миллисекундах.
 */
uint32_t hal_millis(void);

/**
 * @brief Возвращает время с момента старта прошивки в микросекундах.
 *
 * Младшая часть значения вычисляется по текущему состоянию счётчика SysTick.
 */
uint32_t hal_micros(void);

/**
 * @brief Выполняет блокирующую задержку в миллисекундах.
 */
void hal_delay_ms(uint32_t ms);

/**
 * @brief Выполняет блокирующую задержку в микросекундах.
 */
void hal_delay_us(uint32_t us);
