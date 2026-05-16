#pragma once

/**
 * @file ADC_init.h
 * @brief Совместимые (Arduino-подобные) утилиты для работы с АЦП ATmega328P.
 *
 * @details
 * Этот заголовок содержит минимальный набор совместимости, необходимый существующему
 * коду проекта:
 * - определения некоторых «битовых» макросов;
 * - псевдо-номера аналоговых пинов A0..A7 (как в Arduino);
 * - обёртки `analogReference()` и `analogRead()` поверх регистров ADC.
 *
 * Реализация ориентирована на ATmega328P (ADC0..ADC7).
 *
 * @warning
 * Заголовок содержит совместимые имена, которые могут конфликтовать с другими библиотеками
 * (например, `min/max`, `bitSet`, `interrupts`). Подключать его следует только там, где
 * действительно требуется совместимость с историческим кодом.
 */

#ifndef F_CPU
/** @brief Тактовая частота MCU (Гц), если не определена настройками проекта. */
#define F_CPU 16000000UL
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>          // abs()
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "../hal/compat.hpp"  // базовые типы/совместимость HAL проекта

/* =========================================================================
 *  Совместимые константы и макросы (минимальный набор, по мере необходимости)
 * ========================================================================= */

/* Бинарные константы (историческая совместимость). */
#ifndef B00000001
#define B00000001 0b00000001
#endif
#ifndef B00000111
#define B00000111 0b00000111
#endif
#ifndef B11110111
#define B11110111 0b11110111
#endif
#ifndef B11111011
#define B11111011 0b11111011
#endif
#ifndef B11111110
#define B11111110 0b11111110
#endif

#ifndef bit
/** @brief Маска одного бита: (1U << n). */
#define bit(n) (1U << (n))
#endif
#ifndef bitSet
/** @brief Установить бит @p n в переменной @p x. */
#define bitSet(x,n)   ((x) |=  (1U << (n)))
#endif
#ifndef bitClear
/** @brief Сбросить бит @p n в переменной @p x. */
#define bitClear(x,n) ((x) &= ~(1U << (n)))
#endif
#ifndef bitRead
/** @brief Прочитать бит @p n из переменной @p x (0/1). */
#define bitRead(x,n)  (((x) >> (n)) & 0x1U)
#endif

/*
 * min/max часто определяются и в других местах (в т.ч. как макросы).
 * Оставляем guard-и, но использовать предпочтительнее std::min/std::max в C++-коде.
 */
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

/* =========================================================================
 *  Псевдо-номера аналоговых пинов (как в Arduino для ATmega328P)
 * ========================================================================= */
#ifndef A0
#define A0 14
#define A1 15
#define A2 16
#define A3 17
#define A4 18
#define A5 19
#define A6 20   /**< ADC6 доступен только на корпусах с выводом ADC6 (например, TQFP). */
#define A7 21   /**< ADC7 доступен только на корпусах с выводом ADC7 (например, TQFP). */
#endif

/* =========================================================================
 *  Управление прерываниями (совместимость)
 * ========================================================================= */
#ifndef noInterrupts
/** @brief Запрет глобальных прерываний. */
#define noInterrupts() cli()
#endif
#ifndef interrupts
/** @brief Разрешение глобальных прерываний. */
#define interrupts()   sei()
#endif

/* =========================================================================
 *  Выбор опорного напряжения АЦП (совместимые идентификаторы)
 * ========================================================================= */
#ifndef DEFAULT
/** @brief Опора АЦП: AVcc (конденсатор на AREF). */
#define DEFAULT   1
#endif
#ifndef INTERNAL
/** @brief Опора АЦП: внутренняя 1.1 В. */
#define INTERNAL  3
#endif

/**
 * @brief Настроить опорное напряжение АЦП.
 *
 * @param mode Режим опоры: @ref DEFAULT или @ref INTERNAL.
 *
 * @details
 * Изменяет только биты REFS1:REFS0 регистра ADMUX. Выбор канала не затрагивается.
 *
 * @warning
 * После переключения опоры рекомендуется выдержать паузу и/или выполнить «холостое»
 * преобразование перед использованием результата (см. datasheet AVR).
 */
static inline void analogReference(uint8_t mode)
{
    if (mode == INTERNAL) {
        /* REFS1:REFS0 = 11 -> Internal 1.1V */
        ADMUX = (uint8_t)((ADMUX & (uint8_t)~(_BV(REFS1) | _BV(REFS0))) | (uint8_t)(_BV(REFS1) | _BV(REFS0)));
    } else {
        /* DEFAULT: REFS1:REFS0 = 01 -> AVcc */
        ADMUX = (uint8_t)((ADMUX & (uint8_t)~(_BV(REFS1) | _BV(REFS0))) | (uint8_t)_BV(REFS0));
    }
}

/**
 * @brief Выполнить одно измерение АЦП (10 бит) на выбранном канале.
 *
 * @param apin Номер аналогового пина (A0..A7) либо 0..7.
 * @return Результат преобразования 0..1023.
 *
 * @details
 * - Канал выбирается по `apin`:
 *   - A0..A7 (14..21) -> ADC0..ADC7;
 *   - 0..7 -> ADC0..ADC7 (допускается для исторического кода);
 *   - иное -> ADC0.
 * - Предделитель АЦП устанавливается в 128, что даёт ~125 кГц при F_CPU=16 МГц.
 * - Запуск преобразования и ожидание окончания выполняются блокирующе.
 *
 * @warning
 * Функция каждый вызов включает ADC и выставляет предделитель. Если код измеряет часто,
 * лучше вынести конфигурацию ADC в отдельную инициализацию, а здесь оставить только
 * выбор канала и старт преобразования.
 */
static inline int analogRead(uint8_t apin)
{
    uint8_t mux = 0;

    if (apin >= 14U && apin <= 21U) {
        mux = (uint8_t)(apin - 14U);         /* A0..A7 */
    } else if (apin <= 7U) {
        mux = apin;                          /* 0..7 -> ADC0..ADC7 */
    } else {
        mux = 0U;
    }

    /* Сохраняем REFS1:REFS0, меняем только MUX[2:0]. */
    const uint8_t refs = (uint8_t)(ADMUX & (uint8_t)(_BV(REFS1) | _BV(REFS0)));
    ADMUX = (uint8_t)(refs | (mux & 0x07U));

    /* Включаем ADC и выставляем предделитель 128: ADPS2:0 = 111. */
    ADCSRA = (uint8_t)(_BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0));

    /* Старт преобразования. */
    ADCSRA |= (uint8_t)_BV(ADSC);
    while ((ADCSRA & (uint8_t)_BV(ADSC)) != 0U) {
        /* busy-wait */
    }

    /* Важно: ADCL читается первым. */
    const uint8_t low  = ADCL;
    const uint8_t high = ADCH;

    return (int)(((uint16_t)high << 8) | (uint16_t)low);
}
