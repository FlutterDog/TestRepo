
/**
 * @file rtc_status.hpp
 * @brief Диагностика локального RTC ATSAM3X8E.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Инициализирует диагностику локального RTC.
 */
void rtc_status_init(void);

/**
 * @brief Обслуживает диагностику локального RTC.
 */
void rtc_status_poll(void);

/**
 * @brief Печатает отчёт локального RTC в USB service console.
 */
void rtc_status_print_report(void);

/**
 * @brief Обрабатывает команду локального RTC.
 *
 * @param command Командная строка без завершающего перевода строки.
 *
 * @return 1 если команда обработана.
 */
uint8_t rtc_status_handle_command(const char* command);
