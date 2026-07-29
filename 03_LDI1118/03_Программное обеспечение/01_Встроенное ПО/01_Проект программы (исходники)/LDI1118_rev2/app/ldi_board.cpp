#include "ldi_board.hpp"

/**
 * @file ldi_board.cpp
 * @brief Реализация аппаратного слоя платы LDI.
 *
 * @details
 * В этом модуле сосредоточена привязка сигналов LDI к портам ATmega328P:
 * - адресный переключатель PC0...PC4;
 * - RS-485 direction на PD2;
 * - 74HC165: latch PD3, clock PD4, data PD5;
 * - 74HC595: latch PD3, clock PD4, data PB1;
 * - светодиод состояния связи PB0.
 *
 * Линии latch и clock используются совместно для 74HC165 и 74HC595.
 * Поэтому при чтении входов аппаратный слой сохраняет корректное состояние
 * LED-панели и восстанавливает его после тактирования 74HC165.
 */

namespace
{
    // -------------------------------------------------------------------------
    // Адресный переключатель
    // -------------------------------------------------------------------------

    /**
     * @brief Маска входов адресного переключателя.
     *
     * @details
     * Используются линии PC0...PC4.
     */
    static const uint8_t ADDRESS_INPUTS_MASK =
        static_cast<uint8_t>(_BV(PC0) | _BV(PC1) | _BV(PC2) | _BV(PC3) | _BV(PC4));

    // -------------------------------------------------------------------------
    // RS-485
    // -------------------------------------------------------------------------

    /**
     * @brief Маска линии управления направлением RS-485.
     *
     * @details
     * Линия DE/RE RS-485-трансивера подключена к PD2.
     */
    static const uint8_t RS485_DE_MASK = _BV(PD2);

    // -------------------------------------------------------------------------
    // Светодиод состояния связи
    // -------------------------------------------------------------------------

    /**
     * @brief Маска светодиода состояния связи.
     *
     * @details
     * Светодиод подключён к PB0.
     */
    static const uint8_t STATUS_LED_MASK = _BV(PB0);

    // -------------------------------------------------------------------------
    // Сдвиговые регистры
    // -------------------------------------------------------------------------

    /**
     * @brief Маска линии latch для 74HC165 и 74HC595.
     *
     * @details
     * Линия подключена к PD3.
     */
    static const uint8_t SR_LATCH_MASK = _BV(PD3);

    /**
     * @brief Маска линии clock для 74HC165 и 74HC595.
     *
     * @details
     * Линия подключена к PD4.
     */
    static const uint8_t SR_CLOCK_MASK = _BV(PD4);

    /**
     * @brief Маска входа данных от 74HC165.
     *
     * @details
     * Линия подключена к PD5.
     */
    static const uint8_t SR_INPUT_DATA_MASK = _BV(PD5);

    /**
     * @brief Маска выхода данных на 74HC595.
     *
     * @details
     * Линия подключена к PB1.
     */
    static const uint8_t SR_LED_DATA_MASK = _BV(PB1);

    /**
     * @brief Последняя маска, выведенная на LED-панель.
     *
     * @details
     * Используется для восстановления LED-панели после чтения входов,
     * так как 74HC165 и 74HC595 имеют общие линии latch и clock.
     */
    static uint16_t s_lastPanelMask = 0x0000u;

    // -------------------------------------------------------------------------
    // Низкоуровневые операции
    // -------------------------------------------------------------------------

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
     * @brief Формирует короткую паузу между фронтами управляющих сигналов.
     *
     * @details
     * Используется при ручном тактировании сдвиговых регистров.
     */
    inline void ioNop(void)
    {
        asm volatile ("nop");
    }

    /**
     * @brief Устанавливает уровень линии данных 74HC595.
     *
     * @param state true — логическая единица; false — логический ноль.
     */
    void ledShiftDataWrite(bool state)
    {
        writePortMask(PORTB, SR_LED_DATA_MASK, state);
    }

    /**
     * @brief Формирует тактовый импульс для сдвиговых регистров.
     *
     * @details
     * Последовательность импульса: низкий уровень -> высокий уровень -> низкий уровень.
     */
    void shiftClockPulse(void)
    {
        PORTD |= SR_CLOCK_MASK;
        ioNop();

        PORTD &= static_cast<uint8_t>(~SR_CLOCK_MASK);
        ioNop();
    }

    /**
     * @brief Формирует импульс latch для записи данных в 74HC595.
     *
     * @details
     * Последовательность импульса: низкий уровень -> высокий уровень -> низкий уровень.
     */
    void latch595Pulse(void)
    {
        PORTD |= SR_LATCH_MASK;
        ioNop();

        PORTD &= static_cast<uint8_t>(~SR_LATCH_MASK);
        ioNop();
    }

    /**
     * @brief Выполняет захват состояния параллельных входов 74HC165.
     *
     * @details
     * Для 74HC165 линия SH/LD активна низким уровнем.
     * Последовательность:
     * - перевод линии в высокий уровень;
     * - кратковременный низкий уровень для загрузки параллельных входов;
     * - возврат в высокий уровень для последовательного сдвига.
     */
    void capture165Inputs(void)
    {
        PORTD |= SR_LATCH_MASK;
        ioNop();

        PORTD &= static_cast<uint8_t>(~SR_LATCH_MASK);
        ioNop();

        PORTD |= SR_LATCH_MASK;
        ioNop();
    }

    /**
     * @brief Оставляет latch-линию в низком уровне.
     *
     * @details
     * Это исходное состояние линии между циклами обмена.
     */
    void latchLow(void)
    {
        PORTD &= static_cast<uint8_t>(~SR_LATCH_MASK);
    }

    /**
     * @brief Зеркально разворачивает младшие пять бит.
     *
     * @param value Входное значение.
     *
     * @return Значение с развёрнутыми пятью младшими битами.
     *
     * @details
     * Используется для преобразования порядка битов адресного переключателя.
     */
    uint8_t mirrorFiveBits(uint8_t value)
    {
        uint8_t result = 0u;

        if ((value & 0x01u) != 0u) { result |= 0x10u; } // PC0 -> bit 4
        if ((value & 0x02u) != 0u) { result |= 0x08u; } // PC1 -> bit 3
        if ((value & 0x04u) != 0u) { result |= 0x04u; } // PC2 -> bit 2
        if ((value & 0x08u) != 0u) { result |= 0x02u; } // PC3 -> bit 1
        if ((value & 0x10u) != 0u) { result |= 0x01u; } // PC4 -> bit 0

        return result;
    }

    /**
     * @brief Возвращает состояние бита в маске.
     *
     * @param value Битовая маска.
     * @param bit Номер бита.
     *
     * @return true — бит установлен; false — бит сброшен.
     */
    bool isBitSet(uint16_t value, uint8_t bit)
    {
        return ((value & static_cast<uint16_t>(1u << bit)) != 0u);
    }

    /**
     * @brief Передаёт маску в 74HC595 без изменения сохранённого состояния.
     *
     * @param state Битовая маска светодиодов.
     *
     * @details
     * Сначала передаются исходные bit 7...bit 0, затем bit 15...bit 8.
     * Такой порядок соответствует разводке LED-панели.
     */
    void transferPanelMask(uint16_t state)
    {
        for (int8_t bit = 7; bit >= 0; --bit)
        {
            ledShiftDataWrite(isBitSet(state, static_cast<uint8_t>(bit)));
            shiftClockPulse();
        }

        for (int8_t bit = 15; bit >= 8; --bit)
        {
            ledShiftDataWrite(isBitSet(state, static_cast<uint8_t>(bit)));
            shiftClockPulse();
        }

        ledShiftDataWrite(false);
        latch595Pulse();
    }

    /**
     * @brief Восстанавливает состояние LED-панели после чтения входов.
     *
     * @details
     * При чтении 74HC165 линия clock также тактирует 74HC595. Поэтому после
     * чтения входов необходимо повторно загрузить последнюю LED-маску.
     */
    void restorePanelMask(void)
    {
        transferPanelMask(s_lastPanelMask);
    }
}

namespace ldi_board
{
    /**
     * @brief Инициализирует аппаратные линии платы LDI.
     */
    void initIo(void)
    {
        /*
         * Адресный переключатель: PC0...PC4, входы с внутренней подтяжкой.
         * Переключатели активны низким уровнем.
         */
        DDRC &= static_cast<uint8_t>(~ADDRESS_INPUTS_MASK);
        PORTC |= ADDRESS_INPUTS_MASK;

        /*
         * UART/RS-485:
         * - PD0 RXD, вход без внутренней подтяжки;
         * - PD1 TXD, выход;
         * - PD2 DE/RE, выход, начально режим приёма.
         */
        DDRD &= static_cast<uint8_t>(~_BV(PD0));
        DDRD |= static_cast<uint8_t>(_BV(PD1) | RS485_DE_MASK);

        PORTD &= static_cast<uint8_t>(~_BV(PD0));
        PORTD &= static_cast<uint8_t>(~_BV(PD1));
        rs485ReceiveMode();

        /*
         * Сдвиговые регистры:
         * - PD3 latch, выход;
         * - PD4 clock, выход;
         * - PD5 data from 74HC165, вход без внутренней подтяжки;
         * - PB1 data to 74HC595, выход.
         */
        DDRD |= static_cast<uint8_t>(SR_LATCH_MASK | SR_CLOCK_MASK);
        DDRD &= static_cast<uint8_t>(~SR_INPUT_DATA_MASK);

        PORTD &= static_cast<uint8_t>(~SR_LATCH_MASK);
        PORTD &= static_cast<uint8_t>(~SR_CLOCK_MASK);
        PORTD &= static_cast<uint8_t>(~SR_INPUT_DATA_MASK);

        DDRB |= SR_LED_DATA_MASK;
        PORTB &= static_cast<uint8_t>(~SR_LED_DATA_MASK);

        /*
         * Светодиод состояния связи: PB0, выход, начально выключен.
         */
        DDRB |= STATUS_LED_MASK;
        statusLedWrite(false);

        /*
         * Начальное состояние панели индикации.
         */
        writePanelMask(0x0000u);
    }

    /**
     * @brief Считывает Modbus-адрес с аппаратного адресного переключателя.
     *
     * @return Modbus-адрес устройства.
     */
    uint8_t readSlaveAddress(void)
    {
        /*
         * Входы подтянуты к Vcc, активное положение переключателя даёт ноль.
         * Поэтому состояние входов инвертируется.
         */
        const uint8_t raw =
            static_cast<uint8_t>((~PINC) & ADDRESS_INPUTS_MASK);

        uint8_t addr = mirrorFiveBits(raw);

        if (addr == 0u)
        {
            addr = 1u;
        }

        return addr;
    }

    /**
     * @brief Устанавливает состояние светодиода связи.
     *
     * @param state true — включить светодиод; false — выключить светодиод.
     */
    void statusLedWrite(bool state)
    {
        writePortMask(PORTB, STATUS_LED_MASK, state);
    }

    /**
     * @brief Инвертирует состояние светодиода связи.
     */
    void statusLedToggle(void)
    {
        PORTB ^= STATUS_LED_MASK;
    }

    /**
     * @brief Переводит RS-485-трансивер в режим передачи.
     */
    void rs485TransmitMode(void)
    {
        PORTD |= RS485_DE_MASK;
    }

    /**
     * @brief Переводит RS-485-трансивер в режим приёма.
     */
    void rs485ReceiveMode(void)
    {
        PORTD &= static_cast<uint8_t>(~RS485_DE_MASK);
    }

    /**
     * @brief Проверяет состояние линии управления RS-485.
     *
     * @return true — активен режим передачи.
     * @return false — активен режим приёма.
     */
    bool rs485IsTransmitMode(void)
    {
        return ((PORTD & RS485_DE_MASK) != 0u);
    }

    /**
     * @brief Считывает состояние шестнадцати дискретных входов.
     *
     * @return Битовое состояние входов.
     */
    uint16_t readInputs(void)
    {
        uint16_t state = 0u;

        capture165Inputs();

        for (uint8_t i = 0u; i < INPUT_COUNT; ++i)
        {
            state <<= 1;

            if ((PIND & SR_INPUT_DATA_MASK) != 0u)
            {
                state |= 0x0001u;
            }

            shiftClockPulse();
        }

        latchLow();
        restorePanelMask();

        return state;
    }

    /**
     * @brief Выводит состояние входов на LED-панель.
     *
     * @param state Битовое состояние входов.
     */
    void writeInputLeds(uint16_t state)
    {
        writePanelMask(state);
    }

    /**
     * @brief Выводит битовую маску на LED-панель.
     *
     * @param state Битовая маска светодиодов.
     */
    void writePanelMask(uint16_t state)
    {
        s_lastPanelMask = state;
        transferPanelMask(state);
    }
}