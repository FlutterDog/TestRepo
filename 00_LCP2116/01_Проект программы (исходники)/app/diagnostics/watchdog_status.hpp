
/**
 * @file watchdog_status.hpp
 * @brief Диагностика watchdog-таймера ATSAM3X8E.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Инициализирует watchdog-диагностику и включает watchdog.
 */
void watchdog_status_init(void);

/**
 * @brief Обслуживает watchdog.
 */
void watchdog_status_poll(void);

/**
 * @brief Печатает отчёт watchdog в USB service console.
 */
void watchdog_status_print_report(void);

/**
 * @brief Обрабатывает команду watchdog.
 *
 * @param command Командная строка без завершающего перевода строки.
 *
 * @return 1 если команда обработана.
 */
uint8_t watchdog_status_handle_command(const char* command);
