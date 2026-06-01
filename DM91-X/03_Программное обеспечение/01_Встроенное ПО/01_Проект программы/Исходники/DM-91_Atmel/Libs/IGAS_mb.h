#pragma once

#ifndef IGAS_MB_H_
#define IGAS_MB_H_

#include "compat.hpp"

#include <stdint.h>

/**
 * @file IGAS_mb.h
 * @brief Интерфейс ведомого устройства Modbus RTU по RS-485.
 *
 * @details
 * Модуль реализует транспортный уровень Modbus RTU поверх UART0.
 *
 * Поддерживаемые функции Modbus:
 * - FC03: Read Holding Registers;
 * - FC06: Write Single Register;
 * - FC16: Write Multiple Registers.
 *
 * Доступ к прикладной карте Holding-регистров выполняется через делегат,
 * назначаемый функцией IGAS_mb_485::setDelegate().
 *
 * Управление направлением RS-485 выполняется через аппаратный слой платы.
 */

/*----------------------------------------------------------------------------
 * Коды функций Modbus
 *---------------------------------------------------------------------------*/

/**
 * @brief Поддерживаемые коды функций Modbus.
 */
enum ModbusFunctionCode
{
    FC_READ_REGS  = 0x03, /**< FC03: Read Holding Registers. */
    FC_WRITE_REG  = 0x06, /**< FC06: Write Single Register. */
    FC_WRITE_REGS = 0x10  /**< FC16: Write Multiple Registers. */
};

/**
 * @brief Таблица поддерживаемых кодов функций Modbus.
 *
 * @details
 * Используется при проверке входящего запроса.
 */
static const uint8_t fsupported[] =
{
    static_cast<uint8_t>(FC_READ_REGS),
    static_cast<uint8_t>(FC_WRITE_REG),
    static_cast<uint8_t>(FC_WRITE_REGS)
};

/*----------------------------------------------------------------------------
 * Ограничения и размеры пакетов
 *---------------------------------------------------------------------------*/

/**
 * @brief Максимальное количество регистров за один запрос FC03.
 */
static const uint8_t MAX_READ_REGS = 0x0F;

/**
 * @brief Максимальное количество регистров за один запрос FC16.
 */
static const uint8_t MAX_WRITE_REGS = 0x0F;

/**
 * @brief Максимальная длина Modbus RTU кадра, байт.
 */
static const uint8_t MAX_MESSAGE_LENGTH = 40;

/**
 * @brief Минимальная длина стандартного запроса Modbus RTU, байт.
 */
static const uint8_t MIN_REQUEST_SIZE = 8;

/**
 * @brief Минимальная длина запроса FC16, байт.
 */
static const uint8_t MIN_WRITE_MULTIPLE_REQUEST_SIZE = 9;

/**
 * @brief Длина ответа FC06/FC16 без CRC, байт.
 */
static const uint8_t RESPONSE_SIZE = 6;

/**
 * @brief Длина Modbus exception-ответа без CRC, байт.
 */
static const uint8_t EXCEPTION_SIZE = 3;

/**
 * @brief Длина поля CRC16 Modbus RTU, байт.
 */
static const uint8_t CHECKSUM_SIZE = 2;

/*----------------------------------------------------------------------------
 * Коды исключений
 *---------------------------------------------------------------------------*/

/**
 * @brief Коды исключений Modbus и внутренние коды обработчика.
 *
 * @details
 * Значение NO_REPLY используется только внутри обработчика и означает,
 * что ответный кадр формировать не требуется.
 */
enum ModbusException
{
    NO_REPLY       = -1, /**< Ответ не формируется. */
    EXC_FUNC_CODE  = 1,  /**< Illegal Function. */
    EXC_ADDR_RANGE = 2,  /**< Illegal Data Address. */
    EXC_REGS_QUANT = 3,  /**< Illegal Data Value. */
    EXC_EXECUTE    = 4   /**< Slave Device Failure. */
};

/*----------------------------------------------------------------------------
 * Индексы полей Modbus RTU кадра
 *---------------------------------------------------------------------------*/

/**
 * @brief Индексы полей внутри массива Modbus RTU кадра.
 */
enum ModbusFrameIndex
{
    SLAVE = 0, /**< Адрес ведомого устройства. */
    FUNC,     /**< Код функции Modbus. */
    START_H,  /**< Старший байт стартового адреса. */
    START_L,  /**< Младший байт стартового адреса. */
    REGS_H,   /**< Старший байт количества регистров или значения FC06. */
    REGS_L,   /**< Младший байт количества регистров или значения FC06. */
    BYTE_CNT  /**< Количество байт данных в запросе FC16. */
};

/*----------------------------------------------------------------------------
 * Служебные константы
 *---------------------------------------------------------------------------*/

/**
 * @brief Флаг индикации активности передачи.
 */
static const uint8_t dotDisp = 0x08;

/**
 * @brief Количество регистров при обработке одиночной записи.
 */
static const uint8_t singleRegister = 1;

/*----------------------------------------------------------------------------
 * IGAS_mb_485
 *---------------------------------------------------------------------------*/

/**
 * @class IGAS_mb_485
 * @brief Ведомое устройство Modbus RTU по RS-485.
 *
 * @details
 * Класс обслуживает:
 * - приём Modbus RTU кадров;
 * - определение завершения кадра по межкадровой паузе;
 * - проверку CRC16;
 * - проверку slave-адреса;
 * - проверку поддерживаемых функций;
 * - обработку FC03, FC06 и FC16;
 * - формирование ответных кадров;
 * - переключение направления RS-485.
 *
 * Прикладная карта регистров реализуется вне класса и вызывается через
 * делегат, назначенный функцией setDelegate().
 */
class IGAS_mb_485
{
public:
    /**
     * @brief Создаёт объект ведомого устройства Modbus RTU.
     */
    IGAS_mb_485(void);

    /**
     * @brief Инициализирует UART и внутренние параметры Modbus RTU.
     *
     * @param COMM_BPS Скорость UART, бод.
     * @param SERIAL_MODE Формат кадра UART.
     * @param allRegs Верхняя исключающая граница адресного пространства
     * Holding-регистров.
     *
     * @details
     * При allRegs = 1001 допустимы адреса Holding-регистров 0...1000.
     */
    void initMB(
        uint32_t COMM_BPS = 1200UL,
        uint8_t SERIAL_MODE = SERIAL_8N1,
        uint16_t allRegs = 1000U
    );

    /**
     * @brief Обрабатывает входящие запросы Modbus RTU.
     *
     * @return 0, если запрос не обработан или ответ не требуется.
     * @return Код исключения, если сформирован exception-ответ.
     */
    uint8_t update(void);

    /**
     * @brief Назначает делегат доступа к прикладной карте регистров.
     *
     * @param fp Функция обработки регистров.
     *
     * @details
     * Сигнатура делегата:
     * - regs — буфер регистров ответа;
     * - start_addr — стартовый адрес;
     * - reg_count — количество регистров;
     * - value — значение при операции записи.
     */
    void setDelegate(void (*fp)(int16_t* regs, int16_t start_addr, int16_t reg_count, int16_t value));

    /**
     * @brief Отправляет массив байт без добавления CRC.
     *
     * @param query Буфер данных.
     * @param string_length Количество байт для отправки.
     *
     * @return Количество байт, поставленных в UART-драйвер.
     */
    int send(uint8_t* query, uint8_t string_length);

    /**
     * @brief Адрес ведомого устройства Modbus.
     *
     * @details
     * Устанавливается прикладным кодом после чтения аппаратного адреса платы.
     */
    uint8_t slave;

    /**
     * @brief Флаг индикации активности передачи.
     */
    uint8_t transmitLED;

    /**
     * @brief Последний обработанный код функции Modbus.
     *
     * @details
     * Используется прикладным делегатом для различения операций чтения и записи.
     */
    uint8_t func;

    /**
     * @brief Межкадровая пауза Modbus RTU, мс.
     *
     * @details
     * Используется для определения завершения входящего кадра.
     */
    uint16_t interframe_delay;

private:
    /**
     * @brief Рассчитывает CRC16 Modbus RTU.
     *
     * @param buf Буфер данных.
     * @param start Начальный индекс расчёта.
     * @param cnt Конечный индекс расчёта, не включительно.
     *
     * @return CRC16.
     */
    uint16_t crc(uint8_t* buf, uint8_t start, uint8_t cnt);

    /**
     * @brief Формирует заголовок ответа FC03.
     *
     * @param slave Адрес ведомого устройства.
     * @param function Код функции Modbus.
     * @param count Количество регистров.
     * @param packet Буфер формируемого ответа.
     */
    void build_read_packet(
        uint8_t slave,
        uint8_t function,
        uint8_t count,
        uint8_t* packet
    );

    /**
     * @brief Формирует ответ FC16 без CRC.
     *
     * @param slave Адрес ведомого устройства.
     * @param function Код функции Modbus.
     * @param start_addr Стартовый адрес регистров.
     * @param count Количество записанных регистров.
     * @param packet Буфер формируемого ответа.
     */
    void build_write_packet(
        uint8_t slave,
        uint8_t function,
        uint16_t start_addr,
        uint8_t count,
        uint8_t* packet
    );

    /**
     * @brief Формирует ответ FC06 без CRC.
     *
     * @param slave Адрес ведомого устройства.
     * @param function Код функции Modbus.
     * @param write_addr Адрес записываемого регистра.
     * @param reg_val Записываемое значение.
     * @param packet Буфер формируемого ответа.
     */
    void build_write_single_packet(
        uint8_t slave,
        uint8_t function,
        uint16_t write_addr,
        uint16_t reg_val,
        uint8_t* packet
    );

    /**
     * @brief Формирует Modbus exception-ответ без CRC.
     *
     * @param slave Адрес ведомого устройства.
     * @param function Код функции исходного запроса.
     * @param exception Код исключения Modbus.
     * @param packet Буфер формируемого ответа.
     */
    void build_error_packet(
        uint8_t slave,
        uint8_t function,
        uint8_t exception,
        uint8_t* packet
    );

    /**
     * @brief Добавляет CRC16 в конец пакета.
     *
     * @param packet Буфер пакета.
     * @param string_length Длина пакета без CRC.
     */
    void modbus_reply(uint8_t* packet, uint8_t string_length);

    /**
     * @brief Отправляет Modbus-ответ с добавлением CRC16.
     *
     * @param query Буфер ответа без CRC.
     * @param string_length Длина ответа без CRC.
     *
     * @return 0 после постановки кадра на передачу.
     */
    int send_reply(uint8_t* query, uint8_t string_length);

    /**
     * @brief Читает доступные байты из RX-буфера UART.
     *
     * @param received_string Буфер назначения.
     *
     * @return Количество принятых байт.
     * @return NO_REPLY при переполнении буфера.
     */
    int receive_request(uint8_t* received_string);

    /**
     * @brief Принимает и проверяет входящий Modbus RTU кадр.
     *
     * @param slave Ожидаемый slave-адрес.
     * @param data Буфер принятого кадра.
     *
     * @return Длина кадра при успешной проверке.
     * @return NO_REPLY при ошибке CRC, неподходящем адресе или некорректной длине.
     */
    int16_t modbus_request(uint8_t slave, uint8_t* data);

    /**
     * @brief Проверяет запрос на поддерживаемость и допустимость диапазонов.
     *
     * @param data Принятый Modbus RTU кадр.
     * @param length Длина кадра.
     *
     * @return 0 при успешной проверке.
     * @return Код Modbus exception при ошибке.
     */
    int validate_request(uint8_t* data, uint8_t length);

    /**
     * @brief Обрабатывает запись нескольких регистров через прикладной делегат.
     *
     * @param start_addr Стартовый адрес регистров.
     * @param query Принятый Modbus RTU кадр.
     * @param regs Временный буфер регистров.
     *
     * @return Количество обработанных регистров.
     */
    int write_regs(uint16_t start_addr, uint8_t* query, int16_t* regs);

    /**
     * @brief Обрабатывает функцию FC16.
     *
     * @param slave Адрес ведомого устройства.
     * @param start_addr Стартовый адрес регистров.
     * @param count Количество регистров.
     * @param query Принятый Modbus RTU кадр.
     * @param regs Временный буфер регистров.
     *
     * @return Результат отправки ответа.
     */
    int preset_multiple_registers(
        uint8_t slave,
        uint16_t start_addr,
        uint8_t count,
        uint8_t* query,
        int16_t* regs
    );

    /**
     * @brief Обрабатывает функцию FC06.
     *
     * @param slave Адрес ведомого устройства.
     * @param write_addr Адрес записываемого регистра.
     * @param query Принятый Modbus RTU кадр.
     * @param regs Временный буфер регистров.
     *
     * @return Результат отправки ответа.
     */
    int write_single_register(
        uint8_t slave,
        uint16_t write_addr,
        uint8_t* query,
        int16_t* regs
    );

    /**
     * @brief Обрабатывает функцию FC03.
     *
     * @param slave Адрес ведомого устройства.
     * @param start_addr Стартовый адрес регистров.
     * @param reg_count Количество читаемых регистров.
     * @param regs Буфер регистров ответа.
     *
     * @return Результат отправки ответа.
     */
    int read_holding_registers(
        uint8_t slave,
        uint16_t start_addr,
        uint8_t reg_count,
        int16_t* regs
    );

    /**
     * @brief Делегат доступа к прикладной карте регистров.
     */
    void (*fpAction)(int16_t*, int16_t, int16_t, int16_t);

    /**
     * @brief Вызывает делегат карты регистров, если он назначен.
     *
     * @param t1 Буфер регистров.
     * @param t2 Стартовый адрес.
     * @param t3 Количество регистров.
     * @param t4 Значение при операции записи.
     */
    void doAction(int16_t* t1, int16_t t2, int16_t t3, int16_t t4);

    /**
     * @brief Верхняя исключающая граница адресного пространства Holding-регистров.
     */
    uint16_t regLimit;

    /**
     * @brief Время последнего изменения количества принятых байт.
     */
    uint32_t Nowdt;

    /**
     * @brief Последнее зафиксированное количество байт в RX-буфере.
     */
    int16_t lastBytesReceived;
};

/**
 * @brief Дополнительное имя класса транспорта Modbus RTU.
 */
typedef IGAS_mb_485 IGAS_X2X_485;

#endif /* IGAS_MB_H_ */