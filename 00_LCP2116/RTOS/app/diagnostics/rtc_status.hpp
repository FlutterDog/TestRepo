/**
 * @file rtc_status.hpp
 * @brief Неблокирующая диагностика локального RTC ATSAM3X8E.
 *
 * Команда `rtc set` запускает update state machine и сразу возвращает управление
 * общей LCP task. ACKUPD, запись TIMR/CALR, проверка и timeout выполняются через
 * rtc_status_poll(). Поэтому удалять этот poll из app loop нельзя.
 *
 * Синтаксис пользовательской строки разбирается в rtc_status.cpp, а работа с
 * аппаратными регистрами изолирована в `hal/sam3x_rtc.*`.
 */

#pragma once

#include <stdint.h>

/** @brief Инициализирует HAL RTC и сбрасывает срок ожидаемого update. */
void rtc_status_init(void);

/**
 * @brief Продвигает RTC update state machine и контролирует общий timeout.
 *
 * Функция не ожидает ACKUPD в цикле и должна вызываться на каждом проходе
 * основной прикладной задачи.
 */
void rtc_status_poll(void);

/** @brief Печатает источник slow clock, дату, update state и raw-регистры. */
void rtc_status_print_report(void);

/**
 * @brief Обрабатывает `rtc`, `rtc status` и `rtc set ...`.
 *
 * Формат установки: `YYYY-MM-DD HH:MM:SS`; также принимаются двухзначный год,
 * разделитель точки и буква T. Фактическое завершение проверяется командой
 * `rtc` после последующих poll-вызовов.
 *
 * @param[in] command Нормализованная строка нижнего регистра без CR/LF.
 * @return 1 для распознанной RTC-команды, иначе 0.
 */
uint8_t rtc_status_handle_command(const char* command);
