/**
 * @file lcp_field_ports.hpp
 * @brief Аппаратные Modbus RTU transports пользовательских портов S1..S4.
 */

#pragma once

#include <stdint.h>

#include "../hal/sam3x_uart.hpp"
#include "../protocol/modbus_rtu/modbus_rtu_master.hpp"

/** @brief Стабильный идентификатор физического последовательного порта Lorentz. */
enum LcpFieldPortId : uint8_t
{
    LCP_FIELD_PORT_S1 = 0U, /**< Внешний UART автоматически найденного SC16IS7xx. */
    LCP_FIELD_PORT_S2 = 1U, /**< Встроенный HAL UART port 1. */
    LCP_FIELD_PORT_S3 = 2U, /**< Встроенный HAL UART port 3. */
    LCP_FIELD_PORT_S4 = 3U, /**< Встроенный HAL UART port 2. */
    LCP_FIELD_PORT_COUNT = 4U /**< Количество физических пользовательских портов. */
};

/** @brief Физические параметры одного пользовательского UART. */
struct LcpFieldPortConfig
{
    uint32_t baudrate; /**< Реальная скорость в бодах. */
    HalUartFrame frame; /**< Формат 8N1, 8O1 или 8E1. */
};

/**
 * @brief Инициализирует S1..S4 и их RS-485 direction control.
 *
 * S1 использует карту, полученную автоопределением SC16IS740/752. S2, S3 и S4
 * используют встроенные UART/USART ATSAM3X8E. Повторный вызов безопасно
 * переинициализирует параметры после `field reload`.
 *
 * @param[in] configs Массив настроек в порядке S1, S2, S3, S4. Null сохраняет
 *                    ранее установленные значения или baseline defaults.
 */
void lcp_field_ports_init(
    const LcpFieldPortConfig configs[LCP_FIELD_PORT_COUNT]);

/**
 * @brief Возвращает статический Modbus RTU transport выбранного порта.
 * @param[in] port_id Идентификатор S1..S4.
 * @return Ссылка, действительная в течение всего времени работы прошивки.
 */
const ModbusRtuTransport& lcp_field_port_transport(LcpFieldPortId port_id);

/**
 * @brief Проверяет наличие физического transport.
 * @return Для S1 результат автоопределения SC16IS; для S2..S4 всегда 1.
 */
uint8_t lcp_field_port_present(LcpFieldPortId port_id);

/** @return Краткое имя `S1`, `S2`, `S3` или `S4`. */
const char* lcp_field_port_name(LcpFieldPortId port_id);

/**
 * @brief Возвращает фактически применённые аппаратные параметры.
 * @return Неизменяемая ссылка; некорректный ID нормализуется к S1.
 */
const LcpFieldPortConfig& lcp_field_port_config(LcpFieldPortId port_id);

/**
 * @brief Возвращает накопительный счётчик аппаратных ошибок RX.
 * @return OVRE/FRAME/PARE для встроенных UART; для текущего S1 transport — 0.
 */
uint32_t lcp_field_port_error_count(LcpFieldPortId port_id);
