/**
 * @file diagnostic_output.hpp
 * @brief Общие функции форматирования USB service console.
 *
 * Этот заголовок является единой точкой оформления диагностических отчётов.
 * При добавлении нового раздела не следует вручную копировать длинные строки
 * из символов `=` или собственный расчёт процентов/uptime. Используйте функции
 * ниже, чтобы отдельная команда и общий `status` выглядели одинаково.
 */

#pragma once

#include <stdint.h>

#include "../../hal/sam3x_uart.hpp"
#include "../../platform/platform.hpp"

/**
 * @brief Печатает крупный заголовок полного отчёта.
 *
 * Используется только для верхнего уровня, например `status`, `help` и `rtos`.
 * Вложенные подсистемы должны использовать diagnostic_print_section().
 *
 * @param title Короткое имя отчёта без перевода строки. Null допускается и
 *              печатается как пустой заголовок.
 */
inline void diagnostic_print_banner(const char* title)
{
    SerialUSB.print("\r\n============================================================\r\n");
    SerialUSB.print((title != 0) ? title : "");
    SerialUSB.print("\r\n============================================================\r\n");
}

/**
 * @brief Печатает заголовок самостоятельной диагностической подсистемы.
 *
 * Эту функцию следует вызывать первой в `*_print_report()`. Тогда один и тот
 * же отчёт остаётся читаемым как при прямой команде (`field`, `eth`, `x2x`),
 * так и внутри общего `status`.
 *
 * @param title Название подсистемы без квадратных скобок и перевода строки.
 */
inline void diagnostic_print_section(const char* title)
{
    SerialUSB.print("\r\n[ ");
    SerialUSB.print((title != 0) ? title : "");
    SerialUSB.print(" ]\r\n");
}

/**
 * @brief Печатает заголовок логической группы внутри одного отчёта.
 *
 * Пример: группы `RAM`, `FreeRTOS heap` и `LCP task stack` в команде `rtos`.
 *
 * @param title Название группы.
 */
inline void diagnostic_print_group(const char* title)
{
    SerialUSB.print("\r\n-- ");
    SerialUSB.print((title != 0) ? title : "");
    SerialUSB.print(" --\r\n");
}

/**
 * @brief Возвращает человекочитаемое имя формата UART.
 *
 * Используйте эту функцию во всех диагностических отчётах S1..S4 и встроенных
 * UART. При добавлении нового HalUartFrame сначала реализуйте его в HAL, затем
 * добавьте соответствующую строку в этот switch.
 *
 * @param frame Формат кадра HAL.
 * @return Статическая строка `8N1`, `8O1` или `8E1`.
 */
inline const char* diagnostic_uart_frame_text(HalUartFrame frame)
{
    switch (frame)
    {
        case HAL_UART_FRAME_8O1:
            return "8O1";
        case HAL_UART_FRAME_8E1:
            return "8E1";
        case HAL_UART_FRAME_8N1:
        default:
            return "8N1";
    }
}

/**
 * @brief Печатает долю `part / total` в процентах с одной десятичной цифрой.
 *
 * Вычисление выполняется целочисленно и не подключает printf или float.
 * Значение насыщается на 100.0 %, если `part` больше `total`. При нулевом
 * `total` печатается `n/a`.
 *
 * @param part Использованная или свободная часть.
 * @param total Полный объём.
 */
inline void diagnostic_print_percent(uint32_t part, uint32_t total)
{
    if (total == 0U)
    {
        SerialUSB.print("n/a");
        return;
    }

    if (part > total)
    {
        part = total;
    }

    const uint32_t tenths = static_cast<uint32_t>(
        (static_cast<uint64_t>(part) * 1000ULL + (total / 2U)) / total);

    SerialUSB.print(static_cast<unsigned long>(tenths / 10U));
    SerialUSB.print(".");
    SerialUSB.print(static_cast<unsigned long>(tenths % 10U));
    SerialUSB.print("%");
}

/**
 * @brief Печатает длительность в привычном виде `D d HH:MM:SS`.
 *
 * Функция предназначена для human-readable представления `millis()`. Сырое
 * значение миллисекунд следует печатать отдельно для отладки. ATSAM3X8E
 * использует 32-битный `millis()`, поэтому после примерно 49.7 суток счётчик
 * переполняется; все внутренние интервалы прошивки рассчитаны с учётом такого
 * переполнения.
 *
 * @param milliseconds Длительность в миллисекундах.
 */
inline void diagnostic_print_duration(uint32_t milliseconds)
{
    const uint32_t total_seconds = milliseconds / 1000U;
    const uint32_t days = total_seconds / 86400U;
    const uint32_t hours = (total_seconds / 3600U) % 24U;
    const uint32_t minutes = (total_seconds / 60U) % 60U;
    const uint32_t seconds = total_seconds % 60U;

    SerialUSB.print(static_cast<unsigned long>(days));
    SerialUSB.print(" d ");

    if (hours < 10U)
    {
        SerialUSB.print("0");
    }
    SerialUSB.print(static_cast<unsigned long>(hours));
    SerialUSB.print(":");

    if (minutes < 10U)
    {
        SerialUSB.print("0");
    }
    SerialUSB.print(static_cast<unsigned long>(minutes));
    SerialUSB.print(":");

    if (seconds < 10U)
    {
        SerialUSB.print("0");
    }
    SerialUSB.print(static_cast<unsigned long>(seconds));
}
