#include "lai_board.hpp"

/**
 * @file lai_board.cpp
 * @brief Реализация аппаратного слоя платы LAI1118 rev2.
 */

namespace
{
    /**
     * @brief Маска линии управления направлением RS-485.
     */
    static const uint8_t RS485_DE_MASK = _BV(PD2);

    /**
     * @brief Маска линии выбора SPI-регистра светодиодов.
     */
    static const uint8_t LED_SS_MASK = _BV(PD3);

    /**
     * @brief Маска линии выбора SPI-регистра режимов аналоговых входов.
     */
    static const uint8_t MODE_SS_MASK = _BV(PD4);

    /**
     * @brief Маска линии разрешения сдвиговых регистров.
     */
    static const uint8_t SHIFT_ENABLE_MASK = _BV(PD5);

    /**
     * @brief Маска адресных входов на порту D.
     */
    static const uint8_t ADDR_D_MASK = static_cast<uint8_t>(_BV(PD6) | _BV(PD7));

    /**
     * @brief Маска адресных входов на порту B.
     */
    static const uint8_t ADDR_B_MASK = static_cast<uint8_t>(_BV(PB0) | _BV(PB1) | _BV(PB2));

    /**
     * @brief Маска светодиода состояния связи.
     */
    static const uint8_t STATUS_LED_MASK = _BV(PC0);

    /**
     * @brief Текущее состояние направления RS-485.
     *
     * @details
     * Ненулевое значение соответствует режиму передачи.
     */
    static volatile uint8_t s_rs485TransmitMode = 0U;

    /**
     * @brief Устанавливает биты в регистре порта.
     *
     * @param port Регистр PORTx.
     * @param mask Маска устанавливаемых битов.
     */
    static inline void setPortBits(volatile uint8_t& port, uint8_t mask)
    {
        port |= mask;
    }

    /**
     * @brief Сбрасывает биты в регистре порта.
     *
     * @param port Регистр PORTx.
     * @param mask Маска сбрасываемых битов.
     */
    static inline void clearPortBits(volatile uint8_t& port, uint8_t mask)
    {
        port &= static_cast<uint8_t>(~mask);
    }

    /**
     * @brief Проверяет, находится ли входная линия в низком уровне.
     *
     * @param pinReg Регистр PINx.
     * @param mask Маска проверяемого входа.
     *
     * @return true — вход находится в низком уровне; false — вход находится в высоком уровне.
     */
    static inline bool isPinLow(volatile uint8_t& pinReg, uint8_t mask)
    {
        return ((pinReg & mask) == 0U);
    }

    /**
     * @brief Активирует выбор SPI-регистра светодиодов.
     */
    static inline void selectLedRegister(void)
    {
        clearPortBits(PORTD, LED_SS_MASK);
    }

    /**
     * @brief Деактивирует выбор SPI-регистра светодиодов.
     */
    static inline void deselectLedRegister(void)
    {
        setPortBits(PORTD, LED_SS_MASK);
    }

    /**
     * @brief Активирует выбор SPI-регистра режимов аналоговых входов.
     */
    static inline void selectModeRegister(void)
    {
        clearPortBits(PORTD, MODE_SS_MASK);
    }

    /**
     * @brief Деактивирует выбор SPI-регистра режимов аналоговых входов.
     */
    static inline void deselectModeRegister(void)
    {
        setPortBits(PORTD, MODE_SS_MASK);
    }
}

namespace lai_board
{
    void initIo(void)
    {
        /*
         * PD2 — управление направлением RS-485.
         * PD3 — выбор SPI-регистра светодиодов.
         * PD4 — выбор SPI-регистра режимов аналоговых входов.
         * PD5 — разрешение сдвиговых регистров.
         */
        DDRD |= static_cast<uint8_t>(
            RS485_DE_MASK |
            LED_SS_MASK |
            MODE_SS_MASK |
            SHIFT_ENABLE_MASK
        );

        clearPortBits(PORTD, RS485_DE_MASK);
        setPortBits(PORTD, static_cast<uint8_t>(LED_SS_MASK | MODE_SS_MASK | SHIFT_ENABLE_MASK));

        /*
         * PD6, PD7 — адресные входы с внутренней подтяжкой.
         */
        DDRD &= static_cast<uint8_t>(~ADDR_D_MASK);
        setPortBits(PORTD, ADDR_D_MASK);

        /*
         * PB0, PB1, PB2 — адресные входы с внутренней подтяжкой.
         * PB2 после чтения аппаратного адреса используется как линия SPI SS.
         */
        DDRB &= static_cast<uint8_t>(~ADDR_B_MASK);
        setPortBits(PORTB, ADDR_B_MASK);

        /*
         * PC0 — светодиод состояния связи.
         */
        DDRC |= STATUS_LED_MASK;
        clearPortBits(PORTC, STATUS_LED_MASK);

        s_rs485TransmitMode = 0U;
    }

    uint8_t readSlaveAddress(void)
    {
        uint8_t address = 0U;

        if (isPinLow(PIND, _BV(PD6)))
        {
            address |= static_cast<uint8_t>(1U << 0);
        }

        if (isPinLow(PIND, _BV(PD7)))
        {
            address |= static_cast<uint8_t>(1U << 1);
        }

        if (isPinLow(PINB, _BV(PB0)))
        {
            address |= static_cast<uint8_t>(1U << 2);
        }

        if (isPinLow(PINB, _BV(PB1)))
        {
            address |= static_cast<uint8_t>(1U << 3);
        }

        if (isPinLow(PINB, _BV(PB2)))
        {
            address |= static_cast<uint8_t>(1U << 4);
        }

        return address;
    }

    void statusLedWrite(bool state)
    {
        if (state)
        {
            setPortBits(PORTC, STATUS_LED_MASK);
        }
        else
        {
            clearPortBits(PORTC, STATUS_LED_MASK);
        }
    }

    void statusLedToggle(void)
    {
        PORTC ^= STATUS_LED_MASK;
    }

    void rs485TransmitMode(void)
    {
        setPortBits(PORTD, RS485_DE_MASK);
        s_rs485TransmitMode = 1U;
    }

    void rs485ReceiveMode(void)
    {
        clearPortBits(PORTD, RS485_DE_MASK);
        s_rs485TransmitMode = 0U;
    }

    bool rs485IsTransmitMode(void)
    {
        return (s_rs485TransmitMode != 0U);
    }

    void setShiftEnable(bool enabled)
    {
        if (enabled)
        {
            setPortBits(PORTD, SHIFT_ENABLE_MASK);
        }
        else
        {
            clearPortBits(PORTD, SHIFT_ENABLE_MASK);
        }
    }

    void writeLedRegister(uint8_t value)
    {
        selectLedRegister();
        (void)hal::spi_txrx(value);
        deselectLedRegister();
    }

    void writeModeRegister(uint8_t value)
    {
        selectModeRegister();
        (void)hal::spi_txrx(value);
        deselectModeRegister();
    }
}