
/**
 * @file platform.hpp
 * @brief Верхний платформенный интерфейс проекта.
 *
 * Файл задаёт базовые типы, константы и функции, используемые прикладным кодом.
 * Реализация функций находится внутри проекта и опирается на HAL ATSAM3X8E.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#include "binary_constants.hpp"

using byte = uint8_t;
using boolean = bool;
using word = uint16_t;

#ifndef HIGH
#define HIGH 1U
#endif

#ifndef LOW
#define LOW 0U
#endif

#ifndef INPUT
#define INPUT 0U
#endif

#ifndef OUTPUT
#define OUTPUT 1U
#endif

#ifndef INPUT_PULLUP
#define INPUT_PULLUP 2U
#endif

#ifndef SERIAL_8N1
#define SERIAL_8N1 0U
#endif

#ifndef SERIAL_8O1
#define SERIAL_8O1 1U
#endif

#ifndef SERIAL_8E1
#define SERIAL_8E1 2U
#endif

#ifndef DEC
#define DEC 10U
#endif

#ifndef HEX
#define HEX 16U
#endif

#ifndef OCT
#define OCT 8U
#endif

#ifndef BIN
#define BIN 2U
#endif

#ifndef F
#define F(str) (str)
#endif

#ifndef bit
#define bit(b) (1UL << (b))
#endif

#ifndef _BV
#define _BV(b) (1UL << (b))
#endif

#ifndef bitRead
#define bitRead(value, b) (((value) >> (b)) & 0x01U)
#endif

#ifndef bitSet
#define bitSet(value, b) ((value) |= (1UL << (b)))
#endif

#ifndef bitClear
#define bitClear(value, b) ((value) &= ~(1UL << (b)))
#endif

#ifndef bitWrite
#define bitWrite(value, b, bitvalue) ((bitvalue) ? bitSet(value, b) : bitClear(value, b))
#endif

static const uint32_t A0 = 54U;
static const uint32_t A1 = 55U;
static const uint32_t A2 = 56U;
static const uint32_t A3 = 57U;
static const uint32_t A4 = 58U;
static const uint32_t A5 = 59U;
static const uint32_t A6 = 60U;
static const uint32_t A7 = 61U;

/**
 * @brief Возвращает время работы прошивки в миллисекундах.
 *
 * Значение формируется системным тикером `hal/sam3x_tick`.
 *
 * @return Количество миллисекунд с момента запуска.
 */
uint32_t millis(void);

/**
 * @brief Возвращает время работы прошивки в микросекундах.
 *
 * Значение формируется системным тикером `hal/sam3x_tick`.
 *
 * @return Количество микросекунд с момента запуска.
 */
uint32_t micros(void);

/**
 * @brief Выполняет задержку в миллисекундах.
 *
 * Функция блокирует вызывающий поток выполнения до истечения заданного времени.
 *
 * @param ms Длительность задержки в миллисекундах.
 */
void delay(uint32_t ms);

/**
 * @brief Выполняет задержку в микросекундах.
 *
 * Функция блокирует вызывающий поток выполнения до истечения заданного времени.
 *
 * @param us Длительность задержки в микросекундах.
 */
void delayMicroseconds(uint32_t us);

/**
 * @brief Настраивает режим цифровой линии.
 *
 * @param pin Номер платформенного вывода.
 * @param mode Режим линии: `INPUT`, `OUTPUT` или `INPUT_PULLUP`.
 */
void pinMode(uint32_t pin, uint32_t mode);

/**
 * @brief Устанавливает выходной уровень цифровой линии.
 *
 * @param pin Номер платформенного вывода.
 * @param value Уровень `LOW` или `HIGH`.
 */
void digitalWrite(uint32_t pin, uint32_t value);

/**
 * @brief Считывает входной уровень цифровой линии.
 *
 * @param pin Номер платформенного вывода.
 *
 * @return `LOW` или `HIGH`.
 */
uint32_t digitalRead(uint32_t pin);

#include "serial_port.hpp"
#include "spi.hpp"
#include "../libs/SAM_USB/USBAPI.h"

/**
 * @brief Встроенный последовательный порт X2X.
 */
extern SerialPort Serial;

/**
 * @brief Встроенный последовательный порт S2.
 */
extern SerialPort Serial1;

/**
 * @brief Встроенный последовательный порт S4.
 */
extern SerialPort Serial2;

/**
 * @brief Встроенный последовательный порт S3.
 */
extern SerialPort Serial3;
