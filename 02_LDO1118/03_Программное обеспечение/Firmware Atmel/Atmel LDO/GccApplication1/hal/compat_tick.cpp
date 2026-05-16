#include "compat.hpp"
#include <avr/interrupt.h>
#include <util/delay_basic.h>  // _delay_loop_2()

/**
 * @file compat_tick.cpp
 * @brief Базовый миллисекундный тикер и задержки для AVR.
 *
 * @details
 * Реализует:
 * - миллисекундный счётчик на Timer0 в режиме CTC;
 * - функции `hal::millis()`, `hal::delay()`, `hal::delayMicroseconds()`.
 *
 * Конфигурация `tick_init()` рассчитана на @c F_CPU=16 МГц:
 * предделитель /64, OCR0A=249 -> 1000 Гц (1 мс).
 *
 * @note Если @c F_CPU отличается, требуется пересчитать OCR0A и/или предделитель,
 *       иначе частота тикера будет неверной.
 */

/**
 * @brief Счётчик миллисекунд, инкрементируемый в ISR Timer0 COMPA.
 *
 * @note Переменная 32-битная; чтение/запись неатомарны на 8-битных AVR.
 */
static volatile uint32_t s_millis = 0;

namespace
{
    /**
     * @brief Атомарно прочитать 32-битный счётчик миллисекунд.
     *
     * @details
     * На 8-битных AVR чтение 32-битной переменной может быть прервано ISR.
     * Эта функция кратковременно запрещает прерывания, чтобы получить согласованное значение.
     */
    static inline uint32_t atomic_read_millis()
    {
        const uint8_t sreg = SREG;
        cli();
        const uint32_t v = s_millis;
        SREG = sreg;
        return v;
    }
} // namespace

namespace hal
{
    void tick_init(void)
    {
        /*
         * Timer0:
         * - CTC (WGM01=1);
         * - prescaler /64 (CS01|CS00);
         * - OCR0A=249 -> 16e6/64/(249+1)=1000 Hz.
         *
         * Примечание: намеренно не трогаем TCNT0: старт с 0 допустим,
         * но если таймер уже использовался, счётчик может быть не нулевым.
         */
        TCCR0A = (uint8_t)_BV(WGM01);
        TCCR0B = (uint8_t)(_BV(CS01) | _BV(CS00));
        OCR0A  = 249u;
        TIMSK0 = (uint8_t)_BV(OCIE0A);
    }

    uint32_t millis(void)
    {
        /* Возвращаем согласованное значение, безопасное относительно ISR. */
        return atomic_read_millis();
    }

    void delay(uint32_t ms)
    {
        const uint32_t start = millis();

        /*
         * Сравнение через разницу корректно работает при переполнении счётчика (uint32_t wrap-around).
         * Требование: `millis()` должен быть монотонным (в рамках переполнения).
         */
        while ((uint32_t)(millis() - start) < ms) {
            /* busy-wait */
        }
    }

    void delayMicroseconds(uint16_t us)
    {
        /*
         * _delay_loop_2(N) выполняет цикл длительностью примерно 4*N тактов.
         * Подбираем N так, чтобы один вызов соответствовал ~1 мкс.
         *
         * iters_per_us = F_CPU / 4_000_000.
         * Для F_CPU=16 МГц -> iters_per_us = 4 (порядка 16 тактов на 1 мкс).
         */
        const uint16_t iters_per_us = (uint16_t)(F_CPU / 4000000UL);
        if (iters_per_us == 0u) {
            return;
        }

        while (us-- != 0u) {
            _delay_loop_2(iters_per_us);
        }
    }
} // namespace hal

ISR(TIMER0_COMPA_vect)
{
    s_millis++;
}
