#include "IGAS_mb.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

/**
 * @file IGAS_mb.cpp
 * @brief Транспорт Modbus RTU по RS-485: приём/разбор запросов и формирование ответов.
 *
 * @details
 * Реализованы функции:
 * - опрос UART0 и выделение Modbus-кадра по межкадровой паузе (interframe_delay);
 * - проверка CRC, проверка адреса slave и базовая валидация запроса;
 * - обработка FC03/FC06/FC16 через делегат (callback) fpAction;
 * - управление направлением RS-485 (перевод драйвера в TX перед отправкой и возврат в RX по TX complete ISR).
 *
 * @note
 * Код является адаптацией существующего стека. Логика разделения кадра по межкадровой паузе
 * основана на наблюдении за стабилизацией количества байт в RX-буфере.
 */

/**
 * @brief Инициализация Modbus RTU по RS-485.
 * @param COMM_BPS    Скорость UART (бод).
 * @param SERIAL_MODE Формат кадра UART (константы SERIAL_*).
 * @param allRegs     Логический размер адресного пространства Holding-регистров (верхняя граница).
 *
 * @details
 * Настраивает UART0 и включает прерывание TX Complete, которое используется для возврата
 * драйвера RS-485 в режим приёма после окончания передачи.
 */
void IGAS_mb_485::initMB(int COMM_BPS, int SERIAL_MODE, int allRegs)
{
	Serial.begin((uint32_t)COMM_BPS, (uint8_t)SERIAL_MODE);

	// TX complete interrupt: по завершении физической передачи кадра возвращаем RS-485 в RX.
	UCSR0B |= (1U << TXCIE0);

	regLimit = allRegs;
}

/**
 * @brief Обработка входящих запросов Modbus (неблокирующий опрос).
 * @return 0 если запрос не сформирован/не обработан; иначе код исключения/статус обработчика.
 *
 * @details
 * Алгоритм выделения кадра:
 * 1) Если в RX нет байт — выходим.
 * 2) Если количество байт меняется — запоминаем время последнего изменения (Nowdt) и ждём стабилизации.
 * 3) Если пауза между изменениями превысила interframe_delay — считаем, что кадр завершён.
 *
 * Далее выполняется:
 * - чтение кадра, CRC, проверка slave-id;
 * - validate_request();
 * - обработка FC03/FC06/FC16 и формирование ответа.
 */
uint8_t IGAS_mb_485::update()
{
	const int16_t length_now = (int16_t)Serial.available();
	if (length_now == 0) {
		lastBytesReceived = 0;
		return 0;
	}

	// Если драйвер сейчас в TX-режиме — входящий кадр не обслуживаем.
	// transmitMode/receiveMode — маски управления линией направления RS-485 (см. IGAS_mb.h).
	if ((PORTD & transmitMode) == transmitMode) {
		return 0;
	}

	unsigned char query[MAX_MESSAGE_LENGTH];
	unsigned char errpacket[EXCEPTION_SIZE + CHECKSUM_SIZE];

	const unsigned long t_ms = (unsigned long)millis();

	// Детектор окончания кадра: ждём межкадровую паузу (время без изменения длины RX).
	if (lastBytesReceived != length_now) {
		lastBytesReceived = length_now;
		Nowdt = t_ms;
		return 0;
	}
	if ((unsigned long)(t_ms - Nowdt) < interframe_delay) {
		return 0;
	}

	// Длина стабилизировалась и выдержана пауза — читаем кадр.
	lastBytesReceived = 0;

	const int16_t frame_len = modbus_request(slave, query);
	if (frame_len < 1) {
		return (uint8_t)frame_len;
	}

	const int exception = validate_request(query, (unsigned char)frame_len);
	if (exception) {
		build_error_packet(slave, query[FUNC], (unsigned char)exception, errpacket);
		(void)send_reply(errpacket, EXCEPTION_SIZE);
		return (uint8_t)exception;
	}

	const unsigned int start_addr =
		((unsigned int)query[START_H] << 8) | (unsigned int)query[START_L];

	// Буфер регистров-ответа/операнда. Тип int исторически использован в проекте и на AVR 16-битный.
	int regs[MAX_MESSAGE_LENGTH];

	func = query[FUNC];

	switch (query[FUNC]) {
		case FC_READ_REGS:
			return (uint8_t)read_holding_registers(slave, start_addr, query[REGS_L], regs);

		case FC_WRITE_REGS:
			return (uint8_t)preset_multiple_registers(slave, start_addr, query[REGS_L], query, regs);

		case FC_WRITE_REG:
			(void)write_single_register(slave, start_addr, query, regs);
			return 0;

		default:
			// validate_request() должен был отфильтровать неподдержанные коды.
			return 0;
	}
}

/**
 * @brief Расчёт CRC16 Modbus RTU.
 * @param buf   Буфер с данными.
 * @param start Начальный индекс.
 * @param cnt   Количество байт (конец диапазона не включительно).
 * @return CRC16 в «перевёрнутом» порядке байт (совместимо с остальной реализацией).
 *
 * @details
 * Реализация соответствует стандартному полиному 0xA001 (LSB-first).
 * В конце порядок байт разворачивается, чтобы дальнейшая упаковка/распаковка CRC
 * выполнялась одинаково во всём модуле.
 */
unsigned int IGAS_mb_485::crc(unsigned char* buf, unsigned char start, unsigned char cnt)
{
	unsigned char i, j;
	unsigned int temp = 0xFFFF;

	for (i = start; i < cnt; i++) {
		temp ^= (unsigned int)buf[i];

		for (j = 0; j < 8; j++) {
			const unsigned int lsb = temp & 0x0001U;
			temp >>= 1;
			if (lsb) {
				temp ^= 0xA001U;
			}
		}
	}

	// Swap bytes (high<->low) for the expected wire-order handling in this codebase.
	temp = (unsigned int)((temp << 8) | (temp >> 8));
	return temp;
}

/**
 * @brief Заполнение заголовка ответа FC03 (Read Holding Registers).
 */
void IGAS_mb_485::build_read_packet(unsigned char slave, unsigned char function,
                                    unsigned char count, unsigned char* packet)
{
	packet[SLAVE] = slave;
	packet[FUNC]  = function;
	packet[2]     = (unsigned char)(count * 2); // byte count
}

/**
 * @brief Заполнение заголовка ответа FC16 (Write Multiple Registers).
 */
void IGAS_mb_485::build_write_packet(unsigned char slave, unsigned char function,
                                     unsigned int start_addr, unsigned char count,
                                     unsigned char* packet)
{
	packet[SLAVE]   = slave;
	packet[FUNC]    = function;
	packet[START_H] = (unsigned char)(start_addr >> 8);
	packet[START_L] = (unsigned char)(start_addr & 0x00FF);
	packet[REGS_H]  = 0x00;
	packet[REGS_L]  = count;
}

/**
 * @brief Заполнение ответа FC06 (Write Single Register).
 */
void IGAS_mb_485::build_write_single_packet(unsigned char slave, unsigned char function,
                                            unsigned int write_addr, unsigned int reg_val,
                                            unsigned char* packet)
{
	packet[SLAVE]   = slave;
	packet[FUNC]    = function;
	packet[START_H] = (unsigned char)(write_addr >> 8);
	packet[START_L] = (unsigned char)(write_addr & 0x00FF);
	packet[REGS_H]  = (unsigned char)(reg_val >> 8);
	packet[REGS_L]  = (unsigned char)(reg_val & 0x00FF);
}

/**
 * @brief Построение Modbus exception-ответа.
 */
void IGAS_mb_485::build_error_packet(unsigned char slave, unsigned char function,
                                     unsigned char exception, unsigned char* packet)
{
	packet[SLAVE] = slave;
	packet[FUNC]  = (unsigned char)(function + 0x80);
	packet[2]     = exception;
}

/**
 * @brief Добавить CRC в конец пакета.
 * @param packet         Буфер пакета.
 * @param string_length  Длина пакета без CRC.
 */
void IGAS_mb_485::modbus_reply(unsigned char* packet, unsigned char string_length)
{
	const unsigned int temp_crc = crc(packet, 0, string_length);
	packet[string_length++] = (unsigned char)(temp_crc >> 8);
	packet[string_length++] = (unsigned char)(temp_crc & 0x00FF);
}

/**
 * @brief Отправить ответ Modbus RTU (добавляет CRC и переводит RS-485 в TX).
 */
int IGAS_mb_485::send_reply(unsigned char* query, unsigned char string_length)
{
	modbus_reply(query, string_length);
	string_length = (unsigned char)(string_length + 2);
	(void)send(query, string_length);
	return 0;
}

/**
 * @brief Физическая отправка байтов по UART и перевод RS-485 в режим передачи.
 * @return Количество байт, переданных в UART-драйвер.
 *
 * @note
 * Возврат RS-485 в RX выполняется в ISR(USART_TX_vect).
 */
int IGAS_mb_485::send(unsigned char* query, unsigned char string_length)
{
	UCSR0A |= (1U << TXC0); // сброс TXC перед стартом передачи

	const uint8_t oldSREG = SREG;
	cli();
	PORTD |= transmitMode; // RS-485: TX enable
	SREG = oldSREG;

	transmitLED = dotDisp;

	unsigned char i = 0;
	for (i = 0; i < string_length; i++) {
		Serial.write(query[i]);
	}
	return (int)i;
}

/**
 * @brief Прочитать все доступные байты из RX-буфера UART в received_string.
 * @return Количество принятых байт; NO_REPLY при переполнении буфера.
 */
int IGAS_mb_485::receive_request(unsigned char* received_string)
{
	int bytes_received = 0;

	while (Serial.available()) {
		received_string[bytes_received] = (unsigned char)Serial.read();
		bytes_received++;

		if (bytes_received >= MAX_MESSAGE_LENGTH) {
			return NO_REPLY;
		}
	}
	return bytes_received;
}

/**
 * @brief Принять Modbus-кадр, проверить CRC и адрес устройства.
 * @param slave  Ожидаемый slave-id.
 * @param data   Буфер для принятого кадра.
 * @return Длина кадра (>=1) при успехе; NO_REPLY/0 при ошибке.
 */
int16_t IGAS_mb_485::modbus_request(unsigned char slave, unsigned char* data)
{
	const int response_length = receive_request(data);
	if (response_length <= 0) {
		return (int16_t)response_length;
	}

	const unsigned int crc_calc = crc(data, 0, (unsigned char)(response_length - 2));

	unsigned int crc_received = (unsigned int)data[response_length - 2];
	crc_received = (unsigned int)((crc_received << 8) | (unsigned int)data[response_length - 1]);

	if (crc_calc != crc_received) {
		return NO_REPLY;
	}

	// Проверка slave-id. Нулевой адрес (broadcast) намеренно не поддержан в этой реализации.
	if (slave != data[SLAVE]) {
		return NO_REPLY;
	}

	return (int16_t)response_length;
}

/**
 * @brief Валидация Modbus-запроса на уровне диапазона и количества регистров.
 * @param data   Принятый кадр.
 * @param length Длина кадра.
 * @return 0 при успехе; код исключения Modbus при ошибке.
 */
int IGAS_mb_485::validate_request(unsigned char* data, unsigned char length)
{
	(void)length;

	uint16_t i;
	uint16_t fcnt = 0;
	uint16_t regs_num = 0;
	uint16_t start_addr = 0;
	unsigned char max_regs_num = 0;

	// Проверка кода функции.
	for (i = 0; i < (uint16_t)sizeof(fsupported); i++) {
		if (fsupported[i] == data[FUNC]) {
			fcnt = 1;
			break;
		}
	}
	if (fcnt == 0) {
		return EXC_FUNC_CODE;
	}

	if (data[FUNC] == FC_WRITE_REG) {
		// Для FC06 адрес регистра находится в полях START_H/START_L.
		regs_num = (uint16_t)(((uint16_t)data[START_H] << 8) | (uint16_t)data[START_L]);
		if (regs_num >= (uint16_t)regLimit) {
			return EXC_ADDR_RANGE;
		}
		return 0;
	}

	// Для FC03/FC16 количество регистров в REGS_H/REGS_L.
	regs_num = (uint16_t)(((uint16_t)data[REGS_H] << 8) | (uint16_t)data[REGS_L]);

	if (data[FUNC] == FC_READ_REGS) {
		max_regs_num = MAX_READ_REGS;
	} else if (data[FUNC] == FC_WRITE_REGS) {
		max_regs_num = MAX_WRITE_REGS;
	}

	if ((regs_num < 1U) || (regs_num > (uint16_t)max_regs_num)) {
		return EXC_REGS_QUANT;
	}

	start_addr = (uint16_t)(((uint16_t)data[START_H] << 8) | (uint16_t)data[START_L]);
	if (start_addr > (uint16_t)regLimit) {
		return EXC_ADDR_RANGE;
	}

	return 0;
}

/**
 * @brief Записать значения из Modbus-кадра в регистры через делегат.
 * @param start_addr Стартовый адрес регистров.
 * @param query      Принятый кадр.
 * @param regs       Буфер регистров/операндов (передаётся в делегат).
 * @return Количество обработанных регистров.
 */
int IGAS_mb_485::write_regs(unsigned int start_addr, unsigned char* query, int* regs)
{
	int temp = 0;
	unsigned int i = 0;

	for (i = 0; i < (unsigned int)query[REGS_L]; i++) {
		// Собираем 16-бит значение (big-endian в Modbus payload).
		temp = ((int)query[(BYTE_CNT + 1) + i * 2] << 8);
		temp |= (int)query[(BYTE_CNT + 2) + i * 2];

		doAction((int16_t*)regs, (int16_t)(start_addr + i), singleRegister, (int16_t)temp);
	}
	return (int)i;
}

/**
 * @brief FC16: запись нескольких регистров.
 */
int IGAS_mb_485::preset_multiple_registers(unsigned char slave,
                                           unsigned int start_addr,
                                           unsigned char count,
                                           unsigned char* query,
                                           int* regs)
{
	const unsigned char function = FC_WRITE_REGS;
	unsigned char packet[RESPONSE_SIZE + CHECKSUM_SIZE];

	build_write_packet(slave, function, start_addr, count, packet);

	if (write_regs(start_addr, query, regs)) {
		return send_reply(packet, RESPONSE_SIZE);
	}
	return 0;
}

/**
 * @brief FC06: запись одного регистра.
 */
int IGAS_mb_485::write_single_register(unsigned char slave,
                                       unsigned int write_addr,
                                       unsigned char* query,
                                       int* regs)
{
	const unsigned char function = FC_WRITE_REG;
	const unsigned int reg_val =
		((unsigned int)query[REGS_H] << 8) | (unsigned int)query[REGS_L];

	unsigned char packet[RESPONSE_SIZE + CHECKSUM_SIZE];
	build_write_single_packet(slave, function, write_addr, reg_val, packet);

	doAction((int16_t*)regs, (int16_t)write_addr, singleRegister, (int16_t)reg_val);
	return send_reply(packet, RESPONSE_SIZE);
}

/**
 * @brief FC03: чтение Holding-регистров.
 */
int IGAS_mb_485::read_holding_registers(unsigned char slave,
                                        unsigned int start_addr,
                                        unsigned char reg_count,
                                        int* regs)
{
	const unsigned char function = FC_READ_REGS;

	unsigned char packet[MAX_MESSAGE_LENGTH];
	int packet_size = 3;

	build_read_packet(slave, function, reg_count, packet);

	doAction((int16_t*)regs, (int16_t)start_addr, (int16_t)reg_count, 0);

	for (unsigned int i = 0; i < (unsigned int)reg_count; i++) {
		packet[packet_size++] = (unsigned char)(regs[i] >> 8);
		packet[packet_size++] = (unsigned char)(regs[i] & 0x00FF);
	}

	return send_reply(packet, (unsigned char)packet_size);
}

/**
 * @brief Назначить делегат для доступа к карте регистров (чтение/запись).
 * @param fp Указатель на функцию обработчика.
 */
void IGAS_mb_485::setDelegate(void (*fp)(int16_t*, int16_t, int16_t, int16_t))
{
	fpAction = fp;
}

/**
 * @brief Вызов делегата обработки регистров.
 *
 * @note
 * Защита от null-pointer добавлена, чтобы исключить неопределённое поведение
 * при ошибочной инициализации.
 */
void IGAS_mb_485::doAction(int16_t* t1, int16_t t2, int16_t t3, int16_t t4)
{
	if (fpAction != 0) {
		(*fpAction)(t1, t2, t3, t4);
	}
}

/**
 * @brief ISR завершения передачи UART0: возврат RS-485 драйвера в режим приёма.
 *
 * @details
 * Срабатывает, когда последний байт физически передан (TXC).
 * receiveMode — маска для снятия TX enable (см. IGAS_mb.h).
 */
ISR(USART_TX_vect)
{
	PORTD &= receiveMode;
}
