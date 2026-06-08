
/**
 * @file ethernet_echo_test.hpp
 * @brief Диагностический TCP echo-test двух W5500.
 */

#pragma once

/**
 * @brief Инициализирует W5500 и TCP echo-server.
 */
void ethernet_echo_test_init(void);

/**
 * @brief Обслуживает TCP echo-server.
 */
void ethernet_echo_test_poll(void);

/**
 * @brief Печатает отчёт Ethernet-инициализации один раз после открытия USB-порта.
 */
void ethernet_echo_test_print_report_once(void);
