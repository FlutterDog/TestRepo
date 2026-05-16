#include "app.hpp"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <stdint.h>

#include "../Libs/IGAS_mb.h"

/**
 * @file app.cpp
 * @brief Прикладная логика модуля релейных выходов LDO.
 *
 * @details
 * Это первая очищенная версия старого Arduino-подобного приложения.
 *
 * Что сделано в этой версии:
 * - сохранена модель setup()/loop();
 * - убран analogRead() для адресного переключателя;
 * - убраны A0...A5 из прикладного кода;
 * - убраны pinMode()/digitalWrite() из прикладного кода;
 * - работа с реле, светодиодом, RS-485 direction и адресными входами
 *   перенесена на прямую работу с портами ATmega328P;
 * - сохранена старая карта Modbus-регистров;
 * - сохранена старая логика watchdog, потери связи и перехода в bootloader.
 *
 * @warning
 * EEPROM-регистры пока оставлены как зарезервированные участки.
 * Реальная EEPROM-логика будет восстановлена отдельно после проверки старой
 * карты сервисных данных.
 */

namespace
{
    // -------------------------------------------------------------------------
    // Аппаратная привязка LDO к ATmega328P
    // -------------------------------------------------------------------------

    /**
     * @brief Маска светодиода статуса.
     *
     * @details
     * Старое обозначение: A0.
     * Для ATmega328P / Arduino Nano-подобной разводки это PC0.
     */
    static const uint8_t STATUS_LED_MASK = _BV(PC0);

    /**
     * @brief Маска входов адресного переключателя.
     *
     * @details
     * Старые обозначения:
     * - A1 -> PC1;
     * - A2 -> PC2;
     * - A3 -> PC3;
     * - A4 -> PC4;
     * - A5 -> PC5.
     *
     * Входы читаются как обычные цифровые входы.
     * Логика активная высокая: уровень один означает установленный бит адреса.
     */
    static const uint8_t ADDRESS_INPUTS_MASK =
        static_cast<uint8_t>(_BV(PC1) | _BV(PC2) | _BV(PC3) | _BV(PC4) | _BV(PC5));

    /**
     * @brief Маска пина управления направлением RS-485.
     *
     * @details
     * Старое обозначение: RS_Pin = 2.
     * Для ATmega328P / Arduino Nano-подобной разводки это PD2.
     *
     * Низкий уровень — приём.
     * Высокий уровень — передача.
     *
     * @note
     * Здесь задаётся только безопасное начальное состояние. Фактическое
     * переключение приём/передача выполняется внутри Modbus-библиотеки.
     */
    static const uint8_t RS485_DE_MASK = _BV(PD2);

    /**
     * @brief Таймаут потери связи, мс.
     *
     * @details
     * Если в течение этого времени не было обращения по Modbus, модуль
     * сбрасывает команду реле в ноль и переводит индикацию в режим мигания.
     */
    static const uint32_t LOST_CONNECTION_TIME_MS = 10000UL;

    /**
     * @brief Период быстрой периодической задачи, мс.
     *
     * @details
     * В этой задаче обновляется состояние реле и обрабатывается программный reboot.
     */
    static const uint32_t LOOP_100_MS = 100UL;

    /**
     * @brief Период медленной периодической задачи, мс.
     *
     * @details
     * В этой задаче проверяется потеря связи и выполняется мигание светодиодом.
     */
    static const uint32_t LOOP_500_MS = 500UL;

    /**
     * @brief Тип указателя на функцию без аргументов.
     *
     * @details
     * Используется для перехода в bootloader.
     */
    typedef void (*AppPtr_t)(void);

    /**
     * @brief Адрес перехода в bootloader.
     *
     * @details
     * Значение оставлено из старого проекта.
     *
     * @warning
     * Перед финальной фиксацией проекта адрес нужно сверить с реальным
     * bootloader и fuse-настройками.
     */
    AppPtr_t GoToBootloader = reinterpret_cast<AppPtr_t>(0x3F07);

    // -------------------------------------------------------------------------
    // Глобальное состояние приложения
    // -------------------------------------------------------------------------

    /**
     * @brief Объект Modbus/RS-485.
     *
     * @details
     * IGAS_X2X_485 является совместимым старым именем класса IGAS_mb_485.
     * typedef должен быть объявлен в IGAS_mb.h.
     */
    IGAS_X2X_485 IGAS_mb_X;

    /**
     * @brief Текущее физически установленное состояние реле.
     *
     * @details
     * Каждый бит соответствует одному релейному выходу.
     */
    uint8_t relayState = 0x00;

    /**
     * @brief Байтовый код ошибки устройства.
     *
     * @details
     * Сейчас логика формирования ошибки в старом коде отсутствует. Переменная
     * оставлена для совместимости с картой Modbus-регистров.
     */
    uint8_t errorByte = 0x00;

    /**
     * @brief Новое требуемое состояние реле.
     *
     * @details
     * Значение записывается через Modbus и затем применяется в периодической
     * задаче loop_100().
     */
    uint8_t newState = 0x00;

    /**
     * @brief Modbus-адрес устройства.
     *
     * @details
     * Адрес считывается с аппаратного адресного переключателя. Если все биты
     * адреса равны нулю, используется адрес 1.
     */
    uint8_t slaveAddr = 0x01;

    /**
     * @brief Флаг сервисного режима.
     *
     * @details
     * Устанавливается через регистр 580 при записи сервисного ключа.
     */
    uint8_t serviceMode = 0x00;

    /**
     * @brief Флаг программной перезагрузки.
     *
     * @details
     * При установке флага включается короткий watchdog timeout, после чего
     * микроконтроллер перезагружается.
     */
    uint8_t reboot = 0x00;

    /**
     * @brief Текущее время приложения, мс.
     *
     * @details
     * Обновляется в начале каждого вызова loop().
     */
    uint32_t currentTime = 0UL;

    /**
     * @brief Время последнего принятого Modbus-запроса, мс.
     */
    uint32_t resetWDtimer = 0UL;

    /**
     * @brief Время последнего запуска задачи loop_100(), мс.
     */
    uint32_t loop_100_start = 0UL;

    /**
     * @brief Время последнего запуска задачи loop_500(), мс.
     */
    uint32_t loop_500_start = 0UL;

    /**
     * @brief Состояние связи с верхним уровнем.
     *
     * @details
     * Ноль — связь не установлена или потеряна.
     * Один — был принят хотя бы один корректный Modbus-запрос.
     */
    uint8_t x2xState = 0x00;

    // -------------------------------------------------------------------------
    // Локальные функции
    // -------------------------------------------------------------------------

    void initBoardIo(void);
    uint8_t readSlaveAddress(void);
    void statusLedWrite(bool state);
    void statusLedToggle(void);
    void relaysWrite(uint8_t state);
    void rs485ReceiveMode(void);

    void loop_100(void);
    void loop_500(void);
    void initCommunication(void);

    void modbusProceed(
        int16_t* regs,
        int16_t start_addr_,
        int16_t reg_count_,
        int16_t Value_
    );

    // -------------------------------------------------------------------------
    // Низкоуровневая работа с портами платы
    // -------------------------------------------------------------------------

    /**
     * @brief Устанавливает или сбрасывает бит порта.
     *
     * @param port Регистр PORTx.
     * @param mask Маска изменяемого бита или группы битов.
     * @param state Требуемое состояние: true — установить, false — сбросить.
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
     * @brief Переводит интерфейс RS-485 в режим приёма.
     *
     * @details
     * Сбрасывает PD2 в ноль.
     */
    void rs485ReceiveMode(void)
    {
        PORTD &= static_cast<uint8_t>(~RS485_DE_MASK);
    }

    /**
     * @brief Инициализирует порты ввода-вывода платы LDO.
     *
     * @details
     * Настраивает:
     * - PC0 как выход светодиода статуса;
     * - PC1...PC5 как входы адресного переключателя;
     * - PD2 как выход управления направлением RS-485;
     * - PB0...PB2 и PD3...PD7 как выходы реле.
     *
     * Подтяжки на адресных входах здесь не включаются. Это соответствует
     * предположению, что аппаратная схема сама формирует устойчивые уровни
     * ноль/один.
     */
    void initBoardIo(void)
    {
        // Светодиод статуса: PC0, выход, начально выключен.
        DDRC |= STATUS_LED_MASK;
        PORTC &= static_cast<uint8_t>(~STATUS_LED_MASK);

        // Адресные входы: PC1...PC5, входы, внутренние подтяжки отключены.
        DDRC &= static_cast<uint8_t>(~ADDRESS_INPUTS_MASK);
        PORTC &= static_cast<uint8_t>(~ADDRESS_INPUTS_MASK);

        // RS-485 direction: PD2, выход, начально режим приёма.
        DDRD |= RS485_DE_MASK;
        rs485ReceiveMode();

        /*
         * Релейные выходы.
         *
         * Старый порядок Arduino-пинов:
         * - bit 0 -> pin 10 -> PB2;
         * - bit 1 -> pin 9  -> PB1;
         * - bit 2 -> pin 8  -> PB0;
         * - bit 3 -> pin 7  -> PD7;
         * - bit 4 -> pin 6  -> PD6;
         * - bit 5 -> pin 5  -> PD5;
         * - bit 6 -> pin 4  -> PD4;
         * - bit 7 -> pin 3  -> PD3.
         */
        DDRB |= static_cast<uint8_t>(_BV(PB0) | _BV(PB1) | _BV(PB2));
        DDRD |= static_cast<uint8_t>(_BV(PD3) | _BV(PD4) | _BV(PD5) | _BV(PD6) | _BV(PD7));

        relaysWrite(0x00);
    }

    /**
     * @brief Считывает Modbus-адрес с адресного переключателя.
     *
     * @return Адрес устройства.
     *
     * @details
     * Соответствие входов и битов адреса:
     * - PC5 -> 0x01, 0b00000001;
     * - PC4 -> 0x02, 0b00000010;
     * - PC3 -> 0x04, 0b00000100;
     * - PC2 -> 0x08, 0b00001000;
     * - PC1 -> 0x10, 0b00010000.
     *
     * Если все входы равны нулю, возвращается адрес 1.
     */
    uint8_t readSlaveAddress(void)
    {
        uint8_t addr = 0x00;

        if ((PINC & _BV(PC5)) != 0u) { addr |= 0x01; } // 0b00000001
        if ((PINC & _BV(PC4)) != 0u) { addr |= 0x02; } // 0b00000010
        if ((PINC & _BV(PC3)) != 0u) { addr |= 0x04; } // 0b00000100
        if ((PINC & _BV(PC2)) != 0u) { addr |= 0x08; } // 0b00001000
        if ((PINC & _BV(PC1)) != 0u) { addr |= 0x10; } // 0b00010000

        if (addr == 0x00)
        {
            addr = 0x01;
        }

        return addr;
    }

    /**
     * @brief Устанавливает состояние светодиода статуса.
     *
     * @param state true — включить светодиод, false — выключить.
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
     * @brief Записывает состояние всех релейных выходов.
     *
     * @param state Битовое состояние реле.
     *
     * @details
     * Соответствие битов и выходов:
     * - bit 0 -> PB2, 0x01, 0b00000001;
     * - bit 1 -> PB1, 0x02, 0b00000010;
     * - bit 2 -> PB0, 0x04, 0b00000100;
     * - bit 3 -> PD7, 0x08, 0b00001000;
     * - bit 4 -> PD6, 0x10, 0b00010000;
     * - bit 5 -> PD5, 0x20, 0b00100000;
     * - bit 6 -> PD4, 0x40, 0b01000000;
     * - bit 7 -> PD3, 0x80, 0b10000000.
     */
    void relaysWrite(uint8_t state)
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

    // -------------------------------------------------------------------------
    // Прикладные периодические задачи
    // -------------------------------------------------------------------------

    /**
     * @brief Обрабатывает задачи с периодом около 100 мс.
     *
     * @details
     * Выполняет:
     * - применение нового состояния реле;
     * - обработку флага программной перезагрузки.
     */
    void loop_100(void)
    {
        if (static_cast<uint32_t>(currentTime - loop_100_start) >= LOOP_100_MS)
        {
            loop_100_start = currentTime;

            if (relayState != newState)
            {
                relayState = newState;
                relaysWrite(relayState);
            }

            if (reboot != 0u)
            {
                wdt_disable();
                wdt_enable(WDTO_15MS);

                for (;;)
                {
                    // Ожидание срабатывания watchdog.
                }
            }
        }
    }

    /**
     * @brief Обрабатывает задачи с периодом около 500 мс.
     *
     * @details
     * Выполняет:
     * - контроль таймаута связи;
     * - перевод реле в безопасное состояние при потере связи;
     * - мигание светодиодом при отсутствии связи.
     */
    void loop_500(void)
    {
        if (static_cast<uint32_t>(currentTime - loop_500_start) >= LOOP_500_MS)
        {
            loop_500_start = currentTime;

            if (static_cast<uint32_t>(currentTime - resetWDtimer) >= LOST_CONNECTION_TIME_MS)
            {
                x2xState = 0x00;
                newState = 0x00; // 0b00000000
            }

            if (x2xState == 0x00)
            {
                statusLedToggle();
            }
        }
    }

    // -------------------------------------------------------------------------
    // Связь и Modbus
    // -------------------------------------------------------------------------

    /**
     * @brief Инициализирует интерфейс связи Modbus/RS-485.
     *
     * @details
     * Настраивает:
     * - скорость UART;
     * - формат кадра;
     * - callback-функцию обработки регистров;
     * - адрес slave-устройства.
     */
    void initCommunication(void)
    {
        IGAS_mb_X.initMB(9600UL, SERIAL_8N1, 1001U);
        IGAS_mb_X.setDelegate(modbusProceed);
        IGAS_mb_X.slave = slaveAddr;
    }

    /**
     * @brief Обрабатывает обращение к Modbus-регистрам устройства.
     *
     * @param regs Указатель на массив регистров ответа.
     * @param start_addr_ Начальный адрес регистра.
     * @param reg_count_ Количество обрабатываемых регистров.
     * @param Value_ Значение, переданное при операции записи.
     *
     * @details
     * Поддерживаемая карта регистров:
     * - register 0: запись нового состояния реле или чтение текущего;
     * - register 1: чтение текущего состояния реле;
     * - register 3: чтение байта ошибки;
     * - register 580: вход в сервисный режим по ключу;
     * - registers 590...593: зарезервированы под EEPROM/сервисные данные;
     * - register 594: программная перезагрузка;
     * - register 900: переход в bootloader.
     *
     * При любом корректном обращении обновляется таймер связи.
     */
    void modbusProceed(
        int16_t* regs,
        int16_t start_addr_,
        int16_t reg_count_,
        int16_t Value_
    )
    {
        int16_t registerAddr_ = start_addr_;
        bool writeReg = false;

        resetWDtimer = currentTime;

        if ((IGAS_mb_X.func == FC_WRITE_REGS) || (IGAS_mb_X.func == FC_WRITE_REG))
        {
            writeReg = true;
        }

        if (x2xState == 0x00)
        {
            x2xState = 0x01;
            statusLedWrite(true);
        }

        for (int16_t i = 0; i < reg_count_; ++i)
        {
            switch (registerAddr_)
            {
                case 0:
                    // Register 0:
                    // Write: новое требуемое состояние реле.
                    // Read: текущее физически применённое состояние реле.
                    if (writeReg)
                    {
                        newState = static_cast<uint8_t>(Value_ & 0x00FF);
                    }
                    else
                    {
                        regs[i] = static_cast<int16_t>(relayState);
                    }
                    break;

                case 1:
                    // Register 1:
                    // Read-only mirror текущего состояния реле.
                    regs[i] = static_cast<int16_t>(relayState);
                    break;

                case 3:
                    // Register 3:
                    // Read-only байт ошибки устройства.
                    regs[i] = static_cast<int16_t>(errorByte);
                    break;

                case 580:
                    // Register 580:
                    // Запись сервисного ключа включает сервисный режим.
                    if (Value_ == 20118)
                    {
                        serviceMode = 0x01;
                    }
                    break;

                case 590:
                    // Register 590:
                    // Зарезервировано под EEPROM/серийный номер.
                    if (serviceMode != 0u)
                    {
                        // Reserved for EEPROM/service data.
                        // Old code example:
                        // IGAS_EEPROM.saveInt(72, Value_);
                    }
                    break;

                case 591:
                    // Register 591:
                    // Зарезервировано под EEPROM/серийный номер.
                    if (serviceMode != 0u)
                    {
                        // Reserved for EEPROM/service data.
                        // Old code example:
                        // IGAS_EEPROM.saveInt(74, Value_);
                    }
                    break;

                case 592:
                    // Register 592:
                    // Зарезервировано под данные поверки.
                    if (serviceMode != 0u)
                    {
                        // Reserved for EEPROM verification data.
                        // Old code example:
                        // year = Value_;
                        // IGAS_EEPROM.saveByte(111, year);
                    }
                    break;

                case 593:
                    // Register 593:
                    // Зарезервировано под данные поверки.
                    if (serviceMode != 0u)
                    {
                        // Reserved for EEPROM verification data.
                        // Old code example:
                        // month = Value_;
                        // IGAS_EEPROM.saveByte(112, month);
                    }
                    break;

                case 594:
                    // Register 594:
                    // Команда программной перезагрузки через watchdog.
                    reboot = 0x01;
                    break;

                case 900:
                    // Register 900:
                    // Переход в bootloader.
                    cli();
                    GoToBootloader();

                    for (;;)
                    {
                        // Защитный бесконечный цикл на случай возврата из bootloader.
                    }

                default:
                    // Неподдерживаемый регистр.
                    // Старое поведение сохранено: вернуть -1.
                    regs[i] = -1;
                    break;
            }

            ++registerAddr_;
        }
    }
}

// -----------------------------------------------------------------------------
// Публичные функции приложения
// -----------------------------------------------------------------------------

/**
 * @brief Выполняет однократную инициализацию приложения LDO.
 */
void setup(void)
{
    /*
     * После watchdog reset на AVR флаг WDRF может удерживать watchdog включённым.
     * Поэтому сначала очищаем WDRF, затем отключаем watchdog и включаем его заново
     * с нужным таймаутом.
     */
    wdt_reset();
    MCUSR &= static_cast<uint8_t>(~_BV(WDRF));

    wdt_disable();
    wdt_enable(WDTO_2S);

    initBoardIo();

    slaveAddr = readSlaveAddress();

    currentTime = millis();
    resetWDtimer = currentTime;
    loop_100_start = currentTime;
    loop_500_start = currentTime;

    initCommunication();

    wdt_reset();
}

/**
 * @brief Выполняет один проход основного цикла приложения LDO.
 */
void loop(void)
{
    currentTime = millis();

    loop_100();
    loop_500();

    IGAS_mb_X.update();

    wdt_reset();
}