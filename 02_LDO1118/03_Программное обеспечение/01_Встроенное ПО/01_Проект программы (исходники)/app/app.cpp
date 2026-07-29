#include "app.hpp"
#include "ldo_board.hpp"

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
 * Модуль реализует:
 * - инициализацию приложения;
 * - контроль связи с ведущим устройством;
 * - применение состояния релейных выходов;
 * - обработку Holding-регистров Modbus;
 * - программную перезагрузку через watchdog;
 * - отложенный переход в bootloader.
 *
 * Аппаратная карта платы находится в ldo_board.cpp.
 * Транспорт Modbus RTU находится в IGAS_mb.cpp.
 */

namespace
{
    // -------------------------------------------------------------------------
    // Тайминги приложения
    // -------------------------------------------------------------------------

    /**
     * @brief Таймаут потери связи, мс.
     *
     * @details
     * Если в течение этого времени не было корректного обращения по Modbus,
     * модуль сбрасывает команду реле в безопасное состояние и переводит
     * индикацию в режим отсутствия связи.
     */
    static const uint32_t LOST_CONNECTION_TIME_MS = 10000UL;

    /**
     * @brief Период быстрой периодической задачи, мс.
     *
     * @details
     * В этой задаче применяется новое состояние релейных выходов и
     * обрабатываются команды перезагрузки.
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
    typedef void (*boot_ptr_t)(void);

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
    void jump_to_bootloader(void);

    // -------------------------------------------------------------------------
    // Состояние приложения
    // -------------------------------------------------------------------------

    /**
     * @brief Экземпляр транспорта Modbus RTU по RS-485.
     */
    IGAS_X2X_485 IGAS_mb_X;

    /**
     * @brief Текущее физически применённое состояние релейных выходов.
     *
     * @details
     * Каждый бит соответствует одному релейному выходу.
     */
    uint8_t relayState = 0x00;

    /**
     * @brief Код ошибки устройства.
     *
     * @details
     * Поле публикуется через Modbus-регистр состояния.
     */
    uint8_t errorByte = 0x00;

    /**
     * @brief Требуемое состояние релейных выходов.
     *
     * @details
     * Значение записывается через Modbus и применяется в периодической
     * задаче loop_100().
     */
    uint8_t newState = 0x00;

    /**
     * @brief Modbus-адрес устройства.
     *
     * @details
     * Адрес считывается с аппаратного адресного переключателя.
     * Если все биты адреса равны нулю, используется адрес 1.
     */
    uint8_t slaveAddr = 0x01;

    /**
     * @brief Флаг сервисного режима.
     *
     * @details
     * Устанавливается записью сервисного ключа в соответствующий
     * Modbus-регистр.
     */
    uint8_t serviceMode = 0x00;

    /**
     * @brief Флаг запроса программной перезагрузки.
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
     * Обновляется в начале каждого прохода loop().
     */
    uint32_t currentTime = 0UL;

    /**
     * @brief Время запроса перехода в bootloader, мс.
     *
     * @details
     * Нулевое значение означает, что переход не запрошен.
     */
    uint32_t rebootBOOT = 0UL;

    /**
     * @brief Время последнего корректного Modbus-запроса, мс.
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
     * @brief Состояние связи с ведущим устройством.
     *
     * @details
     * Значение 0 означает отсутствие связи или потерю связи.
     * Значение 1 означает, что был принят хотя бы один корректный Modbus-запрос.
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
    // Аппаратный слой платы
    // -------------------------------------------------------------------------

    /**
     * @brief Инициализирует аппаратные линии платы LDO.
     */
    void initBoardIo(void)
    {
        ldo_board::initIo();
    }

    /**
     * @brief Считывает Modbus-адрес с адресного переключателя.
     *
     * @return Адрес устройства.
     */
    uint8_t readSlaveAddress(void)
    {
        return ldo_board::readSlaveAddress();
    }

    /**
     * @brief Устанавливает состояние светодиода статуса.
     *
     * @param state true — включить светодиод; false — выключить светодиод.
     */
    void statusLedWrite(bool state)
    {
        ldo_board::statusLedWrite(state);
    }

    /**
     * @brief Инвертирует состояние светодиода статуса.
     */
    void statusLedToggle(void)
    {
        ldo_board::statusLedToggle();
    }

    /**
     * @brief Записывает состояние релейных выходов.
     *
     * @param state Битовое состояние релейных выходов.
     */
    void relaysWrite(uint8_t state)
    {
        ldo_board::relaysWrite(state);
    }

    // -------------------------------------------------------------------------
    // Периодические задачи
    // -------------------------------------------------------------------------

    /**
     * @brief Обрабатывает задачи с периодом около 100 мс.
     *
     * @details
     * Выполняет:
     * - применение нового состояния релейных выходов;
     * - обработку программной перезагрузки;
     * - отложенный переход в bootloader.
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
                    /*
                     * Ожидание перезагрузки по watchdog.
                     */
                }
            }

            if ((rebootBOOT != 0UL) &&
                (static_cast<uint32_t>(currentTime - rebootBOOT) > BOOTLOADER_JUMP_DELAY_MS))
            {
                rebootBOOT = 0UL;
                jump_to_bootloader();

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
     * - сброс релейных выходов при потере связи;
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
                newState = 0x00;
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
     * @brief Инициализирует Modbus RTU по RS-485.
     *
     * @details
     * Настраивает скорость обмена, формат кадра, обработчик карты регистров
     * и slave-адрес устройства.
     */
    void initCommunication(void)
    {
        IGAS_mb_X.initMB(9600UL, SERIAL_8N1, 1001U);
        IGAS_mb_X.setDelegate(modbusProceed);
        IGAS_mb_X.slave = slaveAddr;
    }

    /**
     * @brief Обрабатывает обращение к Holding-регистрам Modbus.
     *
     * @param regs Массив регистров ответа.
     * @param start_addr_ Начальный адрес регистра.
     * @param reg_count_ Количество обрабатываемых регистров.
     * @param Value_ Значение регистра при операции записи.
     *
     * @details
     * Карта регистров:
     * - HR 0: чтение текущего состояния релейных выходов или запись нового состояния;
     * - HR 1: чтение текущего состояния релейных выходов;
     * - HR 3: чтение кода ошибки;
     * - HR 580: вход в сервисный режим по сервисному ключу;
     * - HR 590...593: резерв сервисных данных;
     * - HR 594: программная перезагрузка;
     * - HR 900: отложенный переход в bootloader.
     *
     * При каждом корректном обращении обновляется таймер контроля связи.
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
                    /*
                     * HR 0.
                     * Write: новое состояние релейных выходов.
                     * Read: текущее применённое состояние релейных выходов.
                     */
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
                    /*
                     * HR 1.
                     * Read: текущее применённое состояние релейных выходов.
                     */
                    regs[i] = static_cast<int16_t>(relayState);
                    break;

                case 3:
                    /*
                     * HR 3.
                     * Read: код ошибки устройства.
                     */
                    regs[i] = static_cast<int16_t>(errorByte);
                    break;

                case 580:
                    /*
                     * HR 580.
                     * Write: сервисный ключ.
                     */
                    if (writeReg && (Value_ == 20118))
                    {
                        serviceMode = 0x01;
                    }

                    regs[i] = static_cast<int16_t>(serviceMode);
                    break;

                case 590:
                    /*
                     * HR 590.
                     * Резерв сервисных данных.
                     */
                    regs[i] = 0;
                    break;

                case 591:
                    /*
                     * HR 591.
                     * Резерв сервисных данных.
                     */
                    regs[i] = 0;
                    break;

                case 592:
                    /*
                     * HR 592.
                     * Резерв сервисных данных.
                     */
                    regs[i] = 0;
                    break;

                case 593:
                    /*
                     * HR 593.
                     * Резерв сервисных данных.
                     */
                    regs[i] = 0;
                    break;

                case 594:
                    /*
                     * HR 594.
                     * Write: программная перезагрузка через watchdog.
                     */
                    if (writeReg)
                    {
                        reboot = 0x01;
                    }

                    regs[i] = static_cast<int16_t>(reboot);
                    break;

                case 900:
                    /*
                     * HR 900.
                     * Write: отложенный переход в bootloader.
                     * Read: состояние запроса перехода.
                     */
                    if (writeReg)
                    {
                        rebootBOOT = currentTime;
                    }

                    regs[i] = (rebootBOOT != 0UL) ? 1 : 0;
                    break;

                default:
                    /*
                     * Неподдерживаемый регистр.
                     */
                    regs[i] = -1;
                    break;
            }

            ++registerAddr_;
        }
    }

    // -------------------------------------------------------------------------
    // Переход в bootloader
    // -------------------------------------------------------------------------

    /**
     * @brief Передаёт управление в bootloader.
     */
    void jump_to_bootloader(void)
    {
        cli();

        wdt_disable();

#ifdef UCSR0B
        UCSR0B = 0u;
#endif

#ifdef SPCR
        SPCR = 0u;
#endif

#ifdef TWCR
        TWCR = 0u;
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
        reinterpret_cast<boot_ptr_t>(BOOT_START_ADDR_WORDS)();
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