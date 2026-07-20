/**
 * @file sc16is_echo_test.hpp
 * @brief Диагностический echo-test внешних UART SC16IS7xx.
 */

#pragma once

#include <stdint.h>

/** @brief Инициализирует диагностический echo-test внешних UART. */
void sc16is_echo_test_init(void);

/** @brief Обслуживает разрешённые echo-test внешних UART. */
void sc16is_echo_test_poll(void);

/** @brief Разрешает или запрещает echo-test физического порта S1. */
void sc16is_echo_test_set_s1_enabled(uint8_t enabled);

/** @brief Возвращает 1, если echo-test S1 разрешён. */
uint8_t sc16is_echo_test_s1_enabled(void);

/** @brief Печатает отчёт автоопределения один раз после открытия USB-порта. */
void sc16is_echo_test_print_report_once(void);
