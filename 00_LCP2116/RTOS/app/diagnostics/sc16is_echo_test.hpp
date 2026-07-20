/**
 * @file sc16is_echo_test.hpp
 * @brief Неблокирующий echo-test внешних UART SC16IS7xx.
 *
 * PC/HMI могут оставаться в echo-режиме. Логический S1 должен быть отключён до
 * передачи порта FieldSensor service. Реализация хранит частичный TX offset и
 * не теряет хвост кадра при временно заполненном FIFO.
 */

#pragma once

#include <stdint.h>

/** @brief Определяет SC16IS, назначает PC/HMI/S1 и запускает echo UART. */
void sc16is_echo_test_init(void);

/**
 * @brief Принимает ограниченное число байтов и продолжает отложенный TX.
 *
 * Функция не ожидает FIFO и должна вызываться циклически.
 */
void sc16is_echo_test_poll(void);

/**
 * @brief Передаёт или освобождает владение логическим S1.
 * @param[in] enabled Ненулевое значение разрешает диагностический echo.
 */
void sc16is_echo_test_set_s1_enabled(uint8_t enabled);

/** @return 1, если S1 принадлежит echo-test, иначе 0. */
uint8_t sc16is_echo_test_s1_enabled(void);

/** @brief Печатает сохранённую карту SC16IS один раз после открытия USB. */
void sc16is_echo_test_print_report_once(void);
