#include "app.hpp"
#include "lai_board.hpp"
#include "ads1115.hpp"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>

#include <stdint.h>

#include "../Libs/IGAS_mb.h"

/**
 * @file app.cpp
 * @brief Прикладная логика модуля LAI.
 *
 * @details
 * Модуль выполняет:
 * - инициализацию аппаратных ресурсов платы LAI;
 * - настройку интерфейсов RS-485, SPI и I2C;
 * - чтение восьми аналоговых каналов через два преобразователя ADS1115;
 * - управление регистрами индикации и режимов аналоговых входов;
 * - расчёт калиброванных значений аналоговых каналов;
 * - фиксацию максимальных значений по каналам;
 * - обслуживание карты Holding-регистров Modbus RTU;
 * - сохранение параметров в EEPROM;
 * - программную перезагрузку;
 * - отложенный переход в загрузчик.
 */

namespace
{
    /* -------------------------------------------------------------------------
     * Конфигурация устройства
     * ---------------------------------------------------------------------- */

    /**
     * @brief Скорость обмена Modbus RTU, бод.
     */
    static const uint32_t MODBUS_BAUDRATE = 9600UL;

    /**
     * @brief Верхняя исключающая граница адресного пространства Holding-регистров.
     *
     * @details
     * Значение должно быть больше максимального используемого адреса
     * Holding-регистра в карте Modbus.
     */
    static const uint16_t MODBUS_REGISTER_LIMIT = 2001U;

    /**
     * @brief Количество аналоговых каналов модуля.
     */
    static const uint8_t NUM_CHANNELS = 8U;

    /**
     * @brief Версия встроенного программного обеспечения.
     */
    static const uint16_t FIRMWARE_DEF = 101U;

    /**
     * @brief Период выполнения быстрой сервисной задачи, мс.
     */
    static const uint32_t LOOP_100_MS = 100UL;

    /**
     * @brief Период выполнения задачи индикации связи, мс.
     */
    static const uint32_t LOOP_500_MS = 500UL;

    /**
     * @brief Таймаут отсутствия Modbus-обмена, мс.
     */
    static const uint32_t CONNECTION_TIMEOUT_MS = 10000UL;

    /**
     * @brief Задержка фиксации окончания импульса тока, мс.
     */
    static const uint32_t STOP_DELAY_MS = 5000UL;

    /**
     * @brief Задержка перед переходом в загрузчик после принятия команды, мс.
     */
    static const uint32_t BOOTLOADER_JUMP_DELAY_MS = 50UL;

    /* -------------------------------------------------------------------------
     * Загрузчик
     * ---------------------------------------------------------------------- */

    /**
     * @brief Стартовый адрес загрузчика в байтах.
     */
    static const uint16_t BOOT_START_ADDR_BYTES = 0x7000U;

    /**
     * @brief Стартовый адрес загрузчика в словах команд AVR.
     */
    static const uint16_t BOOT_START_ADDR_WORDS =
        static_cast<uint16_t>(BOOT_START_ADDR_BYTES / 2U);

    /**
     * @brief Тип указателя на точку входа загрузчика.
     */
    typedef void (*BootEntryPtr)(void);

    /* -------------------------------------------------------------------------
     * EEPROM-карта
     * ---------------------------------------------------------------------- */

    /**
     * @brief Адрес серийного номера в EEPROM.
     */
    static const uint16_t EEPROM_SERIAL_NUMBER_ADDR = 72U;

    /**
     * @brief Адрес Modbus-адреса устройства в EEPROM.
     */
    static const uint16_t EEPROM_MODBUS_ADDRESS_ADDR = 100U;

    /**
     * @brief Адрес порога фиксации максимального тока в EEPROM.
     */
    static const uint16_t EEPROM_MOTOR_STOP_THRESHOLD_ADDR = 110U;

    /**
     * @brief Адрес режима аналоговых входов в EEPROM.
     */
    static const uint16_t EEPROM_AI_MODE_ADDR = 115U;

    /**
     * @brief Базовое смещение области калибровочных параметров в EEPROM.
     */
    static const uint16_t EEPROM_CALIBRATION_START = 3U;

    /**
     * @brief Размер EEPROM-блока калибровки одного канала, байт.
     */
    static const uint16_t EEPROM_CALIBRATION_SHIFT = 8U;

    /**
     * @brief Начальные адреса EEPROM-блоков калибровки каналов.
     *
     * @details
     * Индексация массива соответствует прикладной нумерации каналов:
     * используются элементы с индексами 1...8.
     */
    static const uint16_t EEPROM_START_ADDRESS[NUM_CHANNELS + 1U] =
    {
        1U,
        static_cast<uint16_t>(EEPROM_CALIBRATION_START + EEPROM_CALIBRATION_SHIFT),
        static_cast<uint16_t>(EEPROM_CALIBRATION_START + 2U * EEPROM_CALIBRATION_SHIFT),
        static_cast<uint16_t>(EEPROM_CALIBRATION_START + 3U * EEPROM_CALIBRATION_SHIFT),
        static_cast<uint16_t>(EEPROM_CALIBRATION_START + 4U * EEPROM_CALIBRATION_SHIFT),
        static_cast<uint16_t>(EEPROM_CALIBRATION_START + 5U * EEPROM_CALIBRATION_SHIFT),
        static_cast<uint16_t>(EEPROM_CALIBRATION_START + 6U * EEPROM_CALIBRATION_SHIFT),
        static_cast<uint16_t>(EEPROM_CALIBRATION_START + 7U * EEPROM_CALIBRATION_SHIFT),
        static_cast<uint16_t>(EEPROM_CALIBRATION_START + 8U * EEPROM_CALIBRATION_SHIFT)
    };

    /* -------------------------------------------------------------------------
     * Состояния
     * ---------------------------------------------------------------------- */

    /**
     * @brief Состояние обработки импульса по аналоговому каналу.
     */
    enum ChannelState
    {
        inActive = 0, /**< Канал не находится в активном импульсе. */
        waitStart = 1, /**< Обнаружено превышение порога, выполняется подтверждение. */
        Active = 2 /**< Канал находится в активном импульсе. */
    };

    /**
     * @brief Идентификатор микросхемы ADS1115.
     */
    enum AdcChipId
    {
        AIC_1 = 0, /**< Первый ADS1115. */
        AIC_2 = 1 /**< Второй ADS1115. */
    };

    /**
     * @brief Состояние автомата чтения ADS1115.
     */
    enum AdsState
    {
        requestAI = 0, /**< Запуск одиночного преобразования. */
        delayADC = 1, /**< Минимальная задержка после запуска преобразования. */
        checkAIReady = 2, /**< Проверка готовности результата. */
        getAIx = 3, /**< Чтение результата преобразования. */
        calcOutput = 4 /**< Обновление битовых масок индикации и Modbus. */
    };

    /* -------------------------------------------------------------------------
     * Объекты
     * ---------------------------------------------------------------------- */

    /**
     * @brief Объект ведомого устройства Modbus RTU.
     */
    static IGAS_mb_485 IGAS_mb_X;

    /**
     * @brief Первый преобразователь ADS1115.
     */
    static ADS1115 ADS_1(0x49U);

    /**
     * @brief Второй преобразователь ADS1115.
     */
    static ADS1115 ADS_2(0x48U);

    /* -------------------------------------------------------------------------
     * Переменные приложения
     * ---------------------------------------------------------------------- */

    /**
     * @brief Текущий Modbus-адрес устройства.
     */
    static uint8_t slaveAddr = 0U;

    /**
     * @brief Флаг разрешения записи защищённых параметров.
     */
    static uint8_t pinCode = 0U;

    /**
     * @brief Флаг запроса программной перезагрузки.
     */
    static uint8_t rebootRequested = 0U;

    /**
     * @brief Текущее системное время, мс.
     */
    static uint32_t currentTime = 0UL;

    /**
     * @brief Время последнего корректного Modbus-обмена.
     */
    static uint32_t resetWDtimer = 0UL;

    /**
     * @brief Время последнего выполнения быстрой сервисной задачи.
     */
    static uint32_t loop100StartTime = 0UL;

    /**
     * @brief Время последнего выполнения задачи индикации.
     */
    static uint32_t loop500StartTime = 0UL;

    /**
     * @brief Время получения запроса на переход в загрузчик.
     */
    static uint32_t bootloaderRequestTime = 0UL;

    /**
     * @brief Флаг состояния связи по Modbus.
     */
    static uint8_t x2xState = 0U;

    /**
     * @brief Порог фиксации максимального значения по каналу.
     */
    static float MOTOR_STOP_THRESHOLD = 0.5F;

    /**
     * @brief Калибровочные значения Y для каналов.
     */
    static int16_t gasConcentrationY[2U * NUM_CHANNELS + 1U];

    /**
     * @brief Калибровочные значения X для каналов.
     */
    static int16_t sensorAnalogX[2U * NUM_CHANNELS + 1U];

    /**
     * @brief Коэффициенты наклона калибровочных прямых.
     */
    static float k[NUM_CHANNELS + 1U];

    /**
     * @brief Смещения калибровочных прямых.
     */
    static float b[NUM_CHANNELS + 1U];

    /**
     * @brief Сырые значения аналоговых каналов и битовая маска состояния.
     *
     * @details
     * Элемент 0 используется как битовая маска.
     * Элементы 1...8 используются для значений каналов.
     */
    static int16_t AIData[NUM_CHANNELS + 1U];

    /**
     * @brief Маска режимов аналоговых входов.
     */
    static uint8_t AI_MODE = 0xFFU;

    /**
     * @brief Состояния обработки импульсов по каналам.
     */
    static uint8_t channelActive[NUM_CHANNELS + 1U];

    /**
     * @brief Зафиксированные максимальные значения по каналам.
     */
    static float maxCurrArray[NUM_CHANNELS + 1U];

    /**
     * @brief Текущие максимальные значения активных импульсов.
     */
    static float buffer[NUM_CHANNELS + 1U];

    /**
     * @brief Таймеры обработки активных импульсов по каналам.
     */
    static uint32_t stopTimers[NUM_CHANNELS + 1U];

    /**
     * @brief Калиброванные значения аналоговых каналов.
     */
    static float ADC_Converted[NUM_CHANNELS + 1U];

    /**
     * @brief Буфер перестановки каналов для регистра индикации.
     */
    static int16_t ledData[NUM_CHANNELS + 1U];

    /**
     * @brief Последнее переданное значение регистра индикации.
     */
    static uint8_t AIBuffer = 0U;

    /**
     * @brief Состояния автоматов чтения ADS1115.
     */
    static uint8_t ADSstate[2] = { requestAI, requestAI };

    /**
     * @brief Текущие номера каналов для чтения ADS1115.
     */
    static uint8_t AIX[2] = { 0U, 0U };

    /**
     * @brief Время запуска текущих преобразований ADS1115.
     */
    static uint32_t startAItime[2] = { 0UL, 0UL };

    /* -------------------------------------------------------------------------
     * Предварительные объявления
     * ---------------------------------------------------------------------- */

    /**
     * @brief Делегат доступа к карте Holding-регистров Modbus.
     *
     * @param regs Буфер регистров ответа.
     * @param startAddr Стартовый адрес регистра.
     * @param regCount Количество регистров.
     * @param value Значение при операции записи.
     */
    void modbusProceed(int16_t* regs, int16_t startAddr, int16_t regCount, int16_t value);

    /* -------------------------------------------------------------------------
     * EEPROM helpers
     * ---------------------------------------------------------------------- */

    /**
     * @brief Читает байт из EEPROM.
     *
     * @param address Адрес EEPROM.
     *
     * @return Прочитанное значение.
     */
    static uint8_t eepromReadByte(uint16_t address)
    {
        return eeprom_read_byte(reinterpret_cast<const uint8_t*>(address));
    }

    /**
     * @brief Обновляет байт в EEPROM.
     *
     * @param address Адрес EEPROM.
     * @param value Записываемое значение.
     */
    static void eepromUpdateByte(uint16_t address, uint8_t value)
    {
        eeprom_update_byte(reinterpret_cast<uint8_t*>(address), value);
    }

    /**
     * @brief Читает шестнадцатибитное знаковое значение из EEPROM.
     *
     * @param address Адрес старшего байта.
     *
     * @return Прочитанное значение.
     */
    static int16_t eepromReadInt(uint16_t address)
    {
        const uint16_t hi = eepromReadByte(address);
        const uint16_t lo = eepromReadByte(static_cast<uint16_t>(address + 1U));

        return static_cast<int16_t>((hi << 8) | lo);
    }

    /**
     * @brief Обновляет шестнадцатибитное знаковое значение в EEPROM.
     *
     * @param address Адрес старшего байта.
     * @param value Записываемое значение.
     */
    static void eepromUpdateInt(uint16_t address, int16_t value)
    {
        const uint16_t raw = static_cast<uint16_t>(value);

        eepromUpdateByte(address, static_cast<uint8_t>(raw >> 8));
        eepromUpdateByte(
            static_cast<uint16_t>(address + 1U),
            static_cast<uint8_t>(raw & 0x00FFU)
        );
    }

    /**
     * @brief Читает значение float из EEPROM.
     *
     * @param address Адрес первого байта значения.
     * @param defaultValue Значение по умолчанию при пустой ячейке EEPROM.
     *
     * @return Прочитанное значение или значение по умолчанию.
     */
    static float eepromReadFloat(uint16_t address, float defaultValue)
    {
        union
        {
            float f;
            uint8_t b[4];
        } u;

        uint8_t empty = 1U;

        for (uint8_t i = 0U; i < 4U; ++i)
        {
            u.b[i] = eepromReadByte(static_cast<uint16_t>(address + i));

            if (u.b[i] != 0xFFU)
            {
                empty = 0U;
            }
        }

        if (empty != 0U)
        {
            return defaultValue;
        }

        return u.f;
    }

    /**
     * @brief Обновляет один шестнадцатибитный фрагмент EEPROM-представления float.
     *
     * @param address Адрес старшего байта фрагмента.
     * @param raw Записываемое шестнадцатибитное значение.
     */
    static void eepromUpdateFloatRegisterPart(uint16_t address, uint16_t raw)
    {
        eepromUpdateByte(address, static_cast<uint8_t>(raw >> 8));
        eepromUpdateByte(
            static_cast<uint16_t>(address + 1U),
            static_cast<uint8_t>(raw & 0x00FFU)
        );
    }

    /* -------------------------------------------------------------------------
     * Float / Modbus helpers
     * ---------------------------------------------------------------------- */

    /**
     * @brief Возвращает один Modbus-регистр из IEEE-754 значения float.
     *
     * @param value Значение float.
     * @param order Индекс шестнадцатибитного слова: 0 или 1.
     *
     * @return Шестнадцатибитное слово для передачи через Modbus.
     */
    static int16_t convertFloat(float value, uint8_t order)
    {
        union
        {
            float flt;
            int32_t i32;
            uint8_t bytes[4];
        } u;

        u.flt = value;

        return static_cast<int16_t>(
            (static_cast<uint16_t>(u.bytes[1U + 2U * order]) << 8) |
            static_cast<uint16_t>(u.bytes[2U * order])
        );
    }

    /**
     * @brief Возвращает слово float-значения по адресу регистра пары.
     *
     * @param value Значение float.
     * @param firstRegister Первый регистр пары в карте Modbus.
     * @param registerAddr Текущий адрес регистра.
     *
     * @return Старшее или младшее слово float-значения.
     */
    static int16_t floatRegisterByPair(
        float value,
        uint16_t firstRegister,
        uint16_t registerAddr
    )
    {
        const uint8_t order =
            static_cast<uint8_t>((registerAddr - firstRegister) & 0x01U);

        return convertFloat(value, order);
    }

    /**
     * @brief Возвращает модуль значения int16_t с защитой от переполнения.
     *
     * @param value Исходное значение.
     *
     * @return Модуль значения, ограниченный диапазоном int16_t.
     */
    static int16_t absInt16ToInt16(int16_t value)
    {
        if (value >= 0)
        {
            return value;
        }

        if (value == static_cast<int16_t>(0x8000))
        {
            return 32767;
        }

        return static_cast<int16_t>(-value);
    }

    /* -------------------------------------------------------------------------
     * Калибровка
     * ---------------------------------------------------------------------- */

    /**
     * @brief Рассчитывает коэффициенты линейной калибровки канала.
     *
     * @param channel Номер канала в диапазоне 1...8.
     */
    static void calculateCalibration(uint8_t channel)
    {
        const int16_t x1 =
            sensorAnalogX[static_cast<uint8_t>(2U * channel - 1U)];

        const int16_t x2 =
            sensorAnalogX[static_cast<uint8_t>(2U * channel)];

        const int16_t y1 =
            gasConcentrationY[static_cast<uint8_t>(2U * channel - 1U)];

        const int16_t y2 =
            gasConcentrationY[static_cast<uint8_t>(2U * channel)];

        if (x2 == x1)
        {
            k[channel] = 1.0F;
            b[channel] = 0.0F;
            return;
        }

        k[channel] =
            1.0F * static_cast<float>(y2 - y1) / static_cast<float>(x2 - x1);

        b[channel] =
            static_cast<float>(x1) - static_cast<float>(y1) / k[channel];
    }

    /**
     * @brief Сохраняет калибровочные точки канала в EEPROM.
     *
     * @param channel Номер канала в диапазоне 1...8.
     */
    static void calibrToEEPROM(uint8_t channel)
    {
        calculateCalibration(channel);

        const uint16_t base = EEPROM_START_ADDRESS[channel];

        eepromUpdateInt(
            base,
            sensorAnalogX[static_cast<uint8_t>(2U * channel)]
        );

        eepromUpdateInt(
            static_cast<uint16_t>(base + 2U),
            gasConcentrationY[static_cast<uint8_t>(2U * channel)]
        );

        eepromUpdateInt(
            static_cast<uint16_t>(base + 4U),
            sensorAnalogX[static_cast<uint8_t>(2U * channel - 1U)]
        );

        eepromUpdateInt(
            static_cast<uint16_t>(base + 6U),
            gasConcentrationY[static_cast<uint8_t>(2U * channel - 1U)]
        );
    }

    /**
     * @brief Загружает калибровочные точки канала из EEPROM.
     *
     * @param channel Номер канала в диапазоне 1...8.
     */
    static void calibrFromEEPROM(uint8_t channel)
    {
        const uint16_t base = EEPROM_START_ADDRESS[channel];

        sensorAnalogX[static_cast<uint8_t>(2U * channel)] =
            eepromReadInt(base);

        gasConcentrationY[static_cast<uint8_t>(2U * channel)] =
            eepromReadInt(static_cast<uint16_t>(base + 2U));

        sensorAnalogX[static_cast<uint8_t>(2U * channel - 1U)] =
            eepromReadInt(static_cast<uint16_t>(base + 4U));

        gasConcentrationY[static_cast<uint8_t>(2U * channel - 1U)] =
            eepromReadInt(static_cast<uint16_t>(base + 6U));

        calculateCalibration(channel);
    }

    /**
     * @brief Пересчитывает код АЦП в калиброванное значение канала.
     *
     * @param channel Номер канала в диапазоне 1...8.
     * @param value Код АЦП.
     *
     * @return Калиброванное значение.
     */
    static float sensorToConcentration(uint8_t channel, int16_t value)
    {
        return k[channel] * (static_cast<float>(value) - b[channel]);
    }

    /* -------------------------------------------------------------------------
     * Системные действия
     * ---------------------------------------------------------------------- */

    /**
     * @brief Выполняет программную перезагрузку через watchdog.
     */
    static void forceWatchdogReset(void)
    {
        wdt_disable();
        wdt_enable(WDTO_15MS);

        for (;;)
        {
            /*
             * Ожидание аппаратного сброса.
             */
        }
    }

    /**
     * @brief Выполняет переход в загрузчик.
     *
     * @details
     * Перед переходом отключаются основные аппаратные интерфейсы,
     * запрещаются прерывания и переключается таблица векторов прерываний.
     */
    static void jumpToBootloader(void)
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

        asm volatile ("clr __zero_reg__");

        SP = RAMEND;

        MCUCR = static_cast<uint8_t>(_BV(IVCE));
        MCUCR = static_cast<uint8_t>(_BV(IVSEL));

        reinterpret_cast<BootEntryPtr>(BOOT_START_ADDR_WORDS)();
    }

    /* -------------------------------------------------------------------------
     * Периодические задачи
     * ---------------------------------------------------------------------- */

    /**
     * @brief Выполняет быструю сервисную задачу приложения.
     *
     * @details
     * Задача обрабатывает отложенную перезагрузку и отложенный переход
     * в загрузчик.
     */
    static void loop100Task(void)
    {
        if (static_cast<uint32_t>(currentTime - loop100StartTime) >= LOOP_100_MS)
        {
            loop100StartTime = currentTime;

            if (rebootRequested != 0U)
            {
                forceWatchdogReset();
            }

            if ((bootloaderRequestTime != 0UL) &&
                (static_cast<uint32_t>(currentTime - bootloaderRequestTime) >
                 BOOTLOADER_JUMP_DELAY_MS))
            {
                bootloaderRequestTime = 0UL;
                jumpToBootloader();

                for (;;)
                {
                    /*
                     * Защитный цикл на случай возврата из загрузчика.
                     */
                }
            }
        }
    }

    /**
     * @brief Выполняет задачу контроля связи и индикации состояния.
     */
    static void loop500Task(void)
    {
        if (static_cast<uint32_t>(currentTime - loop500StartTime) >= LOOP_500_MS)
        {
            loop500StartTime = currentTime;

            if (static_cast<uint32_t>(currentTime - resetWDtimer) >
                CONNECTION_TIMEOUT_MS)
            {
                x2xState = 0U;
            }

            if (x2xState == 0U)
            {
                lai_board::statusLedToggle();
            }
        }
    }

    /* -------------------------------------------------------------------------
     * Логика LAI
     * ---------------------------------------------------------------------- */

    /**
     * @brief Инициализирует интерфейсы обмена.
     *
     * @details
     * SPI инициализируется после чтения аппаратного адреса платы, поскольку
     * линия PB2 используется при старте как вход адресного переключателя,
     * а затем переводится в состояние выхода SPI SS.
     */
    static void initCommunication(void)
    {
        hal::spi_init();

        IGAS_mb_X.initMB(MODBUS_BAUDRATE, SERIAL_8N1, MODBUS_REGISTER_LIMIT);
        IGAS_mb_X.setDelegate(modbusProceed);
        IGAS_mb_X.slave = slaveAddr;
    }

    /**
     * @brief Обрабатывает фиксацию максимального значения по каналам.
     */
    static void processMotorCurrent(void)
    {
        for (uint8_t i = 1U; i <= NUM_CHANNELS; ++i)
        {
            const int16_t adcBuffer =
                absInt16ToInt16(AIData[static_cast<uint8_t>(9U - i)]);

            ADC_Converted[i] = sensorToConcentration(i, adcBuffer);

            if (ADC_Converted[i] > MOTOR_STOP_THRESHOLD)
            {
                switch (channelActive[i])
                {
                    case inActive:
                        channelActive[i] = waitStart;
                        buffer[i] = 0.0F;
                        stopTimers[i] = currentTime;
                        break;

                    case waitStart:
                        if (static_cast<uint32_t>(currentTime - stopTimers[i]) >
                            500UL)
                        {
                            channelActive[i] = Active;
                            stopTimers[i] = currentTime;
                        }
                        break;

                    case Active:
                        if (ADC_Converted[i] > buffer[i])
                        {
                            buffer[i] = ADC_Converted[i];
                        }

                        stopTimers[i] = currentTime;
                        break;

                    default:
                        channelActive[i] = inActive;
                        break;
                }
            }
            else
            {
                if (channelActive[i] == Active)
                {
                    if (static_cast<uint32_t>(currentTime - stopTimers[i]) >
                        STOP_DELAY_MS)
                    {
                        channelActive[i] = inActive;
                        maxCurrArray[i] = buffer[i];
                        buffer[i] = 0.0F;
                        stopTimers[i] = 0UL;
                    }
                }
            }
        }
    }

    /**
     * @brief Обслуживает автомат чтения одного ADS1115.
     *
     * @param chipID Идентификатор преобразователя.
     */
    static void handleConversion(uint8_t chipID)
    {
        ADS1115* ads = 0;

        switch (chipID)
        {
            case AIC_1:
                ads = &ADS_1;
                break;

            case AIC_2:
                ads = &ADS_2;
                break;

            default:
                return;
        }

        switch (ADSstate[chipID])
        {
            case requestAI:
                (void)ads->startSingleShot(AIX[chipID]);
                startAItime[chipID] = currentTime;
                ADSstate[chipID] = delayADC;
                break;

            case delayADC:
                if (static_cast<uint32_t>(currentTime - startAItime[chipID]) >
                    2UL)
                {
                    ADSstate[chipID] = checkAIReady;
                }
                break;

            case checkAIReady:
                if (!ads->isBusy())
                {
                    ADSstate[chipID] = getAIx;
                }
                break;

            case getAIx:
                AIData[static_cast<uint8_t>(8U - 4U * chipID - AIX[chipID])] =
                    ads->getValue();

                ++AIX[chipID];

                if (AIX[chipID] > 3U)
                {
                    AIX[chipID] = 0U;
                }

                ADSstate[chipID] = calcOutput;
                break;

            case calcOutput:
            {
                uint8_t ledBit = 0U;
                uint8_t mbBit = 0U;

                for (uint8_t i = 1U; i <= 4U; ++i)
                {
                    ledData[static_cast<uint8_t>(2U * i)] =
                        AIData[static_cast<uint8_t>(2U * i - 1U)];

                    ledData[static_cast<uint8_t>(2U * i - 1U)] =
                        AIData[static_cast<uint8_t>(2U * i)];
                }

                for (uint8_t i = 0U; i < NUM_CHANNELS; ++i)
                {
                    if (ledData[static_cast<uint8_t>(i + 1U)] > 5000)
                    {
                        ledBit = static_cast<uint8_t>(ledBit | (1U << i));
                    }

                    if (AIData[static_cast<uint8_t>(i + 1U)] > 5000)
                    {
                        mbBit = static_cast<uint8_t>(mbBit | (1U << (7U - i)));
                    }
                }

                if (AIBuffer != ledBit)
                {
                    AIBuffer = ledBit;

                    lai_board::writeLedRegister(AIBuffer);

                    AIData[0] = static_cast<int16_t>(mbBit);
                    ledData[0] = AIBuffer;
                }

                ADSstate[chipID] = requestAI;
                break;
            }

            default:
                ADSstate[chipID] = requestAI;
                break;
        }
    }

    /* -------------------------------------------------------------------------
     * Карта Modbus
     * ---------------------------------------------------------------------- */

    /**
     * @brief Обрабатывает доступ к одному Holding-регистру.
     *
     * @param registerAddr Адрес Holding-регистра.
     * @param writeReg true — операция записи; false — операция чтения.
     * @param value Значение при операции записи.
     *
     * @return Значение регистра для ответа Modbus.
     */
    static int16_t accessRegister(uint16_t registerAddr, bool writeReg, int16_t value)
    {
        const uint16_t rawValue = static_cast<uint16_t>(value);

        switch (registerAddr)
        {
            case 0:
                return AIData[0];

            case 1 ... 16:
            {
                const uint8_t channel =
                    static_cast<uint8_t>(9U - ((registerAddr + 1U) / 2U));

                return floatRegisterByPair(ADC_Converted[channel], 1U, registerAddr);
            }

            case 39:
                return eepromReadInt(EEPROM_SERIAL_NUMBER_ADDR);

            case 40:
                return static_cast<int16_t>(FIRMWARE_DEF);

            case 51 ... 66:
            {
                const uint8_t channel =
                    static_cast<uint8_t>(((registerAddr - 51U) / 2U) + 1U);

                return floatRegisterByPair(maxCurrArray[channel], 51U, registerAddr);
            }

            case 90:
                if (writeReg)
                {
                    for (uint8_t j = 1U; j <= NUM_CHANNELS; ++j)
                    {
                        calibrToEEPROM(j);
                    }
                }

                return 0;

            case 100:
                if (writeReg)
                {
                    slaveAddr = static_cast<uint8_t>(rawValue & 0x00FFU);
                    eepromUpdateByte(EEPROM_MODBUS_ADDRESS_ADDR, slaveAddr);
                    IGAS_mb_X.slave = slaveAddr;
                }

                return static_cast<int16_t>(slaveAddr);

            case 101:
                if (writeReg && (pinCode != 0U))
                {
                    eepromUpdateFloatRegisterPart(
                        static_cast<uint16_t>(EEPROM_MOTOR_STOP_THRESHOLD_ADDR + 2U),
                        rawValue
                    );
                }

                return convertFloat(MOTOR_STOP_THRESHOLD, 0U);

            case 102:
                if (writeReg && (pinCode != 0U))
                {
                    eepromUpdateFloatRegisterPart(
                        EEPROM_MOTOR_STOP_THRESHOLD_ADDR,
                        rawValue
                    );

                    MOTOR_STOP_THRESHOLD =
                        eepromReadFloat(EEPROM_MOTOR_STOP_THRESHOLD_ADDR, 0.5F);
                }

                return convertFloat(MOTOR_STOP_THRESHOLD, 1U);

            case 115:
                if (writeReg && (pinCode != 0U))
                {
                    AI_MODE = static_cast<uint8_t>(rawValue & 0x00FFU);
                    eepromUpdateByte(EEPROM_AI_MODE_ADDR, AI_MODE);
                    lai_board::writeModeRegister(AI_MODE);
                }

                return static_cast<int16_t>(AI_MODE);

            case 201 ... 216:
                return sensorAnalogX[registerAddr - 200U];

            case 301 ... 316:
                return gasConcentrationY[registerAddr - 300U];

            case 401 ... 416:
                if (writeReg)
                {
                    sensorAnalogX[registerAddr - 400U] = value;
                }

                return sensorAnalogX[registerAddr - 400U];

            case 501 ... 516:
                if (writeReg)
                {
                    gasConcentrationY[registerAddr - 500U] = value;
                }

                return gasConcentrationY[registerAddr - 500U];

            case 594:
                if (writeReg && (rawValue == 1U))
                {
                    rebootRequested = 1U;
                }

                return 0;

            case 600:
                return AIData[0];

            case 601 ... 608:
                return AIData[609U - registerAddr];

            case 611 ... 618:
                return static_cast<int16_t>(channelActive[619U - registerAddr]);

            case 621 ... 636:
            {
                const uint8_t channel =
                    static_cast<uint8_t>(((registerAddr - 621U) / 2U) + 1U);

                return floatRegisterByPair(buffer[channel], 621U, registerAddr);
            }

            case 701 ... 708:
            {
                const uint8_t channel =
                    static_cast<uint8_t>(709U - registerAddr);

                return static_cast<int16_t>(100.0F * ADC_Converted[channel]);
            }

            case 800:
                if (writeReg)
                {
                    pinCode = (rawValue == 2210U) ? 1U : 0U;
                }

                return static_cast<int16_t>(pinCode);

            case 900:
                if (writeReg)
                {
                    bootloaderRequestTime = currentTime;
                }

                return (bootloaderRequestTime != 0UL) ? 1 : 0;

            case 1000:
                return 4149;

            case 1001:
                return 1118;

            case 1100:
                return 2;

            case 1101:
                return 171;

            default:
                return -1;
        }
    }

    /**
     * @brief Делегат карты Holding-регистров Modbus.
     *
     * @param regs Буфер регистров ответа.
     * @param startAddr Стартовый адрес регистра.
     * @param regCount Количество регистров.
     * @param value Значение при операции записи.
     */
    void modbusProceed(
        int16_t* regs,
        int16_t startAddr,
        int16_t regCount,
        int16_t value
    )
    {
        if ((regs == 0) || (regCount <= 0))
        {
            return;
        }

        const bool writeReg =
            ((IGAS_mb_X.func == static_cast<uint8_t>(FC_WRITE_REGS)) ||
             (IGAS_mb_X.func == static_cast<uint8_t>(FC_WRITE_REG)));

        resetWDtimer = currentTime;

        if (x2xState == 0U)
        {
            x2xState = 1U;
            lai_board::statusLedWrite(true);
        }

        for (int16_t i = 0; i < regCount; ++i)
        {
            const uint16_t registerAddr =
                static_cast<uint16_t>(startAddr + i);

            regs[i] = accessRegister(registerAddr, writeReg, value);
        }
    }
}

/* ----------------------------------------------------------------------------
 * Публичные функции приложения
 * ------------------------------------------------------------------------- */

void setup(void)
{
    wdt_reset();
    MCUSR &= static_cast<uint8_t>(~_BV(WDRF));

    wdt_disable();
    wdt_enable(WDTO_4S);

    delay(20UL);

    lai_board::initIo();

    slaveAddr = lai_board::readSlaveAddress();

    if (slaveAddr == 0U)
    {
        slaveAddr = 1U;
    }

    delay(20UL);

    initCommunication();

    currentTime = millis();
    resetWDtimer = currentTime;
    loop100StartTime = currentTime;
    loop500StartTime = currentTime;

    (void)ADS_1.begin();
    (void)ADS_2.begin();

    ADS_1.setGain(0U);
    ADS_2.setGain(0U);

    ADS_1.setMode(1U);
    ADS_2.setMode(1U);

    ADS_1.setDataRate(7U);
    ADS_2.setDataRate(7U);

    lai_board::writeLedRegister(0U);

    AI_MODE = eepromReadByte(EEPROM_AI_MODE_ADDR);
    lai_board::writeModeRegister(AI_MODE);

    lai_board::setShiftEnable(false);

    for (uint8_t i = 1U; i <= NUM_CHANNELS; ++i)
    {
        calibrFromEEPROM(i);
    }

    MOTOR_STOP_THRESHOLD =
        eepromReadFloat(EEPROM_MOTOR_STOP_THRESHOLD_ADDR, 0.5F);

    wdt_reset();
}

void loop(void)
{
    currentTime = millis();

    loop100Task();
    loop500Task();

    (void)IGAS_mb_X.update();

    handleConversion(AIC_1);
    handleConversion(AIC_2);

    processMotorCurrent();

    wdt_reset();
}