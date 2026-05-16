#pragma once
#ifndef IGAS_MB_H_
#define IGAS_MB_H_

#include <stdint.h>
#include "ADC_init.h"   // совместимость: byte, millis(), SERIAL_* (через compat.hpp)

/**
 * @file IGAS_mb.h
 * @brief Заголовок транспорта Modbus RTU по RS-485 (UART0) для AVR.
 *
 * @details
 * Класс IGAS_mb_485 реализует приём/валидацию Modbus RTU запросов и формирование ответов
 * для функций FC03/FC06/FC16. Доступ к карте регистров инкапсулирован в делегате fpAction.
 *
 * Управление направлением RS-485 выполняется через маски PORTD:
 * - при отправке драйвер переводится в TX (PORTD |= transmitMode);
 * - по окончании передачи (TX complete ISR) выполняется возврат в RX (PORTD &= receiveMode).
 */

/* ========================= Modbus function codes ========================= */

/**
 * @brief Поддерживаемые коды функций Modbus.
 */
enum ModbusFunctionCode : uint8_t {
	FC_READ_REGS  = 0x03,  /**< Read Holding Registers (FC03). */
	FC_WRITE_REG  = 0x06,  /**< Write Single Register (FC06). */
	FC_WRITE_REGS = 0x10   /**< Write Multiple Registers (FC16). */
};

/**
 * @brief Список поддерживаемых функций (для validate_request()).
 */
static const uint8_t fsupported[] = { FC_READ_REGS, FC_WRITE_REG, FC_WRITE_REGS };

/* ========================= Limits / packet sizes ========================= */

/**
 * @brief Ограничения реализации (проектные лимиты, не протокольные).
 */
enum : uint8_t {
	MAX_READ_REGS       = 0x0F, /**< Максимум регистров за запрос чтения (FC03). */
	MAX_WRITE_REGS      = 0x0F, /**< Максимум регистров за запрос записи (FC16). */
	MAX_MESSAGE_LENGTH  = 40    /**< Максимальная длина Modbus кадра (байт) в данной реализации. */
};

/**
 * @brief Размеры служебных пакетов.
 */
enum : uint8_t {
	RESPONSE_SIZE   = 6, /**< Длина базового ответа FC06/FC16 без CRC. */
	EXCEPTION_SIZE  = 3, /**< Длина exception-ответа без CRC. */
	CHECKSUM_SIZE   = 2  /**< CRC16 (2 байта). */
};

/* ========================= Exceptions ========================= */

/**
 * @brief Коды исключений/ошибок.
 *
 * @note
 * NO_REPLY используется как внутренний код (не Modbus exception), означает что ответ не формируется.
 */
enum ModbusException : int8_t {
	NO_REPLY       = -1, /**< Внутренняя ошибка/нет ответа (CRC, адрес, переполнение и т.п.). */
	EXC_FUNC_CODE  = 1,  /**< Illegal Function. */
	EXC_ADDR_RANGE = 2,  /**< Illegal Data Address. */
	EXC_REGS_QUANT = 3,  /**< Illegal Data Value (quantity). */
	EXC_EXECUTE    = 4   /**< Slave Device Failure (резерв). */
};

/* ========================= Frame field positions ========================= */

/**
 * @brief Индексы полей внутри массива кадра Modbus RTU.
 */
enum ModbusFrameIndex : uint8_t {
	SLAVE = 0,   /**< Адрес slave. */
	FUNC,        /**< Код функции. */
	START_H,     /**< Старший байт стартового адреса. */
	START_L,     /**< Младший байт стартового адреса. */
	REGS_H,      /**< Старший байт количества регистров / значения (FC06). */
	REGS_L,      /**< Младший байт количества регистров / значения (FC06). */
	BYTE_CNT     /**< Byte count (для FC16). */
};

/* ========================= RS-485 direction control ========================= */

/**
 * @brief Маска для возврата RS-485 драйвера в режим приёма.
 *
 * @details
 * PORTD &= receiveMode; снимает бит направления (TX enable).
 * Значение маски зависит от разводки платы (какой бит PORTD управляет DE/RE).
 */
static const uint8_t receiveMode  = 0b11111011;

/**
 * @brief Маска перевода RS-485 драйвера в режим передачи.
 *
 * @details
 * PORTD |= transmitMode; выставляет бит направления (TX enable).
 * Значение маски зависит от разводки платы (какой бит PORTD управляет DE/RE).
 */
static const uint8_t transmitMode = 0b00000100;

/**
 * @brief Бит "точка" на индикаторе (проектная индикация передачи).
 */
static const uint8_t dotDisp = 0x08;

/**
 * @brief Константа "один регистр" для делегата doAction().
 */
static const uint8_t singleRegister = 1;

/* ========================= IGAS_mb_485 ========================= */

/**
 * @class IGAS_mb_485
 * @brief Modbus RTU slave по RS-485 (UART0) с делегированием обработки регистров.
 *
 * @details
 * Внешний код обязан:
 * - настроить поле @ref slave (адрес устройства),
 * - задать @ref interframe_delay (пауза t3.5, мс),
 * - назначить делегат через setDelegate(),
 * - вызывать update() в основном цикле.
 */
class IGAS_mb_485
{
public:
	/**
	 * @brief Инициализация UART и внутренних лимитов Modbus.
	 * @param COMM_BPS    Скорость (бод).
	 * @param SERIAL_MODE Формат кадра (SERIAL_*).
	 * @param allRegs     Верхняя граница адресного пространства регистров.
	 */
	void initMB(int16_t COMM_BPS = 1200, int16_t SERIAL_MODE = SERIAL_8N1, int16_t allRegs = 1000);

	/**
	 * @brief Обработка входящих запросов Modbus RTU.
	 * @return 0 если ничего не обработано; иначе код исключения/статус.
	 */
	uint8_t update();

	/**
	 * @brief Назначить делегат для доступа к карте регистров.
	 * @param fp Функция вида (regs, addr, count, value).
	 */
	void setDelegate(void (*fp)(int16_t*, int16_t, int16_t, int16_t));

	/**
	 * @brief Низкоуровневая отправка массива байт (без добавления CRC).
	 * @param query         Буфер с данными.
	 * @param string_length Длина.
	 * @return Количество байт, записанных в UART-драйвер.
	 */
	int send(unsigned char* query, unsigned char string_length);

	/** @brief Адрес slave устройства. Должен быть установлен пользователем. */
	uint8_t slave = 0;

	/** @brief Флаг/байт индикации передачи (используется проектной логикой). */
	uint8_t transmitLED = 0;

	/** @brief Последняя обработанная функция Modbus. */
	uint8_t func = 0;

	/**
	 * @brief Межкадровая пауза (t3.5) в миллисекундах.
	 *
	 * @details
	 * Используется для детектора "кадр завершён" при приёме через буфер UART.
	 * Типичные значения зависят от скорости; в проекте часто используется 2 мс.
	 */
	uint16_t interframe_delay = 2;

private:
	unsigned int crc(unsigned char* buf, unsigned char start, unsigned char cnt);

	void build_read_packet(unsigned char slave, unsigned char function,
	                       unsigned char count, unsigned char* packet);

	void build_write_packet(unsigned char slave, unsigned char function,
	                        unsigned int start_addr, unsigned char count,
	                        unsigned char* packet);

	void build_write_single_packet(unsigned char slave, unsigned char function,
	                               unsigned int write_addr, unsigned int reg_val,
	                               unsigned char* packet);

	void build_error_packet(unsigned char slave, unsigned char function,
	                        unsigned char exception, unsigned char* packet);

	void modbus_reply(unsigned char* packet, unsigned char string_length);

	int send_reply(unsigned char* query, unsigned char string_length);

	int receive_request(unsigned char* received_string);

	int16_t modbus_request(unsigned char slave, unsigned char* data);

	int validate_request(unsigned char* data, unsigned char length);

	int write_regs(unsigned int start_addr, unsigned char* query, int* regs);

	int preset_multiple_registers(unsigned char slave, unsigned int start_addr,
	                              unsigned char count, unsigned char* query,
	                              int* regs);

	int write_single_register(unsigned char slave, unsigned int write_addr,
	                          unsigned char* query, int* regs);

	int read_holding_registers(unsigned char slave, unsigned int start_addr,
	                           unsigned char reg_count, int* regs);

	/**
	 * @brief Делегат доступа к регистрам (реальная карта регистров находится вне этого класса).
	 */
	void (*fpAction)(int16_t*, int16_t, int16_t, int16_t) = 0;

	void doAction(int16_t* t1, int16_t t2, int16_t t3, int16_t t4);

	/** @brief Верхняя граница адресного пространства Holding-регистров. */
	uint16_t regLimit = 0;

	/** @brief Таймстамп последнего изменения длины RX (детектор межкадровой паузы). */
	unsigned long Nowdt = 0;

	/** @brief Последнее наблюдаемое число байт в RX-буфере. */
	int16_t lastBytesReceived = 0;
};

#endif // IGAS_MB_H_
