/**
 * @file sc16is_echo_test.hpp
 * @brief Неблокирующий echo-test внешних UART PC и HMI.
 *
 * Логический S1 с момента setup принадлежит FieldSensor и не проходит через
 * временный echo. PC/HMI остаются диагностическими каналами. Частичный TX
 * сохраняется между poll-вызовами и не теряет хвост кадра при заполненном FIFO.
 */

#pragma once

/** @brief Определяет SC16IS и запускает только логические PC/HMI в 9600 8N1. */
void sc16is_echo_test_init(void);

/**
 * @brief Принимает ограниченное число байтов и продолжает отложенный TX.
 *
 * Функция не ждёт FIFO и должна вызываться циклически.
 */
void sc16is_echo_test_poll(void);

/** @brief Печатает сохранённую карту SC16IS один раз после открытия USB. */
void sc16is_echo_test_print_report_once(void);
