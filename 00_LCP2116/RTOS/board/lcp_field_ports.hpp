/**
 * @file lcp_field_ports.hpp
 * @brief Аппаратные транспорты пользовательских RS-485 портов S1..S4.
 */

#pragma once

#include <stdint.h>

#include "../hal/sam3x_uart.hpp"
#include "../protocol/modbus_rtu/modbus_rtu_master.hpp"

/** @brief Идентификатор пользовательского последовательного порта Lorentz. */
enum LcpFieldPortId : uint8_t
{
    LCP_FIELD_PORT_S1 = 0U,
    LCP_FIELD_PORT_S2 = 1U,
    LCP_FIELD_PORT_S3 = 2U,
    LCP_FIELD_PORT_S4 = 3U,
    LCP_FIELD_PORT_COUNT = 4U
};

/** @brief Физические параметры одного пользовательского UART. */
struct LcpFieldPortConfig
{
    uint32_t baudrate;
    HalUartFrame frame;
};

/**
 * @brief Инициализирует S1..S4 для прикладного Modbus RTU.
 *
 * S1 использует обнаруженный SC16IS7xx. S2, S3 и S4 используют встроенные
 * UART/USART ATSAM3X8E. Все четыре порта поддерживают 8N1, 8O1 и 8E1.
 */
void lcp_field_ports_init(const LcpFieldPortConfig configs[LCP_FIELD_PORT_COUNT]);

/** @brief Возвращает транспорт Modbus RTU выбранного порта. */
const ModbusRtuTransport& lcp_field_port_transport(LcpFieldPortId port_id);

/** @brief Возвращает 1, если физический порт доступен. */
uint8_t lcp_field_port_present(LcpFieldPortId port_id);

/** @brief Возвращает краткое имя порта: S1, S2, S3 или S4. */
const char* lcp_field_port_name(LcpFieldPortId port_id);

/** @brief Возвращает применённую конфигурацию порта. */
const LcpFieldPortConfig& lcp_field_port_config(LcpFieldPortId port_id);

/** @brief Возвращает накопительный счётчик аппаратных ошибок UART. */
uint32_t lcp_field_port_error_count(LcpFieldPortId port_id);
