/**
 * @file lcp_sc16is.hpp
 * @brief Карта и автоопределение внешних UART SC16IS7xx.
 */

#pragma once

#include <stdint.h>
#include "../hal/sc16is7xx.hpp"

/**
 * @brief Линия CS первого внешнего UART.
 */
static const uint32_t LCP_SC16IS_UART1_CS = 3U;

/**
 * @brief Линия CS второго внешнего UART.
 */
static const uint32_t LCP_SC16IS_UART2_CS = 5U;

/**
 * @brief Линия CS третьего внешнего UART.
 */
static const uint32_t LCP_SC16IS_UART3_CS = 6U;

/**
 * @brief Тип обнаруженной внешней UART-архитектуры.
 */
enum LcpSc16isLayout
{
    LCP_SC16IS_LAYOUT_UNKNOWN = 0,
    LCP_SC16IS_LAYOUT_DUAL_ON_UART2 = 1,
    LCP_SC16IS_LAYOUT_TWO_SINGLE_UART2_UART3 = 2
};

/**
 * @brief Дескриптор внешнего UART-канала.
 */
struct LcpSc16isPort
{
    uint32_t chip_select;
    Sc16isChannel channel;
    uint8_t present;
};

/**
 * @brief Результат автоопределения SC16IS7xx.
 */
struct LcpSc16isMap
{
    LcpSc16isLayout layout;
    LcpSc16isPort pc;
    LcpSc16isPort hmi;
    LcpSc16isPort s1;

    uint8_t uart1_ch_a_present;
    uint8_t uart2_ch_a_present;
    uint8_t uart2_ch_b_present;
    uint8_t uart3_ch_a_present;
};

/**
 * @brief Инициализирует линии CS внешних UART.
 */
void lcp_sc16is_init_pins(void);

/**
 * @brief Выполняет автоопределение внешних UART.
 */
void lcp_sc16is_probe(void);

/**
 * @brief Возвращает результат автоопределения.
 */
const LcpSc16isMap& lcp_sc16is_get_map(void);

/**
 * @brief Печатает результат автоопределения в USB service port.
 */
void lcp_sc16is_print_probe_report(void);

/**
 * @brief Инициализирует обнаруженные UART-каналы для диагностического режима.
 */
void lcp_sc16is_begin_detected_ports(uint32_t baudrate);
