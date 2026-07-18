/**
 * @file x2x_status.hpp
 * @brief USB-диагностика конфигурации и опроса модулей X2X.
 */

#pragma once

#include <stdint.h>

/** @brief Печатает полный отчёт X2X в SerialUSB. */
void x2x_status_print_report(void);

/**
 * @brief Обрабатывает сервисную команду X2X.
 *
 * Поддерживаются команды:
 * - x2x, x2x status;
 * - x2x reload;
 * - x2x pause, x2x resume;
 * - x2x ldo SLAVE VALUE.
 *
 * @return 1, если команда относится к X2X, иначе 0.
 */
uint8_t x2x_status_handle_command(const char* command);
