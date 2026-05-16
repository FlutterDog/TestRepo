#pragma once
#ifndef KNOWN_16BIT_TIMERS_H_
#define KNOWN_16BIT_TIMERS_H_

#include <stdint.h>

/**
 * @file known_16bit_timers.h
 * @brief Привязка линий 16-битного таймера (Timer1) к логическим номерам пинов.
 *
 * @details
 * Заголовок содержит константы для типового соответствия Arduino-совместимой нумерации
 * пинов ATmega328P (UNO/Nano) к выводам Timer1:
 * - OC1A / OC1B: выходы аппаратного PWM (Compare Match)
 * - ICP1: вход захвата (Input Capture)
 * - T1: внешний тактовый вход таймера
 *
 * Если проект собирается под другую плату/МК, значения должны быть актуализированы.
 */

/** @brief Логический пин, соответствующий выходу OC1A (Timer1 Compare A). */
static const uint8_t TIMER1_A_PIN = 9;

/** @brief Логический пин, соответствующий выходу OC1B (Timer1 Compare B). */
static const uint8_t TIMER1_B_PIN = 10;

/** @brief Логический пин, соответствующий входу ICP1 (Timer1 Input Capture). */
static const uint8_t TIMER1_ICP_PIN = 8;

/** @brief Логический пин, соответствующий входу T1 (внешний тактовый вход Timer1). */
static const uint8_t TIMER1_CLK_PIN = 5;

#endif // KNOWN_16BIT_TIMERS_H_
