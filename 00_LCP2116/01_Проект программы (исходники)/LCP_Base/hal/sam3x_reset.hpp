
/**
 * @file sam3x_reset.hpp
 * @brief Управляемый переход ATSAM3X8E в ROM-загрузчик SAM-BA.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Запрашивает переход в ROM-загрузчик через заданное число миллисекунд.
 *
 * @param delay_ms Задержка перед переходом в миллисекундах.
 */
void sam3x_bootloader_request(uint32_t delay_ms);

/**
 * @brief Отменяет ожидающий переход в ROM-загрузчик.
 */
void sam3x_bootloader_cancel(void);

/**
 * @brief Обрабатывает миллисекундный таймер перехода в ROM-загрузчик.
 *
 * Функция вызывается из SysTick с периодом одна миллисекунда.
 */
void sam3x_bootloader_tick(void);
