#include "compat.hpp"

/**
 * @file compat_gpio.cpp
 * @brief Реализация HAL-операций GPIO для AVR (pinMode/digitalWrite/digitalRead).
 *
 * @details
 * Реализуется упрощённая нумерация пинов, совместимая с типовой раскладкой Arduino UNO:
 * - 0..7   -> PORTD (бит = p)
 * - 8..13  -> PORTB (бит = p - 8)
 * - 14..19 -> PORTC (бит = p - 14)
 *
 * Для пинов вне поддерживаемого диапазона операции игнорируются (без побочных эффектов).
 */

namespace
{
    /**
     * @brief Описание аппаратного пина (DDR/PORT/PIN + номер бита).
     *
     * @details
     * Поля являются указателями на регистры ввода-вывода AVR. Для неподдерживаемого
     * логического номера `valid == false`, а указатели равны `nullptr`.
     */
    struct PinDesc
    {
        volatile uint8_t* ddr;   /**< Регистр направления (DDRx). */
        volatile uint8_t* port;  /**< Регистр данных/подтяжки (PORTx). */
        volatile uint8_t* pin;   /**< Регистр входного состояния (PINx). */
        uint8_t bit;             /**< Номер бита в выбранном порте. */
        bool valid;              /**< Признак корректного сопоставления. */
    };

    /**
     * @brief Сопоставление логического номера пина с портом/битом AVR.
     *
     * @param p Логический номер пина (см. @ref compat_gpio.cpp).
     * @return Заполненный дескриптор. Если пин не поддерживается, `valid == false`.
     *
     * @note
     * Данная функция не выполняет проверок на конкретный MCU/платформу кроме диапазонов.
     * Корректность нумерации обеспечивается уровнем проекта.
     */
    static inline PinDesc resolve(uint8_t p)
    {
        PinDesc r = { nullptr, nullptr, nullptr, 0u, false };

        if (p <= 7u)
        {
            r.ddr = &DDRD;
            r.port = &PORTD;
            r.pin = &PIND;
            r.bit = p;
            r.valid = true;
            return r;
        }

        if (p <= 13u)
        {
            r.ddr = &DDRB;
            r.port = &PORTB;
            r.pin = &PINB;
            r.bit = (uint8_t)(p - 8u);
            r.valid = true;
            return r;
        }

        if ((p >= 14u) && (p <= 19u))
        {
            r.ddr = &DDRC;
            r.port = &PORTC;
            r.pin = &PINC;
            r.bit = (uint8_t)(p - 14u);
            r.valid = true;
            return r;
        }

        return r;
    }
} // namespace

namespace hal
{
    /**
     * @brief Настройка режима GPIO-линии.
     *
     * @param p    Логический номер пина.
     * @param mode Режим линии: @ref INPUT, @ref OUTPUT, @ref INPUT_PULLUP.
     *
     * @details
     * - OUTPUT: бит в DDR выставляется в 1.
     * - INPUT:  бит в DDR сбрасывается в 0; подтяжка отключается.
     * - INPUT_PULLUP: бит в DDR сбрасывается в 0; подтяжка к Vcc включается через PORT.
     *
     * Для неподдерживаемых номеров пинов функция ничего не делает.
     */
    void pinMode(uint8_t p, PinMode mode)
    {
        const PinDesc d = resolve(p);
        if (!d.valid)
        {
            return;
        }

        const uint8_t mask = (uint8_t)(1u << d.bit);

        if (mode == OUTPUT)
        {
            *d.ddr |= mask;
        }
        else
        {
            *d.ddr &= (uint8_t)~mask;

            if (mode == INPUT_PULLUP)
            {
                *d.port |= mask;
            }
            else
            {
                *d.port &= (uint8_t)~mask;
            }
        }
    }

    /**
     * @brief Установить уровень на GPIO-линии.
     *
     * @param p  Логический номер пина.
     * @param hi true — логическая «1», false — логический «0».
     *
     * @details
     * Функция устанавливает/сбрасывает бит в PORT-регистре выбранного порта.
     * Для входа изменение PORT управляет подтяжкой (если линия сконфигурирована как INPUT).
     *
     * Для неподдерживаемых номеров пинов функция ничего не делает.
     */
    void digitalWrite(uint8_t p, bool hi)
    {
        const PinDesc d = resolve(p);
        if (!d.valid)
        {
            return;
        }

        const uint8_t mask = (uint8_t)(1u << d.bit);

        if (hi)
        {
            *d.port |= mask;
        }
        else
        {
            *d.port &= (uint8_t)~mask;
        }
    }

    /**
     * @brief Прочитать текущее состояние GPIO-линии.
     *
     * @param p Логический номер пина.
     * @return true, если линия в логической «1», иначе false.
     *
     * Для неподдерживаемого номера пина возвращает false.
     */
    bool digitalRead(uint8_t p)
    {
        const PinDesc d = resolve(p);
        if (!d.valid)
        {
            return false;
        }

        const uint8_t mask = (uint8_t)(1u << d.bit);
        return ((*d.pin & mask) != 0u);
    }
} // namespace hal
