#include "compat.hpp"

/**
 * @file compat_gpio.cpp
 * @brief Реализация базовых GPIO-операций HAL для ATmega328P.
 *
 * @details
 * Файл реализует минимальные совместимые функции:
 * - hal::pinMode();
 * - hal::digitalWrite();
 * - hal::digitalRead().
 *
 * Используется упрощённая проектная нумерация пинов, совместимая
 * с типовой раскладкой Arduino UNO / Nano на ATmega328P:
 *
 * - 0..7   -> PORTD, bit 0..7;
 * - 8..13  -> PORTB, bit 0..5;
 * - 14..19 -> PORTC, bit 0..5.
 *
 * Для LDO этот файл является универсальным compatibility-слоем.
 * Платозависимая логика LDO — реле, RS485 direction, светодиод статуса,
 * адресные переключатели — должна находиться выше, например в app/ldo_board.*.
 *
 * @note
 * Пины PC6 / RESET не поддерживаются.
 *
 * @note
 * Для неподдерживаемых номеров пинов операции игнорируются.
 *
 * @warning
 * Операции изменения PORT/DDR выполняются через read-modify-write.
 * Если один и тот же порт изменяется одновременно из основного кода и ISR,
 * вызывающая сторона должна сама обеспечить синхронизацию.
 */

namespace
{
    /**
     * @brief Описание аппаратного GPIO-пина AVR.
     *
     * @details
     * Структура связывает проектный номер пина с регистрами:
     * - DDRx — направление линии;
     * - PORTx — выходной уровень или подтяжка;
     * - PINx — текущее входное состояние.
     */
    struct PinDesc
    {
        volatile uint8_t* ddr;   /**< Регистр направления DDRx. */
        volatile uint8_t* port;  /**< Регистр PORTx. */
        volatile uint8_t* pin;   /**< Регистр входного состояния PINx. */
        uint8_t bit;             /**< Номер бита внутри порта. */
        bool valid;              /**< true, если номер пина поддерживается. */
    };

    /**
     * @brief Сопоставляет проектный номер пина с портом ATmega328P.
     *
     * @param p Проектный номер пина.
     *
     * @return Дескриптор пина.
     *
     * @details
     * Таблица соответствия:
     *
     * - p = 0..7:
     *   - PORTD;
     *   - bit = p.
     *
     * - p = 8..13:
     *   - PORTB;
     *   - bit = p - 8.
     *
     * - p = 14..19:
     *   - PORTC;
     *   - bit = p - 14.
     *
     * Для неподдерживаемых номеров возвращается дескриптор с valid == false.
     */
    static inline PinDesc resolve(uint8_t p)
    {
        PinDesc r = { 0, 0, 0, 0u, false };

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
            r.bit = static_cast<uint8_t>(p - 8u);
            r.valid = true;
            return r;
        }

        if ((p >= 14u) && (p <= 19u))
        {
            r.ddr = &DDRC;
            r.port = &PORTC;
            r.pin = &PINC;
            r.bit = static_cast<uint8_t>(p - 14u);
            r.valid = true;
            return r;
        }

        return r;
    }

    /**
     * @brief Формирует битовую маску по номеру бита.
     *
     * @param bit Номер бита внутри порта.
     *
     * @return Маска вида 1 << bit.
     */
    static inline uint8_t bitMask(uint8_t bit)
    {
        return static_cast<uint8_t>(1u << bit);
    }
}

namespace hal
{
    /**
     * @brief Настраивает режим GPIO-линии.
     *
     * @param p Проектный номер пина.
     * @param mode Режим линии:
     * - INPUT — вход без внутренней подтяжки;
     * - OUTPUT — цифровой выход;
     * - INPUT_PULLUP — вход с внутренней подтяжкой к Vcc.
     *
     * @details
     * Поведение режимов:
     *
     * - OUTPUT:
     *   - соответствующий бит DDRx устанавливается в 1;
     *   - текущее значение PORTx не изменяется.
     *
     * - INPUT:
     *   - соответствующий бит DDRx сбрасывается в 0;
     *   - соответствующий бит PORTx сбрасывается в 0;
     *   - внутренняя подтяжка отключается.
     *
     * - INPUT_PULLUP:
     *   - соответствующий бит DDRx сбрасывается в 0;
     *   - соответствующий бит PORTx устанавливается в 1;
     *   - внутренняя подтяжка включается.
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

        const uint8_t mask = bitMask(d.bit);

        if (mode == OUTPUT)
        {
            *d.ddr |= mask;
            return;
        }

        *d.ddr &= static_cast<uint8_t>(~mask);

        if (mode == INPUT_PULLUP)
        {
            *d.port |= mask;
        }
        else
        {
            *d.port &= static_cast<uint8_t>(~mask);
        }
    }

    /**
     * @brief Устанавливает уровень цифровой линии.
     *
     * @param p Проектный номер пина.
     * @param hi true — установить логическую единицу, false — установить логический ноль.
     *
     * @details
     * Для линии, настроенной как OUTPUT, функция задаёт выходной уровень.
     *
     * Для линии, настроенной как INPUT, запись в PORTx управляет внутренней подтяжкой:
     * - true — подтяжка включена;
     * - false — подтяжка выключена.
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

        const uint8_t mask = bitMask(d.bit);

        if (hi)
        {
            *d.port |= mask;
        }
        else
        {
            *d.port &= static_cast<uint8_t>(~mask);
        }
    }

    /**
     * @brief Читает текущее состояние цифровой линии.
     *
     * @param p Проектный номер пина.
     *
     * @return true, если на входе логическая единица.
     * @return false, если на входе логический ноль или номер пина не поддерживается.
     *
     * @details
     * Чтение выполняется из регистра PINx.
     */
    bool digitalRead(uint8_t p)
    {
        const PinDesc d = resolve(p);

        if (!d.valid)
        {
            return false;
        }

        const uint8_t mask = bitMask(d.bit);

        return ((*d.pin & mask) != 0u);
    }
}