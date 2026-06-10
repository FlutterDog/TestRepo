/**
 * @file sc16is_echo_test.hpp
 * @brief Диагностический echo-test внешних UART SC16IS7xx.
 */

#pragma once

/**
 * @brief Инициализирует диагностический echo-test внешних UART.
 */
void sc16is_echo_test_init(void);

/**
 * @brief Обслуживает echo-test внешних UART.
 */
void sc16is_echo_test_poll(void);

/**
 * @brief Печатает отчёт автоопределения один раз после открытия USB-порта.
 */
void sc16is_echo_test_print_report_once(void);
