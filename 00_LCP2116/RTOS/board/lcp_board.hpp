/**
 * @file lcp_board.hpp
 * @brief Назначение дискретных линий платы LCP Basic.
 *
 * Файл задаёт платформенные номера линий, используемые прикладным кодом.
 * Инициализация линий выполняется функцией lcp_board_init_gpio().
 */

#pragma once

#include <stdint.h>

#include "../platform/platform.hpp"

static const uint32_t LCP_PIN_WIZ_INT       = 2U;

static const uint32_t LCP_PIN_UART1_CS      = 3U;
static const uint32_t LCP_PIN_UART2_CS      = 5U;
static const uint32_t LCP_PIN_UART3_CS      = 6U;

static const uint32_t LCP_PIN_RS485_DE_1    = 7U;
static const uint32_t LCP_PIN_RS485_DE_2    = 8U;
static const uint32_t LCP_PIN_RS485_DE_3    = 9U;

static const uint32_t LCP_PIN_ETH1_CS       = 10U;
static const uint32_t LCP_PIN_MICROSD_CS    = 23U;
static const uint32_t LCP_PIN_X2X_LED       = 24U;

static const uint32_t LCP_PIN_PLC_OK        = 40U;
static const uint32_t LCP_PIN_SD_OK         = 41U;

static const uint32_t LCP_PIN_X2X_TRANSMIT  = 45U;
static const uint32_t LCP_PIN_RS485_DE_0    = 45U;

static const uint32_t LCP_PIN_ETH2_CS       = 48U;

static const uint32_t LCP_PIN_A0            = A0;
static const uint32_t LCP_PIN_A7            = A7;

static const uint32_t LCP_RS485_TRANSMIT    = HIGH;
static const uint32_t LCP_RS485_RECEIVE     = LOW;

/**
 * @brief Инициализирует дискретные линии платы LCP Basic.
 *
 * Функция задаёт направления GPIO, исходные состояния линий Chip Select,
 * светодиодных выходов и линий управления приёмопередатчиками RS-485.
 */
void lcp_board_init_gpio(void);
