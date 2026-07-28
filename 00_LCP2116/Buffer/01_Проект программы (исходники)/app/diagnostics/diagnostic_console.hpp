
/**
 * @file diagnostic_console.hpp
 * @brief USB service console базовой диагностической прошивки LCP.
 */

#pragma once

/**
 * @brief Инициализирует USB service console.
 */
void diagnostic_console_init(void);

/**
 * @brief Обслуживает ввод команд и вывод отчётов USB service console.
 */
void diagnostic_console_poll(void);
