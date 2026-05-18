#include "ldo_board.hpp"

/**
 * @file ldo_board.cpp
 * @brief Реализация аппаратного слоя платы LDO.
 *
 * @details
 * В этом модуле сосредоточена привязка сигналов LDO к портам ATmega328P:
 * - адресный переключатель;
 * - светодиод статуса;
 * - линия управления RS-485 DE/RE;
 * - релейные выходы.
 *
 * Для выбора аппаратной ревизии используется константа LDO_BOARD_REVISION.
 * После изменения ревизии требуется пересборка прошивки.
 */

namespace
{
    /**
     * @brief Выбранная аппаратная ревизия платы.
     *
     * @details
     * Допустимые значения:
     * - ldo_board::BOARD_OLD_RS485_PB2;
     * - ldo_board::BOARD_NEW_RS485_PD2.
     */
    static const ldo_board::BoardRevision LDO_BOARD_REVISION =
        ldo_board::BOARD_NEW_RS485_PD2;

    /**
     * @brief Маска линии светодиода статуса.
     *
     * @details
     * Светодиод статуса подключён к PC0.
     */
    static const uint8_t STATUS_LED_MASK = _BV(PC0);

    /**
     * @brief Маска входов адресного переключателя.
     *
     * @details
     * Используются входы PC1...PC5.
     */
    static const uint8_t ADDRESS_INPUTS_MASK =
        static_cast<uint8_t>(_BV(PC1) | _BV(PC2) | _BV(PC3) | _BV(PC4) | _BV(PC5));

    /**
     * @brief Устанавливает или сбрасывает выбранные биты PORT-регистра.
     *
     * @param port Регистр PORTx.
     * @param mask Маска изменяемых битов.
     * @param state true — установить биты; false — сбросить биты.
     */
    void writePortMask(volatile uint8_t& port, uint8_t mask, bool state)
    {
        if (state)
        {
            port |= mask;
        }
        else
        {
            port &= static_cast<uint8_t>(~mask);
        }
    }

    /**
     * @brief Настраивает линию управления RS-485 как выход.
     *
     * @details
     * Для выбранной аппаратной ревизии настраивает соответствующий DDR-регистр.
     */
    void rs485ConfigureOutput(void)
    {
        if (LDO_BOARD_REVISION == ldo_board::BOARD_OLD_RS485_PB2)
        {
            DDRB |= _BV(PB2);
        }
        else
        {
            DDRD |= _BV(PD2);
        }
    }
}

namespace ldo_board
{
    /**
     * @brief Инициализирует аппаратные линии платы LDO.
     */
    void initIo(void)
    {
        /*
         * Светодиод статуса: PC0, выход, начальное состояние — выключен.
         */
        DDRC |= STATUS_LED_MASK;
        PORTC &= static_cast<uint8_t>(~STATUS_LED_MASK);

        /*
         * Адресный переключатель: PC1...PC5, входы без внутренней подтяжки.
         * Стабильные уровни должны формироваться внешней схемой платы.
         */
        DDRC &= static_cast<uint8_t>(~ADDRESS_INPUTS_MASK);
        PORTC &= static_cast<uint8_t>(~ADDRESS_INPUTS_MASK);

        /*
         * Линия управления RS-485 DE/RE.
         */
        rs485ConfigureOutput();
        rs485ReceiveMode();

        /*
         * Релейные выходы.
         *
         * Ревизия BOARD_OLD_RS485_PB2:
         * - RS-485 DE/RE: PB2;
         * - реле: PD2...PD7, PB0, PB1.
         *
         * Ревизия BOARD_NEW_RS485_PD2:
         * - RS-485 DE/RE: PD2;
         * - реле: PD3...PD7, PB0, PB1, PB2.
         */
        if (LDO_BOARD_REVISION == BOARD_OLD_RS485_PB2)
        {
            DDRD |= static_cast<uint8_t>(
                _BV(PD2) | _BV(PD3) | _BV(PD4) | _BV(PD5) | _BV(PD6) | _BV(PD7)
            );

            DDRB |= static_cast<uint8_t>(
                _BV(PB0) | _BV(PB1)
            );
        }
        else
        {
            DDRD |= static_cast<uint8_t>(
                _BV(PD3) | _BV(PD4) | _BV(PD5) | _BV(PD6) | _BV(PD7)
            );

            DDRB |= static_cast<uint8_t>(
                _BV(PB0) | _BV(PB1) | _BV(PB2)
            );
        }

        relaysWrite(0x00);
    }

    /**
     * @brief Считывает Modbus-адрес с адресного переключателя.
     *
     * @return Modbus-адрес устройства.
     *
     * @details
     * Соответствие входов и битов адреса:
     * - PC5 -> bit 0, 0b00000001;
     * - PC4 -> bit 1, 0b00000010;
     * - PC3 -> bit 2, 0b00000100;
     * - PC2 -> bit 3, 0b00001000;
     * - PC1 -> bit 4, 0b00010000.
     *
     * Если все входы равны нулю, возвращается адрес 1.
     */
    uint8_t readSlaveAddress(void)
    {
        uint8_t addr = 0x00;

        if ((PINC & _BV(PC5)) != 0u) { addr |= 0x01u; }
        if ((PINC & _BV(PC4)) != 0u) { addr |= 0x02u; }
        if ((PINC & _BV(PC3)) != 0u) { addr |= 0x04u; }
        if ((PINC & _BV(PC2)) != 0u) { addr |= 0x08u; }
        if ((PINC & _BV(PC1)) != 0u) { addr |= 0x10u; }

        if (addr == 0x00u)
        {
            addr = 0x01u;
        }

        return addr;
    }

    /**
     * @brief Устанавливает состояние светодиода статуса.
     *
     * @param state true — включить светодиод; false — выключить светодиод.
     */
    void statusLedWrite(bool state)
    {
        writePortMask(PORTC, STATUS_LED_MASK, state);
    }

    /**
     * @brief Инвертирует состояние светодиода статуса.
     */
    void statusLedToggle(void)
    {
        PORTC ^= STATUS_LED_MASK;
    }

    /**
     * @brief Переводит RS-485-трансивер в режим передачи.
     */
    void rs485TransmitMode(void)
    {
        if (LDO_BOARD_REVISION == BOARD_OLD_RS485_PB2)
        {
            PORTB |= _BV(PB2);
        }
        else
        {
            PORTD |= _BV(PD2);
        }
    }

    /**
     * @brief Переводит RS-485-трансивер в режим приёма.
     */
    void rs485ReceiveMode(void)
    {
        if (LDO_BOARD_REVISION == BOARD_OLD_RS485_PB2)
        {
            PORTB &= static_cast<uint8_t>(~_BV(PB2));
        }
        else
        {
            PORTD &= static_cast<uint8_t>(~_BV(PD2));
        }
    }

    /**
     * @brief Проверяет текущее состояние линии управления RS-485.
     *
     * @return true — активен режим передачи.
     * @return false — активен режим приёма.
     */
    bool rs485IsTransmitMode(void)
    {
        if (LDO_BOARD_REVISION == BOARD_OLD_RS485_PB2)
        {
            return ((PORTB & _BV(PB2)) != 0u);
        }

        return ((PORTD & _BV(PD2)) != 0u);
    }

    /**
     * @brief Записывает состояние релейных выходов.
     *
     * @param state Битовое состояние релейных выходов.
     *
     * @details
     * Ревизия BOARD_OLD_RS485_PB2:
     * - bit 0 -> PB1;
     * - bit 1 -> PB0;
     * - bit 2 -> PD7;
     * - bit 3 -> PD6;
     * - bit 4 -> PD5;
     * - bit 5 -> PD4;
     * - bit 6 -> PD3;
     * - bit 7 -> PD2.
     *
     * Ревизия BOARD_NEW_RS485_PD2:
     * - bit 0 -> PB2;
     * - bit 1 -> PB1;
     * - bit 2 -> PB0;
     * - bit 3 -> PD7;
     * - bit 4 -> PD6;
     * - bit 5 -> PD5;
     * - bit 6 -> PD4;
     * - bit 7 -> PD3.
     */
    void relaysWrite(uint8_t state)
    {
        if (LDO_BOARD_REVISION == BOARD_OLD_RS485_PB2)
        {
            writePortMask(PORTB, _BV(PB1), (state & 0x01u) != 0u); // 0b00000001
            writePortMask(PORTB, _BV(PB0), (state & 0x02u) != 0u); // 0b00000010

            writePortMask(PORTD, _BV(PD7), (state & 0x04u) != 0u); // 0b00000100
            writePortMask(PORTD, _BV(PD6), (state & 0x08u) != 0u); // 0b00001000
            writePortMask(PORTD, _BV(PD5), (state & 0x10u) != 0u); // 0b00010000
            writePortMask(PORTD, _BV(PD4), (state & 0x20u) != 0u); // 0b00100000
            writePortMask(PORTD, _BV(PD3), (state & 0x40u) != 0u); // 0b01000000
            writePortMask(PORTD, _BV(PD2), (state & 0x80u) != 0u); // 0b10000000
        }
        else
        {
            writePortMask(PORTB, _BV(PB2), (state & 0x01u) != 0u); // 0b00000001
            writePortMask(PORTB, _BV(PB1), (state & 0x02u) != 0u); // 0b00000010
            writePortMask(PORTB, _BV(PB0), (state & 0x04u) != 0u); // 0b00000100

            writePortMask(PORTD, _BV(PD7), (state & 0x08u) != 0u); // 0b00001000
            writePortMask(PORTD, _BV(PD6), (state & 0x10u) != 0u); // 0b00010000
            writePortMask(PORTD, _BV(PD5), (state & 0x20u) != 0u); // 0b00100000
            writePortMask(PORTD, _BV(PD4), (state & 0x40u) != 0u); // 0b01000000
            writePortMask(PORTD, _BV(PD3), (state & 0x80u) != 0u); // 0b10000000
        }
    }
}