/**
 * @file lcp_sc16is.hpp
 * @brief Карта и автоопределение внешних UART SC16IS7xx контроллера LCP.
 */

#pragma once

#include <stdint.h>

#include "../hal/sc16is7xx.hpp"

/** Линия CS первого внешнего UART. */
static const uint32_t LCP_SC16IS_UART1_CS = 3U;

/** Линия CS второго внешнего UART. */
static const uint32_t LCP_SC16IS_UART2_CS = 5U;

/** Линия CS третьего внешнего UART. */
static const uint32_t LCP_SC16IS_UART3_CS = 6U;

/** @brief Тип обнаруженной внешней UART-архитектуры LCP. */
enum LcpSc16isLayout
{
    LCP_SC16IS_LAYOUT_UNKNOWN = 0, /**< Поддерживаемая схема не распознана. */
    LCP_SC16IS_LAYOUT_DUAL_ON_UART2 = 1, /**< SC16IS752 на UART2_CS. */
    LCP_SC16IS_LAYOUT_TWO_SINGLE_UART2_UART3 = 2 /**< Два SC16IS740 на UART2_CS/UART3_CS. */
};

/** @brief Дескриптор одного логического канала внешнего UART. */
struct LcpSc16isPort
{
    uint32_t chip_select;    /**< GPIO CS выбранной микросхемы. */
    Sc16isChannel channel;   /**< Канал A или B внутри SC16IS7xx. */
    uint8_t present;         /**< Ненулевое значение после успешного probe. */
};

/** @brief Полный результат автоопределения внешних UART LCP. */
struct LcpSc16isMap
{
    LcpSc16isLayout layout; /**< Обнаруженная аппаратная схема. */
    LcpSc16isPort pc;       /**< Канал диагностического PC-порта. */
    LcpSc16isPort hmi;      /**< Канал HMI. */
    LcpSc16isPort s1;       /**< Пользовательский порт S1. */

    uint8_t uart1_ch_a_present; /**< Результат probe UART1_CS/A. */
    uint8_t uart2_ch_a_present; /**< Результат probe UART2_CS/A. */
    uint8_t uart2_ch_b_present; /**< Результат probe UART2_CS/B. */
    uint8_t uart3_ch_a_present; /**< Результат probe UART3_CS/A. */
};

/** @brief Инициализирует и деактивирует все линии CS внешних UART. */
void lcp_sc16is_init_pins(void);

/**
 * @brief Выполняет probe поддерживаемых SC16IS740/752 и строит логическую карту.
 *
 * Повторный вызов полностью обновляет LcpSc16isMap. Файл SC16.txt не нужен.
 */
void lcp_sc16is_probe(void);

/** @return Неизменяемая ссылка на последнюю карту автоопределения. */
const LcpSc16isMap& lcp_sc16is_get_map(void);

/** @brief Печатает подробный результат probe в USB service port. */
void lcp_sc16is_print_probe_report(void);

/**
 * @brief Инициализирует обнаруженные каналы в диагностическом режиме.
 * @param[in] baudrate Общая скорость для временного echo-теста.
 */
void lcp_sc16is_begin_detected_ports(uint32_t baudrate);
