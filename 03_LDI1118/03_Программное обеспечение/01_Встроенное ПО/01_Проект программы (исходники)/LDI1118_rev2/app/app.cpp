#include "app.hpp"
#include "ldi_board.hpp"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <stdint.h>

#include "../Libs/IGAS_mb.h"

/**
 * @file app.cpp
 * @brief Прикладная логика модуля дискретных входов LDI.
 *
 * @details
 * Модуль реализует:
 * - инициализацию приложения;
 * - чтение шестнадцати дискретных входов через аппаратный слой платы;
 * - вывод состояния входов на LED-панель;
 * - контроль связи с ведущим устройством;
 * - обработку Holding-регистров Modbus;
 * - чтение и запись сервисных параметров EEPROM;
 * - программную перезагрузку через watchdog;
 * - отложенный переход в bootloader.
 *
 * Аппаратная карта платы находится в ldi_board.cpp.
 * Транспорт Modbus RTU находится в IGAS_mb.cpp.
 */

namespace
{
    // -------------------------------------------------------------------------
    // Конфигурация устройства
    // -------------------------------------------------------------------------

    /**
     * @brief Скорость обмена Modbus RTU, бод.
     */
    static const uint32_t MODBUS_BAUDRATE = 9600UL;

    /**
     * @brief Верхняя исключающая граница адресного пространства Modbus.
     *
     * @details
     * Значение передаётся в транспорт Modbus. При значении 2001 допустимы
     * Holding-регистры с адресами 0...2000.
     */
    static const uint16_t MODBUS_REGISTER_LIMIT = 2001U;

    /**
     * @brief Версия встроенного программного обеспечения.
     */
    static const uint16_t DEVICE_SOFTWARE_VERSION = 1U;

    /**
     * @brief Идентификатор модуля LDI.
     */
    static const uint16_t DEVICE_ID = 1118U;

    // -------------------------------------------------------------------------
    // EEPROM-карта
    // -------------------------------------------------------------------------

    /**
     * @brief EEPROM-адрес хранения серийного номера устройства.
     *
     * @details
     * Поле занимает два байта EEPROM.
     */
    static const uint16_t EEPROM_DEVICE_SERIAL_NUMBER_ADDR = 1U;

    /**
     * @brief EEPROM-адрес хранения аппаратной версии устройства.
     *
     * @details
     * Поле занимает два байта EEPROM.
     */
    static const uint16_t EEPROM_DEVICE_HARDWARE_VERSION_ADDR = 3U;

    // -------------------------------------------------------------------------
    // Тайминги приложения
    // -------------------------------------------------------------------------

    /**
     * @brief Период быстрой периодической задачи, мс.
     *
     * @details
     * В этой задаче обрабатываются программная перезагрузка и переход
     * в bootloader.
     */
    static const uint32_t LOOP_100_MS = 100UL;

    /**
     * @brief Период медленной периодической задачи, мс.
     *
     * @details
     * В этой задаче проверяется таймаут связи и обновляется индикация
     * состояния соединения.
     */
    static const uint32_t LOOP_500_MS = 500UL;

    /**
     * @brief Таймаут потери связи, мс.
     *
     * @details
     * Если в течение этого времени не было корректного обращения по Modbus,
     * модуль переводит индикацию связи в режим отсутствия обмена.
     */
    static const uint32_t CONNECTION_TIMEOUT_MS = 10000UL;

    /**
     * @brief Задержка перед переходом в bootloader, мс.
     *
     * @details
     * Задержка даёт Modbus-ответу физически уйти в линию RS-485 перед
     * запретом прерываний и передачей управления bootloader.
     */
    static const uint32_t BOOTLOADER_JUMP_DELAY_MS = 50UL;

    // -------------------------------------------------------------------------
    // Bootloader
    // -------------------------------------------------------------------------

    /**
     * @brief Стартовый адрес bootloader в байтах.
     *
     * @details
     * Для ATmega328P значение 0x7000 соответствует boot-секции размером
     * 4096 байт при верхней границе flash 0x7FFF.
     *
     * @warning
     * Значение должно соответствовать фактическим fuse-настройкам BOOTSZ
     * и сборке bootloader.
     */
    static const uint16_t BOOT_START_ADDR_BYTES = 0x7000U;

    /**
     * @brief Стартовый адрес bootloader в словах flash.
     *
     * @details
     * На AVR адреса переходов через function pointer задаются в словах flash,
     * поэтому байтовый адрес делится на два.
     */
    static const uint16_t BOOT_START_ADDR_WORDS =
        static_cast<uint16_t>(BOOT_START_ADDR_BYTES / 2U);

    /**
     * @brief Тип указателя на функцию bootloader.
     */
    typedef void (*BootEntryPtr)(void);

    /**
     * @brief Передаёт управление в bootloader.
     *
     * @details
     * Функция подготавливает микроконтроллер к прямому переходу:
     * - запрещает прерывания;
     * - отключает watchdog;
     * - отключает UART, SPI и TWI, если соответствующие регистры доступны;
     * - восстанавливает нулевой регистр ABI;
     * - устанавливает стек в RAMEND;
     * - переключает таблицу векторов прерываний в boot-секцию;
     * - выполняет переход на стартовый адрес bootloader.
     *
     * @warning
     * Возврат из bootloader в вызывающий код не предусмотрен.
     */
    void jumpToBootloader(void);

    // -------------------------------------------------------------------------
    // Состояние приложения
    // -------------------------------------------------------------------------

    /**
     * @brief Экземпляр транспорта Modbus RTU по RS-485.
     */
    IGAS_X2X_485 IGAS_mb_X;

    /**
     * @brief Текущее состояние шестнадцати дискретных входов.
     *
     * @details
     * Каждый бит соответствует одному дискретному входу.
     * Значение публикуется через HR 0.
     */
    uint16_t inputState = 0x0000U;

    /**
     * @brief Последнее состояние входов, выведенное на LED-панель.
     *
     * @details
     * Используется для исключения лишнего тактирования LED-регистра,
     * если состояние входов не изменилось.
     */
    uint16_t displayedInputState = 0xFFFFU;

    /**
     * @brief Modbus-адрес устройства.
     *
     * @details
     * Адрес считывается с аппаратного адресного переключателя.
     * Если все биты адреса равны нулю, используется адрес 1.
     */
    uint8_t slaveAddr = 0x01U;

    /**
     * @brief Флаг запроса программной перезагрузки.
     *
     * @details
     * При установке флага включается короткий watchdog timeout, после чего
     * микроконтроллер перезагружается.
     */
    uint8_t rebootRequested = 0x00U;

    /**
     * @brief Текущее время приложения, мс.
     *
     * @details
     * Обновляется в начале каждого прохода loop().
     */
    uint32_t currentTime = 0UL;

    /**
     * @brief Время запроса перехода в bootloader, мс.
     *
     * @details
     * Нулевое значение означает, что переход не запрошен.
     */
    uint32_t bootloaderRequestTime = 0UL;

    /**
     * @brief Время последнего корректного Modbus-запроса, мс.
     */
    uint32_t lastModbusRequestTime = 0UL;

    /**
     * @brief Время последнего запуска задачи loop_100(), мс.
     */
    uint32_t loop100StartTime = 0UL;

    /**
     * @brief Время последнего запуска задачи loop_500(), мс.
     */
    uint32_t loop500StartTime = 0UL;

    /**
     * @brief Состояние связи с ведущим устройством.
     *
     * @details
     * Значение 0 означает отсутствие связи или потерю связи.
     * Значение 1 означает, что был принят хотя бы один корректный Modbus-запрос.
     */
    uint8_t communicationState = 0x00U;

    // -------------------------------------------------------------------------
    // Локальные функции
    // -------------------------------------------------------------------------

    void initBoardIo(void);
    uint8_t readSlaveAddress(void);
    void statusLedWrite(bool state);
    void statusLedToggle(void);

    void updateInputs(void);
    void updateInputIndication(void);

    void loop100Task(void);
    void loop500Task(void);
    void initCommunication(void);

    void modbusProceed(
        int16_t* regs,
        int16_t startAddr,
        int16_t regCount,
        int16_t value
    );

    uint16_t eepromReadWord(uint16_t address);
    void eepromUpdateWord(uint16_t address, uint16_t value);

    // -------------------------------------------------------------------------
    // EEPROM
    // -------------------------------------------------------------------------

    /**
     * @brief Читает 16-битное значение из EEPROM.
     *
     * @param address Адрес первого байта EEPROM.
     *
     * @return Прочитанное 16-битное значение.
     */
    uint16_t eepromReadWord(uint16_t address)
    {
        return eeprom_read_word(
            reinterpret_cast<const uint16_t*>(address)
        );
    }

    /**
     * @brief Записывает 16-битное значение в EEPROM.
     *
     * @param address Адрес первого байта EEPROM.
     * @param value Записываемое значение.
     *
     * @details
     * Используется eeprom_update_word(), чтобы не выполнять физическую запись,
     * если значение в EEPROM уже совпадает с новым значением.
     */
    void eepromUpdateWord(uint16_t address, uint16_t value)
    {
        eeprom_update_word(
            reinterpret_cast<uint16_t*>(address),
            value
        );
    }

    // -------------------------------------------------------------------------
    // Аппаратный слой платы
    // -------------------------------------------------------------------------

    /**
     * @brief Инициализирует аппаратные линии платы LDI.
     */
    void initBoardIo(void)
    {
        ldi_board::initIo();
    }

    /**
     * @brief Считывает Modbus-адрес с адресного переключателя.
     *
     * @return Адрес устройства.
     */
    uint8_t readSlaveAddress(void)
    {
        return ldi_board::readSlaveAddress();
    }

    /**
     * @brief Устанавливает состояние светодиода связи.
     *
     * @param state true — включить светодиод; false — выключить светодиод.
     */
    void statusLedWrite(bool state)
    {
        ldi_board::statusLedWrite(state);
    }

    /**
     * @brief Инвертирует состояние светодиода связи.
     */
    void statusLedToggle(void)
    {
        ldi_board::statusLedToggle();
    }

    /**
     * @brief Считывает состояние дискретных входов.
     */
    void updateInputs(void)
    {
        inputState = ldi_board::readInputs();
    }

    /**
     * @brief Обновляет LED-индикацию входов.
     *
     * @details
     * Для снижения лишнего тактирования сдвигового регистра данные выводятся
     * только при изменении состояния входов.
     */
    void updateInputIndication(void)
    {
        if (displayedInputState != inputState)
        {
            ldi_board::writeInputLeds(inputState);
            displayedInputState = inputState;
        }
    }

    // -------------------------------------------------------------------------
    // Периодические задачи
    // -------------------------------------------------------------------------

    /**
     * @brief Обрабатывает задачи с периодом около 100 мс.
     *
     * @details
     * Выполняет:
     * - программную перезагрузку;
     * - отложенный переход в bootloader.
     */
    void loop100Task(void)
    {
        if (static_cast<uint32_t>(currentTime - loop100StartTime) >= LOOP_100_MS)
        {
            loop100StartTime = currentTime;

            if (rebootRequested != 0U)
            {
                wdt_disable();
                wdt_enable(WDTO_15MS);

                for (;;)
                {
                    /*
                     * Ожидание перезагрузки по watchdog.
                     */
                }
            }

            if ((bootloaderRequestTime != 0UL) &&
                (static_cast<uint32_t>(currentTime - bootloaderRequestTime) > BOOTLOADER_JUMP_DELAY_MS))
            {
                bootloaderRequestTime = 0UL;
                jumpToBootloader();

                for (;;)
                {
                    /*
                     * Защитный цикл на случай возврата из bootloader.
                     */
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
     * - мигание светодиодом связи при отсутствии обмена.
     */
    void loop500Task(void)
    {
        if (static_cast<uint32_t>(currentTime - loop500StartTime) >= LOOP_500_MS)
        {
            loop500StartTime = currentTime;

            if (static_cast<uint32_t>(currentTime - lastModbusRequestTime) >= CONNECTION_TIMEOUT_MS)
            {
                communicationState = 0x00U;
            }

            if (communicationState == 0x00U)
            {
                statusLedToggle();
            }
        }
    }

    // -------------------------------------------------------------------------
    // Связь и Modbus
    // -------------------------------------------------------------------------

    /**
     * @brief Инициализирует Modbus RTU по RS-485.
     *
     * @details
     * Настраивает скорость обмена, формат кадра, обработчик карты регистров
     * и slave-адрес устройства.
     */
    void initCommunication(void)
    {
        IGAS_mb_X.initMB(MODBUS_BAUDRATE, SERIAL_8N1, MODBUS_REGISTER_LIMIT);
        IGAS_mb_X.setDelegate(modbusProceed);
        IGAS_mb_X.slave = slaveAddr;
    }

    /**
     * @brief Обрабатывает обращение к Holding-регистрам Modbus.
     *
     * @param regs Массив регистров ответа.
     * @param startAddr Начальный адрес регистра.
     * @param regCount Количество обрабатываемых регистров.
     * @param value Значение регистра при операции записи.
     *
     * @details
     * Карта регистров LDI:
     * - HR 0: состояние шестнадцати дискретных входов;
     * - HR 39: серийный номер устройства;
     * - HR 40: версия встроенного ПО;
     * - HR 100: Modbus-адрес устройства в RAM;
     * - HR 591: запись и чтение серийного номера;
     * - HR 594: программная перезагрузка;
     * - HR 900: отложенный переход в bootloader;
     * - HR 1000: запись и чтение аппаратной версии устройства;
     * - HR 1001: идентификатор модуля.
     *
     * При каждом корректном обращении обновляется таймер контроля связи.
     */
    void modbusProceed(
        int16_t* regs,
        int16_t startAddr,
        int16_t regCount,
        int16_t value
    )
    {
        int16_t registerAddr = startAddr;
        bool writeReg = false;

        lastModbusRequestTime = currentTime;

        if ((IGAS_mb_X.func == FC_WRITE_REGS) || (IGAS_mb_X.func == FC_WRITE_REG))
        {
            writeReg = true;
        }

        if (communicationState == 0x00U)
        {
            communicationState = 0x01U;
            statusLedWrite(true);
        }

        for (int16_t i = 0; i < regCount; ++i)
        {
            switch (registerAddr)
            {
                case 0:
                    /*
                     * HR 0.
                     * Read: текущее состояние шестнадцати дискретных входов.
                     */
                    regs[i] = static_cast<int16_t>(inputState);
                    break;

                case 39:
                    /*
                     * HR 39.
                     * Read: серийный номер устройства.
                     */
                    regs[i] = static_cast<int16_t>(
                        eepromReadWord(EEPROM_DEVICE_SERIAL_NUMBER_ADDR)
                    );
                    break;

                case 40:
                    /*
                     * HR 40.
                     * Read: версия встроенного программного обеспечения.
                     */
                    regs[i] = static_cast<int16_t>(DEVICE_SOFTWARE_VERSION);
                    break;

                case 100:
                    /*
                     * HR 100.
                     * Write: временное изменение Modbus-адреса устройства.
                     * Read: текущий Modbus-адрес устройства.
                     *
                     * Адрес, записанный через этот регистр, действует до перезапуска.
                     * При старте адрес снова считывается с аппаратного переключателя.
                     */
                    if (writeReg)
                    {
                        if ((value > 0) && (value < 248))
                        {
                            slaveAddr = static_cast<uint8_t>(value);
                            IGAS_mb_X.slave = slaveAddr;
                        }
                    }

                    regs[i] = static_cast<int16_t>(slaveAddr);
                    break;

                case 591:
                    /*
                     * HR 591.
                     * Write: запись серийного номера устройства.
                     * Read: текущий серийный номер устройства.
                     */
                    if (writeReg)
                    {
                        eepromUpdateWord(
                            EEPROM_DEVICE_SERIAL_NUMBER_ADDR,
                            static_cast<uint16_t>(value)
                        );
                    }

                    regs[i] = static_cast<int16_t>(
                        eepromReadWord(EEPROM_DEVICE_SERIAL_NUMBER_ADDR)
                    );
                    break;

                case 594:
                    /*
                     * HR 594.
                     * Write 1: программная перезагрузка через watchdog.
                     * Read: состояние запроса перезагрузки.
                     */
                    if (writeReg && (value == 1))
                    {
                        rebootRequested = 0x01U;
                    }

                    regs[i] = static_cast<int16_t>(rebootRequested);
                    break;

                case 900:
                    /*
                     * HR 900.
                     * Write: отложенный переход в bootloader.
                     * Read: состояние запроса перехода.
                     */
                    if (writeReg)
                    {
                        bootloaderRequestTime = currentTime;
                    }

                    regs[i] = (bootloaderRequestTime != 0UL) ? 1 : 0;
                    break;

                case 1000:
                    /*
                     * HR 1000.
                     * Write: запись аппаратной версии устройства.
                     * Read: текущая аппаратная версия устройства.
                     */
                    if (writeReg)
                    {
                        eepromUpdateWord(
                            EEPROM_DEVICE_HARDWARE_VERSION_ADDR,
                            static_cast<uint16_t>(value)
                        );
                    }

                    regs[i] = static_cast<int16_t>(
                        eepromReadWord(EEPROM_DEVICE_HARDWARE_VERSION_ADDR)
                    );
                    break;

                case 1001:
                    /*
                     * HR 1001.
                     * Read: идентификатор модуля.
                     */
                    regs[i] = static_cast<int16_t>(DEVICE_ID);
                    break;

                default:
                    /*
                     * Неподдерживаемый регистр.
                     */
                    regs[i] = -1;
                    break;
            }

            ++registerAddr;
        }
    }

    // -------------------------------------------------------------------------
    // Переход в bootloader
    // -------------------------------------------------------------------------

    /**
     * @brief Передаёт управление в bootloader.
     */
    void jumpToBootloader(void)
    {
        cli();

        wdt_disable();

#ifdef UCSR0B
        UCSR0B = 0U;
#endif

#ifdef SPCR
        SPCR = 0U;
#endif

#ifdef TWCR
        TWCR = 0U;
#endif

        /*
         * Восстановление нулевого регистра ABI AVR-GCC.
         */
        asm volatile ("clr __zero_reg__");

        /*
         * Установка стека в конец SRAM.
         */
        SP = RAMEND;

        /*
         * Перенос таблицы векторов прерываний в boot-секцию.
         * Последовательность IVCE -> IVSEL должна выполняться без задержки
         * между двумя записями.
         */
        MCUCR = static_cast<uint8_t>(_BV(IVCE));
        MCUCR = static_cast<uint8_t>(_BV(IVSEL));

        /*
         * Прямой переход в bootloader.
         * Адрес задаётся в словах flash.
         */
        reinterpret_cast<BootEntryPtr>(BOOT_START_ADDR_WORDS)();
    }
}

// -----------------------------------------------------------------------------
// Публичные функции приложения
// -----------------------------------------------------------------------------

/**
 * @brief Выполняет однократную инициализацию приложения LDI.
 */
void setup(void)
{
    /*
     * После сброса по watchdog флаг WDRF может удерживать watchdog включённым.
     * Поэтому сначала выполняется сброс watchdog, очистка WDRF, затем настройка
     * требуемого таймаута.
     */
    wdt_reset();
    MCUSR &= static_cast<uint8_t>(~_BV(WDRF));

    wdt_disable();
    wdt_enable(WDTO_1S);

    initBoardIo();

    slaveAddr = readSlaveAddress();

    currentTime = millis();
    lastModbusRequestTime = currentTime;
    loop100StartTime = currentTime;
    loop500StartTime = currentTime;

    updateInputs();
    updateInputIndication();

    initCommunication();

    wdt_reset();
}

/**
 * @brief Выполняет один проход основного цикла приложения LDI.
 */
void loop(void)
{
    currentTime = millis();

    updateInputs();
    updateInputIndication();

    loop100Task();
    loop500Task();

    IGAS_mb_X.update();

    wdt_reset();
}