
/**
 * @file lcp_rs485.hpp
 * @brief Карта встроенных RS-485 линий контроллера LCP.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Линия управления передачей X2X.
 */
static const uint32_t LCP_RS485_PIN_X2X_TRANSMIT = 45U;

/**
 * @brief Линия управления передачей порта S2.
 */
static const uint32_t LCP_RS485_PIN_S2_TRANSMIT = 7U;

/**
 * @brief Линия управления передачей порта S4.
 */
static const uint32_t LCP_RS485_PIN_S4_TRANSMIT = 8U;

/**
 * @brief Линия управления передачей порта S3.
 */
static const uint32_t LCP_RS485_PIN_S3_TRANSMIT = 9U;

/**
 * @brief Уровень включения передачи RS-485.
 */
static const uint8_t LCP_RS485_TRANSMIT_LEVEL = 1U;

/**
 * @brief Уровень приёма RS-485.
 */
static const uint8_t LCP_RS485_RECEIVE_LEVEL = 0U;

/**
 * @brief Инициализирует встроенные линии управления RS-485.
 */
void lcp_rs485_init_builtin_ports(void);
