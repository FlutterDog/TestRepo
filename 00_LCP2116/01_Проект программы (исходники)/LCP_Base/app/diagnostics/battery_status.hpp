
/**
 * @file battery_status.hpp
 * @brief Диагностика резервной батареи CR2032.
 */

#pragma once

/**
 * @brief Инициализирует диагностику резервной батареи.
 */
void battery_status_init(void);

/**
 * @brief Обслуживает диагностику резервной батареи.
 */
void battery_status_poll(void);

/**
 * @brief Печатает отчёт резервной батареи в USB service console.
 */
void battery_status_print_report(void);
