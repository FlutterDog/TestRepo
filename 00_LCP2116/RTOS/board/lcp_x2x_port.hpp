/**
 * @file lcp_x2x_port.hpp
 * @brief Аппаратная привязка внутренней шины X2X контроллера LCP2116.
 */

#pragma once

#include <stdint.h>

#include "../protocol/modbus_rtu/modbus_rtu_master.hpp"

/** Скорость штатной шины X2X. */
static const uint32_t LCP_X2X_BAUDRATE = 9600U;

/** @brief Инициализирует UART0 и управление DE/RE шины X2X. */
void lcp_x2x_port_init(void);

/** @brief Возвращает транспорт для Modbus RTU master. */
const ModbusRtuTransport& lcp_x2x_port_transport(void);

/** @brief Возвращает количество аппаратных ошибок приёма UART0. */
uint32_t lcp_x2x_port_error_count(void);
