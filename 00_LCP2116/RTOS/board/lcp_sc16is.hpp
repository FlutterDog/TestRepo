/**
 * @file lcp_sc16is.hpp
 * @brief Одноразовое автоопределение внешних UART SC16IS7xx контроллера LCP.
 *
 * Поддерживаются две аппаратные комплектации: один двухканальный SC16IS752 на
 * UART2_CS либо два одноканальных SC16IS740 на UART2_CS и UART3_CS. Результат
 * probe преобразуется в логические порты PC, HMI и S1, чтобы прикладной код не
 * зависел от варианта платы.
 *
 * SC16IS являются распаянными компонентами и не поддерживают hot-plug. Probe
 * выполняется не более одного раза после старта; повторные вызовы безопасно
 * возвращают сохранённую карту. Файл SC16.txt для выбора варианта не нужен.
 */

#pragma once

#include <stdint.h>

#include "../hal/sc16is7xx.hpp"

/** Линия CS микросхемы логического PC-порта. */
static const uint32_t LCP_SC16IS_UART1_CS = 3U;

/** Линия CS SC16IS752 либо первого SC16IS740. */
static const uint32_t LCP_SC16IS_UART2_CS = 5U;

/** Линия CS второго SC16IS740 в двухмикросхемной комплектации. */
static const uint32_t LCP_SC16IS_UART3_CS = 6U;

/** @brief Тип обнаруженной внешней UART-архитектуры LCP. */
enum LcpSc16isLayout
{
    LCP_SC16IS_LAYOUT_UNKNOWN = 0, /**< Поддерживаемая схема не распознана полностью. */
    LCP_SC16IS_LAYOUT_DUAL_ON_UART2 = 1, /**< SC16IS752: HMI=A, S1=B на UART2_CS. */
    LCP_SC16IS_LAYOUT_TWO_SINGLE_UART2_UART3 = 2 /**< HMI=UART2_CS/A, S1=UART3_CS/A. */
};

/** @brief Дескриптор одного логического канала внешнего UART. */
struct LcpSc16isPort
{
    uint32_t chip_select;   /**< GPIO CS выбранной микросхемы. */
    Sc16isChannel channel;  /**< Канал A или B внутри SC16IS7xx. */
    uint8_t present;        /**< 1 после успешного scratchpad probe. */
};

/**
 * @brief Полный результат автоопределения внешних UART LCP.
 *
 * Поля `pc`, `hmi` и `s1` являются прикладной картой. Поля `uart*_present`
 * оставлены для диагностики физической платы и поиска неисправностей SPI/CS.
 */
struct LcpSc16isMap
{
    LcpSc16isLayout layout; /**< Обнаруженная аппаратная схема. */
    LcpSc16isPort pc;       /**< Диагностический PC-порт на UART1_CS/A. */
    LcpSc16isPort hmi;      /**< Канал HMI с учётом комплектации платы. */
    LcpSc16isPort s1;       /**< Пользовательский порт S1. */

    uint8_t uart1_ch_a_present; /**< Результат probe UART1_CS/A. */
    uint8_t uart2_ch_a_present; /**< Результат probe UART2_CS/A. */
    uint8_t uart2_ch_b_present; /**< Признак независимого канала B UART2_CS. */
    uint8_t uart3_ch_a_present; /**< Результат probe UART3_CS/A. */
};

/**
 * @brief Один раз инициализирует SPI и переводит все CS в неактивный уровень.
 *
 * Повторный вызов ничего не перенастраивает и безопасен для нескольких
 * потребителей board-слоя.
 */
void lcp_sc16is_init_pins(void);

/**
 * @brief Один раз определяет SC16IS740/752 и строит логическую карту.
 *
 * Проверка использует scratchpad-регистр. Повторный вызов возвращается сразу,
 * поскольку hot-plug внешних UART конструкцией не предусмотрен.
 */
void lcp_sc16is_probe(void);

/**
 * @brief Возвращает сохранённую логическую карту.
 *
 * Если probe ещё не выполнялся, функция автоматически запускает его.
 *
 * @return Неизменяемая ссылка со сроком жизни всей программы.
 */
const LcpSc16isMap& lcp_sc16is_get_map(void);

/** @brief Печатает физические каналы и логическое назначение PC/HMI/S1. */
void lcp_sc16is_print_probe_report(void);

/**
 * @brief Инициализирует все обнаруженные каналы для временного echo-теста.
 *
 * FieldSensor позднее перенастраивает только логический S1 согласно baud.TXT и
 * Parity.TXT. PC/HMI остаются в диагностическом режиме.
 *
 * @param[in] baudrate Общая скорость временного echo-теста; 0 игнорируется HAL.
 */
void lcp_sc16is_begin_detected_ports(uint32_t baudrate);
