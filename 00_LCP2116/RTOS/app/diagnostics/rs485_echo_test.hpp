
/**
 * @file rs485_echo_test.hpp
 * @brief Диагностический echo-test встроенных RS-485 портов.
 */

#pragma once

/**
 * @brief Инициализирует встроенные порты для RS-485 echo-test.
 */
void rs485_echo_test_init(void);

/**
 * @brief Обслуживает RS-485 echo-test.
 */
void rs485_echo_test_poll(void);
