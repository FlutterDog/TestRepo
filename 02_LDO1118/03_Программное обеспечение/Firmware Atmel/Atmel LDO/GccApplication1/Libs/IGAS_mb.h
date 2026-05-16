#pragma once
#ifndef IGAS_MB_H_
#define IGAS_MB_H_

#include "compat.hpp"

#include <stdint.h>

/**
 * @file IGAS_mb.h
 * @brief Заголовок транспорта Modbus RTU по RS-485 для AVR.
 *
 * @details
 * Класс IGAS_mb_485 реализует Modbus RTU slave поверх UART0.
 *
 * Поддерживаемые функции:
 * - FC03: Read Holding Registers;
 * - FC06: Write Single Register;
 * - FC16: Write Multiple Registers.
 *
 * Доступ к карте регистров выполняется через callback-делегат,
 * назначаемый функцией setDelegate().
 *
 * UART, millis(), Serial и базовые типы берутся из compat.hpp.
 *
 * @note
 * Файл не зависит от Arduino core.
 *
 * @warning
 * Управление направлением RS-485 пока оставлено внутри этой библиотеки.
 * Для LDO линия направления — PD2.
 */

/*----------------------------------------------------------------------------
 * Modbus function codes
 *---------------------------------------------------------------------------*/

/**
 * @brief Поддерживаемые коды функций Modbus.
 */
enum ModbusFunctionCode : uint8_t
{
    FC_READ_REGS  = 0x03, /**< Read Holding Registers. */
    FC_WRITE_REG  = 0x06, /**< Write Single Register. */
    FC_WRITE_REGS = 0x10  /**< Write Multiple Registers. */
};

/**
 * @brief Список поддерживаемых функций для validate_request().
 */
static const uint8_t fsupported[] =
{
    FC_READ_REGS,
    FC_WRITE_REG,
    FC_WRITE_REGS
};

/*----------------------------------------------------------------------------
 * Limits / packet sizes
 *---------------------------------------------------------------------------*/

/**
 * @brief Ограничения реализации.
 *
 * @details
 * Это проектные лимиты текущей реализации, а не полные лимиты протокола Modbus.
 */
enum : uint8_t
{
    MAX_READ_REGS      = 0x0F, /**< Максимум регистров за запрос чтения FC03. */
    MAX_WRITE_REGS     = 0x0F, /**< Максимум регистров за запрос записи FC16. */
    MAX_MESSAGE_LENGTH = 40    /**< Максимальная длина Modbus RTU кадра в байтах. */
};

/**
 * @brief Размеры служебных пакетов без учёта CRC или с учётом CRC.
 */
enum : uint8_t
{
    RESPONSE_SIZE  = 6, /**< Длина базового ответа FC06/FC16 без CRC. */
    EXCEPTION_SIZE = 3, /**< Длина exception-ответа без CRC. */
    CHECKSUM_SIZE  = 2  /**< Длина CRC16. */
};

/*----------------------------------------------------------------------------
 * Exceptions
 *---------------------------------------------------------------------------*/

/**
 * @brief Коды исключений и внутренних ошибок.
 *
 * @details
 * NO_REPLY является внутренним кодом библиотеки и не является Modbus exception.
 */
enum ModbusException : int8_t
{
    NO_REPLY       = -1, /**< Внутренняя ошибка или отсутствие ответа. */
    EXC_FUNC_CODE  = 1,  /**< Illegal Function. */
    EXC_ADDR_RANGE = 2,  /**< Illegal Data Address. */
    EXC_REGS_QUANT = 3,  /**< Illegal Data Value. */
    EXC_EXECUTE    = 4   /**< Slave Device Failure. */
};

/*----------------------------------------------------------------------------
 * Frame field positions
 *---------------------------------------------------------------------------*/

/**
 * @brief Индексы полей внутри массива Modbus RTU кадра.
 */
enum ModbusFrameIndex : uint8_t
{
    SLAVE = 0, /**< Адрес slave. */
    FUNC,      /**< Код функции. */
    START_H,   /**< Старший байт стартового адреса. */
    START_L,   /**< Младший байт стартового адреса. */
    REGS_H,    /**< Старший байт количества регистров или значения FC06. */
    REGS_L,    /**< Младший байт количества регистров или значения FC06. */
    BYTE_CNT   /**< Byte count для FC16. */
};

/*----------------------------------------------------------------------------
 * RS-485 direction control
 *---------------------------------------------------------------------------*/

/**
 * @brief Маска бита управления направлением RS-485.
 *
 * @details
 * По умолчанию используется PD2:
 * - PORTD |= transmitMode — режим передачи;
 * - PORTD &= receiveMode — режим приёма.
 *
 * Для другой платы можно переопределить IGAS_RS485_TX_MASK в project.hpp
 * до подключения IGAS_mb.h.
 */
#ifndef IGAS_RS485_TX_MASK
#  define IGAS_RS485_TX_MASK _BV(PD2)
#endif

/**
 * @brief Маска перевода RS-485 драйвера в режим передачи.
 */
static const uint8_t transmitMode =
    static_cast<uint8_t>(IGAS_RS485_TX_MASK);

/**
 * @brief Маска возврата RS-485 драйвера в режим приёма.
 */
static const uint8_t receiveMode =
    static_cast<uint8_t>(~IGAS_RS485_TX_MASK);

/**
 * @brief Бит проектной индикации передачи.
 *
 * @details
 * Оставлено для совместимости со старой логикой.
 */
static const uint8_t dotDisp = 0x08;

/**
 * @brief Константа "один регистр" для вызова делегата.
 */
static const uint8_t singleRegister = 1;

/*----------------------------------------------------------------------------
 * IGAS_mb_485
 *---------------------------------------------------------------------------*/

/**
 * @class IGAS_mb_485
 * @brief Modbus RTU slave по RS-485 поверх UART0.
 *
 * @details
 * Внешний код обязан:
 * - задать slave-адрес;
 * - назначить callback через setDelegate();
 * - вызвать initMB();
 * - регулярно вызывать update() в основном цикле.
 */
class IGAS_mb_485
{
public:
    /**
     * @brief Инициализирует UART и внутренние лимиты Modbus.
     *
     * @param COMM_BPS Скорость UART, бод.
     * @param SERIAL_MODE Формат кадра UART, например SERIAL_8N1.
     * @param allRegs Размер адресного пространства Holding-регистров.
     *
     * @details
     * allRegs трактуется как верхняя исключающая граница.
     * Например, при allRegs = 1001 допустимы адреса 0...1000.
     */
    void initMB(
        uint32_t COMM_BPS = 1200UL,
        uint8_t SERIAL_MODE = SERIAL_8N1,
        uint16_t allRegs = 1000U
    );

    /**
     * @brief Обрабатывает входящие запросы Modbus RTU.
     *
     * @return 0, если ничего не обработано или ответ не нужен.
     * @return Код исключения, если запрос некорректен и сформирован exception-ответ.
     */
    uint8_t update(void);

    /**
     * @brief Назначает делегат доступа к карте регистров.
     *
     * @param fp Функция вида: regs, start_addr, reg_count, value.
     */
    void setDelegate(void (*fp)(int16_t*, int16_t, int16_t, int16_t));

    /**
     * @brief Низкоуровневая отправка массива байт без добавления CRC.
     *
     * @param query Буфер с данными.
     * @param string_length Длина буфера.
     *
     * @return Количество байт, записанных в UART-драйвер.
     */
    int send(uint8_t* query, uint8_t string_length);

    /**
     * @brief Адрес slave-устройства.
     *
     * @details
     * Должен быть установлен прикладным кодом после чтения адресного переключателя.
     */
    uint8_t slave = 0;

    /**
     * @brief Флаг/байт проектной индикации передачи.
     */
    uint8_t transmitLED = 0;

    /**
     * @brief Последняя обработанная функция Modbus.
     *
     * @details
     * Используется прикладным callback, чтобы отличать чтение от записи.
     */
    uint8_t func = 0;

    /**
     * @brief Межкадровая пауза, мс.
     *
     * @details
     * Используется для определения завершения Modbus RTU кадра.
     */
    uint16_t interframe_delay = 2;

private:
    uint16_t crc(uint8_t* buf, uint8_t start, uint8_t cnt);

    void build_read_packet(
        uint8_t slave,
        uint8_t function,
        uint8_t count,
        uint8_t* packet
    );

    void build_write_packet(
        uint8_t slave,
        uint8_t function,
        uint16_t start_addr,
        uint8_t count,
        uint8_t* packet
    );

    void build_write_single_packet(
        uint8_t slave,
        uint8_t function,
        uint16_t write_addr,
        uint16_t reg_val,
        uint8_t* packet
    );

    void build_error_packet(
        uint8_t slave,
        uint8_t function,
        uint8_t exception,
        uint8_t* packet
    );

    void modbus_reply(uint8_t* packet, uint8_t string_length);

    int send_reply(uint8_t* query, uint8_t string_length);

    int receive_request(uint8_t* received_string);

    int16_t modbus_request(uint8_t slave, uint8_t* data);

    int validate_request(uint8_t* data, uint8_t length);

    int write_regs(uint16_t start_addr, uint8_t* query, int16_t* regs);

    int preset_multiple_registers(
        uint8_t slave,
        uint16_t start_addr,
        uint8_t count,
        uint8_t* query,
        int16_t* regs
    );

    int write_single_register(
        uint8_t slave,
        uint16_t write_addr,
        uint8_t* query,
        int16_t* regs
    );

    int read_holding_registers(
        uint8_t slave,
        uint16_t start_addr,
        uint8_t reg_count,
        int16_t* regs
    );

    /**
     * @brief Делегат доступа к карте регистров.
     */
    void (*fpAction)(int16_t*, int16_t, int16_t, int16_t) = 0;

    /**
     * @brief Вызывает прикладной делегат, если он назначен.
     */
    void doAction(int16_t* t1, int16_t t2, int16_t t3, int16_t t4);

    /**
     * @brief Верхняя исключающая граница адресного пространства Holding-регистров.
     */
    uint16_t regLimit = 0;

    /**
     * @brief Таймстамп последнего изменения количества принятых байт.
     */
    uint32_t Nowdt = 0;

    /**
     * @brief Последнее наблюдаемое количество байт в RX-буфере.
     */
    int16_t lastBytesReceived = 0;
};

/**
 * @brief Совместимое старое имя класса.
 *
 * @details
 * Оставлено, чтобы старый прикладной код мог использовать IGAS_X2X_485.
 */
typedef IGAS_mb_485 IGAS_X2X_485;

#endif // IGAS_MB_H_