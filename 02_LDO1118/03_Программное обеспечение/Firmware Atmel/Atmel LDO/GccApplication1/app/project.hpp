#pragma once

/**
 * @file project.hpp
 * @brief Общие настройки проекта LDO.
 *
 * @details
 * Файл содержит базовые настройки компиляции и подключает общий HAL/compat-слой.
 *
 * Этот файл не должен содержать прикладную логику, карту Modbus-регистров,
 * EEPROM-карту или глобальные переменные.
 */

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <stdint.h>
#include <stdbool.h>

#include "../hal/compat.hpp"

/**
 * @brief Маска линии управления направлением RS-485.
 *
 * @details
 * Для LDO используется PD2.
 * Эта маска используется в Modbus-библиотеке IGAS_mb.
 */
#ifndef IGAS_RS485_TX_MASK
#define IGAS_RS485_TX_MASK _BV(PD2)
#endif