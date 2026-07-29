/**
 * @file watchdog_status.hpp
 * @brief Диагностика watchdog-таймера ATSAM3X8E.
 *
 * Watchdog обслуживается из общей LCP task. Команда `watchdog test reset`
 * намеренно прекращает автоматический feed и должна привести к аппаратному
 * reset; это разрушающий сервисный тест, а не штатный режим работы.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Фиксирует reset cause, увеличивает boot counter и включает watchdog.
 *
 * Параметры timeout/feed заданы в watchdog_status.cpp. При их изменении период
 * feed должен оставаться существенно меньше timeout.
 */
void watchdog_status_init(void);

/**
 * @brief Периодически перезапускает watchdog, если reset-test не вооружён.
 *
 * Функцию нельзя удалять из app loop. Она не содержит ожиданий и выполняет
 * аппаратный restart только по истечении feed period.
 */
void watchdog_status_poll(void);

/** @brief Печатает runtime, last reset, boot counter и raw-регистры. */
void watchdog_status_print_report(void);

/**
 * @brief Обрабатывает `watchdog`, `watchdog feed` и `watchdog test reset`.
 *
 * @param[in] command Нормализованная строка нижнего регистра без CR/LF.
 * @return 1 для распознанной watchdog-команды, иначе 0.
 */
uint8_t watchdog_status_handle_command(const char* command);
