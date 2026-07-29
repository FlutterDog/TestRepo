#include "compat.hpp"

#include <avr/interrupt.h>
#include <util/delay_basic.h>

/**
 * @file compat_tick.cpp
 * @brief Миллисекундный системный тикер и блокирующие задержки для AVR.
 *
 * @details
 * Модуль реализует:
 * - системный счётчик миллисекунд на Timer0;
 * - функцию hal::millis();
 * - функцию hal::delay();
 * - функцию hal::delayMicroseconds().
 *
 * Timer0 работает в режиме CTC. При F_CPU = 16000000 Гц используется
 * предделитель 64 и значение OCR0A = 249, что даёт частоту прерывания
 * 1000 Гц и период системного тика 1 мс.
 *
 * @warning
 * При изменении F_CPU необходимо проверить настройки Timer0 и точность
 * delayMicroseconds().
 */

/**
 * @brief Счётчик миллисекунд с момента запуска системного тикера.
 *
 * @details
 * Значение увеличивается в обработчике прерывания TIMER0_COMPA_vect.
 *
 * @note
 * Переменная имеет размер 32 бита. На восьмибитных AVR чтение такого значения
 * должно выполняться атомарно относительно прерываний.
 */
static volatile uint32_t s_millis = 0;

namespace
{
    /**
     * @brief Атомарно читает счётчик миллисекунд.
     *
     * @return Текущее значение счётчика миллисекунд.
     *
     * @details
     * На восьмибитных AVR чтение 32-битной переменной может быть прервано
     * обработчиком ISR. Функция кратковременно запрещает прерывания,
     * считывает значение и восстанавливает прежнее состояние регистра SREG.
     */
    static inline uint32_t atomic_read_millis(void)
    {
        const uint8_t sreg = SREG;

        cli();

        const uint32_t value = s_millis;

        SREG = sreg;

        return value;
    }
}

namespace hal
{
    /**
     * @brief Инициализирует системный миллисекундный тикер.
     *
     * @details
     * Настраивает Timer0:
     * - режим CTC;
     * - предделитель 64;
     * - сравнение по OCR0A;
     * - прерывание TIMER0_COMPA_vect.
     *
     * Для F_CPU = 16000000 Гц:
     * 16000000 / 64 / (249 + 1) = 1000 Гц.
     */
    void tick_init(void)
    {
        TCCR0A = static_cast<uint8_t>(_BV(WGM01));
        TCCR0B = static_cast<uint8_t>(_BV(CS01) | _BV(CS00));
        OCR0A  = 249u;
        TIMSK0 = static_cast<uint8_t>(_BV(OCIE0A));
    }

    /**
     * @brief Возвращает количество миллисекунд с момента запуска тикера.
     *
     * @return Время в миллисекундах.
     *
     * @note
     * Значение переполняется по модулю 2^32. Интервалы времени следует
     * вычислять через разность беззнаковых значений.
     */
    uint32_t millis(void)
    {
        return atomic_read_millis();
    }

    /**
     * @brief Выполняет блокирующую задержку в миллисекундах.
     *
     * @param ms Длительность задержки в миллисекундах.
     *
     * @details
     * Сравнение времени выполняется через разность uint32_t, поэтому корректно
     * работает при переполнении счётчика миллисекунд.
     */
    void delay(uint32_t ms)
    {
        const uint32_t start = millis();

        while (static_cast<uint32_t>(millis() - start) < ms)
        {
            /*
             * Ожидание истечения заданного интервала.
             */
        }
    }

    /**
     * @brief Выполняет блокирующую задержку в микросекундах.
     *
     * @param us Длительность задержки в микросекундах.
     *
     * @details
     * Задержка построена на _delay_loop_2(). Один цикл _delay_loop_2(N)
     * занимает приблизительно 4 * N тактов CPU.
     *
     * Для F_CPU = 16000000 Гц используется 4 итерации на 1 мкс.
     *
     * @warning
     * Точность функции зависит от F_CPU, оптимизации компилятора и накладных
     * расходов на вызов функции и цикл.
     */
    void delayMicroseconds(uint16_t us)
    {
        const uint16_t iters_per_us =
            static_cast<uint16_t>(F_CPU / 4000000UL);

        if (iters_per_us == 0u)
        {
            return;
        }

        while (us-- != 0u)
        {
            _delay_loop_2(iters_per_us);
        }
    }
}

/**
 * @brief Обработчик прерывания системного миллисекундного тикера.
 *
 * @details
 * Вызывается Timer0 при совпадении TCNT0 с OCR0A.
 * Увеличивает системный счётчик миллисекунд.
 */
ISR(TIMER0_COMPA_vect)
{
    ++s_millis;
}